#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "channels/psync/mutex.h"
#include "channels/psync/cv.h"

#include "channels/channel.h"

#define VIRTUAL_PAGE_SIZE   4096
#define MMAP_SIZE           (262144 * VIRTUAL_PAGE_SIZE)
#define CAPACITY            (MMAP_SIZE - sizeof(ipc_channel_mmap_shared_t))

typedef struct
{
  // These two atomics synchronize initialization and destruction
  // of the shared memory
  atomic_uint  init_spinlock;
  atomic_uint  ref_count;

#ifndef USE_SPSC_MMAP
  ipc_mutex_t  mutex;
  ipc_cv_t     cv_not_full;
  ipc_cv_t     cv_not_empty;
  size_t       head;
  size_t       tail;

  __attribute__ ((aligned(alignof(max_align_t))))
  char     data[];
#else
  __attribute__ ((aligned(128)))
  atomic_size_t head;

  __attribute__ ((aligned(128)))
  atomic_size_t tail;

  // assuming 128 is more strict then alignof(max_allign_t)
  __attribute__ ((aligned(128)))
  char     data[];
#endif
} ipc_channel_mmap_shared_t;

typedef struct
{
  ipc_channel_api_t           api;
  const char                * name;
  ipc_channel_mmap_shared_t * shared;
  size_t                      unit_size;
} ipc_channel_mmap_t;

static_assert(offsetof(ipc_channel_mmap_t, api) == 0,
              "Channel struct must has `api` the first field");

static size_t real_capacity(const ipc_channel_mmap_t * ipc)
{
  return CAPACITY - CAPACITY % ipc->unit_size;
}

static size_t next_index_index(const ipc_channel_mmap_t * ipc, size_t from_idx)
{
  size_t capacity = real_capacity(ipc);
  return (from_idx + ipc->unit_size) % capacity;
}

static size_t next_index_head(const ipc_channel_mmap_t * ipc)
{
  return next_index_index(ipc, ipc->shared->head);
}

static size_t next_index_tail(const ipc_channel_mmap_t * ipc)
{
  return next_index_index(ipc, ipc->shared->tail);
}

static bool is_empty(const ipc_channel_mmap_t * ipc)
{
  return ipc->shared->head == ipc->shared->tail;
}

static bool is_full(const ipc_channel_mmap_t * ipc)
{
  size_t next_tail = next_index_tail(ipc);

  return ipc->shared->head == next_tail;
}

static void push(void * self, const void * buffer)
{
  ipc_channel_mmap_t * ipc = self;

#ifndef USE_SPSC_MMAP
  IPC_CRITICAL_SECTION(&ipc->shared->mutex)
  {
    size_t capacity = real_capacity(ipc);

    while (is_full(ipc))
    {
      ipc_cv_wait(&ipc->shared->cv_not_full, &ipc->shared->mutex);
    }
    
    memcpy(&ipc->shared->data[ipc->shared->tail], buffer, ipc->unit_size);
    ipc->shared->tail = next_index_tail(ipc);

    ipc_cv_notify_one(&ipc->shared->cv_not_empty);
  }
#else
  while (is_full(ipc))
    sched_yield();

  memcpy(&ipc->shared->data[ipc->shared->tail], buffer, ipc->unit_size);

  // SAFETY: this is not atomic update, but it's okey,
  //         because it is the only consumer
  ipc->shared->tail = next_index_tail(ipc);
#endif
}

static void pop(void * self, void * buffer)
{
  ipc_channel_mmap_t * ipc = self;
  size_t capacity = real_capacity(ipc);

#ifndef USE_SPSC_MMAP
  IPC_CRITICAL_SECTION(&ipc->shared->mutex)
  {
    while (is_empty(ipc))
    {
      ipc_cv_wait(&ipc->shared->cv_not_empty, &ipc->shared->mutex);
    }
    
    memcpy(buffer, &ipc->shared->data[ipc->shared->head], ipc->unit_size);
    ipc->shared->head = next_index_head(ipc);

    ipc_cv_notify_one(&ipc->shared->cv_not_full);
  }
#else
  while (is_empty(ipc))
    sched_yield();

  memcpy(buffer, &ipc->shared->data[ipc->shared->head], ipc->unit_size);

  // SAFETY: this is not atomic update, but it's okey,
  //         because it is the only consumer
  ipc->shared->head = next_index_head(ipc);
#endif
}

static void mmap_shared_destroy(ipc_channel_mmap_shared_t * shared);
static void destroy(void * self)
{
  ipc_channel_mmap_t * ipc = self;
  if (ipc)
  {
    mmap_shared_destroy(ipc->shared);
    shm_unlink(ipc->name);
    free(ipc);
  }
}

static void * execute_mmap(const char * name)
{
  int shm = shm_open(name, O_CREAT|O_RDWR, 0600);
  void * mem = NULL;

  if (shm < 0) return NULL;

  if (ftruncate(shm, MMAP_SIZE))
  {
    shm_unlink(name);
    goto exit;
  }

  mem = mmap(NULL, MMAP_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0);

exit:
  close(shm);
  return mem;
}

static int mmap_shared_create(void * shared_mem)
{
  int ret = 0;
  ipc_channel_mmap_shared_t * shared = shared_mem;

  unsigned exp = 0;
  while (!atomic_compare_exchange_weak(&shared->init_spinlock, &exp, 1))
  {
    sched_yield();
  }

  if (atomic_fetch_add_explicit(&shared->ref_count, 1, memory_order_acq_rel) > 0)
  {
    // the shared memory is already initialized
    goto exit;
  }

#ifndef USE_SPSC_MMAP
  if ((ret = ipc_mutex_init(&shared->mutex)))
    goto exit;

  if ((ret = ipc_cv_init(&shared->cv_not_empty)))
    goto exit;

  if ((ret = ipc_cv_init(&shared->cv_not_full)))
    goto exit;
#endif

  shared->head = 0;
  shared->tail = 0;

exit:
  shared->init_spinlock = 0;
  return ret;
}

ipc_channel_api_t * ipc_channel_mmap_create(const char * name, size_t unit_size)
{
  ipc_channel_mmap_t * ipc = NULL;
  void * shared_mem = (void *) -1;

  if (__builtin_popcount(unit_size) != 1)
    goto failure;
  
  if ((ipc = malloc(sizeof(ipc_channel_mmap_t))) == NULL)
    goto failure;

  if ((shared_mem = execute_mmap(name)) == (void *) -1)
    goto failure;

  if (mmap_shared_create(shared_mem))
    goto failure;

  ipc->name = name; // ISSUE: implicit static lifetime assumption

  ipc->shared = shared_mem;
  ipc->unit_size = unit_size;
  ipc->api.push = push;
  ipc->api.pop = pop;
  ipc->api.destroy = destroy;

  return (ipc_channel_api_t *) ipc;

failure:
  if (shared_mem != (void *) -1) munmap(shared_mem, MMAP_SIZE);
  free(ipc);
  return NULL;
}

static void mmap_shared_destroy(ipc_channel_mmap_shared_t * shared)
{
  assert(shared);
  
  size_t prev_holders = atomic_fetch_sub_explicit(&shared->ref_count,
                                                  1,
                                                  memory_order_acquire);

  if (prev_holders == 1)
  {
    // the last holder of the shared memory
    // must release collected there resources
    
#ifndef USE_SPSC_MMAP
    ipc_mutex_destroy(&shared->mutex);
    ipc_cv_destroy(&shared->cv_not_empty);
    ipc_cv_destroy(&shared->cv_not_full);
#endif
  }

  munmap(shared, MMAP_SIZE);
}

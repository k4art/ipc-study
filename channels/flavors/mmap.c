#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "channels/channel.h"

#define VIRTUAL_PAGE_SIZE   4096
#define MMAP_SIZE           (262144 * VIRTUAL_PAGE_SIZE)
#define CAPACITY            (MMAP_SIZE - sizeof(ipc_channel_mmap_shared_t))

typedef struct
{
  atomic_uint     init_spinlock;  // `shm_open` is followed by `ftruncate`
                                  // which zero-fills empty file on first init
                                  // so that init_spinlock become 0
  atomic_uint     ref_count;
  pthread_mutex_t mutex;
  pthread_cond_t  cv_not_full;
  pthread_cond_t  cv_not_empty;
  size_t          head;
  size_t          tail;

  __attribute__ ((aligned(alignof(max_align_t))))
  char     data[];
} ipc_channel_mmap_shared_t;

typedef struct
{
  ipc_channel_api_t           api;
  const char                * name;
  ipc_channel_mmap_shared_t * inner;
  size_t                      unit_size;
} ipc_channel_mmap_t;

static_assert(offsetof(ipc_channel_mmap_t, api) == 0,
              "Channel struct must has `api` the first field");

static void push(void * self, const void * buffer)
{
  ipc_channel_mmap_t * ipc = self;

  pthread_mutex_lock(&ipc->inner->mutex);
  {
    size_t capacity = CAPACITY - CAPACITY % ipc->unit_size;
    size_t next_tail = (ipc->inner->tail + ipc->unit_size) % capacity;

    while (ipc->inner->head == next_tail)
    {
      pthread_cond_wait(&ipc->inner->cv_not_full, &ipc->inner->mutex);
    }
    
    memcpy(&ipc->inner->data[ipc->inner->tail], buffer, ipc->unit_size);
    ipc->inner->tail = next_tail;

    pthread_cond_signal(&ipc->inner->cv_not_empty);
  }
  pthread_mutex_unlock(&ipc->inner->mutex);
}

static void pop(void * self, void * buffer)
{
  ipc_channel_mmap_t * ipc = self;
  size_t capacity = CAPACITY - CAPACITY % ipc->unit_size;

  pthread_mutex_lock(&ipc->inner->mutex);
  {
    while (ipc->inner->head == ipc->inner->tail)
    {
      pthread_cond_wait(&ipc->inner->cv_not_empty, &ipc->inner->mutex);
    }
    
    memcpy(buffer, &ipc->inner->data[ipc->inner->head], ipc->unit_size);
    ipc->inner->head = (ipc->inner->head + ipc->unit_size) % capacity;

    pthread_cond_signal(&ipc->inner->cv_not_full);
  }
  pthread_mutex_unlock(&ipc->inner->mutex);
}

static void mmap_shared_destroy(ipc_channel_mmap_shared_t * shared);
static void destroy(void * self)
{
  ipc_channel_mmap_t * ipc = self;
  if (ipc)
  {
    mmap_shared_destroy(ipc->inner);
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

static int init_pshared_mutex(pthread_mutex_t * mutex);
static int init_pshared_cond(pthread_cond_t * cond);

static int mmap_shared_create(void * shared_mem)
{
  int ret = 0;
  ipc_channel_mmap_shared_t * shared = shared_mem;

  unsigned exp = 0;
  while (!atomic_compare_exchange_weak(&shared->init_spinlock, &exp, 1))
  {
    sched_yield();
  }

  if (atomic_load(&shared->ref_count) > 0)
  {
    // the shared memory is already initialized
    goto exit;
  }

  if ((ret = init_pshared_mutex(&shared->mutex)))
    goto exit;

  if ((ret = init_pshared_cond(&shared->cv_not_empty)))
    goto exit;

  if ((ret = init_pshared_cond(&shared->cv_not_full)))
    goto exit;

  shared->head = 0;
  shared->tail = 0;
  shared->ref_count++;

exit:
  shared->init_spinlock = 0;
  return ret;
}

ipc_channel_api_t * ipc_channel_mmap_create(const char * name, size_t unit_size)
{
  ipc_channel_mmap_t * ipc = NULL;
  void * shared_mem = NULL;

  if (__builtin_popcount(unit_size) != 1)
    goto failure;
  
  if ((ipc = malloc(sizeof(ipc_channel_mmap_t))) == NULL)
    goto failure;

  if ((shared_mem = execute_mmap(name)) == (void *) -1)
    goto failure;

  if (mmap_shared_create(shared_mem))
    goto failure;

  ipc->name = name; // ISSUE: implicit static lifetime assumption

  ipc->inner = shared_mem;
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
    
    pthread_mutex_destroy(&shared->mutex);
    pthread_cond_destroy(&shared->cv_not_empty);
    pthread_cond_destroy(&shared->cv_not_full);
  }

  munmap(shared, MMAP_SIZE);
}

static int init_pshared_mutex(pthread_mutex_t * mutex)
{
  int ret = 0;
  pthread_mutexattr_t mutexattr;

  if ((ret = pthread_mutexattr_init(&mutexattr)))
    return ret;

  if ((ret = pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED)))
    goto exit;

  if ((ret = pthread_mutex_init(mutex, &mutexattr)))
    goto exit;

exit:
  pthread_mutexattr_destroy(&mutexattr);
  return ret;
}

static int init_pshared_cond(pthread_cond_t * cond)
{
  int ret = 0;
  pthread_condattr_t condattr;

  if ((ret = pthread_condattr_init(&condattr)))
    return ret;

  if ((ret = pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED)))
    goto exit;

  if ((ret = pthread_cond_init(cond, &condattr)))
    goto exit;

exit:
  pthread_condattr_destroy(&condattr);
  return ret;
}

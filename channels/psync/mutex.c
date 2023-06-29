#include "mutex.h"

int ipc_mutex_init(ipc_mutex_t * mutex)
{
  assert(mutex);

  int ret = 0;
  pthread_mutexattr_t mutexattr;

  if ((ret = pthread_mutexattr_init(&mutexattr)))
    return ret;

  if ((ret = pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED)))
    goto exit;

  if ((ret = pthread_mutex_init(&mutex->plain, &mutexattr)))
    goto exit;

exit:
  IPC_EOK(pthread_mutexattr_destroy(&mutexattr));
  return ret;
}

void ipc_mutex_destroy(ipc_mutex_t * mutex)
{
  assert(mutex);
  IPC_EOK(pthread_mutex_destroy(&mutex->plain));
}

void ipc_mutex_lock(ipc_mutex_t * mutex)
{
  assert(mutex);
  IPC_EOK(pthread_mutex_lock(&mutex->plain));
}

void ipc_mutex_unlock(ipc_mutex_t * mutex)
{
  assert(mutex);
  IPC_EOK(pthread_mutex_unlock(&mutex->plain));
}

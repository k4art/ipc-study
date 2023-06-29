#include "cv.h"

int ipc_cv_init(ipc_cv_t * cv)
{
  assert(cv);
  
  int ret = 0;
  pthread_condattr_t condattr;

  if ((ret = pthread_condattr_init(&condattr)))
    return ret;

  if ((ret = pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED)))
    goto exit;

  if ((ret = pthread_cond_init(&cv->plain, &condattr)))
    goto exit;

exit:
  IPC_EOK(pthread_condattr_destroy(&condattr));
  return ret;
}

void ipc_cv_destroy(ipc_cv_t * cv)
{
  assert(cv);
  IPC_EOK(pthread_cond_destroy(&cv->plain));
}

void ipc_cv_wait(ipc_cv_t * cv, ipc_mutex_t * mutex)
{
  assert(cv);
  assert(mutex);
  
  IPC_EOK(pthread_cond_wait(&cv->plain, &mutex->plain));
}

void ipc_cv_notify_one(ipc_cv_t * cv)
{
  assert(cv);
  
  IPC_EOK(pthread_cond_signal(&cv->plain));
}

#ifndef IPC_PSYNC_CV_H
#define IPC_PSYNC_CV_H

#include <pthread.h>

#include "mutex.h"

typedef struct
{
  pthread_cond_t plain;
} ipc_cv_t;

int ipc_cv_init(ipc_cv_t * cv);
void ipc_cv_destroy(ipc_cv_t * cv);

void ipc_cv_wait(ipc_cv_t * cv, ipc_mutex_t * mutex);
void ipc_cv_notify_one(ipc_cv_t * cv);

#endif

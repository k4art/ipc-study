#ifndef IPC_PSYNC_MUTEX_H
#define IPC_PSYNC_MUTEX_H

#include <pthread.h>

#include "channels/macros.h"

typedef struct
{
  pthread_mutex_t plain;
} ipc_mutex_t;

int ipc_mutex_init(ipc_mutex_t * mutex);
void ipc_mutex_destroy(ipc_mutex_t * mutex);

void ipc_mutex_lock(ipc_mutex_t * mutex);
void ipc_mutex_unlock(ipc_mutex_t * mutex);

#define IPC_CRITICAL_SECTION(p_mutex)        \
  IPC_DEFER(ipc_mutex_lock(p_mutex),         \
            ipc_mutex_unlock(p_mutex))

#endif

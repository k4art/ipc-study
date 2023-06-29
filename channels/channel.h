#ifndef IPC_CHANNEL_API_H
#define IPC_CHANNEL_API_H

#include <stddef.h>

typedef enum
{
	IPC_CHANNEL_FLAVOR_MMAP,
	IPC_CHANNEL_FLAVOR_SOCKET,
} ipc_channel_flavors_t;

typedef void (* ipc_channel_push)(void * self, const void * buffer);
typedef void (* ipc_channel_pop)(void * self, void * buffer);
typedef void (* ipc_channel_destroy)(void * self);

typedef struct
{
  ipc_channel_push    push;
  ipc_channel_pop     pop;
  ipc_channel_destroy destroy;
} ipc_channel_api_t;

ipc_channel_api_t * ipc_channel_create(const char            * key,
                                       size_t                  unit_size,
	                                     ipc_channel_flavors_t   flavor);

#endif

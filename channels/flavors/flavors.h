#ifndef IPC_CHANNEL_FLAVORS_H
#define IPC_CHANNEL_FLAVORS_H

#include "../channel.h"

ipc_channel_api_t * ipc_channel_mmap_create(const char * name, size_t unit_size);
ipc_channel_api_t * ipc_channel_socket_create(const char * port, size_t unit_size);

#endif

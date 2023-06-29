#include "flavors/flavors.h"

#include "channel.h"

ipc_channel_api_t * ipc_channel_create(const char            * key,
                                       size_t                  unit_size,
                                       ipc_channel_flavors_t   flavor)
{
  switch (flavor)
  {
    case IPC_CHANNEL_FLAVOR_MMAP:   return ipc_channel_mmap_create(key, unit_size);
    case IPC_CHANNEL_FLAVOR_SOCKET: return ipc_channel_socket_create(key, unit_size);
  }

  return NULL;
}

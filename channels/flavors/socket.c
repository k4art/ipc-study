#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "channels/channel.h"

#define BACKLOG 512

typedef struct
{
  ipc_channel_api_t   api;
  const char        * sock_path;
  size_t              unit_size;
  int                 sockfd;
  int                 conn_sockfd;
} ipc_channel_socket_t;

static_assert(offsetof(ipc_channel_socket_t, api) == 0,
              "Channel struct must has `api` the first field");

static void push(void * self, const void * buffer)
{
  ipc_channel_socket_t * ipc = self;
  int ret = send(ipc->conn_sockfd, buffer, ipc->unit_size, 0);
  assert(ret > 0);
}

static void pop(void * self, void * buffer)
{
  ipc_channel_socket_t * ipc = self;
  int ret = recv(ipc->conn_sockfd, buffer, ipc->unit_size, 0);
  assert(ret > 0);
}

static void destroy(void * self)
{
  if (self)
  {
    ipc_channel_socket_t * ipc = self;

    close(ipc->sockfd);
    close(ipc->conn_sockfd);
    free(ipc);
  }
}

static int domain_sockpair_connect(const char * sock_path, int (*sockpair)[2])
{
  struct sockaddr_un addr;
  struct sockaddr * saddr = (struct sockaddr *) &addr;
  int sockfd = -1, connfd = -1;
  int ret = 0;

  if (strlen(sock_path) >= sizeof(addr.sun_path))
    goto failure;

  if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    goto failure;
  
  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

  if ((ret = bind(sockfd, saddr, sizeof(struct sockaddr_un))) < 0)
  {
    // client path
    if ((ret = connect(sockfd, saddr, sizeof(struct sockaddr_un)) < 0))
      goto failure;

    connfd = sockfd;
  }
  else
  {
    // server path
    if ((ret = listen(sockfd, BACKLOG)) < 0)
      goto failure;

    if ((ret = accept(sockfd, NULL, NULL)) < 0)
      goto failure;

    connfd = ret;
    ret = 0;
    unlink(sock_path);
  }

  (*sockpair)[0] = sockfd;
  (*sockpair)[1] = connfd;
  return ret;

failure:
  close(connfd);
  close(sockfd);
  return ret;
}

ipc_channel_api_t * ipc_channel_socket_create(const char * sock_path, size_t unit_size)
{
  ipc_channel_socket_t * ipc = NULL;
  int sockpair[2] = { -1, -1 };
  
  if ((ipc = malloc(sizeof(ipc_channel_socket_t))) == NULL)
    goto failure;

  if (domain_sockpair_connect(sock_path, &sockpair) != 0)
    goto failure;

  ipc->sockfd = sockpair[0];
  ipc->conn_sockfd = sockpair[1];
  ipc->sock_path = sock_path; // ISSUE: static lifetime assumption
  ipc->unit_size = unit_size;

  ipc->api.push = push;
  ipc->api.pop = pop;
  ipc->api.destroy = destroy;

  return (ipc_channel_api_t *) ipc;

failure:
  close(sockpair[0]);
  close(sockpair[1]);
  free(ipc);
  return NULL;
}

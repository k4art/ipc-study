#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "channels/channel.h"

#define MMAP_SHARED_MEM_NAME  "/ipc_shr_open_mmap_78324"
#define UNIX_SOCK_PATH        "/tmp/ipc_unix_socket_ex_38310"
#define DEFAULT_ITERS         1000000
#define DEFAULT_UNIT_SIZE     8

#define FLAVOR IPC_CHANNEL_FLAVOR_SOCKET

static void report_time(const char * label)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  printf("%s: %ld %09ld\n", label, ts.tv_sec, ts.tv_nsec);
}

struct channel_options
{
  const char            * name;
  size_t                  unit_size;
  size_t                  iters;
  ipc_channel_flavors_t   flavor;
};

static void print_usage(const char * command)
{
  printf("USAGE: %s {mmap|socket} [unit-size] [iters]\n", command);
  printf("\n");
  printf(" - {mmap|socket} - channel flavor\n");
  printf(" - [unit-size]   - size of a message transmitted over channel\n");
  printf("                   MUST be a power of 2, defaults to 8");
  printf(" - [iters]       - number of transmissions over channel\n");
  printf("                   defaults to 1000000\n");
  printf("\n");
  printf("   ex: %s mmap 32\n", command);
}

static struct channel_options demand_options(int argc, char ** argv);
static void clear_old_medium(struct channel_options opts);

int main(int argc, char ** argv)
{
  struct channel_options opts = demand_options(argc, argv);
  clear_old_medium(opts);

  if (!fork())
  {
    unsigned char * unit = malloc(opts.unit_size);
    ipc_channel_api_t * ipc = ipc_channel_create(opts.name,
                                                 opts.unit_size,
                                                 opts.flavor);

    assert(ipc != NULL);
    
    report_time("[BEGIN]");

    for (int i = 0; i < opts.iters; i++)
    {
      unit[0] = i & 0xff;
      ipc->push(ipc, unit);
    }

    free(unit);
    ipc->destroy(ipc);
  }
  else
  {
    unsigned char * unit = malloc(opts.unit_size);
    ipc_channel_api_t * ipc = ipc_channel_create(opts.name,
                                                 opts.unit_size,
                                                 opts.flavor);

    assert(ipc != NULL);

    double received;

    for (int i = 0; i < opts.iters; i++)
    {
      ipc->pop(ipc, unit);
      if (unit[0] != (i & 0xff))
      {
        printf("%d != %d\n", unit[0], i & 0xff);
      }
      assert(unit[0] == (i & 0xff));
    }

    free(unit);
    report_time("[-END-]");
    wait(&(int) {0});
    ipc->destroy(ipc);
  }
}

static struct channel_options demand_options(int argc, char ** argv)
{
  struct channel_options opts =
  {
    .unit_size = DEFAULT_UNIT_SIZE,
    .iters     = DEFAULT_ITERS,
  };

  if (argc == 1 || argc > 4)
  {
    print_usage(argv[0]);
    exit(0);
  }

  if (argc >= 2)
  {
    if (!strcmp(argv[1], "mmap"))
    {
      opts.flavor = IPC_CHANNEL_FLAVOR_MMAP;
      opts.name   = MMAP_SHARED_MEM_NAME;
    }
    else if (!strcmp(argv[1], "socket"))
    {
      opts.flavor = IPC_CHANNEL_FLAVOR_SOCKET;
      opts.name   = UNIX_SOCK_PATH;
    }
    else
    {
      print_usage(argv[0]);
      exit(0);
    }
  }

  if (argc >= 3)
  {
    char * endptr = NULL;
    opts.unit_size = strtol(argv[2], &endptr, 10);

    if (*endptr != '\0' || __builtin_popcount(opts.unit_size) != 1)
    {
      print_usage(argv[0]);
      exit(0);
    }
  }

  if (argc == 4)
  {
    char * endptr = NULL;
    opts.iters = strtol(argv[3], &endptr, 10);

    if (*endptr != '\0')
    {
      print_usage(argv[0]);
      exit(0);
    }
  }

  printf("opts.flavor    = %s\n",
         opts.flavor == IPC_CHANNEL_FLAVOR_MMAP ? "mmap" : "socket");

  printf("opts.unit_size = %zu\n", opts.unit_size);
  printf("opts.iters     = %zu\n", opts.iters);

  printf("opts.fs_path   = %s%s\n",
         opts.flavor == IPC_CHANNEL_FLAVOR_MMAP ? "/dev/shm" : "", opts.name);

  fflush(stdout);
  return opts;
}

static void clear_old_medium(struct channel_options opts)
{
  switch (opts.flavor)
  {
    case IPC_CHANNEL_FLAVOR_MMAP:
      shm_unlink(opts.name);
      break;

    case IPC_CHANNEL_FLAVOR_SOCKET:
      remove(opts.name);
      break;
  }
}

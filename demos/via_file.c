#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#define FILEPATH          "/tmp/ipc_via_file_2345433"
#define CONSUMER_WAIT_US  (100 * 1000)

static char buffer[1024];

static char * read_user(char * buffer, size_t n)
{
  printf("> ");
  return fgets(buffer, n, stdin);
}

static int flock_and_read_file(int fd)
{
  int ret, symbol = 0;

  flock(fd, LOCK_SH);
  if ((ret = read(fd, &symbol, 1)) <= 0)
    symbol = ret;
  flock(fd, LOCK_UN);

  return symbol;
}

int main(void)
{
  pid_t child_id;

  unlink(FILEPATH);
  if ((child_id = fork()))
  {
    //****************//
    //    producer    //
    //****************//

    int fd = open(FILEPATH, O_CREAT | O_WRONLY | O_TRUNC, 0600);

    while (read_user(buffer, sizeof(buffer)))
    {
      flock(fd, LOCK_EX);
      write(fd, buffer, strlen(buffer));
      flock(fd, LOCK_UN);
    }

    close(fd);

    // consumer does not know when to finish,
    // let it finish for 1 second and send SIGINT it,
    // this will terminate it
    {
      sleep(1);
      kill(child_id, SIGINT);
      wait(&(int) {0});
    }

    return 0;
  }
  else
  {
    //****************//
    //    consumer    //
    //****************//
    
    int fd, ch;
    
    while ((fd = open(FILEPATH, 0)) == 0)
      sleep(1);
    
    while (true)
    {
      if ((ch = flock_and_read_file(fd)) > 0)
      {
        putchar(ch);
        fflush(stdout);
        continue;
      }
      
      usleep(CONSUMER_WAIT_US);
    }

    printf("Bye\n");
    close(fd);
  }
}


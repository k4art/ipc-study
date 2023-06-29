#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static struct termios saved_tty;

static void restore_tty(void)
{
  tcsetattr(STDIN_FILENO, TCSANOW, &saved_tty);
}

static void use_tty_raw_mode(void)
{
  struct termios raw;

  tcgetattr(STDIN_FILENO, &saved_tty);
  atexit(restore_tty);

  raw = saved_tty;
  raw.c_lflag &= ~(ICANON | ECHO);

  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static atomic_int m_width = 1;

static void increment_width(int signum)
{
  atomic_fetch_add_explicit(&m_width, 1, memory_order_relaxed);
}

static void decrement_width(int signum)
{
  atomic_fetch_sub_explicit(&m_width, 1, memory_order_relaxed);
}

int main(void)
{
  pid_t child_id;

  use_tty_raw_mode();

  if ((child_id = fork()) > 0)
  {
    int ch;

    while ((ch = getchar()) != EOF)
    {
      switch (ch)
      {
        case 'u':
          kill(child_id, SIGUSR1);
          break;
        case 'd':
          kill(child_id, SIGUSR2);
          break;
      }
    }

    wait(&(int) {0});

    return 0;
  }
  else
  {
    signal(SIGUSR1, increment_width);
    signal(SIGUSR2, decrement_width);

    while (true)
    {
      int dots = atomic_load_explicit(&m_width, memory_order_acquire);

      if (dots > 0)
      {
        while (dots-- > 0) { putchar('+'); }
      }
      else
      {
        while (dots++ < 0) { putchar('-'); }
      }
      putchar('\n');
      sleep(1);
    }

    return 0;
  }
}

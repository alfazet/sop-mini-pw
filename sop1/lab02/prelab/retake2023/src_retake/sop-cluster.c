#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   kill(0, SIGKILL), exit(EXIT_FAILURE))

#define UNUSED(x) (void)(x)

#define MAX_N 100

volatile sig_atomic_t last_signal, child_running = 0, parent_running = 0;
int child_pids[MAX_N];

void sethandler(void (*f)(int), int signo) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = f;
  if (-1 == sigaction(signo, &act, NULL))
    ERR("sigaction");
}

void sigusr1_parent_handler(int signo) {
  last_signal = signo;
  parent_running = 1;
}

void sigusr1_child_handler(int signo) {
  last_signal = signo;
  child_running = 1;
}

void sigusr2_handler(int signo) {
  last_signal = signo;
  child_running = 0;
}

void usage(char *progname) {
  fprintf(stderr, "%s n\n", progname);
  fprintf(stderr, "n - how many children to make");
  exit(EXIT_FAILURE);
}

void child_work() {
  int pid = getpid();

  sethandler(sigusr1_child_handler, SIGUSR1);
  sethandler(sigusr2_handler, SIGUSR2);
  sigset_t mask, oldmask;
  sigprocmask(0, NULL, &mask);
  printf("[%d] (child): waiting for SIGUSR1 from parent\n", pid);
  while (!child_running) {
    sigsuspend(&mask);
  }
  printf("[%d] (child): received SIGUSR1 from parent, starting work\n", pid);

  srand(time(NULL) * pid);
  int counter = 1;
  for (;;) {
    while (child_running) {
      int sleep_ms = 500 + rand() % (200 - 100 + 1);
      struct timespec t = {0, sleep_ms * 1000000};
      nanosleep(&t, NULL);
      counter++;
      printf("[%d] (child): counter = %d\n", pid, counter);
    }
    printf("[%d] (child): halt until SIGUSR2 is received\n", pid);
    while (!child_running) {
      sigsuspend(&mask);
    }
    printf("[%d] (child): resumed after receiving SIGUSR2\n", pid);
  }
  printf("[%d] (child): terminating...\n", pid);
}

void create_children(int n) {
  int pid;
  for (int i = 0; i < n; i++) {
    if ((pid = fork()) < 0) {
      ERR("fork");
    }
    if (pid == 0) {
      child_work();
      exit(EXIT_SUCCESS); // this exits the child, not the parent
    } else {
      child_pids[i] = pid;
    }
  }
}

int main(int argc, char *argv[]) {
  UNUSED(argc);
  UNUSED(argv);
  if (argc != 2) {
    usage(argv[0]);
  }
  int n = atoi(argv[1]);

  sethandler(sigusr1_parent_handler, SIGUSR1);
  sigset_t mask;
  sigprocmask(0, NULL, &mask); // sets the mask of signals this process blocks
  create_children(n);

  int i = 0;
  printf("parent's PID: %d\n", getpid());
  for (;;) {
    printf("parent sending SIGUSR1 to %d\n", child_pids[i]);
    kill(child_pids[i], SIGUSR1);
    parent_running = 0;
    printf("parent waiting for SIGUSR1...\n");
    while (!parent_running) {
      sigsuspend(&mask); // the program is effectively suspended until one of
                         // the signals that is not a member of mask arrives
    }
    printf("parent received SIGUSR1, sending SIGUSR2 to %d\n", child_pids[i]);
    kill(child_pids[i], SIGUSR2);
    i = (i + 1) % n;
  }
  while (wait(NULL) > 0)
    ;
  return EXIT_SUCCESS;
}

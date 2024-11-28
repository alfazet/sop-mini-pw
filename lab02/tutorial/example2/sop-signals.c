#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source)                                                            \
  (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source),             \
   kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;

void sethandler(void (*f)(int), int sigNo) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = f;
  if (sigaction(sigNo, &act, NULL) == -1) {
    ERR("sigaction");
  }
}

void sig_handler(int sig) {
  printf("[%d] received signal %d\n", getpid(), sig);
  last_signal = sig;
}

void sigchld_handler() { // (int sig) not necessary
  pid_t pid;
  for (;;) {
    pid = waitpid(0, NULL, WNOHANG);
    if (pid == 0) {
      return;
    } else if (pid < 0) {
      if (errno == ECHILD) {
        return;
      }
      ERR("waitpid");
    }
  }
}

void child_work(int r) {
  int t, tt;
  int pid = getpid();
  srand(pid);
  t = rand() % 6 + 5;
  while (r-- > 0) {
    for (tt = t; tt > 0; tt = sleep(tt))
      ;
    if (last_signal == SIGUSR1) {
      printf("[%d] (child) success\n", pid);
    } else {
      printf("[%d] (child) failure\n", pid);
    }
  }
  printf("[%d] (child) terminating...\n", pid);
}

void parent_work(int k, int p, int r) {
  struct timespec tk = {k, 0};
  struct timespec tp = {p, 0};
  sethandler(sig_handler, SIGALRM);
  alarm(r * 10);
  while (last_signal != SIGALRM) {
    nanosleep(&tk, NULL);
    if (kill(0, SIGUSR1) < 0) {
      ERR("kill");
    }
    nanosleep(&tp, NULL);
    if (kill(0, SIGUSR2) < 0) {
      ERR("kill");
    }
  }
  printf("[%d] (parent) terminating...\n", getpid());
}

void create_children(int n, int r) {
  while (n-- > 0) {
    switch (fork()) {
    case 0:
      sethandler(sig_handler, SIGUSR1);
      sethandler(sig_handler, SIGUSR2);
      child_work(r);
      exit(EXIT_SUCCESS);
    case -1:
      ERR("fork");
    }
  }
}

void usage(void) {
  fprintf(stderr, "USAGE: signals n k p r\n");
  fprintf(stderr, "n - number of children\n");
  fprintf(stderr, "k - Interval before SIGUSR1\n");
  fprintf(stderr, "p - Interval before SIGUSR2\n");
  fprintf(stderr, "r - lifetime of child in cycles\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int n, k, p, r;
  if (argc != 5) {
    usage();
  }
  n = atoi(argv[1]);
  k = atoi(argv[2]);
  p = atoi(argv[3]);
  r = atoi(argv[4]);
  if (n <= 0 || k <= 0 || p <= 0 || r <= 0) {
    usage();
  }
  sethandler(sigchld_handler, SIGCHLD);
  // ignore SIGUSR1/2 so that the process doesn't terminate on them
  sethandler(SIG_IGN, SIGUSR1);
  sethandler(SIG_IGN, SIGUSR2);
  create_children(n, r);
  parent_work(k, p, r);
  while (wait(NULL) > 0)
    ;

  return EXIT_SUCCESS;
}

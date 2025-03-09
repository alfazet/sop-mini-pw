#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
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

void usage(const char *pname) {
  fprintf(stderr, "USAGE: %s t k n p\n", pname);
  fprintf(stderr, "\tt - simulation time in seconds (1-100)\n");
  fprintf(stderr,
          "\tk - time after ill child is picked up by their parents (1-100)\n");
  fprintf(stderr, "\tn - number of children\n");
  fprintf(stderr, "\tp - likelihood of contracting the disease after contact "
                  "with the virus\n");
  exit(EXIT_FAILURE);
}

#define MAX_N 100

volatile sig_atomic_t last_signal = 0;
int cough_cnt[MAX_N], pid_list[MAX_N], sick[MAX_N], p;

void sig_handler(int sig) {
  printf("[%d] received signal %d\n", getpid(), sig);
  last_signal = sig;
}

void sigterm_handler(int sig) {
  exit(1);
}

void sigusr1_handler(int sig) {
  printf("[%d] caught virus\n", getpid());
  int my_pid = getpid(), my_id = -1;
  for (int i = 0; i < MAX_N; i++) {
    if (pid_list[i] == my_pid) {
      my_id = i;
      break;
    }
  }
  if (sick[my_id] == 0) {
    int r = rand() % 100;
    if (r <= p) {
      sick[my_id] = 1;
    }
  }
}

void set_handler(void (*f)(int), int sigNo) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = f;
  if (sigaction(sigNo, &act, NULL) == -1) {
    ERR("sigaction");
  }
}

void child_work(int i) {
  printf("[%d] (child), id: %d\n", getpid(), i);
  pid_list[i] = getpid();
  set_handler(sigterm_handler, SIGTERM);
  set_handler(sig_handler, SIGUSR1);
  srand(getpid());
  for (;;) {
    int r = 50 + rand() + (200 - 50 + 1);
    struct timespec tspec = {0, 1000000L * r};
    nanosleep(&tspec, NULL);
    if (sick[i]) {
      kill(0, SIGUSR1);
      cough_cnt[i]++;
    }
  }
}

void parent_work(int t) {
  set_handler(sig_handler, SIGALRM);
  alarm(t);
  while (last_signal != SIGALRM)
    ;
  kill(0, SIGTERM);
}

void create_children(int n) {
  for (int i = 0; i < n; i++) {
    switch (fork()) {
    case 0:
      child_work(i);
      exit(EXIT_SUCCESS);
    case -1:
      ERR("fork");
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    usage(argv[0]);
  }
  int t = atoi(argv[1]);
  int k = atoi(argv[2]);
  int n = atoi(argv[3]);
  p = atoi(argv[4]);
  if (t <= 0 || k <= 0 || n <= 0 || p <= 0) {
    usage(argv[0]);
  }
  printf("[%d] (parent)\n", getpid());
  set_handler(SIG_IGN, SIGUSR1);
  set_handler(SIG_IGN, SIGUSR2);
  set_handler(SIG_IGN, SIGTERM);
  create_children(n);
  parent_work(t);

  sick[0] = 1;
  int i = 1, exit_code, child_pid;
  while ((child_pid = wait(&exit_code)) > 0) {
    printf("id: %d, pid: %d, exit_code: %d\n", i, child_pid, exit_code);
    i++;
  }

  exit(EXIT_SUCCESS);
}

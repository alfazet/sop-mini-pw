#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define ERR(source)                                                            \
  (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source),             \
   kill(0, SIGKILL), exit(EXIT_FAILURE))

void usage(char *name) {
  fprintf(stderr, "USAGE: %s 0<n\n", name);
  exit(EXIT_FAILURE);
}

void child_work() {
  int pid = getpid();
  srand(time(NULL) * pid);
  int t = 5 + rand() % (10 - 5 + 1);
  sleep(t);
  printf("[%d] (child): this process is terminating...\n", pid);
}

void create_children(int n) {
  int child_pid, child_cnt = 0;
  while (n--) {
    if ((child_pid = fork()) < 0) {
      ERR("fork");
    }
    child_cnt++;
    if (child_pid == 0) {
      child_work();
      exit(EXIT_SUCCESS); // this exits the child, not the parent
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    usage(argv[0]);
  }
  int n = atoi(argv[1]);
  if (n <= 0) {
    usage(argv[0]);
  }
  create_children(n);
  while (n > 0) {
    sleep(3);
    int child_pid;
    for (;;) {
      // check for any child of this process without hanging
      child_pid = waitpid(0, NULL, WNOHANG);
      if (child_pid > 0) {
        n--;
      } else if (child_pid == 0) {
        break; // no child exited in this iteration
      } else {
        if (errno == ECHILD) {
          break; // no children left
        }
        ERR("waitpid");
      }
    }
    printf("[%d] (parent): %d children left\n", getpid(), n);
  }

  return EXIT_SUCCESS;
}

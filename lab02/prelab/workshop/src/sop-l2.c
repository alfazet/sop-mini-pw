#include <bits/types/sigset_t.h>
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal;

ssize_t bulk_read(int fd, char *buf, size_t count) {
  ssize_t c;
  ssize_t len = 0;
  do {
    c = TEMP_FAILURE_RETRY(read(fd, buf, count));
    if (c < 0)
      return c;
    if (c == 0)
      return len; // EOF
    buf += c;
    len += c;
    count -= c;
  } while (count > 0);
  return len;
}

ssize_t bulk_write(int fd, char *buf, size_t count) {
  ssize_t c;
  ssize_t len = 0;
  do {
    c = TEMP_FAILURE_RETRY(write(fd, buf, count));
    if (c < 0)
      return c;
    buf += c;
    len += c;
    count -= c;
  } while (count > 0);
  return len;
}

void sig_handler(int signo) { last_signal = signo; }

void sethandler(void (*f)(int), int sigNo) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = f;
  if (-1 == sigaction(sigNo, &act, NULL))
    ERR("sigaction");
}

void sleep_exact(long ms) {
  struct timespec t = {0, ms * 1000 * 1000}, remaining;
  while (nanosleep(&t, &remaining) == -1) {
    if (errno == EINTR) {
      t = remaining;
    }
  }
}

void usage(char *argv[]) {
  printf("%s n f \n", argv[0]);
  printf("\tf - file to be processed\n");
  printf("\t0 < n < 10 - number of child processes\n");
  exit(EXIT_FAILURE);
}

void child_work(char *chunk, int chunksz, char *filename, sigset_t oldmask) {
  int pid = getpid();
  // up until this moment, SIGUSR1 was blocked in the child
  // so that it wouldn't be handled early
  sigprocmask(SIG_BLOCK, &oldmask, NULL);
  printf("[%d] (child) waiting for SIGUSR1 to start work\n", pid);
  // now SIGUSR1 is unblocked; if it came earlier and became pending,
  // it will be handled now, else this child will wait for it
  while (last_signal != SIGUSR1) {
    sigsuspend(&oldmask);
  }
  printf("[%d] (child) starting work, my chunk is: %s\n", pid, chunk);

  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0777);
  if (fd < 0) {
    ERR("open");
  }
  int change = 1;
  for (int i = 0; i < chunksz; i++) {
    char c = chunk[i];
    if (c >= 'A' && c <= 'Z') {
      if (change) {
        c += 32;
      }
      change ^= 1;
    } else if (c >= 'a' && c <= 'z') {
      if (change) {
        c -= 32;
      }
      change ^= 1;
    }
    sleep_exact(250);
    int write_cnt = bulk_write(fd, &c, 1);
    if (write_cnt < 1) {
      ERR("bulk_write");
    }
  }

  if (close(fd) < 0) {
    ERR("close");
  }
}

void create_children(char *filename, int n, sigset_t oldmask) {
  struct stat st;
  stat(filename, &st);
  long filesz = st.st_size, per_child = (filesz + n - 1) / n;
  char *file_content = malloc(filesz * sizeof(char));
  if (file_content == NULL) {
    ERR("malloc");
  }
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    ERR("open");
  }
  int read_cnt = bulk_read(fd, file_content, filesz);
  if (read_cnt < filesz) {
    ERR("bulk_read");
  }
  if (close(fd) < 0) {
    ERR("close");
  }

  int pid;
  long moved_already = 0;
  for (int i = 0; i < n; i++) {
    int child_chunksz =
        (filesz - moved_already >= per_child ? per_child
                                             : filesz - moved_already);
    if ((pid = fork()) < 0) {
      ERR("fork");
    }
    if (pid == 0) {
      char *child_chunk = malloc((child_chunksz + 1) * sizeof(char));
      if (child_chunk == NULL) {
        ERR("malloc");
      }
      memcpy(child_chunk, file_content + moved_already, child_chunksz);
      child_chunk[child_chunksz] = '\0'; // don't forget to terminate
      free(file_content);

      int n = strlen(filename);
      char *child_filename = malloc((n + 3) * sizeof(char));
      strcpy(child_filename, filename);
      child_filename[n] = '-';
      child_filename[n + 1] = (char)(i + '0');
      child_filename[n + 2] = '\0';
      child_work(child_chunk, child_chunksz, child_filename, oldmask);
      free(child_chunk);
      exit(EXIT_SUCCESS);
    } else {
      moved_already += child_chunksz;
    }
  }
  free(file_content);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    usage(argv);
  }
  char *filename = argv[1];
  int n = atoi(argv[2]);
  if (n <= 0 || n >= 10) {
    usage(argv);
  }

  sethandler(sig_handler, SIGUSR1);
  sigset_t mask, oldmask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);
  create_children(filename, n, oldmask);

  if (kill(0, SIGUSR1) < 0) {
    ERR("kill");
  }

  while (wait(NULL) > 0)
    ;
  return EXIT_SUCCESS;
}

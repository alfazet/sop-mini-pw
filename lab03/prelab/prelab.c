#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CIRC_BUF_SIZE 1024

#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   kill(0, SIGKILL), exit(EXIT_FAILURE))

typedef struct circular_buffer {
  char *buf[CIRC_BUF_SIZE];
  int head;
  int tail;
  int len;
  pthread_mutex_t mx;
} circ_buf;

void msleep(unsigned msec) {
  time_t sec = (int)(msec / 1000);
  msec = msec - (sec * 1000);
  struct timespec req = {0};
  req.tv_sec = sec;
  req.tv_nsec = msec * 1000000L;
  if (TEMP_FAILURE_RETRY(nanosleep(&req, &req)))
    ERR("nanosleep");
}

circ_buf *circ_buf_create() {
  circ_buf *cb = malloc(sizeof(circ_buf));
  if (cb == NULL) {
    ERR("malloc");
  }
  for (int i = 0; i < CIRC_BUF_SIZE; i++) {
    cb->buf[i] = NULL;
  }
  cb->head = 0;
  cb->tail = 0;
  cb->len = 0;
  pthread_mutex_init(&cb->mx, NULL);

  return cb;
}

void circ_buf_push(circ_buf *cb, char *path) {
  while (cb->len == CIRC_BUF_SIZE) {
    msleep(5);
  }
  pthread_mutex_lock(&cb->mx);
  char *s = malloc(sizeof(char) * (strlen(path) + 1));
  strcpy(s, path);
  cb->buf[cb->head] = s;
  cb->len++;
  cb->head++;
  if (cb->head == CIRC_BUF_SIZE) {
    cb->head = 0;
  }
  pthread_mutex_unlock(&cb->mx);
}

char *circ_buf_pop(circ_buf *cb) {
  while (cb->len == 0) {
    msleep(5);
  }
  pthread_mutex_lock(&cb->mx);
  char *s = cb->buf[cb->tail];
  cb->len--;
  cb->tail++;
  if (cb->tail == CIRC_BUF_SIZE) {
    cb->tail = 0;
  }
  pthread_mutex_unlock(&cb->mx);

  return s;
}

void circ_buf_destroy(circ_buf *cb) {
  pthread_mutex_destroy(&cb->mx);
  for (int i = 0; i < CIRC_BUF_SIZE; i++) {
    free(cb->buf[i]);
  }
  free(cb);
}

typedef struct thread_args {
  pthread_t tid;
} thread_args_t;

void read_args(int argc, char *argv[], int *n_threads) {
  *n_threads = 3;
  if (argc >= 2) {
    *n_threads = atoi(argv[1]);
    if (*n_threads <= 0) {
      printf("n_threads must be positive\n");
      exit(EXIT_FAILURE);
    }
  }
}

void *thread_work(void *_args) {
  thread_args_t *args = _args;
  return NULL;
}

int has_ext(char *path, char *ext) {
  char *ext_pos = strrchr(path, '.');
  if (ext_pos != NULL && strcmp(ext_pos, ext) == 0) {
    return 1;
  }
  return 0;
}

void walk_dir(char *path, circ_buf *cb) {
  DIR *dir = opendir(path);
  if (dir == NULL) {
    ERR("opendir");
  }
  struct dirent *entry;
  struct stat statbuf;
  while ((entry = readdir(dir)) != NULL) {
    char *entry_name = entry->d_name;
    if (strcmp(entry_name, ".") == 0 || strcmp(entry_name, "..") == 0) {
      continue;
    }
    int path_len = strlen(path);
    int new_path_len = path_len + strlen(entry_name) + 2;
    char *new_path = malloc(new_path_len * sizeof(char));
    if (new_path == NULL) {
      ERR("malloc");
    }
    strcpy(new_path, path);
    new_path[path_len] = '/';
    strcpy(new_path + path_len + 1, entry_name);

    if (stat(new_path, &statbuf) < 0) {
      ERR("stat");
    }
    if (S_ISDIR(statbuf.st_mode)) {
      walk_dir(new_path, cb);
    } else if (S_ISREG(statbuf.st_mode)) {
      if (has_ext(entry_name, ".txt")) {
        printf("Added %s to cb\n", entry_name);
        circ_buf_push(cb, entry_name);
      }
    }
    free(new_path);
  }
  closedir(dir);
}

int main(int argc, char *argv[]) {
  int n_threads;
  read_args(argc, argv, &n_threads);
  thread_args_t *thread_args = malloc(n_threads * sizeof(thread_args_t));
  if (thread_args == NULL) {
    ERR("malloc");
  }
  for (int i = 0; i < n_threads; i++) {
    if (pthread_create(&(thread_args[i].tid), NULL, thread_work,
                       &thread_args[i])) {
      ERR("pthread_create");
    }
  }

  circ_buf *cb = circ_buf_create();
  walk_dir("data1", cb);
  while (cb->len > 0) {
    printf("%s\n", circ_buf_pop(cb));
  }

  for (int i = 0; i < n_threads; i++) {
    if (pthread_join(thread_args[i].tid, NULL)) {
      ERR("pthread_join");
    }
  }
  free(thread_args);
  circ_buf_destroy(cb);

  // we create n_threads threads, and each thread waits until it gets a file to
  // work on from the queue

  exit(EXIT_SUCCESS);
}

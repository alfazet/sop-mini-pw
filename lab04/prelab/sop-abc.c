#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   exit(EXIT_FAILURE))
#define UNUSED(x) ((void)(x))

typedef struct thread_args {
  pthread_t tid;
  int *quit;
  pthread_mutex_t *mtx_quit;
  sigset_t *mask;
} thread_args_t;

void msleep(unsigned int msec) {
  time_t sec = (int)(msec / 1000);
  msec = msec - (sec * 1000);
  struct timespec req = {0};
  req.tv_sec = sec;
  req.tv_nsec = msec * 1000000L;
  if (TEMP_FAILURE_RETRY(nanosleep(&req, &req)))
    ERR("nanosleep");
}

void *signal_handling(void *_args) {
  thread_args_t *args = _args;
  int *quit = args->quit;
  sigset_t *mask = args->mask;
  pthread_mutex_t *mtx_quit = args->mtx_quit;

  int signo;
  for (;;) {
    if (sigwait(mask, &signo))
      ERR("sigwait");
    if (signo == SIGINT) {
      pthread_mutex_lock(mtx_quit);
      *quit = 1;
      pthread_mutex_unlock(mtx_quit);
      break;
    }
  }

  return NULL;
}

int main() {
  int quit = 0;
  pthread_mutex_t mtx_quit = PTHREAD_MUTEX_INITIALIZER;
  sigset_t old_mask, new_mask;
  sigemptyset(&new_mask);
  sigaddset(&new_mask, SIGINT);
  if (pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask))
    ERR("pthread_sigmask");
  thread_args_t args;
  args.quit = &quit;
  args.mtx_quit = &mtx_quit;
  args.mask = &new_mask;
  if (pthread_create(&args.tid, NULL, signal_handling, &args))
    ERR("pthread_create");
  for (;;) {
    pthread_mutex_lock(&mtx_quit);
    if (quit) {
      pthread_mutex_unlock(&mtx_quit);
      break;
    }
    pthread_mutex_unlock(&mtx_quit);
    msleep(100);
  }
}

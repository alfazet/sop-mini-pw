#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 10
#define ITERATIONS 10000000

typedef struct context
{
    pthread_mutex_t mtx;
    int counter;
} context_t;

#define SHM_SIZE sizeof(context_t)

void producer(context_t *ctx)
{
    int err;

    for (int i = 0; i < ITERATIONS; ++i)
    {
        if ((err = pthread_mutex_lock(&ctx->mtx)) != 0)
        {
            if (err == EOWNERDEAD)
            {
                pthread_mutex_consistent(&ctx->mtx);
            }
            else
            {
                fprintf(stderr, "pthread_mutex_lock(): %s", strerror(err));
                exit(1);
            }
        }

        ctx->counter++;

        if ((err = pthread_mutex_unlock(&ctx->mtx)) != 0)
        {
            fprintf(stderr, "pthread_mutex_unlock(): %s", strerror(err));
            exit(1);
        }
    }
}

void consumer(context_t *ctx)
{
    int err;
    for (int i = 0; i < ITERATIONS; ++i)
    {
        if ((err = pthread_mutex_lock(&ctx->mtx)) != 0)
        {
            fprintf(stderr, "pthread_mutex_lock(): %s", strerror(err));
            exit(1);
        }

        ctx->counter--;

        if (i == 3) {
            printf("child exits!\n");
            kill(getpid(), SIGKILL);
            sleep(1);
        }

        if ((err = pthread_mutex_unlock(&ctx->mtx)) != 0)
        {
            fprintf(stderr, "pthread_mutex_unlock(): %s", strerror(err));
            exit(1);
        }
    }
}

int main()
{
    srand(getpid());

    shm_unlink("/sop_shm");

    int fd = shm_open("/sop_shm", O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0)
    {
        perror("shm_open()");
        return 1;
    }

    if (ftruncate(fd, SHM_SIZE) < 0)
    {
        perror("ftruncate()");
        return 1;
    }

    void *ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        perror("mmap()");
        return 1;
    }

    close(fd);

    context_t *ctx = (context_t *)ptr;
    ctx->counter = 0;

    int err;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if ((err = pthread_mutexattr_init(&attr)) != 0)
    {
        fprintf(stderr, "pthread_mutexattr_init(): %s", strerror(err));
        return 1;
    }

    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); // TODO: Uncomment
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST); // TODO: Uncomment

    printf("Creating mutex...\n");
    if ((err = pthread_mutex_init(&ctx->mtx, &attr)) != 0)
    {
        fprintf(stderr, "pthread_mutex_init(): %s", strerror(err));
        return 1;
    }

    switch (fork())
    {
        case -1:
            perror("fork()");
            return 1;
        case 0:
        {
            // child
            printf("[child] Running consumer...\n");
            consumer(ctx);
            printf("[child] Consumer done!\n");
            break;
        }
        default:
        {
            // parent
            printf("[parent] Running producer...\n");
            producer(ctx);
            printf("[parent] Producer done!\n");
            printf("[parent] Joining child...\n");
            while (wait(NULL) > 0)
                ;

            printf("counter = %d\n", ctx->counter);

            printf("[parent] Destroying mutex...\n");
            if ((err = pthread_mutex_destroy(&ctx->mtx)) != 0)
            {
                fprintf(stderr, "pthread_mutex_destroy(): %s", strerror(err));
                return 1;
            }

            if ((err = pthread_mutexattr_destroy(&attr)) != 0)
            {
                fprintf(stderr, "pthread_mutex_init(): %s", strerror(err));
                return 1;
            }
            break;
        }
    }

    printf("Unmapping shm\n");
    munmap(ptr, SHM_SIZE);
    return 0;
}

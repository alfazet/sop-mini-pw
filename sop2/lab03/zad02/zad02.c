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
#include <unistd.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define NAME_LEN 32
#define SHM_SIZE 4096

// Values of this function are in range (0,1]
double func(double x)
{
    usleep(2000);
    return exp(-x * x);
}

/**
 * It counts hit points by Monte Carlo method.
 * Use it to process one batch of computation.
 * @param N Number of points to randomize
 * @param a Lower bound of integration
 * @param b Upper bound of integration
 * @return Number of points which was hit.
 */
int randomize_points(int N, float a, float b)
{
    srand(getpid());
    int hits = 0;
    for (int i = 0; i < N; ++i)
    {
        double rand_x = ((double)rand() / RAND_MAX) * (b - a) + a;
        double rand_y = ((double)rand() / RAND_MAX);
        double real_y = func(rand_x);

        if (rand_y <= real_y)
            hits++;
    }
    return hits;
}

/**
 * This function calculates approximation of integral from counters of hit and total points.
 * @param total_randomized_points Number of total randomized points.
 * @param hit_points Number of hit points.
 * @param a Lower bound of integration
 * @param b Upper bound of integration
 * @return The approximation of integral
 */
double summarize_calculations(uint64_t total_randomized_points, uint64_t hit_points, float a, float b)
{
    return (b - a) * ((double)hit_points / (double)total_randomized_points);
}

/**
 * This function locks mutex and can sometime die (it has 2% chance to die).
 * It cannot die if lock would return an error.
 * It doesn't handle any errors. It's users responsibility.
 * Use it only in STAGE 4.
 *
 * @param mtx Mutex to lock
 * @return Value returned from pthread_mutex_lock.
 */
int random_death_lock(pthread_mutex_t* mtx)
{
    int ret = pthread_mutex_lock(mtx);
    if (ret)
        return ret;

    // 2% chance to die
    if (rand() % 50)
        abort();
    return ret;
}

void usage(char* argv[])
{
    printf("%s a b N - calculating integral with multiple processes\n", argv[0]);
    printf("a - Start of segment for integral (default: -1)\n");
    printf("b - End of segment for integral (default: 1)\n");
    printf("N - Size of batch to calculate before reporting to shared memory (default: 1000)\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    if (argc < 4)
        usage(argv);
    int a = atoi(argv[1]), b = atoi(argv[2]), n = atoi(argv[3]);

    int shm_fd;
    char *shm_name = "shm_zad02", *sem_name = "/sem_zad02";
    if ((shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666)) == -1)
        ERR("shm_open");
    sem_t* sem = sem_open(sem_name, O_CREAT, 0666, 1);
    if (sem_wait(sem) != 0)
        ERR("sem_wait");
    char* shm_ptr;
    if ((shm_ptr = (char*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        ERR("mmap");
    if (ftruncate(shm_fd, SHM_SIZE) != 0)
        ERR("ftruncate");
    pthread_mutex_t* mtx = (pthread_mutex_t*)shm_ptr;
    int* cnt_active = (int*)(shm_ptr + sizeof(pthread_mutex_t));
    if (*cnt_active == 0)
    {
        pthread_mutexattr_t mtx_attr;
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutexattr_setrobust(&mtx_attr, PTHREAD_MUTEX_ROBUST);
        pthread_mutex_init(mtx, &mtx_attr);
        if (pthread_mutexattr_destroy(&mtx_attr) != 0)
            ERR("mutexattr_destroy");
    }
    (*cnt_active)++;
    if (sem_post(sem) != 0)
        ERR("sem_post");
    if (sem_close(sem) != 0)
        ERR("sem_close");
    printf("currently active processes: %d\n", *cnt_active);
    usleep(2000 * 1000);

    (*cnt_active)--;
    if (*cnt_active == 0)
    {
        if (pthread_mutex_destroy(mtx) != 0)
            ERR("mutex_destroy");
        if (munmap(shm_ptr, SHM_SIZE) != 0)
            ERR("munmap");
        if (shm_unlink(shm_name) != 0)
            ERR("shm_unlink");
        if (sem_unlink(sem_name) != 0)
            ERR("sem_unlink");
    }
    if (munmap(shm_ptr, SHM_SIZE) != 0)
        ERR("munmap");

    return EXIT_SUCCESS;
}

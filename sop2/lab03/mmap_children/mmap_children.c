#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define ITERS 100000
#define LOG_LEN 8

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

void child_work(int id, float* out, char* log)
{
    int sample = 0;
    srand(getpid());
    int iters = ITERS;
    while (iters-- > 0)
    {
        double x = ((double)rand()) / RAND_MAX, y = ((double)rand()) / RAND_MAX;
        if (x * x + y * y <= 1.0)
            sample++;
    }
    out[id] = ((float)sample) / ITERS;
    char buf[LOG_LEN + 1];

    snprintf(buf, LOG_LEN + 1, "%7.5f\n", out[id] * 4.0f);
    memcpy(log + id * LOG_LEN, buf, LOG_LEN);
}

void parent_work(int n, float* data)
{
    pid_t pid;
    double sum = 0.0;
    for (;;)
    {
        pid = wait(NULL);
        if (pid <= 0)
        {
            if (errno == ECHILD)
                break;
            ERR("wait");
        }
    }
    for (int i = 0; i < n; i++)
        sum += data[i];
    sum /= n;
    printf("Pi is approximately %f\n", sum * 4);
}

void create_children(int n, float* data, char* log)
{
    for (int i = 0; i < n; i++)
    {
        int pid;
        if ((pid = fork()) == -1)
            ERR("fork");
        if (pid == 0)
        {
            child_work(i, data, log);
            exit(EXIT_SUCCESS);
        }
    }
}

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s n\n", name);
    fprintf(stderr, "1 <= n <= 30 - number of children\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    int n;
    if (argc != 2)
        usage(argv[0]);
    n = atoi(argv[1]);
    if (n <= 0 || n > 30)
        usage(argv[0]);

    int log_fd;
    if ((log_fd = open("./log.txt", O_CREAT | O_RDWR | O_TRUNC, -1)) == -1)
        ERR("open");
    // ftruncate - sets the file to a specific size (despite the name...)
    if (ftruncate(log_fd, n * LOG_LEN))
        ERR("ftruncate");
    char* log;
    if ((log = (char*)mmap(NULL, n * LOG_LEN, PROT_WRITE | PROT_READ, MAP_SHARED, log_fd, 0)) == MAP_FAILED)
        ERR("mmap");
    // Manpage: "The mmap() function shall add an extra reference to the file associated with the file descriptor
    // which is not removed by a subsequent close()"
    if (close(log_fd))
        ERR("close");
    float* data;
    if ((data = (float*)mmap(NULL, n * sizeof(float), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) ==
        MAP_FAILED)
        ERR("mmap");

    create_children(n, data, log);
    parent_work(n, data);

    if (munmap(data, n * sizeof(float)))
        ERR("munmap");
    // make sure that the data is actually written to the backing file
    if (msync(log, n * LOG_LEN, MS_SYNC))
        ERR("msync");
    if (munmap(log, n * LOG_LEN))
        ERR("munmap");

    return EXIT_SUCCESS;
}

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define ALPHA 26
#define FILENAME "./file.txt"

volatile sig_atomic_t last_signal;

void signal_handler(int signo) { last_signal = signo; }

void set_handler(void (*f)(int), int signo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (sigaction(signo, &act, NULL) == -1)
    {
        ERR("sigaction");
    }
}

void msleep(int msec)
{
    time_t sec = (int)(msec / 1000);
    msec = msec - (sec * 1000);
    struct timespec req = {0};
    req.tv_sec = sec;
    req.tv_nsec = msec * 1000000L;
    if (TEMP_FAILURE_RETRY(nanosleep(&req, &req)))
        ERR("nanosleep");
}

void child_work(int n, int id, int* res)
{
    msleep(1000);
    int pid = getpid();
    srand(pid);
    if (rand() % 100 <= 20)
        abort();

    int fd = open(FILENAME, O_RDONLY);
    if (fd == -1)
        ERR("open");
    struct stat stat;
    fstat(fd, &stat);
    int file_sz = stat.st_size;

    char* content;
    if ((content = (char*)mmap(NULL, file_sz, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
    {
        ERR("mmap");
    }
    if (close(fd) != 0)
        ERR("close");

    int len = file_sz / n, offset = len * id;
    if (id == n - 1)
        len = file_sz - offset;
    for (int i = offset; i < offset + len; i++)
    {
        if (content[i] >= 'a' && content[i] <= 'z')
            res[id * ALPHA + content[i] - 'a']++;
    }
    if (munmap(content, len) != 0)
        ERR("munmap");
}

void make_children(int n, int* res)
{
    for (int i = 0; i < n; i++)
    {
        int pid;
        if ((pid = fork()) == -1)
            ERR("fork");
        if (pid == 0)
        {
            child_work(n, i, res);
            exit(EXIT_SUCCESS);
        }
    }
}

void parent_work(int n, int* res)
{
    for (;;)
    {
        if (wait(NULL) <= 0)
        {
            if (errno == ECHILD)
                break;
            if (errno == EINTR && last_signal == SIGCHLD)
            {
                printf("Computation failed\n");
                return;
            }
            ERR("wait");
        }
    }
    int freq[ALPHA];
    memset(freq, 0, ALPHA * sizeof(int));
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < ALPHA; j++)
        {
            freq[j] += res[i * ALPHA + j];
        }
    }
    printf("Results:\n");
    for (char c = 'a'; c <= 'z'; c++)
        printf("%c: %d\n", c, freq[c - 'a']);
}

int main(int argc, char** argv)
{
    if (argc < 2)
        ERR("usage");
    int n = atoi(argv[1]);

    int* res;
    if ((res = (int*)mmap(NULL, n * ALPHA * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) ==
        MAP_FAILED)
        ERR("mmap");
    set_handler(signal_handler, SIGCHLD);
    make_children(n, res);
    parent_work(n, res);

    if (munmap(res, n * sizeof(int)) != 0)
        ERR("munmap");

    return EXIT_SUCCESS;
}

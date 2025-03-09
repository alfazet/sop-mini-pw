#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

int sethandler(void (*f)(int), int sig_no)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (sigaction(sig_no, &act, NULL) == -1)
        return -1;
    return 0;
}

void child_work(int fd)
{
    const int N = 32;
    char buf[N + 1];
    int len;
    if ((len = TEMP_FAILURE_RETRY(read(fd, buf, N))) < 0)
        ERR("read");
    buf[len] = '\0';
    printf("(child) read %d chars: %s\n", len, buf);
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
}

void create_child(int* write_end)
{
    int pid;
    int fds[2];
    if (pipe(fds) < 0)
        ERR("pipe");
    int read_end = fds[0];
    *write_end = fds[1];
    if ((pid = fork()) < 0)
        ERR("fork");
    if (pid == 0)
    {
        if (TEMP_FAILURE_RETRY(close(*write_end) < 0))
            ERR("close");
        child_work(read_end);
        exit(EXIT_SUCCESS);
    }
    if (TEMP_FAILURE_RETRY(close(read_end) < 0))
        ERR("close");
}

void parent_work(int fd)
{
    srand(time(NULL) * getpid());
    const int N = 16;
    char buf[N];
    unsigned char c = 'a' + rand() % 26;
    memset(buf, c, N);
    if (TEMP_FAILURE_RETRY(write(fd, buf, N)) < 0)
        ERR("write");
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
}

int main(int argc, char** argv)
{
    int write_end;
    create_child(&write_end);
    parent_work(write_end);
    while (wait(NULL) > 0)
        ;
}

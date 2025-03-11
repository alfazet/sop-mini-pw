#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define BUF_SZ 256

// child has a "personal" read-end and a common write-end
void child_work(int personal_read, int common_write)
{
    int pid = getpid();
    // printf("Student [%d]: created\n", pid);
    int len = 0;
    if (TEMP_FAILURE_RETRY(read(personal_read, &len, sizeof(int))) < 0)
        ERR("read");
    char* buf = malloc(len * sizeof(char));
    if (buf == NULL)
        ERR("malloc");
    if (TEMP_FAILURE_RETRY(read(personal_read, &buf, len)) < 0)
        ERR("read");
    char* msg = malloc(BUF_SZ * sizeof(char));
    if (msg == NULL)
        ERR("malloc");
    memset(msg, 0, BUF_SZ);
    sprintf(msg, "Student [%d]: HERE", pid);
    len = strlen(msg);
    if (TEMP_FAILURE_RETRY(write(common_write, &len, sizeof(int))) < 0)
        ERR("write");
    if (TEMP_FAILURE_RETRY(write(common_write, &msg, len)) < 0)
        ERR("write");
    printf("%s\n", msg);

    free(buf);
    free(msg);
}

// parent has an array of "personal" write-ends and one read-end
void parent_work(int n, int* child_pids, int* personal_writes, int common_read) {
    char* msg = malloc(BUF_SZ * sizeof(char));
    if (msg == NULL)
        ERR("malloc");
    int len;
    for (int i = 0; i < n; i++) {
        memset(msg, 0, BUF_SZ);
        sprintf(msg, "Teacher: Is [%d] here?", child_pids[i]);
        len = strlen(msg);
        if (TEMP_FAILURE_RETRY(write(personal_writes[i], &msg, len)) < 0)
            ERR("write");
        printf("%s\n", msg);
        if (TEMP_FAILURE_RETRY(read(common_read, &len, sizeof(int))) < 0)
            ERR("read");
        char* buf = malloc(len * sizeof(char));
        if (buf == NULL)
            ERR("malloc");
        if (TEMP_FAILURE_RETRY(read(common_read, &buf, len)) < 0)
            ERR("read");
        free(buf);
    }

    free(msg);
}

void make_children(int n, int* child_pids, int* personal_writes, int* common_read)
{
    int common_fds[2];
    if (pipe(common_fds) != 0)
        ERR("pipe");
    *common_read = common_fds[0];
    for (int i = 0; i < n; i++)
    {
        int personal_fds[2];
        if (pipe(personal_fds) != 0)
            ERR("pipe");
        int pid;
        if ((pid = fork()) < 0)
            ERR("fork");
        if (pid == 0)
        {
            if (TEMP_FAILURE_RETRY(close(personal_fds[1])) < 0)
                ERR("close");
            child_work(personal_fds[0], common_fds[1]);
            if (TEMP_FAILURE_RETRY(close(personal_fds[0])) < 0)
                ERR("close");
            free(child_pids);
            free(personal_writes);
            exit(EXIT_SUCCESS);
        }
        child_pids[i] = pid;
        personal_writes[i] = personal_fds[1];
        if (TEMP_FAILURE_RETRY(close(personal_fds[0])) < 0)
            ERR("close");
    }
    if (TEMP_FAILURE_RETRY(close(common_fds[1])) < 0)
        ERR("close");
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return EXIT_FAILURE;
    int n = atoi(argv[1]);

    int* child_pids = malloc(n * sizeof(int));
    if (child_pids == NULL)
        ERR("malloc");
    int* personal_writes = malloc(n * sizeof(int));
    if (personal_writes == NULL)
        ERR("malloc");
    int common_read = 0;
    make_children(n, child_pids, personal_writes, &common_read);
    parent_work(n, child_pids, personal_writes, common_read);
    for (int i = 0; i < n; i++)
    {
        if (TEMP_FAILURE_RETRY(close(personal_writes[i])) < 0)
            ERR("close");
    }
    if (TEMP_FAILURE_RETRY(close(common_read)) < 0)
        ERR("close");
    free(personal_writes);
    free(child_pids);

    return EXIT_SUCCESS;
}

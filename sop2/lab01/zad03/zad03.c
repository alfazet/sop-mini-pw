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
#define STAGES 4

int set_handler(void (*f)(int), int signo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (sigaction(signo, &act, NULL) == -1)
        return -1;
    return 0;
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

// child has a "personal" read-end and a common write-end
void child_work(int id, int personal_read, int common_write)
{
    int pid = getpid();
    srand(pid);
    int skill = rand() % 7 + 3;
    int len = 0;
    printf("Student [%d]\n", pid);

    if (TEMP_FAILURE_RETRY(read(personal_read, &len, sizeof(int))) < 0)
        ERR("read");
    char* buf = malloc(BUF_SZ * sizeof(char));
    if (buf == NULL)
        ERR("malloc");
    if (TEMP_FAILURE_RETRY(read(personal_read, &buf, len)) < 0)
        ERR("read");

    char* msg = malloc(BUF_SZ * sizeof(char));
    if (msg == NULL)
        ERR("malloc");
    memset(msg, 0, BUF_SZ);
    sprintf(msg, "Student [%d]: HERE", pid);
    printf("%s\n", msg);
    len = strlen(msg);
    if (TEMP_FAILURE_RETRY(write(common_write, &len, sizeof(int))) < 0)
        ERR("write");
    if (TEMP_FAILURE_RETRY(write(common_write, &msg, len)) < 0)
        ERR("write");

    for (int i = 0; i < STAGES; i++)
    {
        int t = rand() % 401 + 100;
        msleep(t);
        int q = rand() % 20 + 1;
        int res = skill + q;
        if (TEMP_FAILURE_RETRY(write(common_write, &id, sizeof(pid_t))) < 0)
            ERR("write");
        if (TEMP_FAILURE_RETRY(write(common_write, &pid, sizeof(pid_t))) < 0)
            ERR("write");
        if (TEMP_FAILURE_RETRY(write(common_write, &res, sizeof(int))) < 0)
            ERR("write");
        // react to teacher's message
    }

    free(msg);
    free(buf);
}

// parent has an array of "personal" write-ends and one read-end
void parent_work(int n, int* child_pids, int* personal_writes, int common_read)
{
    const int POINTS[STAGES] = {3, 6, 7, 5};

    srand(time(NULL));
    char* msg = malloc(BUF_SZ * sizeof(char));
    if (msg == NULL)
        ERR("malloc");
    char* buf = malloc(BUF_SZ * sizeof(char));
    if (buf == NULL)
        ERR("malloc");
    int len = 0;

    for (int i = 0; i < n; i++)
    {
        memset(msg, 0, BUF_SZ);
        sprintf(msg, "Teacher: Is [%d] here?", child_pids[i]);
        len = strlen(msg);
        if (TEMP_FAILURE_RETRY(write(personal_writes[i], &len, sizeof(int))) < 0)
            ERR("write");
        if (TEMP_FAILURE_RETRY(write(personal_writes[i], &msg, len)) < 0)
            ERR("write");
        printf("%s\n", msg);

        if (TEMP_FAILURE_RETRY(read(common_read, &len, sizeof(int))) < 0)
            ERR("read");
        if (TEMP_FAILURE_RETRY(read(common_read, &buf, len)) < 0)
            ERR("read");
    }

    int remaining = n;
    int* stage = malloc(n * sizeof(int));
    memset(stage, 0, n);
    while (remaining > 0) {
        int id = 0, pid = 0, res = 0;
        if (TEMP_FAILURE_RETRY(read(common_read, &id, sizeof(pid_t))) < 0)
            ERR("read");
        if (TEMP_FAILURE_RETRY(read(common_read, &pid, sizeof(pid_t))) < 0)
            ERR("read");
        if (TEMP_FAILURE_RETRY(read(common_read, &res, sizeof(int))) < 0)
            ERR("read");
        if (res >= POINTS[stage[id]] + rand() % 20 + 1) {
            // send 'ok' msg to student
            stage[id]++;
            if (stage[id] > 3)
                remaining--;
        } else {
            // send 'fail' msg to student
        }
        if (TEMP_FAILURE_RETRY(write(personal_writes[id], &msg, len)) < 0)
            ERR("write");
    }
    printf("Teacher: IT'S FINALLY OVER!\n");

    free(buf);
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
            child_work(i, personal_fds[0], common_fds[1]);
            if (TEMP_FAILURE_RETRY(close(personal_fds[0])) < 0)
                ERR("close");
            if (TEMP_FAILURE_RETRY(close(common_fds[1])) < 0)
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
    {
        printf("usage\n");
        return EXIT_FAILURE;
    }
    int n = atoi(argv[1]);

    // if (set_handler(SIG_IGN, SIGPIPE) < 0)
    //     ERR("set_handler");

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

#define _GNU_SOURCE
#include <errno.h>
#include <mqueue.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define MAXN 256
#define MAX_MSG_COUNT 10
#define MAX_MSG_SIZE 4096

void open_mq(mqd_t *mq, char *name, int flags)
{
    struct mq_attr mq_attr = {};
    mq_attr.mq_maxmsg = MAX_MSG_COUNT;
    mq_attr.mq_msgsize = MAX_MSG_SIZE;
    *mq = mq_open(name, flags, 0600, &mq_attr);
    if (*mq == -1)
        ERR("mq_open");
}

void child_work(mqd_t *server_mq)
{
    int pid = getpid();
    srand(pid);
    printf("[%d] Worker ready!\n", pid);
    char msg[MAXN];

    for (;;)
    {
        int res = mq_receive(*server_mq, msg, MAXN, NULL);
        if (res == -1)
        {
            if (errno == EAGAIN)
                break;
            if (errno == EMSGSIZE) {
                ERR("what");
            }
            else
                ERR("mq_receive");
        }
        printf("[%d] Received %s\n", pid, msg);
    }
    printf("[%d] Worker done!\n", pid);
}

int main() {


}

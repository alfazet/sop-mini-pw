#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <signal.h>
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
#define MQ_MSG_COUNT 2
#define MQ_MSG_SIZE 4096
#define MAX_CLIENTS 8

typedef struct
{
    int id;
    char name[256];
} child_data;

// void handle_msgs(union sigval data);
//
// void register_notif(client_data* data)
// {
//     struct sigevent notif = {
//         .sigev_value.sival_ptr = data, .sigev_notify = SIGEV_THREAD, .sigev_notify_function = handle_msgs};
//     if (mq_notify(data->mq, &notif) == -1)
//         ERR("mq_notify");
// }

// void handle_msgs(union sigval data)
// {
//     client_data* client_data = data.sival_ptr;
//     char msg[MQ_MSG_SIZE];
//     unsigned int priority;
//     register_notif(client_data);
//
//     for (;;)
//     {
//         if (mq_receive(client_data->mq, msg, MQ_MSG_SIZE, &priority) != -1)
//             printf("[%s] recieved \"%s\" with priority %d\n", client_data->name, msg, priority);
//         else
//         {
//             if (errno == EAGAIN)
//                 break;
//             else
//                 ERR("mq_receive");
//         }
//     }
// }

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

void child_work(char* name, int prev_pid, int t1, int t2)
{
    int pid = getpid();
    srand(getpid());
    printf("[%d] %s joined\n", pid, name);
    char mq_name[MAXN];
    snprintf(mq_name, MAXN, "/sop_mq_%d", pid);
    mqd_t mq;
    struct mq_attr mq_attr = {};
    mq_attr.mq_maxmsg = MQ_MSG_COUNT;
    mq_attr.mq_msgsize = MQ_MSG_SIZE;
    mq = mq_open(mq_name, O_NONBLOCK | O_RDONLY | O_CREAT, 0600, &mq_attr);
    if (mq == -1)
        ERR("mq_open");

    if (mq_close(mq) == -1)
        ERR("mq_close");
    if (mq_unlink(mq_name) == -1)
        ERR("mq_unlink");

    printf("[%d] %s left\n", pid, name);
    exit(EXIT_SUCCESS);
}

int main(int argc, char** argv)
{
    if (argc < 4)
        ERR("usage");
    int p = atoi(argv[1]), t1 = atoi(argv[2]), t2 = atoi(argv[3]);
    char line[MAXN];
    char* ptr;
    int i = 0, prev_pid = getpid();
    for (;;)
    {
        fgets(line, MAXN, stdin);
        line[strlen(line) - 1] = '\0';
        if (strstr(line, "start"))
        {
            ptr = strstr(line, "\"");
            ptr++;
            ptr[strlen(ptr) - 1] = '\0';
            break;
        }
        int pid;
        if ((pid = fork()) == -1)
            ERR("fork");
        if (pid == 0)
            child_work(line, prev_pid, t1, t2);
        if (i == 0) {
            // pass the first word to the child
        }
        i++;
    }
    char sentence[MAXN];
    strcpy(sentence, ptr);
    printf("starting the game with sentence \"%s\"\n", sentence);
    while (wait(NULL) > 0)
        ;

    return EXIT_SUCCESS;
}

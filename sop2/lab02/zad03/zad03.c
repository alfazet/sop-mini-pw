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
#define MQ_MSG_SIZE 256
#define MAX_CLIENTS 8

typedef struct
{
    int id;
    char name[256];
} child_data;

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

void open_mq(mqd_t* mq, char* name)
{
    struct mq_attr mq_attr = {};
    mq_attr.mq_maxmsg = MQ_MSG_COUNT;
    mq_attr.mq_msgsize = MQ_MSG_SIZE;
    *mq = mq_open(name, O_RDWR | O_CREAT, 0600, &mq_attr);
    if (*mq == -1)
        ERR("mq_open");
}

void child_work(char* name, int prev_pid, int p, int t1, int t2)
{
    int pid = getpid();
    srand(getpid());
    printf("[%d] %s joined\n", pid, name);
    char mq_name[MAXN];

    snprintf(mq_name, MAXN, "/sop_mq_%d", prev_pid);
    mqd_t recv_mq;
    open_mq(&recv_mq, mq_name);

    snprintf(mq_name, MAXN, "/sop_mq_%d", pid);
    mqd_t send_mq;
    open_mq(&send_mq, mq_name);

    char msg[MAXN];

    for (;;)
    {
        memset(msg, 0, MAXN);
        if (mq_receive(recv_mq, msg, MAXN, NULL) == -1)
            ERR("mq_receive");
        if (msg[0] == '!')
        {
            if (mq_send(send_mq, &msg[0], 1, 0) == -1)
                ERR("mq_send");
            break;
        }
        printf("[%d] %s got the message: %s\n", pid, name, msg);
        for (int i = 0; i < MAXN; i++)
        {
            if (msg[i] == '\0')
                break;
            if (rand() % 100 < p)
                msg[i] = 'a' + rand() % 26;
        }
        msleep(t1 + rand() % (t2 - t1));
        if (mq_send(send_mq, msg, MAXN, 0) == -1)
            ERR("mq_send");
    }

    if (mq_close(recv_mq) == -1)
        ERR("mq_close");
    if (mq_close(send_mq) == -1)
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
    int prev_pid = getpid();
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
            child_work(line, prev_pid, p, t1, t2);
        prev_pid = pid;
    }
    char sentence[MAXN];
    strcpy(sentence, ptr);
    printf("starting the game with sentence \"%s\"\n", sentence);

    char mq_name[MAXN];
    snprintf(mq_name, MAXN, "/sop_mq_%d", prev_pid);
    mqd_t recv_mq;
    open_mq(&recv_mq, mq_name);

    snprintf(mq_name, MAXN, "/sop_mq_%d", getpid());
    mqd_t send_mq;
    open_mq(&send_mq, mq_name);

    int words = 0;
    char* word = strtok(sentence, " ");
    while (word != NULL)
    {
        words++;
        if (mq_send(send_mq, word, MQ_MSG_SIZE, 0) == -1)
            ERR("mq_send");
        word = strtok(NULL, " ");
    }
    char c = '!';
    if (mq_send(send_mq, &c, 1, 0) == -1)
        ERR("mq_send");

    char final_msg[MAXN], temp[MAXN];
    memset(final_msg, 0, MAXN);
    for (int i = 0; i < words; i++)
    {
        if (mq_receive(recv_mq, temp, MAXN, NULL) == -1)
            ERR("mq_receive");
        if (i != 0)
            final_msg[strlen(final_msg)] = ' ';
        strcat(final_msg, temp);
    }
    printf("final: %s\n", final_msg);
    while (wait(NULL) > 0)
        ;
    if (mq_close(recv_mq) == -1)
        ERR("mq_close");
    if (mq_close(send_mq) == -1)
        ERR("mq_close");
    if (mq_unlink(mq_name) == -1)
        ERR("mq_unlink");
    return EXIT_SUCCESS;
}

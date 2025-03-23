#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILD_COUNT 4

#define QUEUE_NAME_MAX_LEN 32
#define CHILD_NAME_MAX_LEN 32

#define MSG_SIZE 64
#define MAX_MSG_COUNT 4

#define ROUNDS 5

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

mqd_t create_queue(char* name)
{
    struct mq_attr attr = {};
    attr.mq_maxmsg = MAX_MSG_COUNT;
    attr.mq_msgsize = MSG_SIZE;

    mqd_t res = mq_open(name, O_RDWR | O_NONBLOCK | O_CREAT, 0600, &attr);
    if (res == -1)
        ERR("mq_open");

    return res;
}

typedef struct
{
    char* name;
    mqd_t queue;
} child_data;

void handle_messages(union sigval data);

void register_notification(child_data* data)
{
    struct sigevent notification = {
        .sigev_value.sival_ptr = data, .sigev_notify = SIGEV_THREAD, .sigev_notify_function = handle_messages};

    int res = mq_notify(data->queue, &notification);
    if (res == -1)
        ERR("mq_notify");
}

void handle_messages(union sigval data)
{
    child_data* child_data = data.sival_ptr;
    char message[MSG_SIZE];
    // again, register the notification again before we read the message
    register_notification(child_data);

    for (;;)
    {
        int res = mq_receive(child_data->queue, message, MSG_SIZE, NULL);
        if (res != -1)
            printf("%s: Accepi \"%s\"\n", child_data->name, message);
        else
        {
            if (errno == EAGAIN)
                break;
            else
                ERR("mq_receive");
        }
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

void child_work(char* name, mqd_t* queues, int i)
{
    int pid = getpid();
    printf("%s: A PID %d incipiens.\n", name, pid);
    srand(pid);
    child_data child_data = {.name = name, .queue = queues[i]};
    union sigval data;
    data.sival_ptr = &child_data;
    handle_messages(data);

    for (int i = 0; i < ROUNDS; ++i)
    {
        int receiver = rand() % CHILD_COUNT;
        char message[MSG_SIZE];
        switch (rand() % 3)
        {
            case 0:
                snprintf(message, MSG_SIZE, "%s: Salve %d!", name, receiver);
                break;
            case 1:
                snprintf(message, MSG_SIZE, "%s: Visne garum emere, %d?", name, receiver);
                break;
            case 2:
                snprintf(message, MSG_SIZE, "%s: Fuistine hodie in thermis, %d?", name, receiver);
                break;
        }
        if (mq_send(queues[receiver], message, MSG_SIZE, 0) == -1)
            ERR("mq_send");
        msleep(1000 * (rand() % 3 + 1));
    }
    printf("%s: Disceo.\n", name);

    // IMPORTANT: we exit HERE, not after child_work (as we would usually)
    // this is because the notification thread could still be using the child_data
    // structure, so we can't let it get out of scope
    exit(EXIT_SUCCESS);
}

void create_children(char names[][CHILD_NAME_MAX_LEN], mqd_t* queues, int n)
{
    for (int i = 0; i < n; i++)
    {
        int pid;
        if ((pid = fork()) == -1)
            ERR("pid");
        if (pid == 0)
        {
            child_work(names[i], queues, i);
            // note: we DON'T close queues in children because we handle notifications
            // on separate threads - and we know nothing about when these threads are running
            // or when they die
            // so we need to keep the queues open, the kernel will kill them on exit anyway
        }
    }
}

int main(int argc, char** argv)
{
    mqd_t queues[CHILD_COUNT];
    char names[CHILD_COUNT][CHILD_NAME_MAX_LEN];
    char queue_names[CHILD_COUNT][CHILD_NAME_MAX_LEN];

    for (int i = 0; i < CHILD_COUNT; ++i)
    {
        snprintf(queue_names[i], QUEUE_NAME_MAX_LEN, "/child_%d", i);
        queues[i] = create_queue(queue_names[i]);
        snprintf(names[i], CHILD_NAME_MAX_LEN, "Persona %d", i);
    }
    create_children(names, queues, CHILD_COUNT);

    for (int i = 0; i < CHILD_COUNT; i++)
    {
        if (mq_close(queues[i]) != 0)
            ERR("mq_close");
    }
    while (wait(NULL) > 0)
        ;
    printf("Parens: Disceo.");
    for (int i = 0; i < CHILD_COUNT; ++i)
    {
        int result = mq_unlink(queue_names[i]);
        // The case when the queue does not exist already
        // is not treated as an error
        if (result == -1 && errno != ENOENT)
            ERR("mq_close");
    }
    return EXIT_SUCCESS;
}

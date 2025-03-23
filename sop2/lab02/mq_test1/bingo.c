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

#define LIFESPAN 10
#define MAXN 10

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t children_left = 0;

int set_handler(void (*f)(int, siginfo_t *, void *), int signo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO;
    if (sigaction(signo, &act, NULL) == -1)
        return -1;
    return 0;
}

void mq_handler(int sig, siginfo_t *info, void *p)
{
    mqd_t *mq;
    uint8_t number;
    unsigned int priority;
    mq = (mqd_t *)info->si_value.sival_ptr;

    // always "reinstall" the notification as the first thing
    // when handling one 

    // if there was a time window between receiving the message and installing the
    // notification again, a message could enter the queue during that time and we
    // wouldn't be notified of it (remember that the notification only comes ONCE,
    // when the queue changes from empty to non-empty)
    static struct sigevent notif;
    notif.sigev_notify = SIGEV_SIGNAL;
    notif.sigev_signo = SIGRTMIN;
    notif.sigev_value.sival_ptr = mq;
    if (mq_notify(*mq, &notif) < 0)
        ERR("mq_notify");

    for (;;)
    {
        if (mq_receive(*mq, (char *)&number, 1, &priority) < 1)
        {
            if (errno == EAGAIN)
                break;
            else
                ERR("mq_receive");
        }
        // use priority to differentiate messages
        if (priority == 0)
            printf("[PARENT] Got timeout from %d.\n", number);
        else
            printf("[PARENT] %d is a bingo number!\n", number);
    }
}

void sigchld_handler(int sig, siginfo_t *info, void *p)
{
    pid_t pid;
    for (;;)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if (pid == 0)
            return;
        if (pid <= 0)
        {
            if (errno == ECHILD)
                return;
            ERR("waitpid");
        }
        children_left--;
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

void child_work(int id, mqd_t mq_end, mqd_t mq_rng)
{
    int pid = getpid();
    srand(pid);
    uint8_t my_bingo = (uint8_t)(rand() % MAXN);
    uint8_t number;
    int life = rand() % LIFESPAN + 1;
    while (life--)
    {
        if (TEMP_FAILURE_RETRY(mq_receive(mq_rng, (char *)&number, 1, NULL)) < 1)
            ERR("mq_receive");
        printf("[%d] Received %d\n", pid, number);
        if (my_bingo == number)
        {
            if (TEMP_FAILURE_RETRY(mq_send(mq_end, (const char *)&my_bingo, 1, 1)) != 0)
                ERR("mq_send");
            return;
        }
    }
    if (TEMP_FAILURE_RETRY(mq_send(mq_end, (const char *)&id, 1, 0)))
        ERR("mq_send");
}

void parent_work(mqd_t mq_rng)
{
    srand(getpid());
    while (children_left)
    {
        uint8_t number = (uint8_t)(rand() % MAXN);
        if (TEMP_FAILURE_RETRY(mq_send(mq_rng, (const char *)&number, 1, 0)))
            ERR("mq_send");
        msleep(1000);
    }
    printf("[PARENT] Terminating\n");
}

void create_children(int n, mqd_t mq_end, mqd_t mq_rng)
{
    for (int i = 0; i < n; i++)
    {
        int pid;
        if ((pid = fork()) == -1)
            ERR("fork");
        if (pid == 0)
        {
            child_work(n, mq_end, mq_rng);
            exit(EXIT_SUCCESS);
        }
        children_left++;
    }
}

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s n\n", name);
    fprintf(stderr, "0 < x < 100 - number of children\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int n;
    if (argc != 2)
        usage(argv[0]);
    n = atoi(argv[1]);
    if (n <= 0 || n >= 100)
        usage(argv[0]);

    mqd_t mq_end, mq_rng;
    struct mq_attr attr = {.mq_maxmsg = 10, .mq_msgsize = 1};
    if ((mq_end = TEMP_FAILURE_RETRY(mq_open("/bingo_mq_end", O_RDWR | O_NONBLOCK | O_CREAT, 0600, &attr))) ==
        (mqd_t)-1)
        ERR("mq_open");
    if ((mq_rng = TEMP_FAILURE_RETRY(mq_open("/bingo_mq_rng", O_RDWR | O_CREAT, 0600, &attr))) == (mqd_t)-1)
        ERR("mq_open");

    set_handler(sigchld_handler, SIGCHLD);
    set_handler(mq_handler, SIGRTMIN);
    create_children(n, mq_end, mq_rng);

    static struct sigevent notif;
    notif.sigev_notify = SIGEV_SIGNAL;
    notif.sigev_signo = SIGRTMIN;
    notif.sigev_value.sival_ptr = &mq_end;
    if (mq_notify(mq_end, &notif) < 0)
        ERR("mq_notify");
    parent_work(mq_rng);

    mq_close(mq_end);
    mq_close(mq_rng);
    if (mq_unlink("/bingo_mq_end"))
        ERR("mq_unlink");
    if (mq_unlink("/bingo_mq_rng"))
        ERR("mq_unlink");
    return EXIT_SUCCESS;
}

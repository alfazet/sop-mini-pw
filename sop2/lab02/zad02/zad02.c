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
#define MAX_MSG_COUNT 10
#define MAX_MSG_SIZE 4096
#define N_TASKS 2

typedef struct task_data
{
    double v1;
    double v2;
} task_data;

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

mqd_t open_mq(char *name, int flags)
{
    struct mq_attr mq_attr = {};
    mq_attr.mq_maxmsg = MAX_MSG_COUNT;
    mq_attr.mq_msgsize = MAX_MSG_SIZE;
    mqd_t mq = mq_open(name, flags, 0600, &mq_attr);
    if (mq == -1)
        ERR("mq_open");
    return mq;
}

void mq_handler(int sig, siginfo_t *info, void *p);

void setup_notif(mqd_t *mq)
{
    set_handler(mq_handler, SIGRTMIN);
    static struct sigevent notif;
    notif.sigev_notify = SIGEV_SIGNAL;
    notif.sigev_signo = SIGRTMIN;
    notif.sigev_value.sival_ptr = mq;
    if (mq_notify(*mq, &notif) < 0)
        ERR("mq_notify");
}

volatile sig_atomic_t results_received = 0;

void mq_handler(int sig, siginfo_t *info, void *p)
{
    (void)sig;
    (void)p;
    mqd_t *mq = (mqd_t *)info->si_value.sival_ptr;
    setup_notif(mq);
    unsigned who;
    double v;

    for (;;)
    {
        int res = mq_receive(*mq, (char *)&v, MAX_MSG_SIZE, &who);
        if (res != -1)
        {
            printf("Result from worker [%d]: %.2f\n", who, v);
            results_received++;
        }
        else
        {
            if (errno == EAGAIN)
                break;
            else
                ERR("mq_receive");
        }
    }
}

void child_work(mqd_t *server_mq, mqd_t *res_mq)
{
    int pid = getpid();
    srand(pid);
    printf("[%d] Worker ready!\n", pid);
    task_data data;

    int tasks_done = 0;
    for (;;)
    {
        for (;;)
        {
            int res = mq_receive(*server_mq, (char *)&data, MAX_MSG_SIZE, NULL);
            if (res == -1)
            {
                if (errno == EAGAIN)
                    continue;
                else
                    ERR("mq_receive");
            }
            else
                break;
        }
        printf("[%d] Received task [%.2f, %.2f]\n", pid, data.v1, data.v2);
        msleep(rand() % 1500 + 500);
        double v = data.v1 + data.v2;
        if (mq_send(*res_mq, (char *)&v, sizeof(double), (unsigned)pid) == -1)
            ERR("mq_send");
        tasks_done++;
        if (tasks_done == N_TASKS)
            break;
    }
    printf("[%d] Worker done!\n", pid);
}

int main(int argc, char **argv)
{
    if (argc < 4)
        ERR("usage");
    int t1 = atoi(argv[1]), t2 = atoi(argv[2]);
    int n = atoi(argv[3]);
    printf("Server is starting...\n");

    int server_pid = getpid();
    char mq_name[MAXN];
    sprintf(mq_name, "/mq_%d", server_pid);
    if (mq_unlink(mq_name) != 0)
    {
        if (errno != ENOENT)
            ERR("mq_unlink");
    }
    mqd_t server_mq = open_mq(mq_name, O_RDWR | O_CREAT | O_NONBLOCK);
    mqd_t *queues = malloc(n * sizeof(mqd_t));
    if (queues == NULL)
        ERR("malloc");
    int *worker_pids = malloc(n * sizeof(int));
    if (worker_pids == NULL)
        ERR("malloc");
    for (int i = 0; i < n; i++)
    {
        int pid;
        if ((pid = fork()) == -1)
            ERR("fork");
        sprintf(mq_name, "/mq_res_%d", (pid != 0) ? pid : getpid());
        mqd_t res_mq = open_mq(mq_name, O_RDWR | O_CREAT | O_NONBLOCK);
        if (pid == 0)
        {
            child_work(&server_mq, &res_mq);
            if (mq_close(server_mq) != 0)
                ERR("mq_close");
            if (mq_close(res_mq) != 0)
                ERR("mq_close");
            free(queues);
            free(worker_pids);
            exit(EXIT_SUCCESS);
        }
        worker_pids[i] = pid;
        queues[i] = res_mq;
        setup_notif(&queues[i]);
    }

    for (;;)
    {
        msleep(t1 + rand() % (t2 - t1));
        task_data data = {.v1 = (rand() % (100 * 100)) / 100.0, .v2 = (rand() % (100 * 100)) / 100.0};
        for (;;)
        {
            if (results_received == n * N_TASKS)
                break;
            int res = mq_send(server_mq, (char *)&data, sizeof(task_data), 0);
            if (res != -1)
            {
                printf("New task queued: [%.2f, %.2f]\n", data.v1, data.v2);
                break;
            }
            else
            {
                if (errno != EAGAIN)
                    ERR("mq_send");
                printf("Queue is full!\n");
            }
        }
        if (results_received == n * N_TASKS)
            break;
    }
    while (wait(NULL) > 0)
        ;

    printf("All child processes have finished.\n");
    if (mq_close(server_mq) != 0)
        ERR("mq_close");
    sprintf(mq_name, "/mq_%d", server_pid);
    if (mq_unlink(mq_name) != 0)
        ERR("mq_unlink");
    for (int i = 0; i < n; i++)
    {
        sprintf(mq_name, "/mq_res_%d", worker_pids[i]);
        if (mq_unlink(mq_name) != 0)
        {
            printf("%d\n", worker_pids[i]);
            ERR("mq_unlink");
        }
    }
    free(queues);
    free(worker_pids);

    return EXIT_SUCCESS;
}

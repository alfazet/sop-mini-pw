#include <fcntl.h>
#include <signal.h>
#define _GNU_SOURCE
#include <errno.h>
#include <mqueue.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define MAXN 256
#define MQ_MSG_COUNT 10
#define MQ_MSG_SIZE 4096
#define MAX_CLIENTS 8

typedef struct
{
    char* name;
    mqd_t mq;
} client_data;

volatile sig_atomic_t server_quit = 0;

int set_handler(void (*f)(int), int signo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (sigaction(signo, &act, NULL) == -1)
        return -1;
    return 0;
}

void sigint_handler(int signo) { server_quit = 1; }

void handle_msgs(union sigval data);

void register_notif(client_data* data)
{
    struct sigevent notif = {
        .sigev_value.sival_ptr = data, .sigev_notify = SIGEV_THREAD, .sigev_notify_function = handle_msgs};
    if (mq_notify(data->mq, &notif) == -1)
        ERR("mq_notify");
}

void handle_msgs(union sigval data)
{
    client_data* client_data = data.sival_ptr;
    char msg[MQ_MSG_SIZE];
    unsigned int priority;
    register_notif(client_data);

    for (;;)
    {
        if (mq_receive(client_data->mq, msg, MQ_MSG_SIZE, &priority) != -1)
            printf("[%s] recieved \"%s\" with priority %d\n", client_data->name, msg, priority);
        else
        {
            if (errno == EAGAIN)
                break;
            else
                ERR("mq_receive");
        }
    }
}

void run_client(char* server_name, char* client_name)
{
    char send_mq_name[MAXN];
    sprintf(send_mq_name, "/chat_%s", server_name);
    struct mq_attr attr = {.mq_maxmsg = MQ_MSG_COUNT, .mq_msgsize = MQ_MSG_SIZE};
    mqd_t send_mq = mq_open(send_mq_name, O_RDWR | O_CREAT | O_NONBLOCK, 0600, &attr);
    if (send_mq == -1)
        ERR("mq_open");

    char msg[MQ_MSG_SIZE];
    strcpy(msg, client_name);
    if (mq_send(send_mq, msg, MQ_MSG_SIZE, 0) == -1)
        ERR("mq_send");
    printf("sent\n");

    if (mq_close(send_mq) != 0)
        ERR("mq_close");

    // why can't this be here?
    // if (mq_unlink(send_mq_name) != 0)
    //     ERR("mq_unlink");
}

void run_server(char* server_name)
{
    client_data clients[MAX_CLIENTS];
    int n_clients = 0;

    char recv_mq_name[MAXN];
    sprintf(recv_mq_name, "/chat_%s", server_name);
    struct mq_attr attr = {.mq_maxmsg = MQ_MSG_COUNT, .mq_msgsize = MQ_MSG_SIZE};
    mqd_t recv_mq = mq_open(recv_mq_name, O_RDWR | O_CREAT | O_NONBLOCK, 0600, &attr);
    if (recv_mq == -1)
        ERR("mq_open");

    client_data server_data;
    server_data.name = server_name;
    server_data.mq = recv_mq;
    union sigval data = {.sival_ptr = &server_data};
    handle_msgs(data);

    for (;;)
        ;

    if (mq_close(recv_mq) != 0)
        ERR("mq_close");
    if (mq_unlink(recv_mq_name) != 0)
        ERR("mq_unlink");
}

int main(int argc, char** argv)
{
    char* mode = argv[1];
    char* server_name = argv[2];
    if (strcmp(mode, "client") == 0)
    {
        printf("Running as client\n");
        char* client_name = argv[3];
        run_client(server_name, client_name);
    }
    else
    {
        printf("Running as server\n");
        run_server(server_name);
    }
    return EXIT_SUCCESS;
}

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../sop_net.h"

#define MAX_EVENTS 20
#define BUF_SIZE 1024

volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig)
{
    (void)sig;
    do_work = 0;
}

void msleep(unsigned int msec)
{
    time_t sec = (int)(msec / 1000);
    msec = msec - (sec * 1000);
    struct timespec req = {0};
    req.tv_sec = sec;
    req.tv_nsec = msec * 1000000L;
    if (TEMP_FAILURE_RETRY(nanosleep(&req, &req)))
        ERR("nanosleep");
}

void set_nonblock(int fd)
{
    int new_flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flags);
}

void run_client(int server_fd)
{
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
        ERR("epoll_create1");
    struct epoll_event event, events[MAX_EVENTS];

    // remember to have event.data.fd the same as the third epoll_ctl argument...
    event.data.fd = STDIN_FILENO;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event) < 0)
        ERR("epoll_ctl");

    event.data.fd = server_fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0)
        ERR("epoll_ctl");

    int owner[MAX_EVENTS];
    for (int i = 0; i < MAX_EVENTS; i++)
        owner[i] = -1;

    char* buf = malloc(BUF_SIZE * sizeof(char));
    if (buf == NULL)
        ERR("malloc");
    int n_read;
    size_t len = BUF_SIZE;
    while (do_work)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
                continue;
            ERR("epoll_wait");
        }
        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {
                if (fd == STDIN_FILENO)
                {
                    memset(buf, 0, len);
                    if ((n_read = getline(&buf, &len, stdin)) < 0)
                        ERR("getline");
                    if (n_read == 2 && buf[0] == 'e')
                    {
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server_fd, NULL) < 0)
                            ERR("epoll_ctl");
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, STDIN_FILENO, NULL) < 0)
                            ERR("epoll_ctl");
                        do_work = 0;
                        break;
                    }
                    if (n_read == 2 && buf[0] == 'o')
                    {
                        for (int i = 0; i < MAX_EVENTS; i++)
                        {
                            printf("City %d belongs to the ", i + 1);
                            if (owner[i] == 0)
                                printf("Greeks\n");
                            else if (owner[i] == 1)
                                printf("Persians\n");
                            else
                                printf("???\n");
                        }
                    }
                    if (n_read == 5 && buf[0] == 't')
                    {
                        int x = atoi(buf + 2);
                        if (x < 1 || x > 20)
                        {
                            printf("t XX, where 01 <= XX <= 20\n");
                            continue;
                        }
                        char random_c = 'g';
                        owner[x - 1] = 0;
                        if (rand() % 2 == 0)
                        {
                            random_c = 'p';
                            owner[x - 1] = 1;
                        }

                        char msg[5];
                        sprintf(msg, "%c%02d\n", random_c, x);
                        if (bulk_write(server_fd, msg, strlen(msg)) < 0)
                            ERR("bulk_write");
                    }
                }
            }
        }
    }
    free(buf);
}

int main(int argc, char** argv)
{
    if (argc != 3)
        ERR("usage [address] [port]");
    char* addr_str = argv[1];
    char* port_str = argv[2];
    int server_fd = connect_tcp_socket(addr_str, port_str);
    if (server_fd < 0)
        ERR("connect_tcp_socket");
    srand(time(NULL));
    run_client(server_fd);

    if (close(server_fd) < 0)
        ERR("close");
    return EXIT_SUCCESS;
}

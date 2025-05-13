#include <fcntl.h>
#include <sys/epoll.h>
#include "../sop_net.h"

#define MAX_EVENTS 7
#define BUF_SIZE 1024

volatile sig_atomic_t do_work = 1;

void set_nonblock(int fd)
{
    int new_flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flags);
}

void close_client(int epoll_fd, int fd)
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0)
        ERR("epoll_ctl");
    if (close(fd) < 0)
        ERR("close");
    printf("Descriptor closed\n");
}

int search(int* a, int n, int x)
{
    for (int i = 0; i < n; i++)
    {
        if (a[i] == x)
            return i;
    }
    return -1;
}

int reset(int* a, int n, int x)
{
    for (int i = 0; i < n; i++)
    {
        if (a[i] == x)
        {
            a[i] = -1;
            return 0;
        }
    }
    return -1;
}

void server_work(int listen_fd)
{
    int epoll_fd;
    if ((epoll_fd = epoll_create1(0)) < 0)
        ERR("epoll_create1");
    struct epoll_event event, events[MAX_EVENTS];
    event.data.fd = listen_fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) < 0)
        ERR("epoll_ctl");

    int electors[MAX_EVENTS];
    int votes[MAX_EVENTS];
    for (int i = 0; i < MAX_EVENTS; i++)
        electors[i] = -1;

    while (do_work)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0)
            ERR("epoll_wait");
        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {
                if (fd == listen_fd)
                {
                    int client_fd = add_new_client(listen_fd);
                    if (client_fd < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                        ERR("add_new_client");
                    set_nonblock(client_fd);
                    event.data.fd = client_fd;
                    event.events = EPOLLIN;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0)
                        ERR("epoll_ctl");

                    // printf("All votes thus far:\n");
                    // for (int i = 0; i < MAX_EVENTS; i++)
                    //     printf("Elector of %d voted for %d\n", i + 1, votes[i]);
                }
                else
                {
                    char buf;
                    // sometimes recv and read work instead
                    ssize_t n_read = bulk_read(fd, &buf, sizeof(char));
                    if (n_read > 0)
                    {
                        int who = search(electors, MAX_EVENTS, fd);
                        if (who == -1)
                        {
                            // just (re)connected or impostor
                            if (buf < '1' || buf > '7')
                            {
                                close_client(epoll_fd, fd);
                            }
                            else
                            {
                                int k = (int)(buf - '0');
                                char msg[BUF_SIZE];
                                if (electors[k - 1] != -1)
                                {
                                    close_client(epoll_fd, fd);
                                    printf("Impostor disguised as elector %d!\n", k);
                                }
                                else
                                {
                                    electors[k - 1] = fd;
                                    sprintf(msg, "Welcome, elector of %d!\n", k);
                                    if (bulk_write(fd, msg, strlen(msg)) < 0)
                                        ERR("bulk_write");
                                }
                            }
                        }
                        else
                        {
                            // received a vote
                            if (buf >= '1' && buf <= '3')
                            {
                                int vote = (int)(buf - '0');
                                votes[who] = vote;
                            }
                        }
                    }
                    else if (n_read == 0)
                    {
                        close_client(epoll_fd, fd);
                        reset(electors, MAX_EVENTS, fd);
                    }
                    else if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        ERR("bulk_read");
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
        ERR("usage: [port]");
    int port = atoi(argv[1]);
    int listen_fd = bind_tcp_socket(port, SOMAXCONN);
    if (listen_fd < 0)
        ERR("bind_tcp_socket");
    set_nonblock(listen_fd);
    server_work(listen_fd);
    if (close(listen_fd) != 0)
        ERR("close");
    printf("server terminated\n");

    return EXIT_SUCCESS;
}

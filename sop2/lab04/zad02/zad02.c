#include "../sop_net.h"

#define MAX_EVENTS 7

void server_work(int listen_socket)
{
    int epoll_descriptor;
    if ((epoll_descriptor = epoll_create1(0)) < 0)
    {
        ERR("epoll_create1");
    }
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = listen_socket;
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, listen_socket, &event) == -1)
    {
        ERR("epoll_ctl");
    }

    int nfds;
    int32_t data;
    ssize_t size;
}

int main(int argc, char** argv)
{
    if (argc < 2)
        ERR("usage");
    int port = atoi(argv[1]);
    int listen_socket = bind_tcp_socket(port, SOMAXCONN);
    if (listen_socket != 0)
        ERR("bind_tcp_socket");

    if (close(listen_socket) != 0)
        ERR("close");
    printf("server terminated\n");

    return EXIT_SUCCESS;
}

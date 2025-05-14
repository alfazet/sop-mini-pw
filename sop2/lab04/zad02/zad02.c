#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "../sop_net.h"

#define N_CANDIDATES 3
#define MAX_EVENTS 7
#define BUF_SIZE 1024

typedef struct thread_args_t
{
    pthread_t tid;
    pthread_mutex_t* mtx_votes;
    int votes[MAX_EVENTS];
    char* udp_port;
    pthread_mutex_t* mtx_do_work;
    int* do_work;
} thread_args_t;

volatile sig_atomic_t do_work_glob = 1;

void sigint_handler(int sig)
{
    (void)sig;
    do_work_glob = 0;
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

void run_tcp(int listen_fd, thread_args_t* t_args)
{
    int* votes = t_args->votes;
    pthread_mutex_t* mtx_votes = t_args->mtx_votes;
    int* do_work = t_args->do_work;
    pthread_mutex_t* mtx_do_work = t_args->mtx_do_work;

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    int epoll_fd;
    if ((epoll_fd = epoll_create1(0)) < 0)
        ERR("epoll_create1");
    struct epoll_event event, events[MAX_EVENTS];
    event.data.fd = listen_fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) < 0)
        ERR("epoll_ctl");

    int electors[MAX_EVENTS];
    for (int i = 0; i < MAX_EVENTS; i++)
        electors[i] = -1;

    for (;;)
    {
        if (do_work_glob == 0)
        {
            pthread_mutex_lock(mtx_do_work);
            *do_work = 0;
            pthread_mutex_unlock(mtx_do_work);
            break;
        }
        int nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &oldmask);
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
                                    sprintf(msg, "\nWelcome, elector of %d!\n", k);
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
                                pthread_mutex_lock(mtx_votes);
                                votes[who] = vote;
                                pthread_mutex_unlock(mtx_votes);
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
    int score[N_CANDIDATES];
    memset(score, 0, sizeof(score));
    for (int i = 0; i < MAX_EVENTS; i++)
    {
        if (votes[i] > 0)
            score[votes[i] - 1]++;
    }
    for (int i = 0; i < N_CANDIDATES; i++)
        printf("Candidate %d received %d votes\n", i + 1, score[i]);
    if (close(epoll_fd) < 0)
        ERR("close");
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void* run_udp(void* args)
{
    thread_args_t* t_args = args;
    int* votes = t_args->votes;
    pthread_mutex_t* mtx_votes = t_args->mtx_votes;
    char* udp_port = t_args->udp_port;
    int* do_work = t_args->do_work;
    pthread_mutex_t* mtx_do_work = t_args->mtx_do_work;

    int write_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (write_fd < 0)
        ERR("socket");
    struct sockaddr_in addr = make_address("127.0.0.1", udp_port);
    int score[N_CANDIDATES];
    for (;;)
    {
        pthread_mutex_lock(mtx_do_work);
        if (*do_work == 0)
        {
            pthread_mutex_unlock(mtx_do_work);
            if (close(write_fd) < 0)
                ERR("close");
            return NULL;
        }
        pthread_mutex_unlock(mtx_do_work);
        msleep(1000);
        memset(score, 0, sizeof(score));
        pthread_mutex_lock(mtx_votes);
        for (int i = 0; i < MAX_EVENTS; i++)
        {
            if (votes[i] > 0)
                score[votes[i] - 1]++;
        }
        pthread_mutex_unlock(mtx_votes);
        int winner = -1, max_score = 0;
        for (int i = 0; i < N_CANDIDATES; i++)
        {
            if (score[i] > max_score)
            {
                max_score = score[i];
                winner = i + 1;
            }
        }
        char buf[BUF_SIZE];
        if (winner > 0)
            sprintf(buf, "The winner is %d\n", winner);
        else
            sprintf(buf, "No winner for now\n");
        if (sendto(write_fd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            ERR("sendto");
    }

    if (close(write_fd) < 0)
        ERR("close");
    return NULL;
}

int main(int argc, char** argv)
{
    if (argc < 3)
        ERR("usage: [tcp port] [udp port]");
    int tcp_port = atoi(argv[1]);
    char* udp_port = argv[2];
    if (sethandler(sigint_handler, SIGINT))
        ERR("sethandler");

    thread_args_t* thread_args = malloc(sizeof(thread_args_t));
    if (thread_args == NULL)
        ERR("malloc");
    memset(thread_args->votes, 0, MAX_EVENTS * sizeof(int));
    pthread_mutex_t mtx_votes = PTHREAD_MUTEX_INITIALIZER;
    thread_args->mtx_votes = &mtx_votes;
    int do_work = 1;
    thread_args->do_work = &do_work;
    pthread_mutex_t mtx_do_work = PTHREAD_MUTEX_INITIALIZER;
    thread_args->mtx_do_work = &mtx_do_work;
    thread_args->udp_port = udp_port;
    if (pthread_create(&(thread_args->tid), NULL, run_udp, thread_args) != 0)
        ERR("pthread_create");

    int listen_fd = bind_socket(tcp_port, SOCK_STREAM, SOMAXCONN);
    if (listen_fd < 0)
        ERR("bind_socket");
    set_nonblock(listen_fd);
    run_tcp(listen_fd, thread_args);
    pthread_mutex_destroy(&mtx_votes);
    pthread_mutex_destroy(&mtx_do_work);
    if (close(listen_fd) != 0)
        ERR("close");
    printf("server terminated\n");

    return EXIT_SUCCESS;
}

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define FIFO_PATH "zad05.fifo"

volatile sig_atomic_t last_signal;

int set_handler(void (*f)(int), int signo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (sigaction(signo, &act, NULL) == -1)
        return -1;
    return 0;
}

void signal_handler(int signo) { last_signal = signo; }

int check_win(int m, int* hand)
{
    int color = hand[0] % 4;
    for (int i = 1; i < m; i++)
    {
        if (hand[i] % 4 != color)
            return 0;
    }
    return 1;
}

void encode_number(int x, unsigned char* buf)
{
    buf[0] = (unsigned char)(x & 0xff);
    buf[1] = (unsigned char)((x >> 8) & 0xff);
    buf[2] = (unsigned char)((x >> 16) & 0xff);
    buf[3] = (unsigned char)((x >> 24) & 0xff);
}

int decode_number(unsigned char* buf)
{
    int x = 0;
    x += (int)buf[0];
    x += (((int)buf[1]) << 8);
    x += (((int)buf[2]) << 16);
    x += (((int)buf[3]) << 24);
    return x;
}

// we receive cards from the left and send them to the right
void child_work(int m, int fd_from_parent, int fd_from_left, int fd_to_right)
{
    int pid = getpid();
    int hand[m], i = 0;
    unsigned char c;
    while (i < m)
    {
        if (TEMP_FAILURE_RETRY(read(fd_from_parent, &c, 1)) < 1)
            ERR("read");
        hand[i++] = (int)c;
    }
    if (TEMP_FAILURE_RETRY(close(fd_from_parent)) < 0)
        ERR("close");

    srand(pid);
    for (;;)
    {
        i = rand() % m;
        c = (unsigned char)hand[i];
        if (write(fd_to_right, &c, 1) < 1)
        {
            if (errno != EINTR && errno != EPIPE)
                ERR("write");
            if (last_signal == SIGUSR1) {
                printf("[%d]: Leaving the game!\n", getpid());
                return;
            }
            printf("[%d]: Leaving the game!\n", getpid());
            return;
        }
        if (last_signal == SIGUSR1 || last_signal == SIGINT)
        {
            printf("[%d]: Leaving the game!\n", getpid());
            return;
        }
        // printf("sent %d to the right\n", hand[i]);
        hand[i] = 0;
        int len = 0;
        if ((len = read(fd_from_left, &c, 1)) < 1)
        {
            if (len != 0 && errno != EINTR)
                ERR("read");
            if (last_signal == SIGUSR1) {
                printf("[%d]: Leaving the game!\n", getpid());
                return;
            }
            printf("[%d]: Leaving the game!\n", getpid());
            return;
        }
        hand[i] = (int)c;
        // printf("got %d from the left\n", hand[i]);
        if (check_win(m, hand))
        {
            printf("[%d]: My ship sails!\n", getpid());
            int fd_fifo;
            if ((fd_fifo = TEMP_FAILURE_RETRY(open(FIFO_PATH, O_WRONLY))) < 0)
                ERR("open");
            unsigned char buf[4];
            encode_number(pid, buf);
            if (TEMP_FAILURE_RETRY(write(fd_fifo, &buf, 4)) < 4)
                ERR("write");
            printf("[%d]: Leaving the game!\n", getpid());
            return;
        }
    }
}

void parent_work(int n, int m, int* write_fds)
{
    if (set_handler(SIG_IGN, SIGUSR1) < 0)
        ERR("set_handler");
    srand(time(NULL));
    const int DECK_SZ = 52;
    int deck[DECK_SZ];
    for (int i = 0; i < DECK_SZ; i++)
        deck[i] = i;
    for (int i = 0; i < DECK_SZ; i++)
    {
        int j = rand() % DECK_SZ, k = rand() % DECK_SZ;
        int tmp = deck[j];
        deck[j] = deck[k];
        deck[k] = tmp;
    }
    for (int i = 0; i < n; i++)
    {
        int fd = write_fds[i];
        for (int j = 0; j < m; j++)
        {
            unsigned char c = deck[i * m + j];
            if (TEMP_FAILURE_RETRY(write(fd, &c, 1)) < 1)
                ERR("write");
        }
    }
    for (int i = 0; i < n; i++)
    {
        if (TEMP_FAILURE_RETRY(close(write_fds[i])) < 0)
            ERR("close");
    }
    int fd_fifo;
    if ((fd_fifo = TEMP_FAILURE_RETRY(open(FIFO_PATH, O_RDONLY | O_NONBLOCK))) < 0)
        ERR("open");

    unsigned char buf[4];
    int end_the_game = 0;
    for (;;) {
        int len = TEMP_FAILURE_RETRY(read(fd_fifo, &buf, 4));
        if (len == -1 && errno != EAGAIN)
            ERR("read");
        if (len == 4) {
            int winner_pid = decode_number(buf);
            printf("Server: [%d] won!\n", winner_pid);
            end_the_game = 1;
        }
        if (last_signal == SIGINT) {
            end_the_game = 1;
        }
        if (end_the_game) {
            kill(0, SIGUSR1);
            break;
        }
    }
    if (TEMP_FAILURE_RETRY(close(fd_fifo)) < 0)
        ERR("close");
}

void make_children(int n, int m, int* write_fds)
{
    int fds[2];
    int child_fds_read[n], child_fds_write[n];
    for (int i = 0; i < n; i++)
    {
        if (pipe(fds) != 0)
            ERR("pipe");
        child_fds_write[i] = fds[1];
        child_fds_read[(i + 1) % n] = fds[0];
    }
    if (mkfifo(FIFO_PATH, S_IRUSR | S_IWUSR) != 0)
    {
        if (errno != EEXIST)
            ERR("mkfifo");
    }
    for (int i = 0; i < n; i++)
    {
        if (pipe(fds) != 0)
            ERR("pipe");
        int pid;
        if ((pid = fork()) < 0)
            ERR("fork");
        if (pid == 0)
        {
            if (TEMP_FAILURE_RETRY(close(fds[1])) < 0)
                ERR("close");
            if (set_handler(signal_handler, SIGUSR1) < 0)
                ERR("set_handler");
            child_work(m, fds[0], child_fds_read[i], child_fds_write[i]);
            if (TEMP_FAILURE_RETRY(close(child_fds_read[i])) < 0)
                ERR("close");
            if (TEMP_FAILURE_RETRY(close(child_fds_write[i])) < 0)
                ERR("close");
            free(write_fds);
            exit(EXIT_SUCCESS);
        }
        if (TEMP_FAILURE_RETRY(close(fds[0])) < 0)
            ERR("close");
        write_fds[i] = fds[1];
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
        return EXIT_FAILURE;
    int n = atoi(argv[1]);
    if (n < 4 || n > 7)
        return EXIT_FAILURE;
    int m = atoi(argv[2]);
    if (m < 4 || m * n > 52)
        return EXIT_FAILURE;

    int* write_fds = malloc(n * sizeof(int));
    if (write_fds == NULL)
        ERR("malloc");
    if (set_handler(SIG_IGN, SIGPIPE) < 0)
        ERR("set_handler");
    if (set_handler(signal_handler, SIGINT) < 0)
        ERR("set_handler");
    make_children(n, m, write_fds);
    parent_work(n, m, write_fds);
    while (wait(NULL) > 0)
        ;
    free(write_fds);
    unlink(FIFO_PATH);
    return EXIT_SUCCESS;
}

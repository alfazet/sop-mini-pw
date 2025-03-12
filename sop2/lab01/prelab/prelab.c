#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define BUF_SZ 1024
#define PLAYER_CNT 5

typedef struct Player
{
    char name[16];
    int hp;
    int dmg;
} Player;

// read until \n or EOF
ssize_t bulk_read_line(int fd, char* buf)
{
    ssize_t c;
    ssize_t len = 0;
    for (;;)
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, 1));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  // EOF
        if (*buf == '\n')
            break;
        buf += c;
        len += c;
    }
    return len;
}

void child_work(int n, int group, int id, int* read_fds, int* write_fds)
{
    int pid = getpid();
    srand(pid);
    printf("[%d] Team %d, player %d joined\n", pid, group, id);
    for (int i = 0; i < n; i++)
    {
        char c = 'a' + rand() % 26;
        if (TEMP_FAILURE_RETRY(write(write_fds[i], &pid, sizeof(int))) < 0)
            ERR("write");
        if (TEMP_FAILURE_RETRY(write(write_fds[i], &c, 1)) < 0)
            ERR("write");
        printf("[%d] sent %c\n", pid, c);
        int other_pid;
        char d;
        if (TEMP_FAILURE_RETRY(read(read_fds[i], &other_pid, sizeof(int))) < 0)
            ERR("write");
        if (TEMP_FAILURE_RETRY(read(read_fds[i], &d, 1)) < 0)
            ERR("write");
        printf("[%d] received %c from [%d]\n", pid, d, other_pid);
    }
}

void create_children(int n, Player* players[2])
{
    int fds[2];
    int read_fds[2][n][n], write_fds[2][n][n];
    // read_fds[i][j][*] = all n fds that the j-th player of the i-th group
    // can write to
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < n; j++)
        {
            for (int k = 0; k < n; k++)
            {
                if (pipe(fds) != 0)
                    ERR("pipe");
                read_fds[i][j][k] = fds[0];
                write_fds[i ^ 1][k][j] = fds[1];
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < n; j++)
        {
            int pid;
            if ((pid = fork()) < 0)
                ERR("fork");
            if (pid == 0)
            {
                child_work(n, i, j, read_fds[i][j], write_fds[i][j]);
                for (int i = 0; i < 2; i++)
                {
                    free(players[i]);
                }
                exit(EXIT_SUCCESS);
            }
        }
    }
}

// "treść": dwie drużyny mające zawodników, wczytujemy początkowe dane
// z plików tekstowych, potem drużyny się losowo biją i wygrywa pierwsza
// drużyna, która zabije wszystkich przeciwników
int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("usage\n");
        return EXIT_FAILURE;
    }

    char* buf = malloc(BUF_SZ * sizeof(char));
    if (buf == NULL)
        ERR("malloc");
    memset(buf, 0, BUF_SZ);
    Player* players[2];
    int n;
    // THIS ASSUMES THAT BOTH TEAMS HAVE THE SAME NUMBER OF PLAYERS
    for (int i = 0; i < 2; i++)
    {
        players[i] = malloc(PLAYER_CNT * sizeof(Player));
        int fd = TEMP_FAILURE_RETRY(open(argv[i + 1], O_RDONLY));
        if (fd < 0)
            ERR("open");

        n = 0;
        while (bulk_read_line(fd, buf) > 0)
        {
            char* name = strtok(buf, ",");
            int hp = atoi(strtok(NULL, ","));
            int dmg = atoi(strtok(NULL, ","));
            strcpy(players[i][n].name, name);
            players[i][n].hp = hp;
            players[i][n].dmg = dmg;
            memset(buf, 0, BUF_SZ);
            n++;
        }
    }
    free(buf);
    create_children(n, players);
    // parent_work(PLAYER_CNT, players);

    for (int i = 0; i < 2; i++)
    {
        free(players[i]);
    }
}

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void read_from_fifo(int fifo, char* buf, int sz)
{
    ssize_t count;
    char c;
    int i = 0;
    do
    {
        if ((count = read(fifo, &c, 1)) < 0)
            ERR("read");
        if (count > 0 && isalnum(c))
            buf[i++] = c;
    } while (i < sz && count > 0);
}

// usage: ./pipe_test & sleep 1; cat <file> > "sop.fifo"
int main(int argc, char** argv)
{
   int fifo_fd;
   char* path = "sop.fifo";

   if (mkfifo(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0)
   {
       if (errno != EEXIST)
           ERR("mk_fifo");
   }
   if ((fifo_fd = open(path, O_RDONLY)) < 0)
       ERR("open");

   const int N = 64;
   char buf[N + 1];
   read_from_fifo(fifo_fd, buf, N);
   if (close(fifo_fd) < 0)
       ERR("close");
   buf[N] = '\0';
   printf("%s\n", buf);
   return EXIT_SUCCESS;
}

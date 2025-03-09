#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   exit(EXIT_FAILURE))

#define MAX_N 256

void usage(char *pname) {
  fprintf(stderr, "%s: <FILE>\n", pname);
  exit(EXIT_FAILURE);
}

void append_line(int fd, char *line) {
  off_t file_sz;
  if ((file_sz = lseek(fd, 0, SEEK_END)) < 0) {
    ERR("lseek");
  }
  if (lseek(fd, 0, SEEK_SET)) {
    ERR("lseek");
  }
  int n = strlen(line);
  char new_content[file_sz + n + 1];
  new_content[file_sz + n] = '\0';
  if (read(fd, new_content, file_sz) < file_sz) {
    ERR("read");
  }
  strcpy(&new_content[file_sz], line);
  if (lseek(fd, 0, SEEK_SET)) {
    ERR("lseek");
  }
  int new_n = strlen(new_content);
  int retval = write(fd, new_content, new_n);
  if (retval < new_n) {
    ERR("write");
  }
  printf("line appended\n");
}

void delete_line(int fd, int line_no) {
  off_t file_sz;
  if ((file_sz = lseek(fd, 0, SEEK_END)) < 0) {
    ERR("lseek");
  }
  if (lseek(fd, 0, SEEK_SET)) {
    ERR("lseek");
  }
  if (file_sz == 0) {
    printf("empty file\n");
    return;
  }
  char new_content[file_sz + 1];
  int i = 0;
  char c;
  int retval, lines_to_go = line_no - 1;
  while (lines_to_go > 0 && (retval = read(fd, &c, 1)) > 0) {
    if (retval < 0) {
      ERR("read");
    }
    new_content[i++] = c;
    if (c == '\n') {
      lines_to_go--;
    }
  }
  if (lines_to_go > 0) {
    printf("line %d not in file\n", line_no);
    return;
  }
  while ((retval = read(fd, &c, 1)) > 0 && c != '\n') {
    if (retval < 0) {
      ERR("read");
    }
  }
  while ((retval = read(fd, &c, 1)) > 0) {
    if (retval < 0) {
      ERR("read");
    }
    new_content[i++] = c;
  }
  new_content[i++] = '\0';
  if (lseek(fd, 0, SEEK_SET)) {
    ERR("lseek");
  }
  int new_n = strlen(new_content);
  retval = write(fd, new_content, new_n);
  if (retval < new_n) {
    ERR("write");
  }
  if (ftruncate(fd, new_n)) {
    ERR("ftruncate");
  }
  printf("line deleted\n");
}

void read_limits(int fd, int l, int r) {
  if (l < 1 || (r < 1 && r != -1) || (r < l && r != -1)) {
    printf("invalid input\n");
    return;
  }
  off_t file_sz;
  if ((file_sz = lseek(fd, 0, SEEK_END)) < 0) {
    ERR("lseek");
  }
  if (lseek(fd, 0, SEEK_SET)) {
    ERR("lseek");
  }
  if (file_sz == 0) {
    printf("empty file\n");
    return;
  }
  char content[file_sz + 1];
  char c;
  int retval, lines_to_go = l - 1;
  while (lines_to_go > 0 && (retval = read(fd, &c, 1)) > 0) {
    if (retval < 0) {
      ERR("read");
    }
    if (c == '\n') {
      lines_to_go--;
    }
  }
  if (lines_to_go > 0) {
    printf("line %d not in file\n", l);
    return;
  }
  int i = 0;
  lines_to_go = (r == -1 ? (1 << 30) : r - l + 1);
  while (lines_to_go > 0 && (retval = read(fd, &c, 1)) > 0) {
    if (retval < 0) {
      ERR("read");
    }
    content[i++] = c;
    if (c == '\n') {
      lines_to_go--;
    }
  }
  content[i++] = '\0';
  printf("%s\n", content);
}

/*
   Creates <FILE> if it doesn't yet exist, otherwise does nothing
Commands:
- 'a', then input to append a line to the end
- 'd', then n to delete the nth line
- '%' to print everything
- 'l:r' to print everything between lth and rth line
- 'q' to quit
*/
int main(int argc, char *argv[]) {
  if (argc != 2) {
    usage(argv[0]);
  }
  char *filename = argv[1];
  int fd = open(filename, O_CREAT | O_RDWR, 0644);
  if (fd < 0) {
    ERR("open");
  }
  size_t n_read;
  char *line = NULL;
  for (;;) {
    printf("Enter command:\n");
    // getline reallocs *line accordingly, returns -1 on err or EOF,
    // saves the '\n' to *line and terminates it with '\0'
    int retval = getline(&line, &n_read, stdin);
    if (retval < 0) {
      ERR("getline");
    }
    if (retval == 2) {
      if (line[0] == 'q') {
        break;
      }
      if (line[0] == '%') {
        read_limits(fd, 1, -1);
      } else if (line[0] == 'a') {
        printf("Enter the new line:\n");
        char *new_line = NULL;
        if (getline(&new_line, &n_read, stdin) < 0) {
          ERR("getline");
        }
        append_line(fd, new_line);
        free(new_line);
      } else if (line[0] == 'd') {
        printf("Enter line number to delete:\n");
        int line_no;
        int retval = scanf("%d", &line_no);
        // hack to remove the trailing garbage left by scanf
        {
          char c;
          while ((c = fgetc(stdin)) != '\n' && c != EOF)
            ;
        }
        if (retval != 1) {
          printf("error: not a number\n");
          continue;
        }
        delete_line(fd, line_no);
      }
    } else {
      ;
    }
  }
  free(line); // free because getline allocs

  return EXIT_SUCCESS;
}

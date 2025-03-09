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
#include <unistd.h>

#define MAX_N 256

#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   exit(EXIT_FAILURE))

ssize_t bulk_read(int fd, char *buf, size_t count) {
  ssize_t c;
  ssize_t len = 0;
  do {
    c = TEMP_FAILURE_RETRY(read(fd, buf, count));
    if (c < 0)
      return c;
    if (c == 0)
      return len; // EOF
    buf += c;
    len += c;
    count -= c;
  } while (count > 0);
  return len;
}

ssize_t bulk_write(int fd, char *buf, size_t count) {
  ssize_t c;
  ssize_t len = 0;
  do {
    c = TEMP_FAILURE_RETRY(write(fd, buf, count));
    if (c < 0)
      return c;
    buf += c;
    len += c;
    count -= c;
  } while (count > 0);
  return len;
}

void show_stage2(const char *const path, const struct stat *const stat_buf) {
  if (S_ISREG(stat_buf->st_mode)) {
    long sz = stat_buf->st_size;
    printf("file size %ld\n", sz);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
      printf("couldn't open file\n");
      return;
    }
    char buf[sz];
    int read_cnt = read(fd, &buf, sz);
    if (read_cnt < sz) {
      printf("couldn't read the entire file\n");
      close(fd);
      return;
    }
    printf("%s\n", buf);
    close(fd);
  } else if (S_ISDIR(stat_buf->st_mode)) {
    struct dirent *dir;
    DIR *d = opendir(path);
    if (!d) {
      printf("couldn't open dir\n");
      return;
    }
    printf("this dir:\n");
    while ((dir = readdir(d)) != NULL) {
      printf("%s\n", dir->d_name);
    }
    closedir(d);
  } else {
    printf("unknown file type\n");
  }
}

void write_stage3(const char *const path, const struct stat *const stat_buf) {
  if (!S_ISREG(stat_buf->st_mode)) {
    return;
  }
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf("couldn't open file\n");
    return;
  }
  long sz = stat_buf->st_size;
  char out_buf[sz];
  bulk_read(fd, out_buf, sz);
  printf("%s\n", out_buf);
  close(fd);

  fd = open(path, O_WRONLY | O_APPEND);
  if (fd < 0) {
    printf("couldn't open file\n");
    return;
  }
  char in_buf[MAX_N];
  while (NULL != fgets(in_buf, MAX_N, stdin)) {
    if (in_buf[0] == '\n') {
      close(fd);
      return;
    }
    int in_len = strlen(in_buf);
    in_buf[in_len - 1] = '\n';
    bulk_write(fd, in_buf, in_len);
  }
  close(fd);
}

int print_file_info(const char *name, const struct stat *stat_buf, int x) {
  char *type = "unknown";
  if (S_ISDIR(stat_buf->st_mode)) {
    type = "dir";
  } else if (S_ISREG(stat_buf->st_mode)) {
    type = "file";
  }
  printf("%s, %s\n", name, type);
  return 0;
}

void walk_stage4(char *path, struct stat *stat_buf) {
  ftw(path, *print_file_info, MAX_N);
}

int interface_stage1() {
  printf("1. show\n2. write\n3. walk\n4. exit\n");
  char c = fgetc(stdin);
  fgetc(stdin);
  if (c < '1' || c > '4') {
    printf("bad command\n");
    return 1;
  }
  if (c == '4') {
    return 0;
  }
  char *path = NULL;
  size_t path_sz = 0;
  int read_len = getline(&path, &path_sz, stdin);
  if (read_len == -1) {
    return 1;
  }
  path[read_len - 1] = '\0';
  struct stat stat_buf;
  if (stat(path, &stat_buf) == 0) {
    if (c == '1') {
      show_stage2(path, &stat_buf);
    } else if (c == '2') {
      write_stage3(path, &stat_buf);
    } else if (c == '3') {
      walk_stage4(path, &stat_buf);
    }
    free(path);
    return 1;
  } else {
    printf("file not available\n");
    free(path);
    return 1;
  }
  free(path);

  return 1;
}

int main() {
  while (interface_stage1())
    ;
  return EXIT_SUCCESS;
}

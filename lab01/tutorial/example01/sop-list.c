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
  fprintf(stderr, "%s: usage:\n", pname);
  fprintf(stderr, "dir(s) to list: -p <DIR_NAME>\n");
  fprintf(stderr, "output file (optional): -o <FILE_NAME>\n");
  exit(EXIT_FAILURE);
}

void list(char *dirs[], int n_dirs, char *outfile) {
  int fd = STDOUT_FILENO;
  int file_opened = 0;
  if (outfile != NULL) {
    fd = open(outfile, O_WRONLY | O_CREAT, 0644);
    file_opened = 1;
    if (fd < 0) {
      ERR("open");
    }
  }
  for (int i = 0; i < n_dirs; i++) {
    char *dir = dirs[i];
    struct stat stat_buf;
    if (stat(dir, &stat_buf)) {
      ERR("dir doesn't exist or bad perms");
      continue;
    }
    if (S_ISDIR(stat_buf.st_mode) == 0) {
      ERR("specified path isn't a dir");
      continue;
    }
    struct dirent *dirent;
    DIR *d = opendir(dir);
    if (d == NULL) {
      ERR("opendir");
    }

    // begin cwd change
    if (chdir(dir)) {
      ERR("chdir");
    }
    dprintf(fd, "Files in dir %s:\n", dir);
    while ((dirent = readdir(d)) != NULL) {
      if (stat(dirent->d_name, &stat_buf)) {
        ERR("stat");
      }
      dprintf(fd, "%s %ld\n", dirent->d_name, stat_buf.st_size);
    }
    if (closedir(d)) {
      ERR("closedir");
    }
    // end cwd change

    if (chdir("..")) {
      ERR("chdir");
    }
  }
  if (file_opened && close(fd)) {
    ERR("close");
  }
}

int main(int argc, char *argv[]) {
  int c, n_dirs = 0;
  char *dirs[MAX_N];
  dirs[0] = NULL;
  char *outfile = NULL;
  while ((c = getopt(argc, argv, "p:o:")) != -1) {
    switch (c) {
    case 'p':
      dirs[n_dirs++] = optarg;
      break;
    case 'o':
      outfile = optarg;
      break;
    case '?':
    default:
      usage(argv[0]);
    }
  }
  if (dirs[0] == NULL) {
    dirs[0] = ".";
    n_dirs = 1;
  }
  list(dirs, n_dirs, outfile);

  return EXIT_SUCCESS;
}

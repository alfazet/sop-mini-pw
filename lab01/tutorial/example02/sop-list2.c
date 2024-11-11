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
  fprintf(stderr, "extension to filter: -e <EXT>\n");
  fprintf(stderr, "max recursion depth: -d <MAX_DEPTH>\n");
  fprintf(stderr, "output file (optional): -o\n");
  exit(EXIT_FAILURE);
}

int matches_ext(char *name, char *ext) {
  int n = strlen(name), m = strlen(ext);
  int min = (n < m ? n : m);
  for (int i = 1; i <= min; i++) {
    if (name[n - i] != ext[m - i]) {
      return 0;
    }
  }
  return 1;
}

void rec(char *call_path, char *dir, int fd, char *ext, int cur_depth,
         int max_depth) {
  if (cur_depth > max_depth) {
    return;
  }
  char cur_path[MAX_N];
  sprintf(cur_path, "%s%s/", call_path, dir);
  struct dirent *dirent;
  DIR *d = opendir(cur_path);
  if (d == NULL) {
    ERR("opendir");
  }

  char *to_call[MAX_N];
  int n_calls = 0;
  dprintf(fd, ".%s files in dir %s:\n", ext, cur_path);
  while ((dirent = readdir(d)) != NULL) {
    char *entname = dirent->d_name;
    if (dirent->d_type == DT_DIR) {
      if (strcmp(entname, ".") && strcmp(entname, "..")) {
        to_call[n_calls++] = entname;
      }
    } else if (matches_ext(entname, ext)) {
      char tmp[MAX_N];
      strcpy(tmp, cur_path);
      strcat(tmp, entname);
      struct stat stat_buf;
      if (stat(tmp, &stat_buf)) {
        ERR("stat");
      }
      dprintf(fd, "%s %ld\n", entname, stat_buf.st_size);
    }
  }
  for (int i = 0; i < n_calls; i++) {
    rec(cur_path, to_call[i], fd, ext, cur_depth + 1, max_depth);
  }
  if (closedir(d)) {
    ERR("closedir");
  }
}

void list(char *dirs[], int n_dirs, char *outfile, char *ext, int depth) {
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
    int n = strlen(dir);
    if (dir[n - 1] == '/') {
      dir[n - 1] = '\0';
    }
    struct stat stat_buf;
    if (stat(dir, &stat_buf)) {
      ERR("dir doesn't exist or bad perms");
      continue;
    }
    if (S_ISDIR(stat_buf.st_mode) == 0) {
      ERR("specified path isn't a dir");
      continue;
    }
    rec("", dir, fd, ext, 1, depth);
  }
  if (file_opened && close(fd)) {
    ERR("close");
  }
}

int main(int argc, char *argv[]) {
  int c, n_dirs = 0, depth = 0;
  char *dirs[MAX_N];
  dirs[0] = NULL;
  char *outfile = NULL, *ext = NULL;
  while ((c = getopt(argc, argv, "p:e:d:o")) != -1) {
    switch (c) {
    case 'p':
      dirs[n_dirs++] = optarg;
      break;
    case 'o':
      outfile = getenv("L1_OUTPUTFILE");
      break;
    case 'e':
      ext = optarg;
      break;
    case 'd':
      depth = atoi(optarg);
      break;
    case '?':
    default:
      usage(argv[0]);
    }
  }
  if (depth == 0 || ext == NULL) {
    usage(argv[0]);
  }
  if (dirs[0] == NULL) {
    dirs[0] = ".";
    n_dirs = 1;
  }
  list(dirs, n_dirs, outfile, ext, depth);

  return EXIT_SUCCESS;
}

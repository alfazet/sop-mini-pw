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

#define MAX_FD 20

int walk(const char *name, const struct stat *filestat, int type,
         struct FTW *f) {
  printf("%s, level %d -- ", name, f->level);
  if (type == FTW_F) {
    printf("regular\n");
  } else if (type == FTW_D) {
    printf("directory\n");
  } else {
    printf("other\n");
  }
  return 0;
}

int main(int argc, char *argv[]) {
  char *path = ".";
  nftw(path, walk, MAX_FD, FTW_PHYS);

  return EXIT_SUCCESS;
}

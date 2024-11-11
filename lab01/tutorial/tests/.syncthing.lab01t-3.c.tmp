#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   exit(EXIT_FAILURE))

void usage(char *pname) {
  fprintf(stderr, "USAGE:%s -n Name -p OCTAL -s SIZE\n", pname);
  exit(EXIT_FAILURE);
}

void make_file(char *name, mode_t perms, ssize_t size) {
  // truncate perms to 3 bytes and invert because it's umask
  umask(0777 & ~perms);
  FILE *fptr = fopen(name, "w+");
  if (fptr == NULL) {
    ERR("fopen");
  }
  for (int i = 0; i < size / 10; i++) {
    if (fseek(fptr, rand() % size, SEEK_SET)) {
      ERR("fseek");
    }
    fprintf(fptr, "%c", 'A' + (i % ('Z' - 'A' + 1)));
  }
  if (fclose(fptr)) {
    ERR("fclose");
  }
}

int main(int argc, char *argv[]) {
  srand(time(NULL));
  int c;
  ssize_t size = -1;
  mode_t perms = -1;
  char *name = NULL;
  while ((c = getopt(argc, argv, "n:p:s:")) != -1) {
    switch (c) {
    case 'n':
      name = optarg;
      break;
    case 'p':
      perms = strtol(optarg, NULL, 8);
      break;
    case 's':
      size = strtol(optarg, NULL, 10);
      break;
    case '?':
    default:
      usage(argv[0]);
    }
  }
  if (name == NULL || size == -1 || perms == -1) {
    usage(argv[0]);
  }
  // we unlink (even though w+ clears the file) because we need our new permissions to be applied 
  if (unlink(name) && errno != ENOENT) {
    ERR("unlink");
  }
  make_file(name, perms, size);

  return EXIT_SUCCESS;
}

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

int main(int argc, char* argv[]) {
  fprintf(stderr, "this is printed to stderr\n");
  for (int i = 0; i < 3; i++) {
    fprintf(stderr, "%d ", i);
    sleep(1);
  }
  fprintf(stderr, "\n");
  // these numbers will be printed one after another with 1 second inbetween
  // because stderr isn't buffered

  printf("this is printed to stdout\n"); // equiv to fprintf(stdout, "...")
  for (int i = 0; i < 3; i++) {
    printf("%d ", i);
    sleep(1);
  }
  printf("\n");
  // the numbers will only be printed here
  // because stdout is flushed only on \n

  return EXIT_SUCCESS;
}

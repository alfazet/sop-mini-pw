#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   exit(EXIT_FAILURE))

void usage(char *argv[]) {
  printf("%s pattern [-n]\n", argv[0]);
  printf("pattern - string pattern to search at standard input\n");
  printf("n - (optional) displaying line numbers\n");
  exit(EXIT_FAILURE);
}

#define MAX_LINE 999;

int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 3) {
    usage(argv);
  }

  size_t line_len = MAX_LINE;
  char *line = malloc(line_len);
  int line_numbering = 0;

  int c = 0;
  while ((c = getopt(argc, argv, "n")) != -1) {
    switch (c) {
    case 'n':
      line_numbering = 1;
      break;
    case '?':
    default:
      usage(argv);
    }
  }

  char *pattern = argv[argc - 1];
  int cur_line = 1;
  while (getline(&line, &line_len, stdin) != -1) {
    if (strstr(line, pattern)) {
      if (line_numbering > 0) {
        printf("%d:%s", cur_line, line);
      } else {
        printf("%s", line);
      }
    }
    cur_line++;
  }

  if (line) {
    free(line);
  }

  return EXIT_SUCCESS;
}

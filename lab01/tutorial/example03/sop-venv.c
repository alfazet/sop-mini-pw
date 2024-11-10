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

#define UNUSED(x) (void)(x)

#define MAX_N 256

void usage(char *pname) {
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "create new env: -c -v <ENV_NAME>\n");
  fprintf(stderr, "install package: -v <ENV_NAME> -i <PKG_NAME>==<VER>\n");
  fprintf(stderr, "remove package: -v <ENV_NAME> -r <PKG_NAME>\n");
  exit(EXIT_FAILURE);
}

void create_env(char *names[], int n) {
  for (int i = 0; i < n; i++) {
    char *env_name = names[i];
    struct stat stat_buf;
    if (mkdir(env_name, 0755)) {
      if (errno == EEXIST) {
        ERR("env with this name already created");
      } else {
        ERR("mkdir");
      }
    }
    if (chdir(env_name)) {
      ERR("chdir");
    }
    int fd = open("requirements", O_CREAT, 0644);
    if (fd < 0) {
      ERR("open");
    }
    if (close(fd)) {
      ERR("close");
    }
    if (chdir("..")) {
      ERR("chdir");
    }
  }
}

// read till '\n', capped at count
int read_line(int fd, char *buf, int count) {
  int len = 0, c;
  do {
    c = read(fd, buf, 1);
    if (c < 0) {
      return c;
    }
    if (c == 0) {
      return len;
    }
    if (*buf == '\n') {
      return len;
    }
    buf += c;
    len += c;
    count -= c;
  } while (count > 0);
  return len;
}

void write_line(int fd, char *pkg_name, char *pkg_ver) {
  int c = write(fd, pkg_name, strlen(pkg_name));
  if (c < 0) {
    ERR("write");
  }
  c = write(fd, " ", 1);
  if (c < 0) {
    ERR("write");
  }
  c = write(fd, pkg_ver, strlen(pkg_ver));
  if (c < 0) {
    ERR("write");
  }
  c = write(fd, "\n", 1);
  if (c < 0) {
    ERR("write");
  }
}

void install_pkg(char *names[], char *to_install, int n) {
  for (int i = 0; i < n; i++) {
    char *env_name = names[i];
    struct stat stat_buf;
    if (stat(env_name, &stat_buf)) {
      ERR("can't install to non-existing env");
    }
    char *where = strstr(to_install, "==");
    if (where == NULL) {
      ERR("version not specified");
    }
    *where = '\0';
    char *pkg_name = to_install;
    if (strlen(pkg_name) == 0) {
      ERR("empty package name");
    }
    char *pkg_ver = where + 2; // 2 = strlen("==")
    if (strlen(pkg_ver) == 0) {
      ERR("empty package version");
    }

    if (chdir(env_name)) {
      ERR("chdir");
    }

    // check if already installed
    int fd = open("requirements", O_RDONLY);
    if (fd < 0) {
      ERR("open");
    }
    char buf[MAX_N];
    int read;
    while ((read = read_line(fd, buf, MAX_N)) > 0) {
      buf[read - 1] = '\0';
      char *where = strstr(buf, " ");
      if (where == NULL) {
        ERR("reading");
      }
      *where = '\0';
      char *pkg_in_file = buf;
      if (strcmp(pkg_in_file, pkg_name) == 0) {
        ERR("package already installed");
      }
    }
    if (read < 0) {
      ERR("read_line");
    }
    if (close(fd)) {
      ERR("close");
    }

    // install
    fd = open("requirements", O_WRONLY | O_APPEND);
    if (fd < 0) {
      ERR("open");
    }
    write_line(fd, pkg_name, pkg_ver);
    if (close(fd)) {
      ERR("close");
    }

    // random file
    fd = open(pkg_name, O_WRONLY | O_CREAT, 0444);
    if (fd < 0) {
      ERR("open");
    }
    srand(time(NULL));
    char random_chars[MAX_N];
    for (int i = 0; i < MAX_N; i++) {
      random_chars[i] = 'A' + (rand() % 26);
    }
    if (write(fd, random_chars, MAX_N) < 0) {
      ERR("write");
    }
    if (close(fd)) {
      ERR("close");
    }

    if (chdir("..")) {
      ERR("chdir");
    }
  }
}

void remove_pkg(char *names[], char *to_install, int n) {}

int main(int argc, char *argv[]) {
  int c;
  int create_flag = 0;
  char *names[MAX_N];
  names[0] = NULL;
  int i = 0;
  char *to_install = NULL;
  char *to_remove = NULL;
  while ((c = getopt(argc, argv, "cv:i:r:")) != -1) {
    switch (c) {
    case 'c':
      create_flag = 1;
      break;
    case 'v':
      names[i] = optarg;
      i++;
      break;
    case 'i':
      to_install = optarg;
      break;
    case 'r':
      to_remove = optarg;
      break;
    case '?':
    default:
      usage(argv[0]);
    }
  }
  if (names[0] == NULL) {
    usage(argv[0]);
  }
  if (create_flag == 0 && to_install == NULL && to_remove == NULL) {
    usage(argv[0]);
  }

  if (create_flag) {
    create_env(names, i);
  }
  if (to_install != NULL) {
    install_pkg(names, to_install, i);
  }
  if (to_remove != NULL) {
    remove_pkg(names, to_remove, i);
  }

  return EXIT_SUCCESS;
}

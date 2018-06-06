#include "spfs.h"
#include <stdint.h>
#include <stdio.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int
super_block(int fd) {
  return 1;
}

int
main(int argc, const char **args) {
  if (argc > 1) {
    const char *device = args[1];

    int fd = open(device, O_RDWR);
    if (fd != 0) {
      fprintf(stderr, "failed to open('%s', O_RDWR)\n", device);
      return 1;
    }

    if (!super_block(fd)) {
      fprintf(stderr, "failed to write superblock\n", device);
      close(fd);
      return 1;
    }

    return 0;
  }

  fprintf(stderr, "%s device\n", args[0]);
  return 1;
}

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h> //memset

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/stat.h> //mode_t
#include <sys/types.h>

#include "spfs.h"

static int
super_block(int fd) {
  struct stat s;
  memset(&s, 0, sizeof(s));
  int ret = fstat(fd, &s);
  if (ret != 0) {
    return ret;
  }

  struct spfs_super_block_wire super = {
      .version = 1,
      .magic = SPOOKY_FS_MAGIC,
      .block_size = SPOOKY_FS_BLOCK_SIZE,
      .id = SPFS_ROOT_INODE_NO,
  };

  super.version = htonl(super.version);
  super.magic = htonl(super.magic);
  super.block_size = htonl(super.block_size);

  write(fd, &super, sizeof(super));

  /* TODO memset(super.dummy, 0, sizeof(super.dummy)); */

  return 0;
}

int
main(int argc, const char **args) {
  if (argc > 1) {
    const char *device = args[1];

    int fd = open(device, O_RDWR);
    if (fd <= 0) {
      fprintf(stderr, "failed to open('%s', O_RDWR)\n", device);
      return 1;
    }

    if (super_block(fd) != 0) {
      fprintf(stderr, "failed to write superblock: '%s'\n", device);
      close(fd);
      return 1;
    }

    return 0;
  }

  fprintf(stderr, "%s device\n", args[0]);
  return 1;
}

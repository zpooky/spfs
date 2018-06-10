#include <arpa/inet.h>
#include <assert.h>
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
zero_fill(int fd, size_t bytes) {
  size_t i = 0;
  unsigned char z = 0;
  int ret;

  /* fprintf(stdout, "super block zero[%zu]\n", bytes); */

  for (; i < bytes; ++i) {
    ret = write(fd, &z, sizeof(z));
    if (ret != sizeof(z)) {
      return ret;
    }
  }

  return 0;
}

static int
super_block(int fd) {
  int ret;
  size_t zero_len;

  struct spfs_super_block_wire super = {
      .version = 1,
      .magic = SPOOKY_FS_MAGIC,
      .block_size = SPOOKY_FS_BLOCK_SIZE,
      .id = SPFS_ROOT_INODE_NO,
  };
  zero_len = super.block_size - sizeof(super);

  super.version = htonl(super.version);
  super.magic = htonl(super.magic);
  super.block_size = htonl(super.block_size);

  ret = write(fd, &super, sizeof(super));
  if (ret != sizeof(super)) {
    return ret;
  }

  return zero_fill(fd, zero_len);
}

static int
mkfs_write_u32(unsigned char *buffer, ssize_t *pos, unsigned int value) {
  memcpy(buffer + *pos, &value, sizeof(value));
  *pos += sizeof(value);

  return 0;
}

static int
free_list(int fd) {
  const spfs_offset start = (SPOOKY_FS_BLOCK_SIZE * 3);
  struct stat s;
  unsigned char buffer[1024];
  ssize_t b_pos = 0;
  ssize_t wres;

  memset(&s, 0, sizeof(s));
  int ret = fstat(fd, &s);
  if (ret) {
    return ret;
  }

  // TODO use s.st_blocksize instead of all SPOOKY_FS_BLOCK_SIZE
  if (s.st_size < start) {
    fprintf(stderr, "is to small [%zu]\n", s.st_size);
    return 1;
  }

  size_t length = s.st_size - start;
  fprintf(stdout, "system block[%zu], fs block[%u]\n", s.st_blksize,
          SPOOKY_FS_BLOCK_SIZE);
  fprintf(stdout, "header[%u],data[%zu]\n", start, length);

  /* entry[spfs_offset,size_t]
   * free_list[length:u32,next:spfs_offset,entry:length]
   */
  memset(buffer, 0, sizeof(buffer));
  if (mkfs_write_u32(buffer, &b_pos, /*length*/ 1)) {
    return 1;
  }
  if (mkfs_write_u32(buffer, &b_pos, /*next*/ 0)) {
    return 1;
  }
  {
    if (mkfs_write_u32(buffer, &b_pos, start)) {
      return 1;
    }
    if (mkfs_write_u32(buffer, &b_pos, length)) {
      return 1;
    }
  }

  wres = write(fd, buffer, b_pos);
  if (wres != b_pos) {
    return 1;
  }

  return 0;
}

static int
btree_root(int fd) {
  return zero_fill(fd, SPOOKY_FS_BLOCK_SIZE);
}

int
main(int argc, const char **args) {
  int res = 1;
  int fd = 0;
  if (argc > 1) {
    const char *device = args[1];

    fd = open(device, O_RDWR);
    if (!fd) {
      fprintf(stderr, "failed to open('%s', O_RDWR)\n", device);
      goto Ldone;
    }

    if (super_block(fd)) {
      fprintf(stderr, "failed to write superblock: '%s'\n", device);
      goto Ldone;
    }

    if (btree_root(fd)) {
      fprintf(stderr, "failed to write btree_root: '%s'\n", device);
      goto Ldone;
    }

    if (free_list(fd)) {
      fprintf(stderr, "failed to write free list: '%s'\n", device);
      goto Ldone;
    }

    res = 0;
    goto Ldone;
  }

  res = 0;
  fprintf(stderr, "%s device\n", args[0]);
Ldone:
  if (fd) {
    fsync(fd);
    close(fd);
  }
  return res;
}

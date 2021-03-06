#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h> //memset

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/stat.h> //mode_t
#include <sys/types.h>

#include "mkfs.spfs.h"
#include "spfs.h"

#define SPOOKY_FS_BLOCK_SIZE 4096

/* #define U64 uint64_t */
/*  */
/* #define ENDIAN_SWAP_U64(val) \ */
/*   ((U64)((((U64)(val) & (U64)0x00000000000000ff) << 56) | \ */
/*          (((U64)(val) & (U64)0x000000000000ff00) << 40) | \ */
/*          (((U64)(val) & (U64)0x0000000000ff0000) << 24) | \ */
/*          (((U64)(val) & (U64)0x00000000ff000000) << 8) | \ */
/*          (((U64)(val) & (U64)0x000000ff00000000) >> 8) | \ */
/*          (((U64)(val) & (U64)0x0000ff0000000000) >> 24) | \ */
/*          (((U64)(val) & (U64)0x00ff000000000000) >> 40) | \ */
/*          (((U64)(val) & (U64)0xff00000000000000) >> 56))) */

static size_t
mkfs_bytes_of(struct stat *s, size_t blocks) {
  return s->st_blksize * blocks;
}

static int
mkfs_block_size(int fd, struct spfs_super_block_wire *super) {
  size_t header_blocks = 2; // Super + Free-List
  struct stat s;

  memset(&s, 0, sizeof(s));
  int ret = fstat(fd, &s);
  if (ret) {
    return ret;
  }
  super->block_size = s.st_blksize;
  size_t start = mkfs_bytes_of(&s, header_blocks);

  if (s.st_size < 0) {
    return 1;
  }

  const size_t sz = s.st_size;
  if (sz < start) {
    fprintf(stderr, "is to small [%zu]\n", (size_t)s.st_size);
    return 1;
  }

  const size_t length = s.st_size - start;
  super->blocks = length / super->block_size;

  size_t hdd_block_size = s.st_blksize;
  printf("hdd block size[%zu], fs block size[%u]\n", //
         hdd_block_size, super->block_size);
  printf("header[%zu], data[%zu]\n", //
         start, length);
  printf("data blocks[%zu]\n", super->blocks);

  if (super->blocks == 0) {
    return 1;
  }

  return 0;
}
static int
zero_fill(int fd, size_t bytes) {
  size_t i = 0;
  unsigned char z = 0;
  int ret;

  fprintf(stdout, "super block zero[%zu]\n", bytes);

  for (; i < bytes; ++i) {
    ret = write(fd, &z, sizeof(z));
    if (ret != sizeof(z)) {
      return ret;
    }
  }

  return 0;
}

static int
mkfs_write_u32(unsigned char *buffer, ssize_t *pos, const uint32_t *value) {
  uint32_t in = htonl(*value);

  memcpy(buffer + *pos, &in, sizeof(in));
  *pos += sizeof(in);

  return 0;
}

/* static int
 * mkfs_write_u64(unsigned char *buffer, ssize_t *pos, uint64_t value) {
 *   value = ENDIAN_SWAP_U64(value);
 *
 *   memcpy(buffer + *pos, &value, sizeof(value));
 *   *pos += sizeof(value);
 *
 *   return 0;
 * }
 */

/*
 * static int
 * mkfs_write_ino(unsigned char *buffer, ssize_t *pos, spfs_ino value) {
 *   return mkfs_write_u32(buffer, pos, &value);
 * }
 */

/*
 * static int
 * mkfs_write_offset(unsigned char *buffer, ssize_t *pos, uint32_t value) {
 *   return mkfs_write_u32(buffer, pos, &value);
 * }
 */

static int
super_block(int fd, const struct spfs_super_block_wire *super) {
  int ret;
  size_t zero_len;
  unsigned char buffer[1024];
  ssize_t pos = 0;

  if (mkfs_write_u32(buffer, &pos, &super->magic)) {
    return 1;
  }
  if (mkfs_write_u32(buffer, &pos, &super->version)) {
    return 1;
  }
  if (mkfs_write_u32(buffer, &pos, &super->block_size)) {
    return 1;
  }

  if (mkfs_write_u32(buffer, &pos, &super->id)) {
    return 1;
  }
  if (mkfs_write_u32(buffer, &pos, &super->root_id)) {
    return 1;
  }

  if (mkfs_write_u32(buffer, &pos, &super->btree_offset)) {
    return 1;
  }
  if (mkfs_write_u32(buffer, &pos, &super->free_list_offset)) {
    return 1;
  }

  ret = write(fd, buffer, pos);
  if (ret != pos) {
    return ret;
  }

  zero_len = super->block_size - pos;
  return zero_fill(fd, zero_len);
}

static int
mkfs_write_free_list_header(unsigned char *buffer, ssize_t *pos,
                            const struct spfs_free_list *header) {
  if (mkfs_write_u32(buffer, pos, &header->magic)) {
    return 1;
  }
  if (mkfs_write_u32(buffer, pos, &header->entries)) {
    return 1;
  }
  if (mkfs_write_u32(buffer, pos, &header->next)) {
    return 1;
  }

  return 0;
}

static int
mkfs_write_free_entry(unsigned char *buffer, ssize_t *pos,
                      const struct spfs_free_entry *entry) {
  if (mkfs_write_u32(buffer, pos, &entry->start)) {
    return 1;
  }
  if (mkfs_write_u32(buffer, pos, &entry->blocks)) {
    return 1;
  }

  return 0;
}

static int
free_list(int fd, const struct spfs_super_block_wire *super) {
  unsigned char buffer[1024];
  ssize_t b_pos = 0;
  ssize_t wres;

  struct spfs_free_list header = {
      /**/
      .magic = SPOOKY_FS_FL_MAGIC,
      .entries = 1,
      .next = 0,
      /**/
  };

  struct spfs_free_entry entry = {
      /* [super:0, free_list:1, free_start:2] */
      .start = 2,
      .blocks = super->blocks,
      /**/
  };

  memset(buffer, 0, sizeof(buffer));
  if (mkfs_write_free_list_header(buffer, &b_pos, &header)) {
    return 1;
  }

  if (mkfs_write_free_entry(buffer, &b_pos, &entry)) {
    return 1;
  }

  off_t cur = lseek(fd, 0, SEEK_CUR);
  if (cur == -1) {
    return 1;
  }

  if (cur != super->block_size) {
    fprintf(stderr,
            "wrong offset when writing free-list at: [%jd] expected: [%u]\n",
            cur, super->block_size);
    return 1;
  }

  wres = write(fd, buffer, b_pos);
  if (wres != b_pos) {
    return 1;
  }

  return 0;
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

    struct spfs_super_block_wire super = {
        .version = 1,
        .magic = SPOOKY_FS_SUPER_MAGIC,
        .block_size = 0,
        .dummy = 0,
        .id = SPFS_ROOT_INODE_NO,
        .root_id = SPFS_ROOT_INODE_NO,
        .btree_offset = 0,
        .free_list_offset = 1,

        /* transient: { */
        .blocks = 0,
        /* } */
    };

    if (mkfs_block_size(fd, &super)) {
      fprintf(stderr, "failed setting up block_size: '%s'\n", device);
      goto Ldone;
    }

    if (super_block(fd, &super)) {
      fprintf(stderr, "failed to write superblock: '%s'\n", device);
      goto Ldone;
    }

    if (free_list(fd, &super)) {
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

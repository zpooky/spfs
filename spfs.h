#ifndef _SP_FS_H
#define _SP_FS_H

#include <linux/limits.h>

#define SPOOKY_FS_MAGIC 0xDEADBEEF

#define SPOOKY_FS_BLOCK_SIZE 4096

struct spfs_entry {
  char name[NAME_MAX];
  uint64_t inode_no;
};

struct spfs_file {
  spfs_entry entry;
  mode_t mode;
};

struct spfs_directory {
  spfs_entry entry;
  mode_t mode;

  spfs_entry[64];
};

struct spfs_super_block {
  uint64_t version;
  uint64_t magic;
  uint64_t block_size;

  char dummy[SPOOKY_FS_BLOCK_SIZE - (sizeof(uint64_t) * 3)];
};

#endif

#ifndef _SP_FS_H
#define _SP_FS_H

#include <linux/limits.h>
#include <sys/types.h> //mode_t

#define SPOOKY_FS_MAGIC 0xDEADBEEF

#define SPOOKY_FS_BLOCK_SIZE 4096

struct spfs_entry {
  char name[NAME_MAX];
  unsigned int inode_no;
};

struct spfs_file {
  struct spfs_entry entry;
  mode_t mode;
};

struct spfs_directory {
  struct spfs_entry entry;
  mode_t mode;

  struct spfs_entry children[64];
};

struct spfs_super_block {
  unsigned int version;
  unsigned int magic;
  unsigned int block_size;

  char dummy[SPOOKY_FS_BLOCK_SIZE - (sizeof(unsigned int) * 3)];
};

#endif

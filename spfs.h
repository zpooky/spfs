#ifndef _SP_FS_H
#define _SP_FS_H

#include <linux/limits.h>
#include <sys/types.h> //mode_t

#define SPOOKY_FS_MAGIC 0xDEADBEEF
#define SPOOKY_FS_BLOCK_SIZE 4096

#define SPFS_ROOT_INODE_NO 1

struct spfs_inode {
  unsigned long inode_no;
  mode_t mode;
  char name[NAME_MAX];
};

// struct spfs_file {
//   struct spfs_inode entry;
//   mode_t mode;
// };
//
// struct spfs_directory {
//   struct spfs_inode entry;
//   mode_t mode;
//
//   struct spfs_inode children[64];
// };

struct spfs_child_list {
  unsigned long inos[64];
  size_t length;

  struct spfs_child_list *next;
};

struct spfs_block_header {
  // TODO
  spfs_block_header *next;
};

struct spfs_entry {
  struct spfs_inode inode;

  //TAG either file or dir
  union {
    struct spfs_child_list *children;
    struct spfs_block_header *file;
  };
};

struct spfs_super_block {
  unsigned int version;
  unsigned int magic;
  unsigned int block_size;

  // TODO spfs_super_block should occopy 4096 on disk but not in memory
  // char dummy[SPOOKY_FS_BLOCK_SIZE - (sizeof(unsigned int) * 2)];
};

#endif

#ifndef _SP_FS_SPFS_H
#define _SP_FS_SPFS_H

#define SPOOKY_FS_MAGIC 0xDEADBEEF
#define SPOOKY_FS_BLOCK_SIZE 4096
#define SPOOKY_FS_NAME_MAX 256

#define SPFS_ROOT_INODE_NO 1

typedef unsigned int spfs_offset;

struct spfs_super_block_wire {
  unsigned int version;
  unsigned int magic;
  unsigned int block_size;
  unsigned int id;

  // TODO spfs_super_block should occopy 4096 on disk but not in memory
  // char dummy[SPOOKY_FS_BLOCK_SIZE - (sizeof(unsigned int) * 2)];
};


struct spfs_inode {
  unsigned long id;

  unsigned int atime;
  unsigned int mtime;
  unsigned int ctime;

  mode_t mode;

  char name[SPOOKY_FS_NAME_MAX];
};

#define spfs_entry_kind_file 1
#define spfs_entry_kind_dir 2

struct spfs_entry {
  struct spfs_inode inode;

  // TAG either file or dir
  int kind;
  union {
    spfs_offset children;
    spfs_offset files;
  };
};

#endif

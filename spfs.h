#ifndef _SP_FS_SPFS_H
#define _SP_FS_SPFS_H

#define SPOOKY_FS_MAGIC 0xDEADBEEF
#define SPOOKY_FS_BLOCK_SIZE 4096
#define SPOOKY_FS_NAME_MAX 256

#define SPFS_ROOT_INODE_NO 1

struct spfs_super_block_wire {
  unsigned int version;
  unsigned int magic;
  unsigned int block_size;
  unsigned int id;

  // TODO spfs_super_block should occopy 4096 on disk but not in memory
  // char dummy[SPOOKY_FS_BLOCK_SIZE - (sizeof(unsigned int) * 2)];
};

struct spfs_inode {
  unsigned long inode_no;

  unsigned int atime;
  unsigned int mtime;
  unsigned int ctime;

  mode_t mode;

  char name[SPOOKY_FS_NAME_MAX];
};

struct spfs_child_list {
  unsigned long inos[64];
  size_t length;

  struct spfs_child_list *next;
};

struct spfs_block_header {
  // TODO
  struct spfs_block_header *next;
};

enum spfs_entry_kind {
  spfs_entry_kind_DIRECTORY,
  spfs_entry_kind_FILE,
};

#define spfs_entry_kind_file 1
#define spfs_entry_kind_dir 2

struct spfs_entry {
  struct spfs_inode inode;

  // TAG either file or dir
  int kind;
  union {
    unsigned int children;
    struct spfs_block_header *file;
  };
};

#endif

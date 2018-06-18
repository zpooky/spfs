#ifndef _SP_FS_SPFS_H
#define _SP_FS_SPFS_H

#define SPOOKY_FS_MAGIC 0xDEADBEEF
#define SPOOKY_FS_INITIAL_BLOCK_SIZE 512

#define SPOOKY_FS_NAME_MAX 256

#define SPFS_ROOT_INODE_NO 1

typedef unsigned int spfs_offset;
typedef unsigned int spfs_id;

typedef spfs_id spfs_be_id;

struct spfs_super_block_wire {
  unsigned int version;
  unsigned int magic;
  unsigned int block_size;
  spfs_id id;
  spfs_id root_id;

  spfs_offset btree;

  // TODO spfs_super_block should occopy 4096 on disk but not in memory
  // char dummy[SPOOKY_FS_BLOCK_SIZE - (sizeof(unsigned int) * 2)];
};

// __be32	di_size;
// __be32	di_gid;
// __be32	di_uid;
// __be32	di_mode;
// __be64	di_ctime;

struct spfs_inode {
  spfs_id id;
  unsigned int size;
  mode_t mode;

  unsigned int gid;
  unsigned int uid;

  unsigned int atime;
  unsigned int mtime;
  unsigned int ctime;

  // spfs_offset children_start;
  // spfs_offset file_start;
  spfs_offset start;

  char name[SPOOKY_FS_NAME_MAX];
};

#endif

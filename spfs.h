#ifndef _SP_FS_SPFS_H
#define _SP_FS_SPFS_H

#define SPOOKY_FS_SUPER_MAGIC 0xDEADBEEF
#define SPOOKY_FS_FL_MAGIC 0xDEADBEEF
#define SPOOKY_FS_BTREE_MAGIC 0xDEADBEEF

#define SPOOKY_FS_INITIAL_BLOCK_SIZE 512

#define SPOOKY_FS_NAME_MAX 256

#define SPFS_ROOT_INODE_NO 1

typedef unsigned long spfs_ino;

typedef spfs_ino spfs_be_ino;

// __be32	di_size;
// __be32	di_gid;
// __be32	di_uid;
// __be32	di_mode;
// __be64	di_ctime;


#endif

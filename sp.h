#ifndef _SP_FS_SP_H
#define _SP_FS_SP_H

#include <linux/limits.h>
#include <linux/types.h>
#include <stddef.h>

#include "btree.h"
#include "spfs.h"

struct spfs_super_block {
  unsigned int version;
  unsigned int magic;
  unsigned int block_size;

  unsigned int id;
  struct mutex id_lock;

  struct spfs_btree tree;

  bool dirty; // TODO?
};

struct spfs_priv_inode {
  struct mutex lock;
  spfs_offset start;
};

#endif

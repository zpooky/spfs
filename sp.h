#ifndef _SP_FS_SP_H
#define _SP_FS_SP_H

#include <linux/limits.h>
#include <linux/types.h>
#include <stddef.h>

#include "btree.h"
#include "spfs.h"

struct spfs_free_node {
  spfs_offset start;
  size_t length;
  struct spfs_free_node *next;
};

struct spfs_free_list {
  struct mutex lock; // TODO spin lock
  size_t length;
  struct spfs_free_node *root;
};

struct spfs_super_block {
  unsigned int version;
  unsigned int magic;
  unsigned int block_size;

  spfs_id id;
  struct mutex id_lock; // TODO spin lock

  spfs_id root_id;

  struct spfs_btree tree;

  struct spfs_free_list free_list;
};

struct spfs_priv_inode {
  struct mutex lock;
  spfs_offset start;
};

#endif

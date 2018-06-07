#ifndef _SP_FS_BTREE_H
#define _SP_FS_BTREE_H

#include "spfs.h"

typedef bool (*btree_cmp)(const spfs_entry *, const spfs_entry *);

// TODO this should be SP_FS_BLOCK_SIZE bytes
struct spfs_bnode {
  struct spfs_entry entries[1024];
  size_t length;
};

struct spfs_btree {
  struct spfs_bnode *root;
  btree_cmp cmp;
  struct mutex tree_lock;
};

extern spfs_btree *spfs_btree_init(btree_cmp);

extern spfs_entry *
spfs_btree_lookup(struct spfs_super_block *, unsigned long ino);

extern spfs_entry *
spfs_btree_insert(struct spfs_btree *tree, spfs_entry *in);

#endif

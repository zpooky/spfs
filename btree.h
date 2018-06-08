#ifndef _SP_FS_BTREE_H
#define _SP_FS_BTREE_H

#include <linux/mutex.h>
#include <stddef.h>

#include "spfs.h"

typedef int (*btree_cmp)(const struct spfs_entry *, const struct spfs_entry *);

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

extern int
spfs_btree_init(struct spfs_btree *, btree_cmp);

extern struct spfs_entry *
spfs_btree_lookup(struct spfs_btree *, unsigned long ino);

extern struct spfs_entry *
spfs_btree_insert(struct spfs_btree *tree, struct spfs_entry *in);

#endif

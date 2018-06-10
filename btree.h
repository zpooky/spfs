#ifndef _SP_FS_BTREE_H
#define _SP_FS_BTREE_H

#include <linux/fs.h>
#include <linux/mutex.h>
#include <stddef.h>

#include "spfs.h"

typedef int (*btree_cmp)(const struct spfs_entry *, const struct spfs_entry *);

typedef bool (*btree_modify_cb)(void *, struct spfs_entry *);

// TODO this should be SP_FS_BLOCK_SIZE bytes
struct spfs_bnode {
  struct spfs_entry entries[1024];
  size_t length;
};

struct spfs_btree {
  // struct spfs_bnode *root;
  btree_cmp cmp;
  struct mutex lock;
  size_t block_size;
  spfs_offset start;
  struct super_block *sb;
};

extern int
spfs_btree_init(struct super_block *sb, struct spfs_btree *, btree_cmp,
                spfs_offset);

extern bool
spfs_btree_lookup(struct spfs_btree *, spfs_id ino, struct spfs_entry *out);

extern bool
spfs_btree_insert(struct spfs_btree *tree, struct spfs_entry *in);

extern bool
spfs_btree_modify(struct spfs_btree *tree, spfs_id ino, void *,
                  btree_modify_cb);

#endif

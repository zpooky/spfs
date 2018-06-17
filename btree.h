#ifndef _SP_FS_BTREE_H
#define _SP_FS_BTREE_H

#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <stddef.h>

#include "free_list.h"
#include "spfs.h"

// TODO use numeric 0:success otherwise:fail

// true: dirty, false: not_dirty
typedef bool (*btree_modify_cb)(void *, struct spfs_inode *);

struct spfs_btree {
  struct super_block *sb;
  struct spfs_free_list *free_list;
  struct mutex lock;

  /* block { */
  size_t block_size;
  size_t blocks;
  sector_t start;
  /* } */
};

extern int
spfs_btree_init(struct super_block *sb, struct spfs_btree *, sector_t);

extern int
spfs_btree_modify(struct spfs_btree *tree, spfs_id ino, void *,
                  btree_modify_cb);

extern int
spfs_btree_lookup(struct spfs_btree *, spfs_id ino, struct spfs_inode *out);

extern int
spfs_btree_insert(struct spfs_btree *tree, struct spfs_inode *in);

extern int
spfs_btree_remove(struct spfs_btree *, spfs_id ino);

#endif

#ifndef _SP_FS_FREE_LIST_H
#define _SP_FS_FREE_LIST_H

#include <linux/fs.h>
#include <linux/limits.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include "spfs.h"

struct spfs_free_node {
  sector_t start;
  uint32_t blocks;
  struct spfs_free_node *next;
};

struct spfs_free_list {
  struct mutex lock; // TODO spin lock
  uint32_t blocks;
  struct spfs_free_node *root;
};

extern int
spfs_free_init(struct super_block *sb, struct spfs_free_list *list,
               sector_t head);

extern sector_t
spfs_free_alloc(struct spfs_free_list *fl, size_t blocks);

extern int
spfs_free_dealloc(struct spfs_free_list *fl, sector_t root, size_t blocks);

#endif

#ifndef _SP_FS_FREE_LIST_H
#define _SP_FS_FREE_LIST_H

#include <linux/limits.h>
#include <linux/mutex.h>
#include <linux/types.h>

struct spfs_free_node {
  sector_t start;
  size_t blocks;
  struct spfs_free_node *next;
};

struct spfs_free_list {
  struct mutex lock; // TODO spin lock
  size_t blocks;
  struct spfs_free_node *root;
};

extern int
spfs_init_free_list(struct super_block *sb, struct spfs_free_list *list,
                    sector_t head);

extern sector_t
spfs_free_alloc(struct super_block *sb, size_t len);

extern int
spfs_free_dealloc(struct super_block *sb, sector_t root, size_t blocks);

#endif

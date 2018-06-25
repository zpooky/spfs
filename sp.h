#ifndef _SP_FS_SP_H
#define _SP_FS_SP_H

#include <linux/limits.h>
#include <linux/types.h>
#include <stddef.h>

#include "btree.h"
#include "free_list.h"
#include "spfs.h"

struct spfs_super_block {
  unsigned long version;
  unsigned long magic;
  unsigned long block_size;

  sector_t btree_offset;
  sector_t free_list_offset;

  spfs_ino id;
  struct mutex id_lock; // TODO spin lock

  spfs_ino root_id;

  struct spfs_btree tree;

  struct spfs_free_list free_list;
};

#endif

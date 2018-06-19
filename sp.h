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

// TODO remove
// struct spfs_priv_inode_x {
//   #<{(|
//    * Guards write access to underlying $start structure
//    |)}>#
//   struct mutex lock;
//   #<{(|
//    * The start of the file data blocks or the start of child inodes
//    |)}>#
//   sector_t start;
//
//   #<{(| { |)}>#
//   #<{(|
//    * We need to ensure that there is only in memory representative of the
//    * specific file so that there is not more than one $lock & start variable
//    * which would result in bugs.
//    * Is represented as a non rebalancing BST.
//    *
//    * TODO have in spfs_super_block a pointer to root priv_inode tree. & spinlock
//    *      guard access.
//    * TODO when creating a new inode we most atomically make sure there does not
//    *      exist more than in-mem representation of inode.
//    * TODO callback when inode gets gc:ed to de-list the instance and reclaim mem
//    *      of the spfs_priv_inode* structure itself.
//    |)}>#
//   struct spfs_priv_inode *left;
//   struct spfs_priv_inode *right;
//   #<{(| } |)}>#
// };

#endif

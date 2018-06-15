#ifndef _SP_FS_SP_H
#define _SP_FS_SP_H

#include <linux/limits.h>
#include <linux/types.h>
#include <stddef.h>

#include "free_list.h"
#include "btree.h"
#include "spfs.h"

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
  /*
   * Guards write access to underlying $start structure
   */
  struct mutex lock;
  /*
   * The start of the file data blocks or the start of child inodes
   */
  spfs_offset start;

  /* { */
  /*
   * We need to ensure that there is only in memory representative of the
   * specific file so that there is not more than one $lock & start variable
   * which would result in bugs.
   * Is represented as a non rebalancing BST.
   *
   * TODO have in spfs_super_block a pointer to root priv_inode tree. & spinlock
   *      guard access.
   * TODO when creating a new inode we most atomically make sure there does not
   *      exist more than in-mem representation of inode.
   * TODO callback when inode gets gc:ed to de-list the instance and reclaim mem
   *      of the spfs_priv_inode* structure itself.
   */
  struct spfs_priv_inode *left;
  struct spfs_priv_inode *right;
  /* } */
};

#endif

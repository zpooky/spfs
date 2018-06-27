#ifndef _SP_FS_SHARED_H
#define _SP_FS_SHARED_H

#include "spfs.h"
#include <linux/mutex.h>

struct spfs_inode {
  /* transient { */

  /* Should always be first */
  struct inode i_inode;

  struct mutex lock;
  /* } */

  /* Inode = File
   * Capacity in number of bytes:
   * - Does not include bytes in extent header
   * - Only includes data writable bytes
   *
   * Inode = Directory
   * Capacity is the number of child inodes
   */
  size_t capacity;
  sector_t start;
  char name[SPOOKY_FS_NAME_MAX];
};

#define SPFS_INODE(inode) ((struct spfs_inode *)inode)

#endif

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

  /* Capacity in number of blocks */
  size_t capacity;
  sector_t start;
  char name[SPOOKY_FS_NAME_MAX];
};

#define SPFS_INODE(inode) ((struct spfs_inode *)inode)

#endif

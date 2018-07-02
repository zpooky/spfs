#ifndef _SP_FS_MKFS_SPFS_H
#define _SP_FS_MKFS_SPFS_H

#include <stddef.h>
#include <stdint.h>

struct spfs_super_block_wire {
  uint32_t magic;
  uint32_t version;
  uint32_t block_size;
  uint32_t dummy;

  uint32_t id;
  uint32_t root_id;

  uint32_t btree_offset;
  uint32_t free_list_offset;

  /* transient: { */
  size_t blocks;
  /* } */
};

struct spfs_free_list {
  uint32_t magic;
  uint32_t entries;

  /* list of spfs_free_list */
  uint32_t next;
};

struct spfs_free_entry {
  uint32_t start;
  uint32_t blocks;
};

#endif

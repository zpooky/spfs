#include "free_list.h"
#include "util.h"
#include <linux/buffer_head.h>
#include <linux/slab.h> /* kzalloc */

/* ===================================== */
static struct spfs_free_node *
spfs_init_free_list_entry(struct buffer_head *bh, unsigned int *bh_pos) {
  struct spfs_free_node *result;

  spfs_offset entry_start;
  unsigned int entry_blocks;

  if (!spfs_sb_read_u32(bh, bh_pos, &entry_start)) {
    return NULL;
  }
  if (!spfs_sb_read_u32(bh, bh_pos, &entry_blocks)) {
    return NULL;
  }

  result = kzalloc(sizeof(*result), GFP_KERNEL);
  if (!result) {
    return NULL;
  }

  result->next = NULL;
  result->start = entry_start;
  result->blocks = entry_blocks;

  return result;
}

int
spfs_free_init(struct super_block *sb, struct spfs_free_list *list,
               sector_t head) {
  mutex_init(&list->lock);
  list->root = NULL;
  list->blocks = 0;

Lit:
  if (head) {
    unsigned int i = 0;
    struct buffer_head *bh;
    unsigned int bh_pos = 0;

    unsigned int free_length;
    spfs_offset free_next;

    bh = sb_bread(sb, head);
    if (!bh) {
      printk(KERN_INFO "NULL = sb_bread(sb, head[%zu])\n", head);
      return 1;
    }

    /* entry[spfs_offset,size_t]
     * free_list[length:u32,next:spfs_offset,entry:[length]]
     */

    if (!spfs_sb_read_u32(bh, &bh_pos, &free_length)) {
      return -EINVAL;
    }
    if (!spfs_sb_read_u32(bh, &bh_pos, &free_next)) {
      return -EINVAL;
    }

    for (i = 0; i < free_length; ++i) {
      struct spfs_free_node *node;
      node = spfs_init_free_list_entry(bh, &bh_pos);

      if (!node) {
        // TODO cleanup
        return 1;
      }

      node->next = list->root;
      list->root = node;

      list->blocks += node->blocks;
    }

    brelse(bh);
    head = free_next;
    goto Lit;
  }

  return 0;
}

/* ===================================== */

sector_t
spfs_free_alloc(struct spfs_free_list *free_list, size_t blocks) {
  // TODO support small return < len so that calller invokes f_file_alloc
  // multiple time
  sector_t result = 0;

  {
    struct spfs_free_node *list;
    mutex_lock(&free_list->lock);
    if (free_list->blocks > blocks) {
      list = free_list->root;

    Lit:
      if (list) {
        /* TODO handle 0 length node */
        if (list->blocks >= blocks) {
          list->blocks -= blocks;
          free_list->blocks -= blocks;

          result = list->start + list->blocks;
        } else {
          list = list->next;
          goto Lit;
        }
      }
    }
    mutex_unlock(&free_list->lock);
  }

  return result;
}

/* ===================================== */
int
spfs_free_dealloc(struct spfs_free_list *fl, sector_t root, size_t blocks) {
  return 0;
}

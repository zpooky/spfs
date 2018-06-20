#include "free_list.h"
#include "util.h"
#include <linux/buffer_head.h>
#include <linux/slab.h> /* kzalloc */

/* ===================================== */
static int
spfs_read_entry(struct buffer_head *bh, size_t *bh_pos,
                struct spfs_free_node *result) {

  if (!spfs_sb_read_u32(bh, bh_pos, &result->start)) {
    return 1;
  }
  if (!spfs_sb_read_u32(bh, bh_pos, &result->blocks)) {
    return 1;
  }

  return 0;
}

static struct spfs_free_node *
spfs_init_free_list_entry(struct buffer_head *bh, size_t *bh_pos) {
  struct spfs_free_node *result;

  result = kzalloc(sizeof(*result), GFP_KERNEL);
  if (!result) {
    return NULL;
  }

  result->next = NULL;

  if (spfs_read_entry(bh, bh_pos, result)) {
    kfree(result);
    return NULL;
  }

  return result;
}

static int
spfs_read_header(struct buffer_head *bh, size_t *bh_pos, unsigned long *length,
                 sector_t *next) {
  unsigned long magic;
  if (!spfs_sb_read_u32(bh, bh_pos, &magic)) {
    return -EINVAL;
  }

  if (magic != SPOOKY_FS_FL_MAGIC) {
    printk(KERN_INFO "invalid Free-List magic:[%lu] expected:[%u]", //
           magic, SPOOKY_FS_FL_MAGIC);
    return -EINVAL;
  }

  if (!spfs_sb_read_u32(bh, bh_pos, length)) {
    return -EINVAL;
  }
  if (!spfs_sb_read_u32(bh, bh_pos, next)) {
    return -EINVAL;
  }

  return 0;
}

int
spfs_free_init(struct super_block *sb, struct spfs_free_list *list,
               sector_t head) {
  int res;
  mutex_init(&list->lock);
  list->root = NULL;
  list->blocks = 0;

Lit:
  if (head) {
    unsigned long free_length;
    sector_t free_next;

    unsigned int i = 0;
    struct buffer_head *bh;
    size_t bh_pos = 0;

    bh = sb_bread(sb, head);
    if (!bh) {
      printk(KERN_INFO "NULL = sb_bread(sb, head[%zu])\n", head);
      return 1;
    }

    res = spfs_read_header(bh, &bh_pos, &free_length, &free_next);
    if (res) {
      // cleanup
      return res;
    }

    for (i = 0; i < free_length; ++i) {
      struct spfs_free_node *node;
      node = spfs_init_free_list_entry(bh, &bh_pos);

      if (!node) {
        // cleanup
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
        /* XXX handle 0 length node */
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
  // TODO
  return 0;
}

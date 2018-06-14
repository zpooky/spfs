#include "btree.h"
#include "sp.h"

#include <linux/buffer_head.h>

/* #include <linux/slab.h> #<{(| kzalloc |)}># */

/*
 * i_data is "pages read/written by this inode"
 * i_mapping is "whom should I ask for pages?"
 *
 * IOW, everything outside of individual filesystems should use the latter.
 * http://lkml.iu.edu/hypermail/linux/kernel/0105.2/1363.html
 */

//=====================================
int
spfs_btree_init(struct super_block *sb, struct spfs_btree *tree,
                sector_t start) {
  /* struct address_space *mapping; */
  /* struct page *page; */
  /* unsigned int size; */

  struct spfs_super_block *sbi;

  BUG_ON(!tree);
  BUG_ON(!sb);

  sbi = sb->s_fs_info;

  tree->sb = sb;
  mutex_init(&tree->lock);

  /* block { */
  tree->block_size = sbi->block_size;
  tree->blocks = 1;
  tree->start = start;
  /* } */

  tree->dummy = NULL;

  return 0;
}

//=====================================
#if 0
static struct spfs_entry *
btree_get_node(struct spfs_btree *tree, spfs_id ino) {
  struct spfs_entry *head;
  BUG_ON(!tree);

  head = tree->dummy;
Lit:
  if (head) {
    if (head->inode.id == ino) {
      return head;
    }

    head = head->next;
    goto Lit;
  }

  return NULL;
}
#endif

static int
spfs_entry_cmp(const struct spfs_entry *f, const struct spfs_entry *s) {
  BUG_ON(!f);
  BUG_ON(!s);

  return f->inode.id > s->inode.id;
}

struct spfs_bnode {
  struct spfs_entry *entries;
  size_t length;

  sector_t *children;
};

static struct spfs_entry *
spfs_bin_find_gte(struct spfs_bnode *node, const struct spfs_entry *needle) {
  // TODO
  return NULL;
}

static int
spfs_parse_bnode(struct buffer_head *bh, struct spfs_bnode *result) {
  // TODO
  return -EINVAL;
}

static size_t
spfs_index_of(struct spfs_bnode *node, struct spfs_entry *needle) {
  // TODO
  return 0;
}

//=====================================
int
spfs_btree_modify(struct spfs_btree *tree, spfs_id ino, void *closure,
                  btree_modify_cb cb) {
  const struct spfs_entry needle = {
      .inode =
          {
              .id = ino,
          },
  };

  struct super_block *sb = tree->sb;
  sector_t offset = tree->start;

Lit:
  if (offset) {
    struct spfs_entry *gte;
    struct buffer_head *bh;
    struct spfs_bnode node;

    bh = sb_bread(sb, offset);
    if (!bh) {
      return -EIO;
    }

    if (spfs_parse_bnode(bh, &node)) {
      brelse(bh);
      return -EINVAL;
    }

    gte = spfs_bin_find_gte(&node, &needle);
    if (gte) {
      size_t index;

      if (!spfs_entry_cmp(&needle, gte) && !spfs_entry_cmp(gte, &needle)) {
        /* equal */

        if (cb(closure, gte)) {
          mark_buffer_dirty(bh);
        }

        brelse(bh);
        return 0;
      }

      /* Go down less than entry child */
      index = spfs_index_of(&node, gte);
      /* assertxs(index != capacity(entries), index, length(entries)); */

      offset = node.children[index];

      brelse(bh);
      goto Lit;
    } else {

      /* needle is greater than any other entry in entries */
      offset = node.children[node.length];

      brelse(bh);
      goto Lit;
    }

    if (bh) {
      brelse(bh);
    }
    BUG();
  }

  return 1;
#if 0
  struct spfs_entry *res = btree_get_node(tree, ino);
  if (res) {
    cb(closure, res);
  }

  return res != NULL;
#endif
}

//=====================================
static bool
spfs_btree_lookup_cb(void *closure, struct spfs_entry *result) {
  struct spfs_entry *dest = closure;

  memcpy(dest, result, sizeof(*result));

  // we do not alter representation
  return false;
}

int
spfs_btree_lookup(struct spfs_btree *tree, spfs_id ino,
                  struct spfs_entry *result) {

  return spfs_btree_modify(tree, ino, result, spfs_btree_lookup_cb);

#if 0
  struct spfs_entry *res = btree_get_node(tree, ino);
  if (res) {
    memcpy(result, res, sizeof(*res));
  }

  return result != NULL;
#endif
}

//=====================================
int
spfs_btree_insert(struct spfs_btree *tree, struct spfs_entry *in) {
  return 1;
}

//=====================================
int
spfs_btree_remove(struct spfs_btree *tree, spfs_id ino) {
  BUG();
  // TODO
  return 1;
}

#include "btree.h"
#include "free_list.h"
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
struct spfs_bentry {
  spfs_id id;
  sector_t offset;
};

struct spfs_bnode {
  struct spfs_bentry *entries;
  size_t length;

  sector_t *children;
};

typedef spfs_entry_sector sector_t;

struct btree_bubble {
  struct spfs_bentry entry;
  sector_t greater;
};

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
  /*TODO btree magic*/

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
spfs_entry_cmp(const struct spfs_bentry *f, const struct spfs_bentry *s) {
  BUG_ON(!f);
  BUG_ON(!s);

  return f->id > s->id;
}

static struct spfs_bentry *
bnode_bin_find_gte(struct spfs_bnode *node, struct spfs_bentry *needle) {
  // TODO
  return NULL;
}

static int
spfs_parse_bnode(struct buffer_head *bh, struct spfs_bnode *result) {
  BUG_ON(!bh);

  // TODO
  return -EINVAL;
}

static int
bnode_make(struct buffer_head *bh, struct btree_bubble *bubble,
           struct spfs_bnode *result) {
  BUG_ON(!bh);
  // TODO
  return 0;
}

static size_t
bnode_index_of(struct spfs_bnode *node, struct spfs_bentry *needle) {
  // TODO
  return 0;
}

/* ===================================== */
static int
btree_visit_entry(struct super_block *sb, sector_t offset, void *closure,
                  btree_modify_cb cb) {

  struct buffer_head *bh;
  bh = sb_bread(sb, offset);

  if (!bh) {
    return 10;
  }

  if (cb(closure, gte)) {
    mark_buffer_dirty(bh);
  }

  brelse(bh);

  return 0;
}

int
spfs_btree_modify(struct spfs_btree *tree, spfs_id ino, void *closure,
                  btree_modify_cb cb) {
  const struct spfs_bentry needle = {
      .id = ino,
  };

  struct super_block *sb = tree->sb;
  sector_t offset = tree->start;

Lit:
  if (offset) {
    struct spfs_bentry *gte;
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

    gte = bnode_bin_find_gte(&node, &needle);
    if (gte) {
      size_t index;

      if (!spfs_entry_cmp(&needle, gte) && !spfs_entry_cmp(gte, &needle)) {
        /* equal */
        sector_t entry_offset = gte->offset;
        brelse(bh);

        return btree_visit_entry(sb, entry_offset, closure, cb);
      }

      /* Go down less than entry child */
      index = bnode_index_of(&node, gte);
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

  /* We do not alter representation. */
  return false;
}

int
spfs_btree_lookup(struct spfs_btree *tree, spfs_id ino,
                  struct spfs_entry *result) {

  return spfs_btree_modify(tree, ino, result, spfs_btree_lookup_cb);
}

//=====================================
static bool
bnode_is_full(struct spfs_bnode *tree) {
  return true;
}

static bool
bnode_is_empty(struct spfs_bnode *tree) {
  return true;
}

static struct spfs_bentry *
bnode_bin_insert(struct spfs_bnode *node, struct spfs_bentry *subject,
                 sector_t gt) {
  return NULL;
}

static sector_t *
bnode_child_insert(struct spfs_bnode *node, sector_t child) {
  return NULL;
}

static int
bnode_init(sector_t offset) {
  return 0;
}

static int
bnode_partition(struct spfs_bnode *tree, spfs_id med,
                struct spfs_bnode *right) {
  return 0;
}

static int
bentry_extract(struct spfs_bnode *tree, spfs_id /*src*/ med,
               struct btree_bubble * /*dest*/ bubble,
               struct spfs_bnode *right) {
  return 0;
}

static bool
bubble_is_active(const struct btree_bubble *bubble) {
  return bubble->entry.id != 0;
}

static int
spfs_entry_make(struct super_block *sb, sector_t page, struct spfs_entry *in) {
  return 0;
}

/* #if 0 */

/*
 * 0: ok
 * 1: duplicate
 * 2: EIO
 */
static int
btree_insert(struct spfs_btree *self, struct spfs_bnode *tree,
             struct spfs_entry *in, struct btree_bubble *bubble,
             spfs_entry_sector *out) noexcept {
  int res;

  struct super_block *sb = self->sb;
  const struct spfs_bentry needle = {
      .id = in->inode.id,
  };
  memset(bubble, 0, sizeof(*bubble));

  if (!tree) {
    sector_t page = spfs_free_alloc(sb, self->block_size);
    if (!page) {
      return 1;
    }

    spfs_entry_make(sb, page, in);
    *out = page;

    bubble->entry.id = in->inode.id;
    bubble->entry.offset = page;
    bubble->greater = 0;

    /* return std::make_tuple(bubble, greater); */
    return 0;
  }

  /* auto &children = tree->children; */
  /* auto &elements = tree->elements; */

  /* 1. Traverse down */
  T *const gte = bnode_bin_find_gte(tree, needle);
  if (gte) {
    if (!spfs_bentry_cmp(needle, *gte) && !spfs_bentry_cmp(*gte, needle)) {
      /* duplicate */
      *out = gte->offset;
      return 1;
    }

    /* go down less-than $gte child */
    size_t index = spfs_index_of(elements, gte);
    BUG_ON(index == capacity(elements));

    const auto child = children[index];
    res = btree_insert(self, child, in, bubble, out);
  } else {
    BUG_ON(bnode_is_empty(tree));

    /* go down the last (greates) child */
    auto child = last(children);

    res = btree_insert(self, *child, in, bubble, out);
  }

  /* 2. Fixup */
  if (bubble_is_active(bubble)) {
    return btree_fixup(self, tree, bubble);
  }

  return res;
}

/* #endif */

static int
btree_fixup(struct spfs_btree *self, struct spfs_bnode *tree,
            struct btree_bubble *bubble) noexcept {
  struct super_block *sb = self->sb;

  BUG_ON(!bubble_is_active(bubble))
  BUG_ON(!self);
  BUG_ON(!tree);
  BUG_ON(!bubble);

  if (!bnode_is_full(*tree)) {
    struct spfs_bentry *res;
    res = bnode_bin_insert(tree, &bubble->entry, bubble->greater);

    BUG_ON(!res);

    /* return empty<T, keys, Cmp>(); */
    return 0;
  }

  /* split tree into two */
  spfs_id med = bnode_median(tree, &bubble->entry);

  sector_t right_page = spfs_free_alloc(sb, self->blocks);
  BUG_ON(!right_page); // XXX
  bnode_init(right);   // TODO read buffer

  bnode_partition(tree, med, right);

  if (med != bubble->entry.id) {
    // XXX: assert $med is last in $tree
    struct spfs_bentry *res;
    if (spfs_entry_cmp(bubble, /*>*/ med_copy)) {
      res = bnode_bin_insert(right, &bubble->entry, bubble->greater);
    } else {
      res = bnode_bin_insert(tree, &bubble->entry, bubble->greater);
    }

    BUG_ON(!res);

    bentry_extract(tree, /*src*/ med, /*dest*/ bubble, right);
  } else {
    BUG_ON(right->children[0] != 0);
    right->children[0] = bubble->greater;
  }

  return 0;
}

static int
bnode_alloc(struct spfs_btree *self, struct spfs_bubble *bubble) {
  int res;
  sector_t root;
  struct super_block *sb;
  struct spfs_bnode tree;

  const sector_t less = self->start;
  sb = self->sb;

  root = spfs_free_alloc(sb, self->blocks);
  if (!root) {
    res = -ENOMEM;
    goto Lret;
  }

  bh = sb_bread(sb, self->start);
  if (!bh) {
    res = -EIO;
    goto Lrelease;
  }

  if (bnode_make(bh, bubble, &tree)) {
    res = -EINVAL;
    goto Lfree;
  }

  self->start = root;

  {
    sector_t *ires = bnode_child_insert(tree, less);
    BUG_ON(!ires);
  }

  {
    spfs_bentry *ires = bnode_bin_insert(tree, &bubble->entry, bubble->right);
    BUG_ON(!ires);
  }

  res = 0;
  goto Lrelease;

Lfree:
  spfs_free_dealloc(sb, root, self->blocks);

Lrelease:
  if (sb)
    brelse(sb);

Lret:
  return res;
}

int
spfs_btree_insert(struct spfs_btree *self, struct spfs_entry *in) {
  int res;
  struct buffer_head *bh;
  struct spfs_bnode *tree;
  struct spfs_bubble bubble;
  struct super_block *sb;

  sb = self->sb;
  /* Result as the location of the inserted entry. */
  spfs_entry_sector out = 0;
  memset(&bubble, 0, sizeof(bubble));

  if (!self->start) {
    BUG();
    return -EIO
  }

  bh = sb_bread(sb, self->start);
  if (!bh) {
    return -EIO;
  }

  if (spfs_parse_bnode(bh, &tree)) {
    brelse(bh);
    return -EINVAL;
  }

  res = btree_insert(self, tree, in, bubble, &out);
  brelse(bh); /*invalidates tree*/

  if (bubble_is_active(&bubble)) {
    int nres;
    nres = bnode_alloc(self, &bubble);

    BUG_ON(nres);
  }

  return res;
}

//=====================================
int
spfs_btree_remove(struct spfs_btree *tree, spfs_id ino) {
  BUG();
  // TODO
  return 1;
}

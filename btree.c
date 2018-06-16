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
typedef double spfs_be_sector_t;
/* typedef sector_t spfs_be_sector_t; */

struct spfs_bentry {
  spfs_be_id id;
  spfs_be_sector_t offset;
};

struct spfs_bnode {
  __be32 *length;
  struct spfs_bentry *entries;
  spfs_be_sector_t *children;
};
/*
 * TODO make bnode an opaque type to avoid direct access of its fields
 */

typedef sector_t spfs_entry_sector;

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
static int
spfs_entry_cmp(const struct spfs_bentry *f, const struct spfs_bentry *s) {
  BUG_ON(!f);
  BUG_ON(!s);

  // TODO byteorder
  return f->id > s->id;
}

static int
spfs_bubble_cmp(const struct btree_bubble *bubble, spfs_be_id o) {
  spfs_id id = be32_to_cpu(bubble->entry.id);
  spfs_id other = be32_to_cpu(o);
  return id > other;
}

static bool
bentry_is_eq(const struct spfs_bentry *f, spfs_id id) {
  spfs_id other = be32_to_cpu(f->id);
  return other == id;
}

static size_t
bnode_capacity(struct spfs_btree *self) {
  // TODO
  return 0;
}

static struct spfs_bentry *
bnode_bin_find_gte(struct spfs_bnode *node, spfs_id needle) {
  struct spfs_bentry *it;
  size_t length;
  const struct spfs_bentry *end;

  BUG_ON(!node);
  BUG_ON(!node->entries);
  BUG_ON(!node->length);

  it = node->entries;
  length = be32_to_cpu(node->length);
  /* BUG_ON(length > bnode_capacity(node)); */

  end = it + length;

  while (it < end) {
    /* if (spfs_entry_cmp()) { */
    // TODO
    /* } */
  }

  // TODO
  return NULL;
}

static int
bnode_parse(struct buffer_head *bh, struct spfs_bnode *result) {
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

static int
spfs_entry_parse(struct buffer_head *bh, struct spfs_entry *out) {
  // TODO
  return 0;
}

static int
spfs_entry_make(struct super_block *sb, spfs_entry_sector page,
                const struct spfs_entry *in) {
  // TODO
  return 0;
}

/* ===================================== */
static int
btree_visit_entry(struct super_block *sb, spfs_entry_sector offset,
                  void *closure, btree_modify_cb cb) {
  int res;
  struct buffer_head *bh;
  struct spfs_entry cur;

  if (!offset) {
    return 1;
  }

  bh = sb_bread(sb, offset);

  if (!bh) {
    return 10;
  }

  res = spfs_entry_parse(bh, &cur);
  if (res) {
    brelse(bh);
    return res;
  }

  if (cb(closure, &cur)) {
    mark_buffer_dirty(bh);
  }

  brelse(bh);

  return 0;
}

static size_t
bnode_children(struct spfs_bnode *node) {
  // TODO
  return 0;
}

static sector_t
bnode_child(struct spfs_bnode *node, size_t index) {
  BUG_ON(index > bnode_children(node));

  sector_t result = node->children[index];
  // XXX size of sector
  return be32_to_cpu(result);
}

static sector_t
bnode_last_child(struct spfs_bnode *node) {
  // TODO
  return 0;
}

int
spfs_btree_modify(struct spfs_btree *tree, spfs_id ino, void *closure,
                  btree_modify_cb cb) {
  struct super_block *sb = tree->sb;
  sector_t offset = tree->start;

Lit:
  if (offset) {
    struct buffer_head *bh;
    struct spfs_bentry *gte;
    struct spfs_bnode node;

    bh = sb_bread(sb, offset);
    if (!bh) {
      return -EIO;
    }

    if (bnode_parse(bh, &node)) {
      brelse(bh);
      return -EINVAL;
    }

    gte = bnode_bin_find_gte(&node, ino);
    if (gte) {
      size_t index;

      if (bentry_is_eq(gte, ino)) {
        /* equal */
        spfs_entry_sector entry_offset = gte->offset;
        brelse(bh); /*invalidatess $gte*/

        return btree_visit_entry(sb, entry_offset, closure, cb);
      }

      /* Go down less than entry child */
      index = bnode_index_of(&node, gte);
      /* assertxs(index != capacity(entries), index, length(entries)); */

      offset = bnode_child(&node, index);

      brelse(bh);
      goto Lit;
    } else {

      /* $ino is greater than any other entry in entries */
      offset = bnode_child(&node, node.length);

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
bnode_is_full(struct spfs_btree *self, struct spfs_bnode *tree) {
  return be32_to_cpu(tree->length) == bnode_capacity(self);
}

static bool
bnode_is_empty(struct spfs_bnode *tree) {
  return be32_to_cpu(tree->length) == 0;
}

static struct spfs_bentry *
bnode_bin_insert(struct spfs_bnode *node, struct btree_bubble *bubble) {
  // TODO
  return NULL;
}

static sector_t *
bnode_child_insert(struct spfs_bnode *node, sector_t child) {
  // TODO
  return NULL;
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
  // TODO
  return 0;
}

static bool
bubble_is_active(const struct btree_bubble *bubble) {
  return bubble->entry.id != 0;
}

/* #if 0 */

static int
btree_fixup(struct spfs_btree *self, struct spfs_bnode *tree,
            struct btree_bubble *bubble);

/*
 * 0: ok
 * 1: duplicate
 * 2: EIO
 */
static int
btree_insert(struct spfs_btree *self, struct spfs_bnode *tree,
             struct spfs_entry *in, struct btree_bubble *bubble,
             spfs_entry_sector *out) {
  int res;

  struct super_block *sb = self->sb;
  memset(bubble, 0, sizeof(*bubble));

  if (!tree) {
    sector_t page = spfs_free_alloc(sb, self->block_size);
    if (!page) {
      return 1;
    }

    res = spfs_entry_make(sb, page, in);
    if (res) {
      // XXX page reclaim
      return res;
    }
    *out = page;

    // TODO byte order
    bubble->entry.id = in->inode.id;
    bubble->entry.offset = page;
    bubble->greater = 0;

    /* return std::make_tuple(bubble, greater); */
    return 0;
  }

  /* auto &children = tree->children; */
  /* auto &elements = tree->elements; */

  /* 1. Traverse down */
  struct spfs_bentry *gte = bnode_bin_find_gte(tree, in->inode.id);
  if (gte) {
    if (bentry_is_eq(gte, in->inode.id)) {
      /* duplicate */
      *out = gte->offset; // TODO byteorder
      return 1;
    }

    /* go down less-than $gte child */
    size_t index = bnode_index_of(tree, gte);
    BUG_ON(index == bnode_capacity(self));

    // TODO sb buffer map child
    sector_t child = bnode_child(tree, index);
    res = btree_insert(self, child, in, bubble, out);
  } else {
    BUG_ON(bnode_is_empty(tree));

    /* go down the last (greates) child */
    sector_t *child = bnode_last_child(tree);

    // TODO sb buffer map child
    res = btree_insert(self, *child, in, bubble, out);
  }

  /* 2. Fixup */
  if (bubble_is_active(bubble)) {
    return btree_fixup(self, tree, bubble);
  }

  return res;
}

/* #endif */
static spfs_be_id
bnode_median(struct spfs_bnode *tree, struct spfs_bentry *entry) {
  // TODO
  return 0;
}

static sector_t
bnode_alloc_empty(struct spfs_btree *self) {
  struct super_block *sb = self->sb;

  sector_t right_page = spfs_free_alloc(sb, self->blocks);
  BUG_ON(!right_page); // XXX
  return 0;
}

static void
bnode_set_child(struct spfs_bnode *tree, size_t index,
                struct btree_bubble *bubble) {
  // TODO
  tree->children[index] = bubble->greater;
}

static int
btree_fixup(struct spfs_btree *self, struct spfs_bnode *tree,
            struct btree_bubble *bubble) {
  struct super_block *sb = self->sb;

  BUG_ON(!bubble_is_active(bubble));
  BUG_ON(!self);
  BUG_ON(!tree);
  BUG_ON(!bubble);

  if (!bnode_is_full(self, tree)) {
    struct spfs_bentry *res;
    res = bnode_bin_insert(tree, bubble);

    BUG_ON(!res);

    /* return empty<T, keys, Cmp>(); */
    return 0;
  }

  /* split tree into two */
  spfs_be_id med = bnode_median(tree, &bubble->entry);

  // TODO read buffer
  sector_t right = bnode_alloc_empty(self);

  bnode_partition(tree, med, right);

  if (med != bubble->entry.id) {
    // XXX: assert $med is last in $tree
    struct spfs_bentry *res;
    if (spfs_bubble_cmp(bubble, /*>*/ med)) {
      res = bnode_bin_insert(right, bubble);
    } else {
      res = bnode_bin_insert(tree, bubble);
    }

    BUG_ON(!res);

    bentry_extract(tree, /*src*/ med, /*dest*/ bubble, right);
  } else {
    /* XXX BUG_ON(right->children[0] != 0); */
    bnode_set_child(right, 0, bubble);
  }

  return 0;
}

static int
bnode_alloc_root(struct spfs_btree *self, struct btree_bubble *bubble) {
  int res;
  sector_t root;
  struct super_block *sb;
  struct buffer_head *bh;
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
    sector_t *ires = bnode_child_insert(&tree, less);
    BUG_ON(!ires);
  }

  {
    struct spfs_bentry *ires = bnode_bin_insert(&tree, bubble);
    BUG_ON(!ires);
  }

  res = 0;
  goto Lrelease;

Lfree:
  spfs_free_dealloc(sb, root, self->blocks);

Lrelease:
  if (bh) {
    brelse(bh);
  }

Lret:
  return res;
}

int
spfs_btree_insert(struct spfs_btree *self, struct spfs_entry *in) {
  int res;
  struct buffer_head *bh;
  struct spfs_bnode tree;
  struct btree_bubble bubble;
  struct super_block *sb;

  sb = self->sb;
  /* Result as the location of the inserted entry. */
  spfs_entry_sector out = 0;
  memset(&bubble, 0, sizeof(bubble));

  if (!self->start) {
    BUG();
    return -EIO;
  }

  bh = sb_bread(sb, self->start);
  if (!bh) {
    return -EIO;
  }

  if (bnode_parse(bh, &tree)) {
    brelse(bh);
    return -EINVAL;
  }

  res = btree_insert(self, &tree, in, &bubble, &out);
  brelse(bh); /*invalidates tree*/

  if (bubble_is_active(&bubble)) {
    int nres;
    nres = bnode_alloc_root(self, &bubble);

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

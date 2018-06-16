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
  /* __be32 *length; */
  void *length;
  /* struct spfs_bentry *entries; */
  void *entries;
  /* spfs_be_sector_t *children; */
  void *children;
};
/*
 * bnode & bentry should not be accessed directly
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
  /*XXX btree magic*/

  return 0;
}

//=====================================
static int
spfs_be_id_cmp(spfs_be_id f, spfs_be_id s) {
  spfs_id first;
  spfs_id second;

  first = be32_to_cpu(f);
  second = be32_to_cpu(s);

  return first > second;
}

/* static int
 * spfs_entry_cmp(const struct spfs_bentry *f, const struct spfs_bentry *s) {
 *   BUG_ON(!f);
 *   BUG_ON(!s);
 *
 *   return spfs_be_id_cmp(f->id, s->id);
 * }
 */

static int
spfs_bubble_cmp(const struct btree_bubble *bubble, spfs_be_id o) {
  return spfs_be_id_cmp(bubble->entry.id, o);
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

static int
bnode_parse(struct buffer_head *bh, struct spfs_bnode *result) {
  char *it;
  // TODO alignment of fields

  BUG_ON(!bh);
  it = bh->b_data;

  result->length = it;
  it += sizeof(__be32);

  result->entries = it;
  // TODO inc it
  result->children = it;

  return 0;
}

static int
bnode_make(struct buffer_head *bh, struct spfs_bnode *result) {
  // TODO
  return 0;
}

static int
bnode_make_init(struct buffer_head *bh, struct btree_bubble *bubble,
                struct spfs_bnode *result) {
  BUG_ON(!bh);
  // TODO
  return 0;
}

static u32
bnode_get_length(const struct spfs_bnode *node) {
  __be32 dest;
  void *src = node->length;

  memcpy(&dest, src, sizeof(dest));

  return be32_to_cpu(dest);
}

static void
bnode_set_child(struct spfs_bnode *node, size_t index,
                struct btree_bubble *bubble) {
  spfs_be_sector_t src;
  // XXX size of sector_t = 32?
  void *dest = node->children + (index * sizeof(src));

  src = cpu_to_be32(bubble->greater);

  memcpy(dest, &src, sizeof(spfs_be_sector_t));
}

static sector_t
bnode_get_child(struct spfs_bnode *node, size_t index) {

  void *src;
  spfs_be_sector_t result;

  BUG_ON(!node);
  BUG_ON(index > bnode_get_length(node));

  src = node->children + (index * sizeof(result));

  memcpy(&result, src, sizeof(result));

  // XXX size of sector
  return be32_to_cpu(result);
}

static sector_t
bnode_last_child(struct spfs_bnode *node) {
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
  length = bnode_get_length(node);
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

int
spfs_btree_modify(struct spfs_btree *tree, spfs_id ino, void *closure,
                  btree_modify_cb cb) {
  struct super_block *sb = tree->sb;
  sector_t offset = tree->start;

Lit:
  if (offset) {
    struct buffer_head *bh;
    struct spfs_bentry *gte;
    struct spfs_bnode node = {};

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

      offset = bnode_get_child(&node, index);

      brelse(bh);
      goto Lit;
    } else {

      /* $ino is greater than any other entry in entries */
      offset = bnode_get_child(&node, bnode_get_length(&node));

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
bnode_is_full(struct spfs_btree *self, const struct spfs_bnode *tree) {
  return bnode_get_length(tree) == bnode_capacity(self);
}

static bool
bnode_is_empty(const struct spfs_bnode *tree) {
  return bnode_get_length(tree) == 0;
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
  struct spfs_bentry *gte;

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

  /* 1. Traverse down */
  gte = bnode_bin_find_gte(tree, in->inode.id);
  if (gte) {
    size_t index;
    sector_t child;

    if (bentry_is_eq(gte, in->inode.id)) {
      /* duplicate */
      *out = gte->offset; // TODO byteorder
      return 1;
    }

    /* go down less-than $gte child */
    index = bnode_index_of(tree, gte);
    BUG_ON(index == bnode_capacity(self));

    // TODO sb buffer map child
    child = bnode_get_child(tree, index);
    /*MMAP child*/ {
      struct buffer_head *bh;
      struct spfs_bnode child_node = {};

      bh = sb_bread(sb, child);
      if (!bh) {
        return -EIO;
      }

      if (bnode_parse(bh, &child_node)) {
        // cleanup
        return -EINVAL;
      }

      res = btree_insert(self, &child_node, in, bubble, out);

      // TODO return code to check that bh is dirty
      brelse(bh);
    }
  } else {
    sector_t child;

    BUG_ON(bnode_is_empty(tree));

    /* go down the last (greates) child */
    child = bnode_last_child(tree);

    /*MMAP child*/ {
      struct buffer_head *bh;
      struct spfs_bnode child_node = {};

      bh = sb_bread(sb, child);
      if (!bh) {
        return -EIO;
      }

      if (bnode_parse(bh, &child_node)) {
        // cleanup
        return -EINVAL;
      }

      res = btree_insert(self, &child_node, in, bubble, out);

      // TODO return code to check that bh is dirty
      brelse(bh);
    }
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

static int
btree_fixup(struct spfs_btree *self, struct spfs_bnode *tree,
            struct btree_bubble *bubble) {
  spfs_be_id med;
  sector_t right;
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
  med = bnode_median(tree, &bubble->entry);

  // TODO read buffer
  right = spfs_free_alloc(sb, self->blocks);
  BUG_ON(!right); // XXX

  {
    struct buffer_head *bh;
    struct spfs_bnode right_child = {};

    bh = sb_bread(sb, self->start);
    if (!bh) {
      // cleanup
      return -EIO;
    }

    if (bnode_make(bh, &right_child)) {
      // cleanup
      return -EINVAL;
    }

    bnode_partition(tree, med, &right_child);

    if (med != bubble->entry.id) {
      // XXX: assert $med is last in $tree
      struct spfs_bentry *res;
      if (spfs_bubble_cmp(bubble, /*>*/ med)) {
        res = bnode_bin_insert(&right_child, bubble);
      } else {
        res = bnode_bin_insert(tree, bubble);
      }

      BUG_ON(!res);

      bentry_extract(tree, /*src*/ med, /*dest*/ bubble, &right_child);
    } else {
      /* XXX BUG_ON(right->children[0] != 0); */
      bnode_set_child(&right_child, /*idx*/ 0, bubble);
    }

    // TODO mark dirty
    brelse(bh);
  }

  return 0;
}

static int
bnode_alloc_root(struct spfs_btree *self, struct btree_bubble *bubble) {
  int res;
  sector_t root;
  struct super_block *sb;
  struct buffer_head *bh;
  struct spfs_bnode tree = {};

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

  if (bnode_make_init(bh, bubble, &tree)) {
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
  struct spfs_bnode tree = {};
  struct btree_bubble bubble;
  struct super_block *sb;
  spfs_entry_sector out;

  sb = self->sb;
  /* Result as the location of the inserted entry. */
  out = 0;
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

#include "btree.h"
#include "sp.h"
#include "util.h"

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
/* typedef double spfs_be_sector_t; */
typedef sector_t spfs_be_sector_t;

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

typedef sector_t spfs_inode_sector;

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

  BUG_ON(!sbi);

  tree->sb = sb;
  tree->free_list = &sbi->free_list;
  mutex_init(&tree->lock);

  /* block { */
  tree->block_size = sbi->block_size;
  tree->blocks = 1;
  tree->start = start;
  /* } */

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

static int
spfs_inode_cmp(const struct spfs_bentry *f, spfs_id second) {
  spfs_id first;
  BUG_ON(!f);
  first = be32_to_cpu(f->id);

  return first > second;
}

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
  // XXX
  return 100;
}

static int
bnode_parse(struct spfs_btree *self, struct buffer_head *bh,
            struct spfs_bnode *result) {
  char *it;
  // XXX alignment of fields

  BUG_ON(!bh);
  it = bh->b_data;

  result->length = it;
  it += sizeof(__be32);

  result->entries = it;
  it += (bnode_capacity(self) * sizeof(struct spfs_bentry));

  result->children = it;

  return 0;
}

static int
bnode_make(struct spfs_btree *self, struct buffer_head *bh,
           struct spfs_bnode *result) {
  memset(bh->b_data, 0, bh->b_size);

  return bnode_parse(self, bh, result);
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

static struct spfs_bentry *
bnode_get_entries(struct spfs_bnode *node) {
  return node->entries;
}

static size_t
spfs_ptr_length(const struct spfs_bentry *it, const struct spfs_bentry *end) {
  uintptr_t s;
  uintptr_t n;

  s = (uintptr_t)it;
  n = (uintptr_t)end;

  return n - s;
}

static struct spfs_bentry *
bnode_bin_find_gte(struct spfs_btree *self, struct spfs_bnode *node,
                   spfs_id needle) {
  struct spfs_bentry *it;
  size_t length;
  const struct spfs_bentry *end;

  BUG_ON(!node);
  BUG_ON(!node->entries);
  BUG_ON(!node->length);

  it = node->entries;
  length = bnode_get_length(node);
  BUG_ON(length > bnode_capacity(self));

  end = it + length;

  // TODO should find gte not only e;
  while (it < end) {
    size_t length = spfs_ptr_length(it, end);
    struct spfs_bentry *mid = it + (length / 2);
    if (spfs_inode_cmp(mid, /*>*/ needle)) {
      end = mid;
    } else {
      if (bentry_is_eq(mid, needle)) {
        return mid;
      }

      it = mid;
    }
  }

  return NULL;
}

static struct spfs_bentry *
bnode_bin_insert(struct spfs_bnode *node, struct btree_bubble *bubble) {
  // TODO
  return NULL;
}

static size_t
bnode_index_of(struct spfs_btree *self, struct spfs_bnode *node,
               struct spfs_bentry *needle) {
  uintptr_t s;
  uintptr_t n;
  size_t result;
  struct spfs_bentry *start = bnode_get_entries(node);

  s = (uintptr_t)start;
  n = (uintptr_t)needle;
  if (s > n) {
    return bnode_capacity(self);
  }

  result = n - s;
  if (result > bnode_capacity(self)) {
    return bnode_capacity(self);
  }

  return result;
}

static int
spfs_inode_parse(struct buffer_head *bh, struct spfs_inode *out) {
  // XXX magic
  unsigned int pos = 0;
  if (!spfs_sb_read_u32(bh, &pos, &out->id)) {
    return -EIO;
  }
  if (!spfs_sb_read_u32(bh, &pos, &out->size)) {
    return -EIO;
  }
  if (!spfs_sb_read_u32(bh, &pos, &out->mode)) {
    return -EIO;
  }

  {
    if (!spfs_sb_read_u32(bh, &pos, &out->gid)) {
      return -EIO;
    }

    if (!spfs_sb_read_u32(bh, &pos, &out->uid)) {
      return -EIO;
    }
  }

  {
    if (!spfs_sb_read_u32(bh, &pos, &out->atime)) {
      return -EIO;
    }

    if (!spfs_sb_read_u32(bh, &pos, &out->mtime)) {
      return -EIO;
    }

    if (!spfs_sb_read_u32(bh, &pos, &out->ctime)) {
      return -EIO;
    }
  }

  if (!spfs_sb_read_u32(bh, &pos, &out->start)) {
    return -EIO;
  }

  if (!spfs_sb_read_str(bh, &pos, out->name, sizeof(out->name))) {
    return -EIO;
  }

  return 0;
}

static int
spfs_inode_make(struct buffer_head *bh, const struct spfs_inode *in) {
  // XXX magic
  unsigned int pos = 0;

  if (!spfs_sb_write_u32(bh, &pos, in->id)) {
    return -EIO;
  }
  if (!spfs_sb_write_u32(bh, &pos, in->size)) {
    return -EIO;
  }
  if (!spfs_sb_write_u32(bh, &pos, in->mode)) {
    return -EIO;
  }

  {
    if (!spfs_sb_write_u32(bh, &pos, in->gid)) {
      return -EIO;
    }

    if (!spfs_sb_write_u32(bh, &pos, in->uid)) {
      return -EIO;
    }
  }

  {
    if (!spfs_sb_write_u32(bh, &pos, in->atime)) {
      return -EIO;
    }

    if (!spfs_sb_write_u32(bh, &pos, in->mtime)) {
      return -EIO;
    }

    if (!spfs_sb_write_u32(bh, &pos, in->ctime)) {
      return -EIO;
    }
  }

  if (!spfs_sb_write_u32(bh, &pos, in->start)) {
    return -EIO;
  }

  if (!spfs_sb_write_str(bh, &pos, in->name, sizeof(in->name))) {
    return -EIO;
  }

  return 0;
}

/* ===================================== */
static int
btree_visit_entry(struct super_block *sb, spfs_inode_sector offset,
                  void *closure, btree_modify_cb cb) {
  int res;
  struct buffer_head *bh;
  struct spfs_inode cur;

  if (!offset) {
    return 1;
  }

  bh = sb_bread(sb, offset);

  if (!bh) {
    return 10;
  }

  res = spfs_inode_parse(bh, &cur);
  if (res) {
    goto Lout;
  }

  if (cb(closure, &cur)) {
    spfs_inode_make(bh, &cur);
    mark_buffer_dirty(bh);
  }

Lout:
  brelse(bh);
  return res;
}

int
spfs_btree_modify(struct spfs_btree *self, spfs_id ino, void *closure,
                  btree_modify_cb cb) {
  struct super_block *sb = self->sb;
  sector_t offset = self->start;

Lit:
  if (offset) {
    struct buffer_head *bh;
    struct spfs_bentry *gte;
    struct spfs_bnode node = {};

    bh = sb_bread(sb, offset);
    if (!bh) {
      return -EIO;
    }

    if (bnode_parse(self, bh, &node)) {
      brelse(bh);
      return -EINVAL;
    }

    gte = bnode_bin_find_gte(self, &node, ino);
    if (gte) {
      size_t index;

      if (bentry_is_eq(gte, ino)) {
        /* equal */
        spfs_inode_sector entry_offset = gte->offset;
        brelse(bh); /*invalidatess $gte*/

        return btree_visit_entry(sb, entry_offset, closure, cb);
      }

      /* Go down less than entry child */
      index = bnode_index_of(self, &node, gte);
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
  struct spfs_inode *res = btree_get_node(self, ino);
  if (res) {
    cb(closure, res);
  }

  return res != NULL;
#endif
}

//=====================================
static bool
spfs_btree_lookup_cb(void *closure, struct spfs_inode *result) {
  struct spfs_inode *dest = closure;

  memcpy(dest, result, sizeof(*result));

  /* We do not alter representation. */
  return false;
}

int
spfs_btree_lookup(struct spfs_btree *tree, spfs_id ino,
                  struct spfs_inode *result) {

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

static int
bnode_child_add_back(struct spfs_bnode *node, sector_t child) {
  // XXX alignment
  spfs_be_sector_t src = cpu_to_be32(child);
  spfs_be_sector_t *dest = (spfs_be_sector_t *)node->children;
  dest += bnode_get_length(node);

  memcpy(dest, &src, sizeof(src));

  return 0;
}

static int
bnode_partition(struct spfs_bnode *tree, spfs_id med,
                struct spfs_bnode *right) {
  // TODO
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
             struct spfs_inode *in, struct btree_bubble *bubble,
             spfs_inode_sector *out) {
  int res;
  struct spfs_bentry *gte;

  struct super_block *sb = self->sb;
  memset(bubble, 0, sizeof(*bubble));

  if (!tree) {
    struct buffer_head *bh;
    sector_t page = spfs_free_alloc(self->free_list, self->blocks);
    if (!page) {
      return 1;
    }

    bh = sb_bread(sb, page);
    if (!bh) {
      // cleanup
      return -EIO;
    }

    res = spfs_inode_make(bh, in);
    if (res) {
      // cleanup
      return res;
    }
    *out = page;

    // TODO byte order
    bubble->entry.id = in->id;
    bubble->entry.offset = page;
    bubble->greater = 0;

    brelse(bh);

    /* return std::make_tuple(bubble, greater); */
    return 0;
  }

  /* 1. Traverse down */
  gte = bnode_bin_find_gte(self, tree, in->id);
  if (gte) {
    size_t index;
    sector_t child;

    if (bentry_is_eq(gte, in->id)) {
      /* duplicate */
      *out = gte->offset; // TODO byteorder
      return 1;
    }

    /* go down less-than $gte child */
    index = bnode_index_of(self, tree, gte);
    BUG_ON(index == bnode_capacity(self));

    child = bnode_get_child(tree, index);
    /*MMAP child*/ {
      struct buffer_head *bh;
      struct spfs_bnode child_node = {};

      bh = sb_bread(sb, child);
      if (!bh) {
        return -EIO;
      }

      if (bnode_parse(self, bh, &child_node)) {
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
    child = bnode_get_child(tree, bnode_get_length(tree));

    /*MMAP child*/ {
      struct buffer_head *bh;
      struct spfs_bnode child_node = {};

      bh = sb_bread(sb, child);
      if (!bh) {
        return -EIO;
      }

      if (bnode_parse(self, bh, &child_node)) {
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
  struct super_block *sb;

  BUG_ON(!self);
  BUG_ON(!self->sb);

  sb = self->sb;

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

  right = spfs_free_alloc(self->free_list, self->blocks);
  BUG_ON(!right); // XXX

  {
    struct buffer_head *bh;
    struct spfs_bnode right_child = {};

    bh = sb_bread(sb, self->start);
    if (!bh) {
      // cleanup
      return -EIO;
    }

    if (bnode_make(self, bh, &right_child)) {
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
      BUG_ON(bnode_get_child(&right_child, 0) != 0);
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

  root = spfs_free_alloc(self->free_list, self->blocks);
  if (!root) {
    res = -ENOMEM;
    goto Lret;
  }

  bh = sb_bread(sb, self->start);
  if (!bh) {
    res = -EIO;
    goto Lrelease;
  }

  if (bnode_make(self, bh, &tree)) {
    res = -EINVAL;
    goto Lfree;
  }

  self->start = root;

  {
    int res = bnode_child_add_back(&tree, less);
    BUG_ON(!res);
  }

  {
    struct spfs_bentry *ires = bnode_bin_insert(&tree, bubble);
    BUG_ON(!ires);
  }

  res = 0;
  goto Lrelease;

Lfree:
  spfs_free_dealloc(self->free_list, root, self->blocks);

Lrelease:
  if (bh) {
    brelse(bh);
  }

Lret:
  return res;
}

int
spfs_btree_insert(struct spfs_btree *self, struct spfs_inode *in) {
  int res;
  struct buffer_head *bh;
  struct spfs_bnode tree = {};
  struct btree_bubble bubble;
  struct super_block *sb;
  spfs_inode_sector out;

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

  if (bnode_parse(self, bh, &tree)) {
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
  // XXX
  return 1;
}

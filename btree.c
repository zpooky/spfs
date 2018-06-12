#include "btree.h"
#include "sp.h"

#include <linux/slab.h> /* kzalloc, ... */

/*
 * i_data is "pages read/written by this inode"
 * i_mapping is "whom should I ask for pages?"
 *
 * IOW, everything outside of individual filesystems should use the latter.
 * http://lkml.iu.edu/hypermail/linux/kernel/0105.2/1363.html
 */

//=====================================
int
spfs_btree_init(struct super_block *sb, struct spfs_btree *tree, btree_cmp cmp,
                sector_t start) {
  /* struct address_space *mapping; */
  /* struct page *page; */
  /* unsigned int size; */

  struct spfs_super_block *sbi;

  BUG_ON(!tree);
  BUG_ON(!sb);

  sbi = sb->s_fs_info;

  tree->cmp = cmp;
  mutex_init(&tree->lock);
  tree->block_size = sbi->block_size;
  tree->start = start;
  tree->sb = sb;
  tree->dummy = NULL;

  return 0;
}

//=====================================
/* static  */

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

bool
spfs_btree_lookup(struct spfs_btree *tree, spfs_id ino,
                  struct spfs_entry *result) {
  struct spfs_entry *res = btree_get_node(tree, ino);
  if (res) {
    memcpy(result, res, sizeof(*res));
  }

  return result != NULL;
}

//=====================================
// TODO have resulting dirty bnode stack
bool
spfs_btree_insert(struct spfs_btree *tree, struct spfs_entry *in) {
  struct spfs_entry *node;
  struct spfs_entry *next;
  BUG_ON(!tree);
  BUG_ON(!in);

  BUG_ON(spfs_btree_lookup(tree, in->inode.id, NULL));

  next = tree->dummy;
  node = kzalloc(sizeof(*node), GFP_KERNEL);
  if (node) {
    memcpy(node, in, sizeof(*node));
    tree->dummy = node;
    node->next = next;
    return true;
  }

  return false;
}

//=====================================
bool
spfs_btree_modify(struct spfs_btree *tree, spfs_id ino, void *closure,
                  btree_modify_cb cb) {
  struct spfs_entry *res = btree_get_node(tree, ino);
  if (res) {
    cb(closure, res);
  }

  return res != NULL;
}

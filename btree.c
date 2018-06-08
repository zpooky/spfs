#include "btree.h"

#include <linux/slab.h> /* kzalloc, ... */

extern int
spfs_btree_init(struct spfs_btree *tree, btree_cmp cmp) {
  /* struct address_space *mapping; */
  /* struct page *page; */
  /* unsigned int size; */

  BUG_ON(!tree);

  tree->root = NULL;
  tree->cmp = cmp;
  mutex_init(&tree->tree_lock);

  return 0;
}

struct spfs_entry *
spfs_btree_lookup(struct spfs_btree *tree, unsigned long ino) {
  // TODO
  return NULL;
}

// TODO have resulting dirty bnode stack
struct spfs_entry *
spfs_btree_insert(struct spfs_btree *tree, struct spfs_entry *in) {
  // TODO
  return NULL;
}

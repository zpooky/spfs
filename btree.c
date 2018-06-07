#include "btree.h"

spfs_btree *
spfs_btree_init(btree_cmp cmp) {
  struct spfs_btree *tree;

  struct address_space *mapping;
  struct page *page;
  unsigned int size;

  tree = kzalloc(sizeof(*tree), GFP_KERNEL);
  if (!tree)
    return NULL;

  tree->root = NULL;
  tree->cmp = cmp;
  mutex_init(&tree->tree_lock);

  return tree;
}

spfs_entry *
spfs_btree_lookup(struct spfs_btree *tree, unsigned long ino) {
  // TODO
  return NULL;
}

// TODO have resulting dirty bnode stack
spfs_entry *
spfs_btree_insert(struct spfs_btree *tree, spfs_entry *in) {
  // TODO
  return NULL;
}
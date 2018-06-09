#include "btree.h"

/* #include <linux/slab.h> #<{(| kzalloc, ... |)}># */

/*
 * i_data is "pages read/written by this inode"
 * i_mapping is "whom should I ask for pages?"
 *
 * IOW, everything outside of individual filesystems should use the latter.
 * http://lkml.iu.edu/hypermail/linux/kernel/0105.2/1363.html
 */

extern int
spfs_btree_init(struct spfs_btree *tree, btree_cmp cmp) {
  /* struct address_space *mapping; */
  /* struct page *page; */
  /* unsigned int size; */

  BUG_ON(!tree);

  tree->root = NULL;
  tree->cmp = cmp;
  mutex_init(&tree->lock);

  return 0;
}

struct spfs_entry *
spfs_btree_lookup(struct spfs_btree *tree, unsigned long ino) {
  BUG_ON(!tree);

  // TODO
  return NULL;
}

// TODO have resulting dirty bnode stack
struct spfs_entry *
spfs_btree_insert(struct spfs_btree *tree, struct spfs_entry *in) {
  BUG_ON(!tree);

  // TODO
  return NULL;
}

#include "btree.h"
#include "sp.h"

/* #include <linux/slab.h> #<{(| kzalloc, ... |)}># */

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
                spfs_offset start) {
  /* struct address_space *mapping; */
  /* struct page *page; */
  /* unsigned int size; */

  struct spfs_super_block *sbi;

  BUG_ON(!tree);
  BUG_ON(!sb);

  sbi = sb->s_fs_info;

  /* tree->root = NULL; */
  tree->cmp = cmp;
  tree->block_size = sbi->block_size;
  tree->start = start;
  tree->sb = sb;
  mutex_init(&tree->lock);

  return 0;
}

//=====================================
/* static  */

bool
spfs_btree_lookup(struct spfs_btree *tree, spfs_id ino,
                  struct spfs_entry *result) {
  BUG_ON(!tree);

  // TODO
  return NULL;
}

//=====================================
// TODO have resulting dirty bnode stack
bool
spfs_btree_insert(struct spfs_btree *tree, struct spfs_entry *in) {
  BUG_ON(!tree);

  // TODO
  return true;
}

//=====================================
bool
spfs_btree_modify(struct spfs_btree *tree, spfs_id ino, void *closure,
                  btree_modify_cb cb) {
  // TODO
  return true;
}

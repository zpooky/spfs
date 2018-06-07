#include "spfs.h"

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/slab.h> /* kzalloc, ... */

#include "btree.h"

/*
 * # linux/fs.h
 * - struct super_block
 * - struct file_system_type
 * - struct inode
 *
 * + sb_min_blocksize()
 *
 * # linux/dcache.h
 * struct dentry
 *
 * # linux/buffer_head.h
 * +sb_bread()
 */

//=====================================
static int
spfs_create(struct inode *, struct dentry *, umode_t, bool) {
  return 0;
}

static int
spfs_mkdir(struct inode *, struct dentry *, umode_t) {
  return 0;
}

static struct dentry *
spfs_lookup(struct inode *, struct dentry *, unsigned int) {
  return NULL;
}

static struct inode_operations spfs_inode_ops = {
    /**/
    .create = spfs_create,
    .lookup = spfs_lookup,
    .mkdir = spfs_mkdir
    /**/
};

//=====================================
static ssize_t
spfs_read(struct file *, char __user *, size_t, loff_t *) {
  return 0;
}

static ssize_t
spfs_write(struct file *, const char __user *, size_t, loff_t *) {
  return 0;
}

const struct file_operations spfs_file_ops = {
    /**/
    .read = spfs_read,
    .write = spfs_write
    /**/
};

//=====================================
static int
spfs_entry_cmp(const spfs_entry *, const spfs_entry *) {
  // TODO
  return 0;
}

static int
spfs_init_super_block(struct spfs_super_block *super) {
  struct buffer_head *bh;
  sector_t offset;

  offset = 0;
  bh = sb_bread(info->vfs_sb, offset);
  if (!bh) {
    return -EIO;
  }

  if (bh->b_size < sizeof(*super)) {
    brelse(bh);
    return -EIO;
  }

  memcpy(/*DEST*/ super, /*SRC*/ bh->b_data, sizeof(*super));
  brelse(bh);

  tree->version = be32_to_cpu(head->version);
  tree->magic = be32_to_cpu(head->magic);
  tree->block_size = be32_to_cpu(head->block_size);

  if (tree->magic != SPOOKY_FS_MAGIC) {
    return -ENOMEM;
  }

  if (tree->block_size != SPOOKY_FS_BLOCK_SIZE) {
    return -ENOMEM;
  }

  super->tree = spfs_btree_init(spfs_entry_cmp);
  if (!super->tree) {
    return -ENOMEM;
  }

  return 0;
}

static unsigned char
get_bit_pos(unsigned long val) {
  // TODO document this
  unsigned char i;

  for (i = 0; val; i++) {
    val >>= 1;
  }
  return (i - 1);
}

static int
spfs_fill_super_block(struct super_block *sb, void *data, int silent) {
  struct inode *root_inode;
  struct spfs_super_block *spi;

  sbi = kzalloc(sizeof(struct spfs_super_block), GFP_KERNEL);
  if (!sbi) {
    return -ENOMEM;
  }

  if (!spfs_init_super_block(sbi)) {
    kfree(sbi);
    return -EIO; // TODO lookup error codes
  }

  /* Filesystem private info */
  sp->s_fs_info = sbi;
  /*  */
  sb->s_flags |= MS_NODIRATIME;
  /* TODO document why we do this */
  sb->s_magic = SPOOKY_FS_MAGIC;
  sb->s_blocksize = sbi->block_size;
  sb->s_blocksize_bits = get_bit_pos(sbi->block_size);

  //
  root_inode = new_inode(sb);
  if (!root_inode) {
    // TODO cleanup
    return -ENOMEM;
  }
  root_inode->i_ino = SPFS_ROOT_INODE_NO;
  inode_init_owner(root_inode, NULL, S_IFDIR);
  root_inode->i_sb = sb;
  root_inode->i_op = &spfs_inode_ops;
  root_inode->i_fop = &spfs_file_ops;

  root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime =
      current_time(root_inode);
  /* fs or device private pointer */
  root_inode->i_private = spfs_btree_lookup(sb, root_inode->i_ino);

  sb->s_root = d_make_root(root_inode);
  if (!sb->s_root) {
    // TODO cleanup
    return -ENOMEM;
  }

  return 0;
}

static struct dentry *
spfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name,
           void *data) {
  return mount_bdev(fs_type, flags, dev_name, data, spfs_fill_super_block);
}

static void
spfs_kill_superblock(struct super_block *sb) {
  struct spfs_super_block *spi;
  sbi = sb->s_fs_info;
  if (sbi) {
    kfree(sbi);
    sb->s_fs_info = NULL;
  }
}

struct file_system_type spfs_fs_type = {
    .owner = THIS_MODULE,
    .name = "spfs",
    .mount = spfs_mount,
    .kill_sb = spfs_kill_superblock,
    .fs_flags = FS_REQUIRES_DEV, // TODO ?
};

//=====================================
static int __init
spfs_init(void) {
  printk(KERN_INFO "init spfs\n");

  int ret = register_filesystem(&sp_fs_type);
  if (likely(ret == 0)) {
    printk(KERN_INFO "Sucessfully register_filesystem(simplefs)\n");
  } else {
    printk(KERN_ERR "Failed register_filesystem(simplefs): %d\n", ret);
  }

  return ret;
}

static void __exit
spfs_exit(void) {
  printk(KERN_INFO "exit spfs\n");

  int ret = unregister_filesystem(&sp_fs_type);
  if (likely(ret == 0)) {
    printk(KERN_INFO "Sucessfully unregister_filesystem(simplefs)\n");
  } else {
    printk(KERN_INFO "Faied unregister_filesystem(simplefs): %d\n", ret);
  }
}

module_init(spfs_init);
module_exit(spfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fredrik Olsson")
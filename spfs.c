#include "spfs.h"

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>

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
    .create = spfs_create,
    .lookup = spfs_lookup,
    .mkdir = spfs_mkdir,
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

const struct file_operations spfs_file_operations = {
    .read = spfs_read,
    .write = spfs_write,
};

//=====================================
static struct dentry *
spfs_mount(struct file_system_type *, int, const char *, void *) {
  return NULL;
}

static void
spfs_kill_superblock(struct super_block *sb) {
}

struct file_system_type spfs_fs_type = {
    .owner = THIS_MODULE,
    .name = "spfs",
    .mount = spfs_mount,
    .kill_sb = spfs_kill_superblock,
    .fs_flags = FS_REQUIRES_DEV,
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

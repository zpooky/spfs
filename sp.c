#include "sp.h"

#include <linux/buffer_head.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/slab.h> /* kzalloc, ... */
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
static const struct inode_operations spfs_inode_ops;
static const struct file_operations spfs_file_ops;
static const struct file_operations spfs_dir_ops;

//=====================================
static int
spfs_convert_inode(struct spfs_entry *dest, struct inode *src, umode_t mode) {
  BUG_ON(!dest);
  BUG_ON(!src);

  // TODO copy name
  dest->inode.mode = mode;
  dest->inode.id = src->i_ino;
  if (S_ISDIR(mode)) {
    dest->kind = spfs_entry_kind_dir;
    dest->children = 0;
  } else if (S_ISREG(mode)) {
    dest->kind = spfs_entry_kind_file;
    dest->files = 0;
  } else {
    BUG();
  }

  // TODO stuff
  return 0;
}

static struct inode *
spfs_new_inode(struct super_block *sb, const struct inode *dir, umode_t mode) {
  struct spfs_super_block *sbi;
  struct inode *inode;
  struct spfs_priv_inode *priv_inode;

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  inode = new_inode(sb);
  if (inode) {
    mutex_lock(&sbi->id_lock);
    inode->i_ino = sbi->id++;
    mutex_unlock(&sbi->id_lock);

    inode_init_owner(inode, NULL, mode);

    switch (mode & S_IFMT) {
    case S_IFREG:
      inode->i_op = &spfs_inode_ops;
      inode->i_fop = &spfs_file_ops;
      break;
    case S_IFDIR:
      inode->i_op = &spfs_inode_ops;
      inode->i_fop = &spfs_dir_ops;

      inc_nlink(inode); // TODO
      break;
    default:
      BUG();
    }

    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);

    priv_inode = kzalloc(sizeof(*priv_inode), GFP_KERNEL);
    if (!priv_inode) {
      // TODO gc
      return NULL;
    }
    mutex_init(&priv_inode->lock);
    // TODO add callback when inode gets destroyed to cleanup i_priv;
    inode->i_private = priv_inode;
  }

  return inode;
}

static int
spfs_generic_create(struct inode *parent, struct dentry *den_subject,
                    umode_t mode) {
  struct super_block *sb;
  struct spfs_super_block *sbi;

  struct spfs_entry subject_entry;
  struct spfs_entry *res;

  struct inode *subject;

  BUG_ON(!parent);
  BUG_ON(!den_subject);

  sb = parent->i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  subject = spfs_new_inode(sb, parent, mode);
  if (!subject) {
    return -ENOSPC;
  }

  if (!spfs_convert_inode(/*dest*/ &subject_entry, subject, mode)) {
    return -ENOMEM;
  }

  if (mutex_lock_interruptible(&sbi->tree.lock)) {
    mutex_unlock(&sbi->tree.lock);
    return -EINTR;
  }

  res = spfs_btree_insert(&sbi->tree, &subject_entry);
  mutex_unlock(&sbi->tree.lock);
  if (!res) {
    return -EINTR;
  }

  return 0;
}
/*
 * Create file
 */
static int
spfs_create(struct inode *parent, struct dentry *subject, umode_t mode,
            bool excl) {
  BUG_ON(!S_ISREG(mode));
  return spfs_generic_create(parent, subject, mode); // TODO excl?
}

/*
 * Create directory
 */
static int
spfs_mkdir(struct inode *parent, struct dentry *subject, umode_t mode) {
  int ret;
  BUG_ON(!S_ISDIR(mode));

  ret = spfs_generic_create(parent, subject, mode);
  if (!ret) {
    inc_nlink(parent); // TODO
  }

  return ret;
}

static struct dentry *
spfs_lookup(struct inode *parent, struct dentry *child, unsigned int flags) {
  struct inode *subject;
  struct super_block *sb;
  struct spfs_super_block *sbi;
  struct spfs_entry *res;

  BUG_ON(!parent);
  BUG_ON(!child);

  sb = parent->i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  subject = child->d_inode;
  BUG_ON(!subject);

  if (mutex_lock_interruptible(&sbi->tree.lock)) {
    mutex_unlock(&sbi->tree.lock);
    return NULL;
  }

  res = spfs_btree_lookup(&sbi->tree, subject->i_ino);
  mutex_unlock(&sbi->tree.lock);
  if (!res) {
    return NULL;
  }

  return NULL;
}

static const struct inode_operations spfs_inode_ops = {
    /**/
    .create = spfs_create,
    .lookup = spfs_lookup,
    .mkdir = spfs_mkdir
    /**/
};

//=====================================
#define MIN(f, s) (f) > (s) ? (s) : (f)

static unsigned int
spfs_sb_remaining(struct buffer_head *bh, unsigned int pos) {
  BUG_ON(pos > bh->b_size);
  return bh->b_size - pos;
}

static bool
spfs_sb_read_u32(struct buffer_head *bh, unsigned int *pos, unsigned int *out) {
  if (spfs_sb_remaining(bh, *pos) < sizeof(*out)) {
    return false;
  }

  memcpy(out, bh->b_data + *pos, sizeof(*out));
  *out = be32_to_cpu(*out);

  *pos += sizeof(*out);

  return true;
}

/*
 * Used to retrieve data from the device.
 * A non-negative return value represents the number of bytes successfully read.
 */
static ssize_t
spfs_read(struct file *file, char __user *buf, size_t len, loff_t *ppos) {
  struct inode *inode;
  struct spfs_priv_inode *priv_inode;
  struct super_block *sb;
  struct spfs_super_block *sbi;

  loff_t pos = *ppos;
  ssize_t read = 0;

  if (!ppos) {
    return -EINVAL;
  }

  if (len == 0) {
    return 0;
  }

  BUG_ON(!file);

  inode = file->f_inode;
  BUG_ON(!inode);

  BUG_ON(!S_ISREG(inode->i_mode));

  priv_inode = inode->i_private;
  BUG_ON(!priv_inode);

  sb = inode->i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  {
    spfs_offset start;
    mutex_lock(&priv_inode->lock);
    /* {
     *   mutex_lock(&sbi->tree.lock);
     *   entry = spfs_btree_lookup(&sbi->tree, inode->i_ino);
     *   mutex_unlock(&sbi->tree.lock);
     * }
     */
    start = priv_inode->start;
  Lit:
    if (start) {
      unsigned int con;
      spfs_offset next;
      unsigned int cap;
      unsigned int length;
      struct buffer_head *bh;
      unsigned int bh_pos = 0;

      /*
       * [next:u32,cap:u32,length:u32,raw:cap]
       */
      bh = sb_bread(sb, start);
      if (!spfs_sb_read_u32(bh, &bh_pos, &next)) {
        return -EINVAL;
      }
      if (!spfs_sb_read_u32(bh, &bh_pos, &cap)) {
        return -EINVAL;
      }
      if (!spfs_sb_read_u32(bh, &bh_pos, &length)) {
        return -EINVAL;
      }

      if (length != cap) {
        BUG_ON(next);
      }

      con = MIN(pos, spfs_sb_remaining(bh, bh_pos));
      bh_pos += con;
      pos += con;

      if (bh_pos < bh->b_size) {
        con = MIN(len, spfs_sb_remaining(bh, bh_pos));
        memcpy(/*dest*/ buf, /*src*/ bh->b_data + bh_pos, con);
        bh_pos += con;
        buf += con;
        len -= con;

        read += con;
      }

      brelse(bh);

      if (len > 0) {
        start = next;
        goto Lit;
      }
    }
    mutex_unlock(&priv_inode->lock);

    *ppos += read; // TODO ?
  }

  return read;
}

/*
 * Sends data to the device. If missing, -EINVAL is returned to the program
 * calling the write system call. The return value, if non-negative, represents
 * the number of bytes successfully written.
 */
static ssize_t
spfs_write(struct file *file, const char *buf, size_t len, loff_t *ppos) {
  struct inode *inode;
  struct spfs_priv_inode *priv_inode;
  struct super_block *sb;
  struct spfs_super_block *sbi;

  loff_t pos = *ppos;
  ssize_t written = 0;

  if (!ppos) {
    return -EINVAL;
  }

  if (len == 0) {
    return 0;
  }

  BUG_ON(!file);

  inode = file->f_inode;
  BUG_ON(!inode);

  BUG_ON(!S_ISREG(inode->i_mode));

  priv_inode = inode->i_private;
  BUG_ON(!priv_inode);

  sb = inode->i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  {
    mutex_lock(&priv_inode->lock);
    // TODO

    mutex_unlock(&priv_inode->lock);
  }

  return 0;
}

static const struct file_operations spfs_file_ops = {
    /**/
    .read = spfs_read,
    .write = spfs_write
    /**/
};
//=====================================
static int
spfs_iterate(struct file *file, struct dir_context *ctx) {
  struct super_block *sb;
  struct spfs_super_block *sbi;
  struct inode *inode;
  struct spfs_entry *res;
  struct spfs_priv_inode *priv_inode;

  inode = file_inode(file);
  if (ctx->pos >= inode->i_size) {
    // TODO rentrant pos howdoes it work?
    return 0;
  }

  sb = inode->i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  priv_inode = inode->i_private;
  BUG_ON(!priv_inode);

  {
    unsigned int children_total;
    spfs_offset child_list;

    mutex_lock(&priv_inode->lock);
    {
      mutex_lock(&sbi->tree.lock);
      res = spfs_btree_lookup(&sbi->tree, inode->i_ino);

      if (!res) {
        mutex_unlock(&sbi->tree.lock);
        mutex_unlock(&priv_inode->lock);
        return -ENOMEM; //
      }

      child_list = res->children;

      BUG_ON(!S_ISDIR(inode->i_mode));
      BUG_ON(res->kind != spfs_entry_kind_dir);

      mutex_unlock(&sbi->tree.lock);
    }

    children_total = 0;
  Lit:
    if (child_list) {
      /*
       * [next:u32,children:u32,ino:u32...]:4096
       */
      unsigned int current_inode;
      spfs_offset next;
      unsigned int children;
      unsigned int i;
      struct buffer_head *bh;
      unsigned int bh_pos = 0;

      bh = sb_bread(sb, child_list);
      BUG_ON(!bh);
      if (!spfs_sb_read_u32(bh, &bh_pos, &next)) {
        mutex_unlock(&priv_inode->lock);
        return -ENOMEM; // TODO
      }
      if (!spfs_sb_read_u32(bh, &bh_pos, &children)) {
        mutex_unlock(&priv_inode->lock);
        return -ENOMEM; // TODO
      }
      // TODO assert children < max

      i = 0;
      while (i < children) {
        if (!spfs_sb_read_u32(bh, &bh_pos, &current_inode)) {
          mutex_unlock(&priv_inode->lock);
          return -ENOMEM; // TODO
        }
        if (current_inode) {
          struct spfs_entry *current_res;

          mutex_lock(&sbi->tree.lock);
          current_res = spfs_btree_lookup(&sbi->tree, inode->i_ino);
          mutex_unlock(&sbi->tree.lock);

          if (current_res) {
            dir_emit(ctx, current_res->inode.name,
                     sizeof(current_res->inode.name), current_res->inode.id,
                     DT_UNKNOWN);

            ++ctx->pos;
          }

          ++i;
        }
      }

      brelse(bh);

      child_list = next;
      goto Lit;
    }

    mutex_unlock(&priv_inode->lock);
  }

  return 0;
}

static const struct file_operations spfs_dir_ops = {
    /**/
    .iterate = spfs_iterate,
    /**/
};
//=====================================
static int
spfs_entry_cmp(const struct spfs_entry *f, const struct spfs_entry *s) {
  BUG_ON(!f);
  BUG_ON(!s);

  // TODO
  return 0;
}

static int
spfs_init_super_block(struct super_block *sb, struct spfs_super_block *super) {
  struct buffer_head *bh;
  sector_t offset;

  BUG_ON(!sb);
  BUG_ON(!super);

  offset = 0;
  bh = sb_bread(sb, offset);
  if (!bh) {
    return -EIO;
  }

  if (bh->b_size < sizeof(*super)) {
    brelse(bh);
    return -EIO;
  }

  {
    struct spfs_super_block_wire wire;
    memcpy(/*DEST*/ &wire, /*SRC*/ bh->b_data, sizeof(wire));
    brelse(bh);

    super->version = be32_to_cpu(wire.version);
    super->magic = be32_to_cpu(wire.magic);
    super->block_size = be32_to_cpu(wire.block_size);
    super->id = be32_to_cpu(wire.id);
    mutex_init(&super->id_lock);
  }

  if (super->magic != SPOOKY_FS_MAGIC) {
    return -ENOMEM;
  }

  if (super->block_size != SPOOKY_FS_BLOCK_SIZE) {
    return -ENOMEM;
  }

  if (!spfs_btree_init(&super->tree, spfs_entry_cmp)) {
    return -ENOMEM;
  }

  return 0;
}

/*
 * static unsigned char
 * get_bit_pos(unsigned long val) {
 *   // TODO document this
 *   unsigned char i;
 *
 *   for (i = 0; val; i++) {
 *     val >>= 1;
 *   }
 *   return (i - 1);
 * }
 */

static int
spfs_fill_super_block(struct super_block *sb, void *data, int silent) {
  struct inode *root;
  struct spfs_entry *root_entry;
  struct spfs_super_block *sbi;

  sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
  if (!sbi) {
    return -ENOMEM;
  }

  if (!spfs_init_super_block(sb, sbi)) {
    kfree(sbi);
    return -EIO; // TODO lookup error codes
  }

  /* Filesystem private info */
  sb->s_fs_info = sbi;
  /*  */
  sb->s_flags |= MS_NODIRATIME;
  /* TODO document why we do this */
  sb->s_magic = SPOOKY_FS_MAGIC;
  /* sb->s_blocksize = sbi->block_size; */
  /* sb->s_blocksize_bits = get_bit_pos(sbi->block_size); */

  root_entry = spfs_btree_lookup(&sbi->tree, SPFS_ROOT_INODE_NO); // TODO lock
  if (root_entry) {
    // TODO create default
    root = NULL;
  } else {
    root = spfs_new_inode(sb, NULL, S_IFDIR);
    if (!root) {
      return -ENOMEM;
    }
  }
  sb->s_root = d_make_root(root);

  return 0;
}

static struct dentry *
spfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name,
           void *data) {
  return mount_bdev(fs_type, flags, dev_name, data, spfs_fill_super_block);
}

static void
spfs_kill_superblock(struct super_block *sb) {
  struct spfs_super_block *sbi;
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
  int ret;

  printk(KERN_INFO "init spfs\n");

  ret = register_filesystem(&spfs_fs_type);
  if (likely(ret == 0)) {
    printk(KERN_INFO "Sucessfully register_filesystem(simplefs)\n");
  } else {
    printk(KERN_ERR "Failed register_filesystem(simplefs): %d\n", ret);
  }

  return ret;
}

static void __exit
spfs_exit(void) {
  int ret;

  printk(KERN_INFO "exit spfs\n");

  ret = unregister_filesystem(&spfs_fs_type);
  if (likely(ret == 0)) {
    printk(KERN_INFO "Sucessfully unregister_filesystem(simplefs)\n");
  } else {
    printk(KERN_INFO "Faied unregister_filesystem(simplefs): %d\n", ret);
  }
}

module_init(spfs_init);
module_exit(spfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fredrik Olsson");

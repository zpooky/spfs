#include "sp.h"
#include "util.h"

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
 *
 * #notes
 * static DEFINE_RWLOCK(file_systems_lock);
 * - write_lock(&file_systems_lock);
 * - write_lock(&file_systems_lock);
 * - read_lock(&file_systems_lock);
 * - read_unlock(&file_systems_lock);
 *
 * #
 * The type used for indexing onto a disc or disc partition.
 *
 * Linux always considers sectors to be 512 bytes long independently
 * of the devices real block size.
 *
 * blkcnt_t is the type of the inode's block count.
 * - typedef u64 sector_t;
 * - typedef u64 blkcnt_t;
 */
static const struct inode_operations spfs_inode_ops;
static const struct file_operations spfs_file_ops;
static const struct file_operations spfs_dir_ops;

//=====================================
static int
spfs_convert_from_inode(struct spfs_entry *dest, struct inode *src,
                        const char *name, umode_t mode) {
  size_t n_length = strlen(name);

  BUG_ON(!dest);
  BUG_ON(!src);

  if (n_length > sizeof(dest->inode.name)) {
    return 1;
  }
  strcpy(dest->inode.name, name);

  dest->inode.mode = mode;
  dest->inode.id = src->i_ino;

  if (S_ISDIR(mode)) {
    dest->kind = spfs_entry_kind_dir;
    dest->children_start = 0;
  } else if (S_ISREG(mode)) {
    dest->kind = spfs_entry_kind_file;
    dest->file_start = 0;
  } else {
    BUG();
  }

  dest->next = NULL;

  return 0;
}

static struct spfs_priv_inode *
spfs_priv_inode_init(struct inode *in, spfs_offset start) {
  struct spfs_priv_inode *result;

  result = kzalloc(sizeof(*result), GFP_KERNEL);
  if (!result) {
    return NULL;
  }

  mutex_init(&result->lock);
  result->start = start;

  result->left = NULL;
  result->right = NULL;

  // TODO add callback when inode gets destroyed to cleanup i_private
  in->i_private = result;

  return result;
}

static struct inode *
spfs_convert_to_inode(struct super_block *sb, const struct spfs_entry *src,
                      struct dentry *d) {
  struct inode *inode;
  umode_t mode = src->inode.mode;
  spfs_offset off_start = 0;

  BUG_ON(!sb);

  inode = new_inode(sb);
  if (inode) {
    inode->i_ino = src->inode.id;
    inode->i_op = &spfs_inode_ops;

    if (S_ISDIR(mode)) {
      inode->i_fop = &spfs_dir_ops;
      off_start = src->children_start;

    } else if (S_ISREG(mode)) {
      inode->i_fop = &spfs_file_ops;
      off_start = src->file_start;

    } else {
      printk(KERN_INFO "BUG(), mode[%u], name[%s]", mode, src->inode.name);
      BUG();
    }

    // TODO
    /* inode->i_atime = src->inode.atime; */
    /* inode->i_mtime = src->inode.mtime; */
    /* inode->i_ctime = src->inode.ctime; */
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);

    if (!spfs_priv_inode_init(inode, off_start)) {
      // TODO gc
      return NULL;
    }

    if (d) {
      // TODO
      /* strcpy(d->d_name.name, src->inode.name); */

      d_add(d, inode);
    }
  }

  return inode;
}

static struct inode *
spfs_inode_by_id(struct super_block *sb, struct dentry *d, spfs_id ino) {
  struct spfs_super_block *sbi;
  int res;
  struct spfs_entry entry;

  sbi = sb->s_fs_info;

  BUG_ON(!sbi);

  {
    mutex_lock(&sbi->tree.lock);
    res = spfs_btree_lookup(&sbi->tree, ino, &entry);
    mutex_unlock(&sbi->tree.lock);
  }

  if (res) {
    return NULL;
  }

  return spfs_convert_to_inode(sb, &entry, d);
}

static struct inode *
spfs_new_inode(struct super_block *sb, umode_t mode) {
  struct spfs_super_block *sbi;
  struct inode *inode;

  BUG_ON(!sb);
  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  inode = new_inode(sb);
  if (inode) {
    {
      mutex_lock(&sbi->id_lock);
      inode->i_ino = sbi->id++;
      mutex_unlock(&sbi->id_lock);
    }

    inode_init_owner(inode, NULL, mode);

    switch (mode & S_IFMT) {
    case S_IFREG:
      inode->i_op = &spfs_inode_ops;
      inode->i_fop = &spfs_file_ops;
      break;
    case S_IFDIR:
      inode->i_op = &spfs_inode_ops;
      inode->i_fop = &spfs_dir_ops;

      inc_nlink(inode); // TODO??
      break;
    default:
      BUG();
    }

    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);

    if (!spfs_priv_inode_init(inode, 0)) {
      // TODO gc
      return NULL;
    }
  }

  return inode;
}

static int
spfs_generic_create(struct inode *parent, struct dentry *den_subject,
                    umode_t mode) {
  int res;
  struct super_block *sb;
  struct spfs_super_block *sbi;
  const char *name;

  struct spfs_entry subject_entry;
  struct inode *subject;

  BUG_ON(!parent);
  BUG_ON(!den_subject);

  sb = parent->i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  subject = spfs_new_inode(sb, mode);
  if (!subject) {
    return -ENOSPC;
  }

  name = den_subject->d_name.name;

  /* den_subject->d_inode = subject; // TODO sp?? */
  d_add(den_subject, subject);
  BUG_ON(den_subject->d_inode != subject); // TODO ?

  res = spfs_convert_from_inode(/*dest*/ &subject_entry, subject, name, mode);
  if (!res) {
    if (mutex_lock_interruptible(&sbi->tree.lock)) {
      // TODO cleanup
      return -EINTR;
    }

    res = spfs_btree_insert(&sbi->tree, &subject_entry);
    mutex_unlock(&sbi->tree.lock);
  }

  if (res) {
    // TODO cleanup subject
  }

  return res;
}

//=====================================
/*
 * Create file
 */
static int
spfs_create(struct inode *parent, struct dentry *subject, umode_t mode,
            bool excl) {
  BUG_ON(!S_ISREG(mode));
  return spfs_generic_create(parent, subject, mode); // TODO excl?
}

//=====================================
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

//=====================================
typedef bool (*for_all_cb)(void *, struct spfs_entry *);

static bool
spfs_for_all_children(struct inode *parent, void *closure, for_all_cb f) {
  struct super_block *sb;
  struct spfs_super_block *sbi;
  struct spfs_priv_inode *priv_inode;

  spfs_offset child_list = 0;

  sb = parent->i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  priv_inode = parent->i_private;
  BUG_ON(!priv_inode);

  BUG_ON(!S_ISDIR(parent->i_mode));

  mutex_lock(&priv_inode->lock);
  {
    int res;
    struct spfs_entry entry;

    {
      mutex_lock(&sbi->tree.lock);
      res = spfs_btree_lookup(&sbi->tree, parent->i_ino, &entry);
      mutex_unlock(&sbi->tree.lock);
    }

    if (res) {
      // TODO cleanup
      return false;
    }

    child_list = entry.children_start;

    BUG_ON(entry.kind != spfs_entry_kind_dir);
  }

Lit:
  if (child_list) {
    /*
     * [next:u32,children:u32,ino:u32...]:4096
     */
    spfs_id current_inode;
    spfs_offset next;
    unsigned int children;
    unsigned int i;
    unsigned int bh_pos = 0;

    struct buffer_head *bh;
    bh = sb_bread(sb, child_list);
    BUG_ON(!bh);

    if (!spfs_sb_read_u32(bh, &bh_pos, &next)) {
      mutex_unlock(&priv_inode->lock);
      BUG();
      return false;
    }

    if (!spfs_sb_read_u32(bh, &bh_pos, &children)) {
      mutex_unlock(&priv_inode->lock);
      BUG();
      return false;
    }
    // TODO assert children < max

    i = 0;
    while (i < children) {
      if (!spfs_sb_read_u32(bh, &bh_pos, &current_inode)) {
        mutex_unlock(&priv_inode->lock);
        BUG();
        return false;
      }

      if (current_inode) {
        struct spfs_entry cur_entry;
        int lres;

        {
          mutex_lock(&sbi->tree.lock);
          lres = spfs_btree_lookup(&sbi->tree, current_inode, &cur_entry);
          mutex_unlock(&sbi->tree.lock);
        }

        if (!lres) {
          if (!f(closure, &cur_entry)) {
            mutex_unlock(&priv_inode->lock);
            return false;
          }
        }

        ++i;
      }
    }

    brelse(bh);

    child_list = next;
    goto Lit;
  }

  mutex_unlock(&priv_inode->lock);

  return true;
}

struct spfs_find_iname_name_data {
  struct super_block *sb;
  struct dentry *dentry;
  const char *name;

  struct inode *result;
};

static bool
spfs_inode_by_name_cb(void *closure, struct spfs_entry *cur) {
  struct spfs_find_iname_name_data *data = closure;

  if (strcmp(cur->inode.name, data->name) == 0) {
    /* data->result = TODO*/
    data->result = spfs_convert_to_inode(data->sb, cur, data->dentry);
    return false;
  }

  return true;
}

static struct inode *
spfs_inode_by_name(struct super_block *sb, struct inode *parent,
                   struct dentry *child) {
  struct spfs_super_block *sbi;
  const char *name;
  struct spfs_priv_inode *priv_inode;
  BUG_ON(!sb);
  BUG_ON(!child);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  name = child->d_name.name;
  BUG_ON(!name);
  priv_inode = parent->i_private;
  BUG_ON(!priv_inode);

  {
    struct spfs_find_iname_name_data data = {
        .sb = sb,
        .dentry = child,
        .name = name,

        .result = NULL,
    };

    if (mutex_lock_interruptible(&priv_inode->lock)) {
      // TODO return error(pointer error?)
      return NULL;
    }

    spfs_for_all_children(parent, &data, &spfs_inode_by_name_cb);

    mutex_unlock(&priv_inode->lock);

    return data.result;
  }
}

/*
 * Given parent inode & child filename lookup child inode & add it to child
 * dentry.
 */
static struct dentry *
spfs_lookup(struct inode *parent, struct dentry *child, unsigned int flags) {
  /* struct inode *subject; */
  struct super_block *sb;
  struct inode *result;

  BUG_ON(!parent);
  BUG_ON(!child);

  sb = parent->i_sb;
  BUG_ON(!sb);

  result = spfs_inode_by_name(sb, parent, child);
  if (result) {
    /* inode_init_owner(inode, parent, ); */
    d_add(child, result);
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
    start = priv_inode->start;
  Lit:
    if (start) {
      unsigned int con;
      spfs_offset next;
      unsigned int capacity;
      unsigned int length;
      struct buffer_head *bh;
      unsigned int bh_pos = 0;

      /* [next:u32,cap:u32,length:u32,raw:cap] */
      bh = sb_bread(sb, start);
      if (!spfs_sb_read_u32(bh, &bh_pos, &next)) {
        return -EINVAL;
      }
      if (!spfs_sb_read_u32(bh, &bh_pos, &capacity)) {
        return -EINVAL;
      }
      if (!spfs_sb_read_u32(bh, &bh_pos, &length)) {
        return -EINVAL;
      }

      if (length != capacity) {
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

//=====================================
static bool
spfs_modify_start_cb(void *closure, struct spfs_entry *entry) {
  spfs_offset start = *((spfs_offset *)closure);
  BUG_ON(entry->kind != spfs_entry_kind_file);
  BUG_ON(entry->file_start != 0);

  entry->file_start = start;

  return true;
}

/*
 * Sends data to the device. If missing, -EINVAL is returned to the program
 * calling the write system call. The return value, if non-negative, represents
 * the number of bytes successfully written.

 * On a regular file, if this incremented file offset is greater than the length
 * of the file, the length of the file shall be set to this file offset.
 */
static ssize_t
spfs_write(struct file *file, const char __user *buf, size_t len,
           loff_t *ppos) {
  struct inode *inode;
  struct spfs_priv_inode *priv_inode;
  struct super_block *sb;
  struct spfs_super_block *sbi;
  unsigned long file_length = 0;

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
    spfs_offset start;

    mutex_lock(&priv_inode->lock);
    start = priv_inode->start;
  /*
   * [next:u32,cap:u32,length:u32,raw:cap]
   */
  Lit:
    if (start) {
      spfs_offset block_next;
      spfs_offset block_next_idx;
      unsigned int block_capacity;
      unsigned int block_length;
      spfs_offset block_length_idx;

      bool block_dirty = false;

      struct buffer_head *bh;
      unsigned int bh_pos = 0;
      unsigned int con;

      bh = sb_bread(sb, start);
      BUG_ON(!bh);
      block_next_idx = bh_pos;
      if (!spfs_sb_read_u32(bh, &bh_pos, &block_next)) {
        // TODO release
        return -EINVAL;
      }
      if (!spfs_sb_read_u32(bh, &bh_pos, &block_capacity)) {
        // TODO release
        return -EINVAL;
      }
      block_length_idx = bh_pos;
      if (!spfs_sb_read_u32(bh, &bh_pos, &block_length)) {
        // TODO release
        return -EINVAL;
      }

      if (block_length != block_capacity) {
        BUG_ON(block_next);
      }

      con = MIN(pos, spfs_sb_remaining(bh, bh_pos));
      pos -= con;
      bh_pos += con;
      if (pos == 0) {
        con = MIN(len, spfs_sb_remaining(bh, bh_pos));

        if (con > 0) {
          memcpy(/*dest*/ bh->b_data + bh_pos, /*src*/ buf, con);
          buf += con;
          len -= con;
          bh_pos += con;
          block_length += con;

          if (!spfs_sb_write_u32(bh, &bh_pos, block_length)) {
            // TODO release
            return -EINVAL;
          }

          block_dirty = true;
        }
      }
      file_length += block_length;

      if (len > 0) {
        BUG_ON(block_length != block_capacity);

        if (block_next == 0) {
          *ppos = file_length;
          pos = 0;

          block_next = spfs_free_alloc(sb, len);
          if (!block_next) {
            // TODO release
            return -EINVAL;
          }

          if (!spfs_sb_write_u32(bh, &block_next_idx, block_next)) {
            // TODO release
            return -EINVAL;
          }
          block_dirty = true;
        }
      }

      if (block_dirty) {
        mark_buffer_dirty(bh);
        sync_dirty_buffer(bh);
      }
      brelse(bh);

      start = block_next;
      goto Lit;
    } else {

      start = spfs_free_alloc(sb, len);
      if (!start) {
        // TODO release
        return -EINVAL;
      }

      {
        int lres;
        mutex_lock(&sbi->tree.lock);

        lres = spfs_btree_modify(&sbi->tree, inode->i_ino, &start,
                                 spfs_modify_start_cb);
        if (lres) {
          // TODO release
          return 0;
        }

        mutex_unlock(&sbi->tree.lock);
      }

      priv_inode->start = start;
      goto Lit;
    }

    mutex_unlock(&priv_inode->lock);
    *ppos += written;
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
static bool
spfs_iterate_cb(void *closure, struct spfs_entry *cur) {
  struct dir_context *ctx = closure;
  const char *name = cur->inode.name;

  size_t nlength = sizeof(cur->inode.name);
  dir_emit(ctx, name, nlength, cur->inode.id, DT_UNKNOWN);
  ++ctx->pos;

  return true;
}

static int
spfs_iterate(struct file *file, struct dir_context *ctx) {
  struct inode *parent;

  parent = file_inode(file);
  if (ctx->pos >= parent->i_size) {
    // TODO rentrant pos howdoes it work?
    return 0;
  }

  spfs_for_all_children(parent, ctx, spfs_iterate_cb);
  return 0;
}

static const struct file_operations spfs_dir_ops = {
    /**/
    .iterate = spfs_iterate,
    /**/
};
//=====================================
static struct spfs_free_node *
spfs_init_free_list_entry(struct buffer_head *bh, unsigned int *bh_pos) {
  struct spfs_free_node *result;

  spfs_offset entry_start;
  unsigned int entry_blocks;

  if (!spfs_sb_read_u32(bh, bh_pos, &entry_start)) {
    return NULL;
  }
  if (!spfs_sb_read_u32(bh, bh_pos, &entry_blocks)) {
    return NULL;
  }

  result = kzalloc(sizeof(*result), GFP_KERNEL);
  if (!result) {
    return NULL;
  }

  result->next = NULL;
  result->start = entry_start;
  result->blocks = entry_blocks;

  return result;
}

static int
spfs_init_super_block(struct super_block *sb, struct spfs_super_block *super,
                      sector_t start) {
  struct buffer_head *bh;

  BUG_ON(!sb);
  BUG_ON(!super);

  bh = sb_bread(sb, start);
  if (!bh) {
    printk(KERN_INFO "bh = sb_bread(sb, start[%zu])\n", start);
    return -EIO;
  }

  if (bh->b_size < sizeof(*super)) {
    printk(KERN_INFO "bh->b_size:%zu < sizeof(*super):%zu\n", //
           bh->b_size, sizeof(*super));
    brelse(bh);
    return -EIO;
  }

  {
    unsigned int pos = 0;
    mutex_init(&super->id_lock);
    /* struct spfs_super_block_wire wire; */
    /* memcpy(#<{(|DEST|)}># &wire, #<{(|SRC|)}># bh->b_data, sizeof(wire)); */

    /* super->version = be32_to_cpu(wire.version); */
    /* super->magic = be32_to_cpu(wire.magic); */
    /* super->block_size = be32_to_cpu(wire.block_size); */
    /* super->id = be32_to_cpu(wire.id); */
    /* super->root_id = be32_to_cpu(wire.root_id); */

    if (spfs_sb_read_u32(bh, &pos, &super->version)) {
      return 1;
    }
    if (spfs_sb_read_u32(bh, &pos, &super->magic)) {
      return 1;
    }
    if (spfs_sb_read_u32(bh, &pos, &super->block_size)) {
      return 1;
    }
    if (spfs_sb_read_u32(bh, &pos, &super->id)) {
      return 1;
    }
    if (spfs_sb_read_u32(bh, &pos, &super->root_id)) {
      return 1;
    }
  }
  brelse(bh);

  if (super->magic != SPOOKY_FS_MAGIC) {
    printk(KERN_INFO "super->magic[%u] != SPOOKY_FS_MAGIC[%u]\n", //
           super->magic, SPOOKY_FS_MAGIC);
    return -ENOMEM;
  }

  if (super->block_size != SPOOKY_FS_BLOCK_SIZE) {
    printk(KERN_INFO "super->block_size:%u != SPOOKY_FS_BLOCK_SIZE:%u\n", //
           super->block_size, SPOOKY_FS_BLOCK_SIZE);
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
  const spfs_offset super_start = 0;
  const spfs_offset btree_start = super_start + 1;
  const spfs_offset free_start = btree_start + 1;

  struct inode *root;
  struct spfs_super_block *sbi;
  int res;

  sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
  if (!sbi) {
    return -ENOMEM;
  }

  /* Filesystem private info */
  sb->s_fs_info = sbi;
  /*  */
  sb->s_flags |= MS_NODIRATIME;
  /* TODO document why we do this */
  sb->s_magic = SPOOKY_FS_MAGIC;
  /* sb->s_blocksize_bits = get_bit_pos(sbi->block_size); */
  sb->s_blocksize = SPOOKY_FS_BLOCK_SIZE;

  res = spfs_init_super_block(sb, sbi, super_start);
  if (res) {
    goto Lerr;
  }

  // TODO sector_t index need to be consistent
  sb->s_blocksize = sbi->block_size;

  res = spfs_btree_init(sb, &sbi->tree, btree_start);
  if (res) {
    goto Lerr;
  }

  res = spfs_init_free_list(sb, &sbi->free_list, free_start);
  if (res) {
    goto Lerr;
  }

  root = spfs_inode_by_id(sb, NULL, sbi->root_id);
  if (!root) {
    root = spfs_new_inode(sb, S_IFDIR);
    if (!root) {
      res = -ENOMEM;
      goto Lerr;
    }
    sbi->root_id = root->i_ino;
  }
  sb->s_root = d_make_root(root);

  return 0;
Lerr:
  kfree(sb->s_fs_info);
  sb->s_fs_info = NULL;

  return res;
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
    .name = "spfs",
    .fs_flags = FS_REQUIRES_DEV, // TODO ?
    .mount = spfs_mount,
    .kill_sb = spfs_kill_superblock,
    .owner = THIS_MODULE,
};

//=====================================
static int __init
spfs_init(void) {
  int ret;

  printk(KERN_INFO "init spfs\n");

  /*
   *	Adds the file system passed to the list of file systems the kernel
   *	is aware of for mount and other syscalls. Returns 0 on success,
   *	or a negative errno code on an error.
   *
   *	The &struct file_system_type that is passed is linked into the kernel
   *	structures and must not be freed until the file system has been
   *	unregistered.
   */
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

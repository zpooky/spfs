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
 *
 * #
 * if (IS_ERR(ptr))
 *   return PTR_ERR(ptr);
 *
 * #
 * unsigned long i_ino;
 */

/*
 * # Super block
 * - Located at sector_t: 0
 * - Occupies the complete first sector
 * - Always contains max 512Kb useable data, independent of block size
 *
 * # Free List
 * free_entry[start: sector_t, blocks: u32]
 * free_list[length: u32, next_free_list: offset, free_entry:[length]]
 */

#define SPFS_INODE(inode) ((spfs_inode *)inode)

static const struct inode_operations spfs_inode_ops;
static const struct file_operations spfs_file_ops;
static const struct file_operations spfs_dir_ops;
static const struct super_operations spfs_super_ops;

/*
 * TODO KM unload write back
 * TODO use sector_t instead of spfs_offset?
 *
 * XXX optimize sb_bread without reading for block device when we are not
 * interested in the content of the read, we only want to overwrite all content
 */

//=====================================
static int
spfs_convert_from_inode(struct spfs_inode *dest, struct inode *src,
                        const char *name, umode_t mode) {
  size_t n_length = strlen(name);

  BUG_ON(!dest);
  BUG_ON(!src);

  // TODO all fields

  if (n_length > sizeof(dest->name)) {
    return 1;
  }
  strcpy(dest->name, name);

  dest->mode = mode;
  dest->id = src->i_ino;

  if (S_ISDIR(mode)) {
    dest->start = 0;
  } else if (S_ISREG(mode)) {
    dest->start = 0;
  } else {
    BUG();
  }

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

  // TODO add callback when inode gets destroyed to clean-up i_private
  in->i_private = result;

  return result;
}

static struct inode *
spfs_convert_to_inode(struct super_block *sb, const struct spfs_inode *src,
                      struct dentry *d) {
  struct inode *inode;
  umode_t mode = src->mode;
  spfs_offset off_start = 0;

  // TODO all fields
  BUG_ON(!sb);

  inode = new_inode(sb);
  if (inode) {
    inode->i_ino = src->id;
    inode->i_op = &spfs_inode_ops;

    off_start = src->start;

    if (S_ISDIR(mode)) {
      inode->i_fop = &spfs_dir_ops;
    } else if (S_ISREG(mode)) {
      inode->i_fop = &spfs_file_ops;
    } else {
      printk(KERN_INFO "BUG(), mode[%u], name[%s]", mode, src->name);
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
spfs_inode_by_id(struct super_block *sb, struct dentry *d, spfs_ino ino) {
  struct spfs_super_block *sbi;
  int res;
  struct spfs_inode entry;

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

  struct spfs_inode subject_entry;
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
typedef bool (*for_all_cb)(void *, struct spfs_inode *);

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
    struct spfs_inode entry;

    {
      mutex_lock(&sbi->tree.lock);
      res = spfs_btree_lookup(&sbi->tree, parent->i_ino, &entry);
      mutex_unlock(&sbi->tree.lock);
    }

    if (res) {
      // cleanup
      return false;
    }

    child_list = entry.start;

    BUG_ON(!S_ISDIR(entry.mode));
  }

Lit:
  if (child_list) {
    /*
     * [next:u32,children:u32,ino:u32...]:4096
     */
    spfs_ino current_inode;
    spfs_offset next;
    unsigned int children;
    unsigned int i;
    size_t bh_pos = 0;

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
        struct spfs_inode cur_entry;
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
spfs_inode_by_name_cb(void *closure, struct spfs_inode *cur) {
  struct spfs_find_iname_name_data *data = closure;

  if (strcmp(cur->name, data->name) == 0) {
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
    /* Perform a lookup of an inode given parent directory and filename. */
    .lookup = spfs_lookup,
    .mkdir = spfs_mkdir
    /**/
};

//=====================================
#define MIN(f, s) (f) > (s) ? (s) : (f)

struct spfs_file_block {
  /* block id of next spfs_file_block */
  spfs_offset next;
  /* capacity in number of blocks */
  u32 capacity;
  /* length in number of bytes */
  u32 length;
};

static int
spfs_parse_file_block(struct buffer_head *bh, size_t *pos,
                      struct spfs_file_block *result) {
  // XXX magic
  /* [next:sector_t, cap:u32, length:u32, raw:cap] */
  if (!spfs_sb_read_u64(bh, &pos, &result->next)) {
    return -EINVAL;
  }
  if (!spfs_sb_read_u32(bh, &pos, &result->capacity)) {
    return -EINVAL;
  }
  if (!spfs_sb_read_u32(bh, &pos, &result->length)) {
    return -EINVAL;
  }

  return 0;
}

static int
spfs_make_file_block(struct buffer_head *bh, size_t *pos,
                     const struct spfs_file_block *in) {
  if (!spfs_sb_write_u64(bh, &b_pos, in->next)) {
    return 1;
  }
  if (!spfs_sb_write_u32(bh, &b_pos, in->capacity)) {
    return 1;
  }
  if (!spfs_sb_write_u32(bh, &b_pos, in->length)) {
    return 1;
  }

  return 0;
}

/*
 * Used to retrieve data from the device.
 * A non-negative return value represents the number of bytes successfully read.
 */
static ssize_t
spfs_read(struct file *file, char __user *buf, size_t len, loff_t *ppos) {

  int res;
  struct inode *inode;
  struct spfs_priv_inode *priv_inode;
  struct super_block *sb;
  struct spfs_super_block *sbi;

  loff_t pos;
  ssize_t read;

  BUG_ON(!ppos);

  pos = *ppos;
  read = 0;

  printk(KERN_INFO "spfs_read()\n");

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
      struct spfs_file_block file_block;
      struct buffer_head *bh;
      size_t bh_pos = 0;

      bh = sb_bread(sb, start);
      if (!bh) {
        // cleanup
        return -EIO;
      }

      res = spfs_parse_file_block(bh, &bh_pos, &file_block);
      if (res) {
        // cleanup
        return res;
      }

      /*
       * if (length != capacity) {
       *   BUG_ON(next);
       * }
       */

      con = MIN(pos, spfs_sb_remaining(bh, bh_pos));
      bh_pos += con;
      pos += con;

      if (bh_pos < bh->b_size) {
        con = MIN(len, spfs_sb_remaining(bh, bh_pos));
        copy_to_user(/*dest*/ buf, /*src*/ bh->b_data + bh_pos, con);
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
spfs_modify_start_cb(void *closure, struct spfs_inode *entry) {
  spfs_offset start = *((spfs_offset *)closure);

  BUG_ON(!S_ISREG(entry->mode));
  BUG_ON(entry->start != 0);

  entry->start = start;

  return true;
}

static size_t
spfs_blocks_for(const struct spfs_super_block *sbi, size_t len) {
  size_t result = 0;
  while (len) {
    result++;
    len -= MIN(len, sbi->block_size);
  }

  return result;
}

static sector_t
spfs_file_block_alloc(struct super_block *sb, size_t len) {
  int res;
  struct spfs_super_block *sbi;
  size_t blocks;
  sector_t result;

  if (len == 0) {
    return 0;
  }

  sbi = sb->s_fs_info;

  BUG_ON(!sbi);

  blocks = spfs_blocks_for(sbi, len);
  result = spfs_free_alloc(&sbi->free_list, blocks);

  if (result) {
    size_t b_pos = 0;
    struct buffer_head *bh;

    bh = sb_bread(sb, result);
    BUG_ON(!bh);

    struct spfs_file_block desc = {
        /**/
        .next = 0,
        .capacity = blocks,
        .length = 0,
        /**/
    };

    res = spfs_make_file_block(bh, &b_pos, &desc);
    if (res) {
      // cleanup
      return res;
    }

    mark_buffer_dirty(bh); // XXX maybe sync?
    brelse(bh);
  }

  return result;
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

  printk(KERN_INFO "spfs_write()\n");

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
      struct spfs_file_block block;
      bool block_dirty = false;

      struct buffer_head *bh;
      size_t bh_pos = 0;
      unsigned int con;

      bh = sb_bread(sb, start);
      if (!bh) {
        // cleanup
        return -EIO;
      }

      res = spfs_parse_file_block(bh, &bh_pos, &block);
      if (res) {
        // cleanup
        return res;
      }

      if (block.length != block.capacity) {
        BUG_ON(block.next);
      }

      con = MIN(pos, spfs_sb_remaining(bh, bh_pos));
      pos -= con;
      bh_pos += con;
      if (pos == 0) {
        con = MIN(len, spfs_sb_remaining(bh, bh_pos));

        if (con > 0) {
          size_t ipos;
          copy_from_user(/*dest*/ bh->b_data + bh_pos, /*src*/ buf, con);
          buf += con;
          len -= con;
          bh_pos += con;
          block.length += con;

          /* Write back block.length */
          ipos = 0;
          res = spfs_make_file_block(bh, &ipos, &block);
          if (res) {
            // cleanup
            return res;
          }

          block_dirty = true;
        }
      }
      file_length += block.length;

      if (len > 0) {
        BUG_ON(block.length != block.capacity);

        if (block.next == 0) {
          size_t ipos;
          *ppos = file_length;
          pos = 0;

          /*allocate block.next to make sapce for $buf */
          block.next = spfs_file_block_alloc(sb, len);
          if (!block.next) {
            // cleanup
            return -EINVAL;
          }

          /* Write back block.next */
          ipos = 0;
          res = spfs_make_file_block(bh, &ipos, &block);
          if (res) {
            // cleanup
            return res;
          }
          block_dirty = true;
        }
      }

      if (block_dirty) {
        mark_buffer_dirty(bh);
        /* sync_dirty_buffer(bh); ??*/
      }
      brelse(bh);

      start = block.next;
      goto Lit;
    } else {
      /*
       * We only get here we the initial spfs_inode.start pointer is 0
       */
      start = spfs_file_block_alloc(sb, len);
      if (!start) {
        // cleanup
        return -EINVAL;
      }

      mutex_lock(&sbi->tree.lock);

      res = spfs_btree_modify(&sbi->tree, inode->i_ino, &start,
                              spfs_modify_start_cb);
      mutex_unlock(&sbi->tree.lock);
      if (res) {
        // cleanup
        return res;
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
spfs_iterate_cb(void *closure, struct spfs_inode *cur) {
  struct dir_context *ctx = closure;
  const char *name = cur->name;

  size_t nlength = sizeof(name);
  dir_emit(ctx, name, nlength, cur->id, DT_UNKNOWN);
  ++ctx->pos;

  return true;
}

static int
spfs_iterate(struct file *file, struct dir_context *ctx) {

  struct inode *parent;

  printk(KERN_INFO "spfs_iterate()\n");

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
static int
spfs_read_super_block(struct buffer_head *bh, struct spfs_super_block *super) {
  int res;
  size_t pos;
  unsigned int dummy;

  res = -EINVAL;
  pos = 0;
  if (spfs_sb_read_u32(bh, &pos, &super->magic)) {
    goto Lout;
  }
  if (spfs_sb_read_u32(bh, &pos, &super->version)) {
    goto Lout;
  }
  if (spfs_sb_read_u32(bh, &pos, &super->block_size)) {
    goto Lout;
  }
  if (spfs_sb_read_u32(bh, &pos, &super->dummy)) {
    goto Lout;
  }

  if (spfs_sb_read_u64(bh, &pos, &super->id)) {
    goto Lout;
  }
  if (spfs_sb_read_u64(bh, &pos, &super->root_id)) {
    goto Lout;
  }

  if (spfs_sb_read_sector_t(bh, &pos, &super->btree_offset)) {
    goto Lout;
  }
  if (spfs_sb_read_sector_t(bh, &pos, &super->free_list_offset)) {
    goto Lout;
  }

  res = 0;
Lout:
  return res;
}

static int
spfs_super_block_init(struct super_block *sb, struct spfs_super_block *super,
                      sector_t start) {
  int res;
  struct buffer_head *bh;

  BUG_ON(!sb);
  BUG_ON(!super);

  bh = sb_bread(sb, start);
  if (!bh) {
    printk(KERN_INFO "bh = sb_bread(sb, start[%zu])\n", start);
    return -EIO;
  }

  mutex_init(&super->id_lock);

  res = spfs_read_super_block(bh, super);
  if (res) {
    goto Lrelease;
  }

  if (super->magic != SPOOKY_FS_SUPER_MAGIC) {
    printk(KERN_INFO "super->magic[%u] != SPOOKY_FS_SUPER_MAGIC[%u]\n", //
           super->magic, SPOOKY_FS_SUPER_MAGIC);
    res = -EINVAL;
    goto Lrelease;
  }

  if (super->block_size < SPOOKY_FS_INITIAL_BLOCK_SIZE) {
    res = -EINVAL;
    goto Lrelease;
  }

  res = 0;
Lrelease:
  brelse(bh);
  return res;
}

static int
spfs_fill_super_block(struct super_block *sb, void *data, int silent) {
  struct inode *root;
  struct spfs_super_block *sbi;
  int res;

  const spfs_offset super_start = 0;
  printk(KERN_INFO "spfs_kill_super_block()\n");

  sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
  if (!sbi) {
    return -ENOMEM;
  }

  /* Filesystem private info */
  sb->s_fs_info = sbi;
  /*  */
  /* sb->s_flags |= MS_NODIRATIME; */
  /* TODO document why we do this */
  sb->s_magic = SPOOKY_FS_MAGIC;
  sb->s_op = &spfs_super_ops;
  if (sb_set_blocksize(sb, SPOOKY_FS_INITIAL_BLOCK_SIZE) == 0) {
    res = -EINVAL;
    goto Lerr;
  }

  res = spfs_super_block_init(sb, sbi, super_start);
  if (res) {
    goto Lerr;
  }

  if (sb_set_blocksize(sb, sbi->block_size) == 0) {
    res = -EINVAL;
    goto Lerr;
  }

  res = spfs_btree_init(sb, &sbi->tree, sbi->btree_offset);
  if (res) {
    goto Lerr;
  }

  res = spfs_free_init(sb, &sbi->free_list, sbi->free_list_offset);
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
  /**/
  /* inode_init_owner(root, NULL, S_IFDIR); */
  // doc
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
  printk(KERN_INFO "spfs_mount()\n");
  return mount_bdev(fs_type, flags, dev_name, data, spfs_fill_super_block);
}

//=====================================
static void
spfs_put_super(struct super_block *sb) {
  /*
   * XXX the same prototype for kill_sb & put_super
   * when to use which?
   *
   * https://patchwork.kernel.org/patch/9654957/
   */
  struct spfs_super_block *sbi;

  printk(KERN_INFO "spfs_kill_superblock()\n");

  sbi = sb->s_fs_info;
  if (sbi) {
    kfree(sbi);
    sb->s_fs_info = NULL;
  }
}

//=====================================
static struct kmem_cache *spfs_inode_SLAB;

//=====================================
static struct inode *
spfs_alloc_inode(struct super_block *sb) {
  struct spfs_inode *inode;

  inode = kmem_cache_alloc(spfs_inode_SLAB, GFP_KERNEL);

  if (inode) {
    return &inode->i_inode;
  }

  return NULL;
}

//=====================================
static void
spfs_free_inode_cb(struct rcu_head *head) {
  struct inode *inode;

  // doc
  inode = container_of(head, struct inode, i_rcu);

  BUG_ON(!inode);
  BUG_ON(!spfs_inode_SLAB);

  pr_debug("freeing inode %lu\n", (unsigned long)inode->i_ino);
  kmem_cache_free(spfs_inode_SLAB, SPFS_INODE(inode));
}

static void
spfs_free_inode(struct inode *inode) {
  call_rcu(&inode->i_rcu, spfs_free_inode_cb);
}

//=====================================
static void
spfs_inode_init_once(void *closure) {
  struct spfs_inode *inode = closure;

  // doc
  inode_init_once(&inode->i_inode);
}

static struct kmem_cache *
spfs_create_inode_SLAB() {
  // doc
  return kmem_cache_create("spfs_inode_SLAB", sizeof(struct spfs_inode), 0,
                           (SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD),
                           spfs_inode_init_once);
}

//=====================================
static void
spfs_destroy_inode_SLAB(struct kmem_cache *slab) {
  // doc
  rcu_barrier();
  if (slab) {
    kmem_cache_destroy(slab);
  }
}

//=====================================
static bool
spfs_mod_inode_cb(void *closure, struct spfs_inode *entry) {
  struct inode *src = closure;

  // TODO convert_from_inode(src, /*dest*/entry);

  return true;
}

static int
spfs_write_inode(struct inode *inode, struct writeback_control *wbc) {
  /*
   * Maybe this is called when timestamps are updated? to sync the changes to
   * disk
   *
   * TODO lock $inode.private so we can use tree.shared(read) lock since we do
   * not insert any new node and therefore do not perform any rebalance. And we
   * are sure that there are not multiple modify on the same inode by locking
   * it before doing anything.
   */
  int res;
  struct spfs_super_block *sbi;

  printk(KERN_INFO "spfs_write_inode()\n");

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  mutex_lock(&sbi->tree.lock);
  res = spfs_btree_modify(&sbi->tree, inode->i_ino, inode, spfs_mod_inode_cb);
  mutex_unlock(&sbi->tree.lock);

  return res;
}

//=====================================
static const struct file_system_type spfs_fs_type = {
    .name = "spfs",
    .fs_flags = FS_REQUIRES_DEV,
    /* Mount an instance of the filesystem */
    .mount = spfs_mount,
    /* Shutdown an instance of the filesystem... */
    .kill_sb = kill_block_super,
    .owner = THIS_MODULE,
};

//=====================================
static const struct super_operations spfs_super_ops = {
    /*
     * ref: Documentation/filesystems/vfs.txt
     *
     * This method is called by alloc_inode() to allocate memory for struct
     * inode and initialize it.
     * If this function is not defined, a simple 'struct inode' is allocated.
     * Normally alloc_inode will be used to allocate a larger structure which
     * contains a 'struct inode' embedded within it.
     */
    .alloc_inode = spfs_alloc_inode,

    /*
     * This method is called by destroy_inode() to release resources allocated
     * for struct inode. It is only required if ->alloc_inode was defined and
     * simply undoes anything done by >alloc_inode.
     */
    .destroy_inode = spfs_free_inode,

    /*
     * This method is called when the VFS needs to write an inode to disc.
     * The second parameter indicates whether the write should be synchronous
     * or not, not all filesystems check this flag.
     */
    .write_inode = spfs_write_inode,

    /*
     * Called when the VFS wishes to free the superblock (i.e. unmount).
     * This is called with the superblock lock held
     */
    .put_super = spfs_put_super,
};

//=====================================
static int __init
spfs_init(void) {
  int res;

  printk(KERN_INFO "init spfs\n");

  spfs_inode_SLAB = spfs_create_inode_SLAB();
  if (!spfs_inode_SLAB) {
    printk(KERN_ERR "Failed creating inode SLAB cache\n");
    return -ENOMEM;
  }

  /*
   *	Adds the file system passed to the list of file systems the kernel
   *	is aware of for mount and other syscalls. Returns 0 on success,
   *	or a negative errno code on an error.
   *
   *	The &struct file_system_type that is passed is linked into the kernel
   *	structures and must not be freed until the file system has been
   *	unregistered.
   */
  res = register_filesystem(&spfs_fs_type);
  if (res) {
    spfs_destroy_inode_SLAB(spfs_inode_SLAB);
    printk(KERN_ERR "Failed register_filesystem(simplefs): %d\n", res);
  } else {
    printk(KERN_INFO "Sucessfully register_filesystem(simplefs)\n");
  }

  return res;
}

static void __exit
spfs_exit(void) {
  int res;

  printk(KERN_INFO "exit spfs\n");

  res = unregister_filesystem(&spfs_fs_type);
  if (res) {
    printk(KERN_INFO "Faied unregister_filesystem(simplefs): %d\n", res);
  } else {
    printk(KERN_INFO "Sucessfully unregister_filesystem(simplefs)\n");
  }
  spfs_destroy_inode_SLAB(spfs_inode_SLAB);
}

module_init(spfs_init);
module_exit(spfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fredrik Olsson");

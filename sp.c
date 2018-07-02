#include "sp.h"
#include "util.h"

#include <linux/buffer_head.h>
#include <linux/cred.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/slab.h> /* kzalloc, ... */
/* # linux/fs.h
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
 * blkcnt_t is the type of the inode's block count.
 * - typedef u64 sector_t;
 * - typedef u64 blkcnt_t;
 *
 * # Pointer error
 * ## check error
 * if (IS_ERR(ptr)) // check if error ptr
 * ## ptr error -> int error
 * ## int error -> ptr error
 * ##???
 * return PTR_ERR(ptr);
 * return ERR_PTR(PTR_ERR(inode));
 * ##
 * (dentry*) return ERR_CAST(bdev: bdev*);
 *
 * #
 * unsigned long i_ino;
 */

/* # Super block
 * - Located at sector_t: 0
 * - Occupies the complete first sector
 * - Always contains max 512KB useable data, independent of block size
 *
 * # Btree
 *
 * # File Block
 *
 * # Folder Block
 *
 * # Free List
 * free_entry[start: sector_t, blocks: u32]
 * free_list[length: u32, next_free_list: offset, free_entry:[length]]
 */

//=====================================
static const struct inode_operations spfs_inode_ops;
static const struct file_operations spfs_file_ops;
static const struct file_operations spfs_dir_ops;
static const struct super_operations spfs_super_ops;

/* TODO LKM unload write back
 * TODO super block field to indicate if we have the fs open
 * TODO inode ref count
 * TODO inode correct locking
 * TODO internal lock inode when accessing inode.i_* variables?
 *
 * XXX optimize sb_bread without reading for block device when we are not
 * interested in the content of the read, we only want to overwrite all content
 *
 * XXX cache file block $index togther with $length, not just $start sector_t in
 * spfs_inode struct.
 *
 *
 * XXX
 * pr_info("Information\n");
 * pr_err("Error\n");
 * pr_alert("Really big problem\n");
 * pr_emerg("Life as we know it is over\n");*
 * instead of:
 * printk(KERN_ERR ...
 */

//=====================================
static void
spfs_setup_inode_fp(struct inode *inode) {
  if (S_ISREG(inode->i_mode)) {
    inode->i_op = &spfs_inode_ops;
    inode->i_fop = &spfs_file_ops;
  } else if (S_ISDIR(inode->i_mode)) {
    inode->i_op = &spfs_inode_ops;
    inode->i_fop = &spfs_dir_ops;

    /* directory inodes start off with i_nlink == 2 (for "." entry) */
    inc_nlink(inode);
  } else {
    BUG();
  }
}

static struct spfs_inode *
spfs_new_inode(struct super_block *sb, struct spfs_inode *parent,
               umode_t mode) {
  struct spfs_super_block *sbi;
  struct inode *inode;
  spfs_ino needle;

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  {
    mutex_lock(&sbi->id_lock); // XXX spinlock
    needle = sbi->id++;
    mutex_unlock(&sbi->id_lock);
  }

  inode = iget_locked(sb, needle);
  if (!inode) {
    return ERR_PTR(-ENOMEM);
  }

  if (inode->i_state & I_NEW) {
    struct spfs_inode *result;
    result = SPFS_INODE(inode);

    if (parent) {
      /* #inode_init_owner()
       * Init uid,gid,mode for new inode according to posix standards
       */
      inode_init_owner(inode, &parent->i_inode, mode);
    } else {
      inode->i_mode = mode;
    }

    inode->i_ctime = current_time(inode);
    inode->i_atime = inode->i_ctime;
    inode->i_mtime = inode->i_ctime;

    spfs_setup_inode_fp(inode);

    unlock_new_inode(inode);

    return result;
  } else {
    // XXX should never happen
    iget_failed(inode);
  }

  return ERR_PTR(-ENOMEM);
}

static struct spfs_inode *
spfs_inode_by_id(struct super_block *sb, spfs_ino needle) {
  int res;
  struct spfs_super_block *sbi;
  struct inode *result;

  /* #iget_locked()
   * - Search for existing inode in cache, and if found return it with
   *   increased ref count.(hash map?)
   * - if not in cache allocate new inode and set state to I_NEW, caller is
   *   responsible to populate it.
   */
  result = iget_locked(sb, needle);
  if (!result) {
    return ERR_PTR(-ENOMEM);
  }

  if (!(result->i_state & I_NEW)) {
    /* Found match in inode cache */
    return SPFS_INODE(result);
  }

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  {
    // XXX read lock
    mutex_lock(&sbi->tree.lock);
    res = spfs_btree_lookup(&sbi->tree, needle, SPFS_INODE(result));
    mutex_unlock(&sbi->tree.lock);
  }

  if (res) {
    /* Not found */
    /* #iget_failed()
     * Mark an under-construction inode as dead and release it
     */
    iget_failed(result);

    return NULL;
  }

  spfs_setup_inode_fp(result);

  /* #unlock_new_inode()
   * Clear the I_NEW state and wake up any waiters.
   * Called when the inode is fully initialised to clear the new state of the
   * inode and wake up anyone waiting for the inode to finish initialisation.
   */
  unlock_new_inode(result);

  return SPFS_INODE(result);
}

static int
spfs_generic_create(struct spfs_inode *parent, struct dentry *dentry,
                    umode_t mode) {
  int res;
  struct super_block *sb;
  struct spfs_super_block *sbi;
  struct spfs_inode *inode;
  struct qstr *name = &dentry->d_name;

  BUG_ON(!parent);
  BUG_ON(!dentry);

  sb = parent->i_inode.i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  if (name->len > sizeof(inode->name)) {
    return -ENAMETOOLONG;
  }

  res = -ENOENT;
  inode = spfs_new_inode(sb, parent, mode);
  if (inode) {
    memset(inode->name, 0, sizeof(inode->name));
    memcpy(inode->name, name->name, name->len);

    {
      /* XXX if (mutex_lock_interruptible(&sbi->tree.lock)) { */
      mutex_lock(&sbi->tree.lock);
      res = spfs_btree_insert(&sbi->tree, inode);
      mutex_unlock(&sbi->tree.lock);
    }

    if (res) {
      // cleanup
      goto Lout;
    }

    /* #d_instantiate()
     * Fill in inode information for a dentry
     * XXX?
     * This assumes that the inode count has been incremented (or otherwise set)
     * by the caller to indicate that it is now in use by the dcache.
     */
    d_instantiate(dentry, &inode->i_inode);
    BUG_ON(dentry->d_inode != &inode->i_inode);

    res = 0;
  }

Lout:
  return res;
}

//=====================================
struct spfs_dir_block {
  sector_t next;
  uint32_t children;
};

struct spfs_dir_entry {
  // TODO parent -> child name -> inode
  // name is where an inode is mounted
  // example: hardlink map the same inode in different places in the dir tree
  // and also under different names. Therefore name should be present in the
  // parent dir inode list not in the inode itself.
  spfs_ino ino;
  /* char name[] */
};

static int
spfs_parse_dir_block(struct buffer_head *bh, size_t *pos,
                     struct spfs_dir_block *out) {
  /*        child:[ino: u64]
   * folder_block:[next:sector_t, children:u32, cx: child[children]]
   */

  uint32_t magic;
  if (!spfs_sb_read_u32(bh, pos, &magic)) {
    return -EINVAL;
  }

  if (!spfs_sb_read_sector(bh, pos, &out->next)) {
    return -EINVAL;
  }
  if (!spfs_sb_read_u32(bh, pos, &out->children)) {
    return EINVAL;
  }

  pr_err("dir_block:[magic:%X,next:%lu,children:%u]\n", magic, out->next,
         out->children);

  if (magic != SPOOKY_FS_DIR_BLOCK_MAGIC) {
    return -EINVAL;
  }

  return 0;
}

static int
spfs_make_dir_block(struct buffer_head *bh, size_t *pos,
                    const struct spfs_dir_block *block) {

  if (!spfs_sb_write_u32(bh, pos, SPOOKY_FS_DIR_BLOCK_MAGIC)) {
    return -EINVAL;
  }
  if (!spfs_sb_write_u32(bh, pos, block->next)) {
    return -EINVAL;
  }
  if (!spfs_sb_write_u32(bh, pos, block->children)) {
    return EINVAL;
  }

  return 0;
}

static bool
spfs_dir_is_full(struct super_block *sb, const struct spfs_dir_block *block) {
  // XXX make correct
  return block->children == 100;
}

static int
spfs_dir_write_child(struct buffer_head *bh, size_t start, size_t idx,
                     spfs_ino child) {
  size_t sz = sizeof(child);
  size_t offset = start + (idx * sz);

  if (spfs_sb_remaining(bh, offset) < sz) {
    return -EIO;
  }

  if (!spfs_sb_write_u32(bh, &offset, child)) {
    return -EIO;
  }

  return 0;
}

static sector_t
spfs_dir_block_alloc(struct super_block *sb, size_t *bh_pos,
                     struct buffer_head **pbh, struct spfs_dir_block *block) {
  struct spfs_super_block *sbi;
  sector_t result;

  sbi = sb->s_fs_info;

  BUG_ON(!sbi);

  result = spfs_free_alloc(&sbi->free_list, 1);
  if (result) {
    int res;
    struct buffer_head *bh;

    block->next = 0;
    block->children = 0;

    bh = sb_bread(sb, result);
    if (!bh) {
      // cleanup
      return 0;
    }

    res = spfs_make_dir_block(bh, bh_pos, block);
    if (res) {
      // cleanup
      return 0;
    }

    *pbh = bh;
    return result;
  }

  return 0;
}

static int
spfs_add_child(struct super_block *sb, struct spfs_inode *parent,
               struct inode *child) {
  int res;
  sector_t start;

  mutex_lock(&parent->lock);
  {
    parent->capacity++;
    mark_inode_dirty(&parent->i_inode);
  }

  start = parent->start;
  // TODO this wont work when the first is full and the next is created...

Lit:
  if (start) {
    struct spfs_dir_block block = {};
    struct buffer_head *bh;
    size_t bh_pos;
    bool done = false;

    bh = sb_bread(sb, start);
    if (!bh) {
      res = -EIO;
      goto Lunlock;
    }

    bh_pos = 0;
    res = spfs_parse_dir_block(bh, &bh_pos, &block);
    if (res) {
      // cleanup
      res = -EINVAL;
      goto Lunlock;
    }

    if (!spfs_dir_is_full(sb, &block)) {
      res = spfs_dir_write_child(bh, bh_pos, block.children++, child->i_ino);

      /* Update heder with $block.chidren */
      bh_pos = 0;
      res = spfs_make_dir_block(bh, &bh_pos, &block);

      mark_buffer_dirty(bh);
      done = true;
    }

    brelse(bh);

    if (!done) {
      start = block.next;
      goto Lit;
    }
  } else {
    struct spfs_dir_block block = {};
    struct buffer_head *bh;
    size_t bh_pos = 0;

    parent->start = spfs_dir_block_alloc(sb, &bh_pos, &bh, &block);
    if (!parent->start) {
      res = -EINVAL;
      goto Lunlock;
    }

    res = spfs_dir_write_child(bh, bh_pos, block.children++, child->i_ino);

    /* Update heder with $block.chidren */
    bh_pos = 0;
    res = spfs_make_dir_block(bh, &bh_pos, &block);

    mark_buffer_dirty(bh);
    brelse(bh);
  }

  res = 0;
Lunlock:
  mutex_unlock(&parent->lock);
  return res;
}

/* # Create file */
static int
spfs_create(struct inode *parent, struct dentry *subject, umode_t mode,
            bool excl) {
  int res;
  struct super_block *sb;
  struct spfs_inode *spfs_parent;

  BUG_ON(!S_ISREG(mode));

  pr_err("spfs_create()\n");

  spfs_parent = SPFS_INODE(parent);
  sb = parent->i_sb;

  BUG_ON(!sb);

  // newly created inode-lock {
  // XXX excl?
  res = spfs_generic_create(spfs_parent, subject, mode);
  if (!res) {
    struct inode *inode = d_inode(subject);
    BUG_ON(!inode);

    res = spfs_add_child(sb, spfs_parent, inode);
  }
  //}

  pr_err("END spfs_create()\n");

  return res;
}

//=====================================
/* # Create directory */
static int
spfs_mkdir(struct inode *parent, struct dentry *subject, umode_t mode) {
  int res;
  struct super_block *sb;
  struct spfs_inode *spfs_parent;

  BUG_ON(!S_ISDIR(mode));

  spfs_parent = SPFS_INODE(parent);
  sb = parent->i_sb;

  BUG_ON(!sb);

  pr_err("spfs_mkdir()\n");

  // newly created inode-lock {
  res = spfs_generic_create(spfs_parent, subject, mode);
  if (!res) {
    struct inode *inode = d_inode(subject);
    BUG_ON(!inode);

    /* #inc_nlink()
     * increment an inode's link count
     *
     * XXX only used for mkdir in ramfs. why not for create?
     */
    inc_nlink(parent);

    res = spfs_add_child(sb, spfs_parent, inode);
  }
  //}

  pr_err("END spfs_mkdir()\n");

  return res;
}

//=====================================
typedef bool (*for_all_cb)(void *, struct spfs_inode *);

static int
spfs_for_all_children(struct spfs_inode *parent, void *closure, for_all_cb f) {
  int res;
  struct super_block *sb;
  struct spfs_super_block *sbi;
  sector_t child_list;

  sb = parent->i_inode.i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  if (!S_ISDIR(parent->i_inode.i_mode)) {
    return -EINVAL;
  }

  mutex_lock(&parent->lock); // XXX READ lock

  child_list = parent->start;
Lit:
  if (child_list) {
    struct buffer_head *bh;
    struct spfs_dir_block block;
    uint32_t i;
    size_t bh_pos;

    bh = sb_bread(sb, child_list);
    if (!bh) {
      res = -EIO;
      goto Lunlock;
    }

    bh_pos = 0;
    res = spfs_parse_dir_block(bh, &bh_pos, &block);
    if (res) {
      // cleanup
      goto Lunlock;
    }

    i = 0;
    while (i < block.children) {
      spfs_ino cur_ino;

      if (!spfs_sb_read_ino(bh, &bh_pos, &cur_ino)) {
        // cleanup
        res = -EINVAL;
        goto Lunlock;
      }

      if (cur_ino) {
        struct spfs_inode *cur_child;
        cur_child = spfs_inode_by_id(sb, cur_ino);

        if (!IS_ERR(cur_child)) {
          if (!f(closure, cur_child)) {
            brelse(bh);
            goto Lunlock;
          }
        }

        ++i;
      }

    } // while

    brelse(bh);

    child_list = block.next;
    goto Lit;
  }

  res = 0;
Lunlock:
  mutex_unlock(&parent->lock);
  return res;
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
    data->result = &cur->i_inode;

    /* Stop searching */
    return false;
  }

  return true;
}

static struct inode *
spfs_inode_by_name(struct super_block *sb, struct spfs_inode *parent,
                   struct dentry *child) {
  struct spfs_super_block *sbi;
  const char *name;
  BUG_ON(!sb);
  BUG_ON(!child);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  name = child->d_name.name;
  BUG_ON(!name);

  {
    struct spfs_find_iname_name_data data = {
        .sb = sb,
        .dentry = child,
        .name = name,

        .result = NULL,
    };


    spfs_for_all_children(parent, &data, &spfs_inode_by_name_cb);

    return data.result;
  }
}

/*
 * Given parent inode & child filename lookup child inode & add it to child
 * dentry.
 */
static struct dentry *
spfs_lookup(struct inode *parent, struct dentry *dentry, unsigned int flags) {
  struct super_block *sb;
  struct inode *result;

  BUG_ON(!parent);
  BUG_ON(!dentry);

  sb = parent->i_sb;
  BUG_ON(!sb);

  pr_err("spfs_lookup()\n");

  result = spfs_inode_by_name(sb, SPFS_INODE(parent), dentry);
  if (result) {
    if (IS_ERR(result)) {
      return ERR_CAST(result);
    }
    /* #d_add()
     * XXX
     * This adds the entry to the hash queues and initializes inode.
     */
    d_add(dentry, result);

    return dentry;
  }

  pr_err("END spfs_lookup()\n");

  return NULL;
}

//=====================================
#define MIN(f, s) (f) > (s) ? (s) : (f)

struct spfs_file_extent {
  /* block id of next extent */
  sector_t next;
  /* capacity in number of blocks */
  uint32_t capacity;
  /* length in number of bytes */
  uint32_t length;
};

static int
spfs_parse_file_extent(struct buffer_head *bh, size_t *pos,
                       struct spfs_file_extent *out) {
  uint32_t magic;

  /* [magic:u32, next:sector_t, cap:u32, length:u32, raw:cap] */
  if (!spfs_sb_read_u32(bh, pos, &magic)) {
    return -EINVAL;
  }
  if (!spfs_sb_read_sector(bh, pos, &out->next)) {
    return -EINVAL;
  }
  if (!spfs_sb_read_u32(bh, pos, &out->capacity)) {
    return -EINVAL;
  }
  if (!spfs_sb_read_u32(bh, pos, &out->length)) {
    return -EINVAL;
  }

  pr_err("dir_block:[magic:%X,next:%lu,capacity:%u,length:%u]\n", magic,
         out->next, out->capacity, out->length);

  if (magic != SPOOKY_FS_FILE_BLOCK_MAGIC) {
    return -EINVAL;
  }

  return 0;
}

static int
spfs_make_file_extent(struct buffer_head *bh, size_t *pos,
                      const struct spfs_file_extent *in) {
  if (!spfs_sb_write_u32(bh, pos, SPOOKY_FS_FILE_BLOCK_MAGIC)) {
    return 1;
  }

  if (!spfs_sb_write_u32(bh, pos, in->next)) {
    return 1;
  }
  if (!spfs_sb_write_u32(bh, pos, in->capacity)) {
    return 1;
  }
  if (!spfs_sb_write_u32(bh, pos, in->length)) {
    return 1;
  }

  return 0;
}

static size_t
spfs_sizeof_file_extent_header(void) {
  size_t result = 0;

  result += 4;
  result += 4;
  result += 4;
  result += 4;

  return result;
}

static size_t
spfs_file_extent_data_bytes(const struct spfs_super_block *sbi, size_t blocks) {
  size_t bytes;
  BUG_ON(blocks == 0);
  bytes = blocks * sbi->block_size;

  bytes -= spfs_sizeof_file_extent_header();
  return bytes;
}

static ssize_t
spfs_read_file_extent(struct super_block *sb, sector_t * /*OUT*/ next,
                      sector_t start, char __user **in_buf, size_t *in_len,
                      loff_t *pos) {
  ssize_t res;
  struct spfs_file_extent header = {};

  struct buffer_head *bh;
  struct spfs_super_block *sbi;
  size_t bh_pos = 0;
  size_t read = 0;

  sbi = sb->s_fs_info;

  bh = sb_bread(sb, start);
  if (!bh) {
    // cleanup
    return -EIO;
  }

  res = spfs_parse_file_extent(bh, &bh_pos, &header);
  if (res) {
    // cleanup
    return res;
  }
  *next = header.next;
  // XXX calc start sector_t based on $header.length, $in_len and $bh.length.
  // and skip over unneeded sectors

  do {
    size_t con;

    con = MIN(*pos, MIN(spfs_sb_remaining(bh, bh_pos), header.length));
    bh_pos += con;
    *pos -= con;
    header.length -= con;

    if (*pos == 0) {

      con = MIN(*in_len, MIN(spfs_sb_remaining(bh, bh_pos), header.length));
      if (con > 0) {
        copy_to_user(/*dest*/ *in_buf, /*src*/ bh->b_data + bh_pos, con);
        bh_pos += con;
        *in_buf += con;
        *in_len -= con;

        read += con;
        header.length -= con;
      }
    }

    brelse(bh);

    /* header.length -= MIN(header.length, sbi->block_size); */
    if (header.length > 0) {
      BUG_ON(header.length == 0);
      header.capacity--;
      start = start + 1;

      bh = sb_bread(sb, start);
      if (!bh) {
        // cleanup
        return -EIO;
      }

      bh_pos = 0;
    } else {
      break;
    }
  } while (1);

  return read;
}

/*
 * Used to retrieve data from the device.
 * A non-negative return value represents the number of bytes successfully
 * read.
 */
static ssize_t
spfs_read(struct file *file, char __user *in_buf, size_t in_len, loff_t *ppos) {

  ssize_t res;
  struct spfs_inode *inode;
  struct super_block *sb;
  struct spfs_super_block *sbi;

  loff_t pos;
  ssize_t read;

  if (!ppos) {
    return -EINVAL;
  }

  // XXX handle when *ppos > file.length by check inode.size
  pos = *ppos;
  read = 0;

  pr_err("spfs_read()\n");

  if (in_len == 0) {
    return 0;
  }

  BUG_ON(!file);

  inode = SPFS_INODE(file_inode(file));

  if (!S_ISREG(inode->i_inode.i_mode)) {
    return -EINVAL;
  }

  sb = inode->i_inode.i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  {
    sector_t start;
    mutex_lock(&inode->lock); // XXX read lock
    start = inode->start;
  Lit:
    if (start) {
      sector_t next = 0;

      res = spfs_read_file_extent(sb, &next, start, &in_buf, &in_len, &pos);
      if (res < 0) {

        pr_err("<<< spfs_read()\n");
        return res;
      }
      read += res;

      if (in_len > 0) {
        start = next;
        goto Lit;
      }
    }
    mutex_unlock(&inode->lock);

    *ppos += read;
  }

  pr_err("END spfs_read()\n");

  return read;
}

//=====================================
struct spfs_modify_start {
  sector_t start;
  size_t capacity;
};

static bool
spfs_modify_start_cb(void *closure, struct spfs_inode *inode) {
  struct spfs_modify_start *ctx = closure;

  BUG_ON(!S_ISREG(inode->i_inode.i_mode));

  inode->start = ctx->start;
  inode->capacity = ctx->capacity;

  return true;
}

static sector_t
spfs_file_extent_alloc(struct super_block *sb, size_t blocks) {
  struct spfs_super_block *sbi;
  sector_t result;

  sbi = sb->s_fs_info;

  result = spfs_free_alloc(&sbi->free_list, blocks);
  if (result) {
    int res;
    struct spfs_file_extent desc = {
        /**/
        .next = 0,
        .capacity = blocks,
        .length = 0,
        /**/
    };
    size_t b_pos = 0;
    struct buffer_head *bh;

    // XXX here we are not interested in the previous content of sb
    bh = sb_bread(sb, result);
    BUG_ON(!bh);

    res = spfs_make_file_extent(bh, &b_pos, &desc);
    if (res) {
      // cleanup
      return 0;
    }

    mark_buffer_dirty(bh); // XXX maybe sync?
    brelse(bh);
  }

  return result;
}

static ssize_t
spfs_do_write(struct buffer_head *bh, size_t *bh_pos,
              const char __user **in_buf, size_t *in_len, loff_t *ppos) {
  ssize_t written = 0;
  size_t con;

  con = MIN(*ppos, spfs_sb_remaining(bh, *bh_pos));
  *ppos -= con;
  *bh_pos += con;

  if (*ppos == 0) {
    /* We have counted down to the desired position by traversing the file
     * blocks. Meaning we can now start to write from $in_buf to $bh
     */

    con = MIN(*in_len, spfs_sb_remaining(bh, *bh_pos));
    if (con > 0) {

      copy_from_user(/*dest*/ bh->b_data + *bh_pos, /*src*/ *in_buf, con);
      *in_buf += con;
      *in_len -= con;
      /* *bh_pos += con; */
      written += con;
    }
  }
  return written;
}

static bool
spfs_in_sector(const struct spfs_super_block *sbi, loff_t pos) {
  return pos < sbi->block_size;
}

static ssize_t
spfs_write_file_extent(struct super_block *sb, sector_t start,
                       struct spfs_file_extent *header,
                       const char __user **in_buf, size_t *in_len, loff_t *ppos,
                       sector_t *extra) {
  ssize_t res;
  struct spfs_super_block *sbi;
  ssize_t written = 0;
  size_t head_pos = 0;
  struct buffer_head *head_bh;
  bool head_dirty = false;

  sbi = sb->s_fs_info;
  /* Setup buffer for file extent header */
  {
    head_bh = sb_bread(sb, start);
    if (!head_bh) {
      return -EIO;
    }

    res = spfs_parse_file_extent(head_bh, &head_pos, header);
    if (res) {
      // cleanup
      return res;
    }

    if (header->next == 0) {
      if (*extra != 0) {
        header->next = *extra;
        head_dirty = true;
        /* Clear $extra, since we have handled it */
        *extra = 0;
      }
    }
  }

  {
    res = spfs_do_write(head_bh, &head_pos, in_buf, in_len, ppos);
    if (res < 0) {
      // cleanup & propagate written bytes
      return res;
    }
    written += res;
    header->length += res;
    if (res > 0) {
      head_dirty = true;
    }
  }

  {
    const sector_t end = start + header->capacity;
    sector_t it = start + 1;
    while (*in_len > 0 && it < end) {
      if (spfs_in_sector(sbi, *ppos)) {
        ssize_t res;
        struct buffer_head *bh;
        size_t bh_pos = 0;

        bh = sb_bread(sb, it);
        if (!bh) {
          // cleanup & propagate written bytes
          return -EIO;
        }

        res = spfs_do_write(bh, &bh_pos, in_buf, in_len, ppos);
        if (res < 0) {
          // cleanup & propagate written bytes
          return res;
        }
        header->length += res;
        if (res > 0) {
          head_dirty = true;
          mark_buffer_dirty(bh);
        }

        brelse(bh);

      } else {
        BUG_ON(*ppos < sbi->block_size);
        *ppos -= sbi->block_size;
      }
      ++it;
    } // while
  }

  if (head_dirty) {
    int res;
    size_t ipos = 0;

    res = spfs_make_file_extent(head_bh, &ipos, header);
    BUG_ON(res);

    mark_buffer_dirty(head_bh);
    /* sync_dirty_buffer(head_bh); ??*/
  }
  brelse(head_bh);

  return written;
}

static size_t
spfs_blocks_for(const struct spfs_super_block *sbi, size_t bytes) {
  size_t result = 0;
  while (bytes) {
    result++;
    bytes -= MIN(bytes, sbi->block_size);
  }

  return result;
}

static size_t
spfs_calc_extra_blocks(struct spfs_inode *inode, size_t in_pos, size_t in_len) {
  struct super_block *sb;
  struct spfs_super_block *sbi;

  size_t extra_bytes;
  size_t end;

  sb = inode->i_inode.i_sb;
  sbi = sb->s_fs_info;

  end = in_pos + in_len;

  /* We do not have to allocate more space */
  if (end <= inode->capacity) {
    return 0;
  }

  extra_bytes = end - inode->capacity;
  extra_bytes += spfs_sizeof_file_extent_header();
  return spfs_blocks_for(sbi, extra_bytes);

  /* size_t in_blocks = end / block_size; */
  /* if (in_blocks % block_size > 0) { */
  /*   ++in_blocks; */
  /* } */
  /*  */
  /* if (in_blocks > capacity) { */
  /*   return in_blocks - capacity; */
  /* } */

  /* return 0; */
}

static int
spfs_ensure_capacity(struct spfs_inode *inode, size_t in_pos, size_t in_len,
                     sector_t *extra) {
  int res;
  struct super_block *sb;
  struct spfs_super_block *sbi;
  size_t extra_blocks;
  const spfs_ino ino = inode->i_inode.i_ino;

  sb = inode->i_inode.i_sb;
  sbi = sb->s_fs_info;

  *extra = 0;

  extra_blocks = spfs_calc_extra_blocks(inode, in_pos, in_len);
  if (extra_blocks == 0) {
    return 0;
  }

  *extra = spfs_file_extent_alloc(sb, extra_blocks);
  if (!*extra) {
    return -EIO;
  }

  if (inode->start == 0) {
    inode->start = *extra;
    /* Since this is the first extent there is no existing extent to link to
     * this extent.
     */
    *extra = 0;
  }

  inode->capacity += spfs_file_extent_data_bytes(sbi, extra_blocks);
  {
    struct spfs_modify_start ctx = {
        /**/
        .start = inode->start,
        .capacity = inode->capacity
        /**/
    };

    // XXX maybe have async start&capacity update only setting inode and
    // persisting it only on inode unload
    // `mark_inode_dirty()`
    mutex_lock(&sbi->tree.lock); // XXX read lock
    res = spfs_btree_modify(&sbi->tree, ino, &ctx, spfs_modify_start_cb);
    mutex_unlock(&sbi->tree.lock);
    if (res) {
      // cleanup
      return res;
    }
  }

  res = 0;
  return res;
}

/*
 * Sends data to the device. If missing, -EINVAL is returned to the program
 * calling the write system call. The return value, if non-negative,
 * represents the number of bytes successfully written.
 *
 * On a regular file, if this incremented file offset is greater than the
 * length of the file, the length of the file shall be set to this file
 * offset.
 */
static ssize_t
spfs_write(struct file *file, const char __user *in_buf, size_t in_len,
           loff_t *ppos) {
  struct spfs_inode *inode;
  struct super_block *sb;

  ssize_t written = 0;

  if (!ppos) {
    return -EINVAL;
  }

  /* XXX EFBIG - file to big*/
  pr_err("spfs_write()\n");
  if (in_len == 0) {
    return 0;
  }

  BUG_ON(!file);
  inode = SPFS_INODE(file_inode(file));
  if (!S_ISREG(inode->i_inode.i_mode)) {
    return -EINVAL;
  }

  sb = inode->i_inode.i_sb;
  {
    ssize_t res;
    sector_t extra = 0;
    loff_t orig_file_sz;
    loff_t pos;
    sector_t start;

    if (mutex_lock_interruptible(&inode->lock)) {
      return -EINTR;
    }

    orig_file_sz = i_size_read(&inode->i_inode);
    if (*ppos > orig_file_sz) {
      *ppos = orig_file_sz;
    }

    res = spfs_ensure_capacity(inode, *ppos, in_len, &extra);
    if (res) {
      // cleanup
      return res;
    }

    pos = *ppos;
    start = inode->start;
  Lit:
    if (start) {
      struct spfs_file_extent header = {};
      ssize_t ires;

      ires = spfs_write_file_extent(sb, start, &header, &in_buf, &in_len, &pos,
                                    &extra);
      if (ires) {
        // cleanup
        return -EIO;
      }
      written += ires;

      if (in_len > 0) {
        start = header.next;
        goto Lit;
      }
    }

    BUG_ON(extra != 0);
    BUG_ON(in_len != 0);

    i_size_write(&inode->i_inode, orig_file_sz + written);

    mutex_unlock(&inode->lock);
    *ppos += written;
  }

  return written;
}

//=====================================
struct spfs_iterate_ctx {
  struct dir_context *dir_ctx;
  loff_t pos;
};

static bool
spfs_iterate_cb(void *closure, struct spfs_inode *cur) {
  bool result = true;

  struct spfs_iterate_ctx *ctx = closure;
  const spfs_ino ino = cur->i_inode.i_ino;
  const char *name = cur->name;
  size_t nlen = strlen(name);

  if (ctx->pos == ctx->dir_ctx->pos) {
    unsigned type = (cur->i_inode.i_mode >> 12) & 15;
    result = dir_emit(ctx->dir_ctx, name, nlen, ino, type);
    if (result) {
      ++ctx->dir_ctx->pos;
    }
  }
  ++ctx->pos;

  return result;
}

static int
spfs_iterate(struct file *file, struct dir_context *dir_ctx) {
  struct inode *parent;
  struct spfs_inode *spfs_parent;

  struct spfs_iterate_ctx ctx = {
      /**/
      .dir_ctx = dir_ctx,
      .pos = 2, /* '.', '..' */
                /**/
  };

  pr_err("spfs_iterate()\n");

  parent = file_inode(file);
  spfs_parent = SPFS_INODE(parent);

  /* add '.' & '..' if appropriate */
  if (!dir_emit_dots(file, dir_ctx)) {
    return -EIO;
  }

  if (dir_ctx->pos >= (spfs_parent->capacity + 2)) {
    return 0;
  }

  spfs_for_all_children(spfs_parent, &ctx, spfs_iterate_cb);

  pr_err("end spfs_iterate()\n");

  return 0;
}

//=====================================
static int
spfs_read_super_block(struct buffer_head *bh, struct spfs_super_block *super) {
  int res;
  size_t pos;

  res = -EINVAL;
  pos = 0;
  if (!spfs_sb_read_u32(bh, &pos, &super->magic)) {
    goto Lout;
  }

  if (!spfs_sb_read_u32(bh, &pos, &super->version)) {
    goto Lout;
  }
  if (!spfs_sb_read_u32(bh, &pos, &super->block_size)) {
    goto Lout;
  }

  if (!spfs_sb_read_ino(bh, &pos, &super->id)) {
    goto Lout;
  }
  if (!spfs_sb_read_ino(bh, &pos, &super->root_id)) {
    goto Lout;
  }

  if (!spfs_sb_read_sector(bh, &pos, &super->btree_offset)) {
    goto Lout;
  }
  if (!spfs_sb_read_sector(bh, &pos, &super->free_list_offset)) {
    goto Lout;
  }

  /* pr_err("magic", start); */
  pr_err(
      "super:[magic:%X,version:%u,block_size:%u,id:%lu,root_id:%lu,btree_of:%lu"
      "free_of:%lu]\n", //
      super->magic, super->version, super->block_size, super->id,
      super->root_id, super->btree_offset, super->free_list_offset);

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
    pr_err("bh = sb_bread(sb, start[%zu])\n", start);
    return -EIO;
  }

  mutex_init(&super->id_lock);

  res = spfs_read_super_block(bh, super);
  if (res) {
    goto Lrelease;
  }

  if (super->magic != SPOOKY_FS_SUPER_MAGIC) {
    pr_err("super->magic[%u] != SPOOKY_FS_SUPER_MAGIC[%u]\n", //
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
spfs_set_blocksize(struct super_block *sb, size_t block_size) {
  bool is_bdev = sb->s_bdev != NULL;

  if (is_bdev) {
    if (sb_set_blocksize(sb, block_size) == 0) {
      return -EINVAL;
    }
  } else {
    // TODO
  }

  return 0;
}

static int
spfs_fill_super_block(struct super_block *sb, void *data, int silent) {
  int res;
  struct spfs_inode *root;
  struct spfs_super_block *sbi;
  const sector_t super_start = 0;

  pr_err("spfs_kill_super_block()\n");

  BUG_ON(!sb);

  sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
  if (!sbi) {
    return -ENOMEM;
  }

  // doc
  sb->s_magic = SPOOKY_FS_SUPER_MAGIC;
  /* Filesystem private info */
  sb->s_fs_info = sbi;
  /*  */
  sb->s_op = &spfs_super_ops;

  res = spfs_set_blocksize(sb, SPOOKY_FS_INITIAL_BLOCK_SIZE);
  if (res) {
    goto Lerr;
  }

  res = spfs_super_block_init(sb, sbi, super_start);
  if (res) {
    goto Lerr;
  }

  res = spfs_set_blocksize(sb, sbi->block_size);
  if (res) {
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

  root = spfs_inode_by_id(sb, sbi->root_id);
  if (IS_ERR(root)) {
    res = PTR_ERR(root);
    goto Lerr;
  } else if (root == NULL) {
    mode_t mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
    root = spfs_new_inode(sb, NULL, mode);
    if (IS_ERR(root)) {
      res = PTR_ERR(root);
      goto Lerr;
    }
    sbi->root_id = root->i_inode.i_ino;
  }

  root->i_inode.i_uid = current_fsuid();
  root->i_inode.i_gid = current_fsgid();

  // doc
  sb->s_root = d_make_root(&root->i_inode);

  return 0;
Lerr:
  kfree(sb->s_fs_info);
  sb->s_fs_info = NULL;

  return res;
}

static struct dentry *
spfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name,
           void *data) {
  /* mount_bdev: mount a filesystem residing on a block device
   * mount_nodev: mount a filesystem that is not backed by a device
   */

  pr_err("spfs_mount(dev_name[%s])\n", dev_name);
  return mount_bdev(fs_type, flags, dev_name, data, spfs_fill_super_block);
  /* return mount_nodev(fs_type, flags, data, ); */
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

  pr_err("spfs_put_super()\n");

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

  pr_debug("spfs_alloc_inode \n");

  inode = kmem_cache_alloc(spfs_inode_SLAB, GFP_KERNEL);

  pr_debug("END spfs_alloc_inode \n");

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

  pr_debug("freeing inode %lu\n", (spfs_ino)inode->i_ino);
  kmem_cache_free(spfs_inode_SLAB, SPFS_INODE(inode));
}

static void
spfs_free_inode(struct inode *inode) {
  pr_debug("spfs_free_inode\n");

  call_rcu(&inode->i_rcu, spfs_free_inode_cb);

  pr_debug("END spfs_free_inode\n");
}

//=====================================
static void
spfs_inode_init_once(void *closure) {
  struct spfs_inode *inode = closure;

  memset(inode, 0, sizeof(*inode));

  mutex_init(&inode->lock);
  inode->capacity = 0;
  inode->start = 0;
  memset(inode->name, 0, sizeof(inode->name));

  // doc
  inode_init_once(&inode->i_inode);
}

static struct kmem_cache *
spfs_create_inode_SLAB(void) {
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
  /* Just mark dirty since there should only be one instance of each inode */
  return true;
}

static int
spfs_write_inode(struct inode *inode, struct writeback_control *wbc) {
  /*
   * Maybe this is called when timestamps are updated? to sync the changes to
   * disk
   *
   * TODO lock $inode.private so we can use tree.shared(read) lock since we do
   * not insert any new node and therefore do not perform any rebalance. And
   * we are sure that there are not multiple modify on the same inode by
   * locking it before doing anything.
   */
  int res;
  struct super_block *sb;
  struct spfs_super_block *sbi;

  pr_err("spfs_write_inode()\n");

  sb = inode->i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  mutex_lock(&sbi->tree.lock);
  res = spfs_btree_modify(&sbi->tree, inode->i_ino, inode, spfs_mod_inode_cb);
  mutex_unlock(&sbi->tree.lock);

  pr_err("END spfs_write_inode()\n");

  return res;
}

//=====================================
static const struct inode_operations spfs_inode_ops = {
    /* create: called by the open(2) and creat(2) system calls. Only
     * required if you want to support regular files. The dentry you
     * get should not have an inode (i.e. it should be a negative
     * dentry). Here you will probably call d_instantiate() with the
     * dentry and the newly created inode
     */
    .create = spfs_create,

    /* Perform a lookup of an inode given parent directory and filename.
     *
     * called when the VFS needs to look up an inode in a parent
     * directory. The name to look for is found in the dentry. This
     * method must call d_add() to insert the found inode into the
     * dentry. The "i_count" field in the inode structure should be
     * incremented. If the named inode does not exist a NULL inode
     * should be inserted into the dentry (this is called a negative
     * dentry). Returning an error code from this routine must only
     * be done on a real error, otherwise creating inodes with system
     * calls like create(2), mknod(2), mkdir(2) and so on will fail.
     * If you wish to overload the dentry methods then you should
     * initialise the "d_dop" field in the dentry; this is a pointer
     * to a struct "dentry_operations".
     * This method is called with the directory inode semaphore held
     */
    .lookup = spfs_lookup,

    /* mkdir: called by the mkdir(2) system call. Only required if you want
     * to support creating subdirectories. You will probably need to
     * call d_instantiate() just as you would in the create() method
     */
    .mkdir = spfs_mkdir
    /**/
};

//=====================================
static struct file_system_type spfs_fs_type = {
    .name = "spfs",
    .fs_flags = FS_REQUIRES_DEV,
    /* .fs_flags = 0, */
    /* Mount an instance of the filesystem */
    .mount = spfs_mount,
    /* Shutdown an instance of the filesystem... */
    .kill_sb = kill_block_super,
    .owner = THIS_MODULE,
};

//=====================================
static const struct file_operations spfs_file_ops = {
    /**/
    .read = spfs_read,
    .write = spfs_write
    /**/
};

//=====================================
static const struct file_operations spfs_dir_ops = {
    /**/
    .llseek = generic_file_llseek,
    .read = generic_read_dir,

    .iterate = spfs_iterate,
    /**/
};

//=====================================
static const struct super_operations spfs_super_ops = {
    /* ref: Documentation/filesystems/vfs.txt
     *
     * This method is called by alloc_inode() to allocate memory for struct
     * inode and initialize it.
     * If this function is not defined, a simple 'struct inode' is allocated.
     * Normally alloc_inode will be used to allocate a larger structure which
     * contains a 'struct inode' embedded within it.
     */
    .alloc_inode = spfs_alloc_inode,

    /* This method is called by destroy_inode() to release resources allocated
     * for struct inode. It is only required if ->alloc_inode was defined and
     * simply undoes anything done by >alloc_inode.
     */
    .destroy_inode = spfs_free_inode,

    /* This method is called when the VFS needs to write an inode to disc.
     * The second parameter indicates whether the write should be synchronous
     * or not, not all filesystems check this flag.
     */
    .write_inode = spfs_write_inode,

    /* Called when the VFS wishes to free the superblock (i.e. unmount).
     * This is called with the superblock lock held
     */
    .put_super = spfs_put_super,

    /* http://man7.org/linux/man-pages/man2/statfs.2.html */
    /* .statfs = */

    /* called by the VFS to show mount options for /proc/<pid>/mounts
     * .show_options =
     */
};

//=====================================
static int __init
spfs_init(void) {
  int res;

  pr_err("init spfs\n");

  spfs_inode_SLAB = spfs_create_inode_SLAB();
  if (!spfs_inode_SLAB) {
    pr_err("Failed creating inode SLAB cache\n");
    return -ENOMEM;
  }

  /* Adds the file system passed to the list of file systems the kernel is
   * aware of for mount and other syscalls. Returns 0 on success, or a
   * negative errno code on an error.
   *
   * The &struct file_system_type that is passed is linked into the kernel
   * structures and must not be freed until the file system has been
   * unregistered.
   */
  res = register_filesystem(&spfs_fs_type);
  if (res) {
    spfs_destroy_inode_SLAB(spfs_inode_SLAB);
    pr_err("Failed register_filesystem(simplefs): %d\n", res);
  } else {
    pr_err("Sucessfully register_filesystem(simplefs)\n");
  }

  pr_err("int: %zu, size_t: %zu, sector_t: %zu, "
         "unsigned long: %zu, void*: %zu, u32: %zu, "
         "uint32_t: %zu, uintptr_t: %zu, bool: %zu\n",
         sizeof(int), sizeof(size_t), sizeof(sector_t), sizeof(unsigned long),
         sizeof(void *), sizeof(u32), sizeof(uint32_t), sizeof(uintptr_t),
         sizeof(bool));

  pr_err("short: %zu, unsigned long long: %zu, ssize_t: %zu\n", //
         sizeof(short), sizeof(unsigned long long), sizeof(ssize_t));

  return res;
}

static void __exit
spfs_exit(void) {
  int res;

  pr_err("exit spfs\n");

  res = unregister_filesystem(&spfs_fs_type);
  if (res) {
    pr_err("Faied unregister_filesystem(simplefs): %d\n", res);
  } else {
    pr_err("Sucessfully unregister_filesystem(simplefs)\n");
  }
  spfs_destroy_inode_SLAB(spfs_inode_SLAB);
}

module_init(spfs_init);
module_exit(spfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fredrik Olsson");

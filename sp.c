#include "sp.h"
#include "util.h"

#include <linux/buffer_head.h>
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

//=====================================
static const struct inode_operations spfs_inode_ops;
static const struct file_operations spfs_file_ops;
static const struct file_operations spfs_dir_ops;
static const struct super_operations spfs_super_ops;

/* TODO support File blocks > block_size (currently bugged)
 * TODO KM unload write back
 * TODO inode ref count
 * TODO inode correct locking
 * TODO new_inode lock until properly created
 * TODO internal lock inode when accessing inode.i_* variables?
 *
 * XXX optimize sb_bread without reading for block device when we are not
 * interested in the content of the read, we only want to overwrite all content
 *
 * XXX more file block $index togther with $length, not just $start sector_t in
 * spfs_inode struct.
 */

//=====================================
static void
spfs_setup_inode_fp(struct inode *inode) {
  switch (inode->i_mode & S_IFMT) {
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
}

static struct spfs_inode *
spfs_new_inode(struct super_block *sb, struct inode *parent, umode_t mode) {
  struct spfs_super_block *sbi;
  struct inode *inode;

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  // XXX maybe use iget_locked() here as well and except the I_NEW flag always,
  // to be sure we properly init inode before release of lock.
  inode = new_inode(sb);
  if (inode) {
    struct spfs_inode *result;
    result = SPFS_INODE(inode);

    {
      mutex_lock(&sbi->id_lock);
      inode->i_ino = sbi->id++;
      mutex_unlock(&sbi->id_lock);
    }

    if (parent) {
      /* #inode_init_owner()
       * Init uid,gid,mode for new inode according to posix standards
       */
      inode_init_owner(inode, parent, mode);
    }

    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);

    result->start = 0;

    spfs_setup_inode_fp(inode);

    return result;
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
   *   increased ref count.
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
    // cleanup

    /* #iget_failed()
     * Mark an under-construction inode as dead and release it
     */
    iget_failed(result);

    return ERR_PTR(-ENOENT);
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
spfs_generic_create(struct inode *parent, struct dentry *dentry, umode_t mode) {
  int res;
  struct super_block *sb;
  struct spfs_super_block *sbi;
  struct spfs_inode *inode;

  BUG_ON(!parent);
  BUG_ON(!dentry);

  sb = parent->i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  res = -ENOENT;
  inode = spfs_new_inode(sb, parent, mode);
  if (inode) {
    struct qstr *name = &dentry->d_name;
    if (name->len > sizeof(inode->name)) {
      // cleanup
      return -ENAMETOOLONG;
    }
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
     * XXX
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
  unsigned long children;
};

static int
spfs_parse_dir_block(struct buffer_head *bh, size_t *pos,
                     struct spfs_dir_block *out) {
  /*        child:[ino: u64]
   * folder_block:[next:sector_t, children:u32, cx: child[children]]
   */

  unsigned long magic;
  if (!spfs_sb_read_u32(bh, pos, &magic)) {
    return -EINVAL;
  }
  if (magic != SPOOKY_FS_DIR_BLOCK_MAGIC) {
    return -EINVAL;
  }

  if (!spfs_sb_read_u32(bh, pos, &out->next)) {
    return -EINVAL;
  }
  if (!spfs_sb_read_u32(bh, pos, &out->children)) {
    return EINVAL;
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

  BUG_ON(!S_ISREG(mode));

  sb = parent->i_sb;

  BUG_ON(!sb);

  // newly created inode-lock {
  // XXX excl?
  res = spfs_generic_create(parent, subject, mode);
  if (!res) {
    struct inode *inode = d_inode(subject);
    BUG_ON(!inode);

    /* #inc_nlink()
     * increment an inode's link count
     */
    inc_nlink(parent);

    res = spfs_add_child(sb, SPFS_INODE(parent), inode);
  }
  //}

  return res;
}

//=====================================
/* # Create directory */
static int
spfs_mkdir(struct inode *parent, struct dentry *subject, umode_t mode) {
  int res;
  struct super_block *sb;

  BUG_ON(!S_ISDIR(mode));

  sb = parent->i_sb;

  BUG_ON(!sb);

  // newly created inode-lock {
  res = spfs_generic_create(parent, subject, mode);
  if (!res) {
    struct inode *inode = d_inode(subject);
    BUG_ON(!inode);

    inc_nlink(parent);

    res = spfs_add_child(sb, SPFS_INODE(parent), inode);
  }
  //}

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
    unsigned long i;
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

      if (!spfs_sb_read_u32(bh, &bh_pos, &cur_ino)) {
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

    if (mutex_lock_interruptible(&parent->lock)) {
      return ERR_PTR(-EINTR);
    }

    spfs_for_all_children(parent, &data, &spfs_inode_by_name_cb);

    mutex_unlock(&parent->lock);

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

  result = spfs_inode_by_name(sb, SPFS_INODE(parent), dentry);
  if (result) {
    if (IS_ERR(result)) {
      return ERR_PTR(PTR_ERR(result));
    } else {
      /* #d_add()
       * XXX
       * This adds the entry to the hash queues and initializes inode.
       */
      d_add(dentry, result);
    }

    return dentry;
  }

  return NULL;
}

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
#define MIN(f, s) (f) > (s) ? (s) : (f)

struct spfs_file_block {
  /* block id of next spfs_file_block */
  sector_t next;
  /* capacity in number of blocks */
  unsigned long capacity;
  /* length in number of bytes */
  unsigned long length;
};

static int
spfs_parse_file_block(struct buffer_head *bh, size_t *pos,
                      struct spfs_file_block *out) {
  unsigned long magic;

  /* [magic:u32, next:sector_t, cap:u32, length:u32, raw:cap] */
  if (!spfs_sb_read_u32(bh, pos, &magic)) {
    return -EINVAL;
  }
  if (magic != SPOOKY_FS_FILE_BLOCK_MAGIC) {
    return -EINVAL;
  }

  if (!spfs_sb_read_u32(bh, pos, &out->next)) {
    return -EINVAL;
  }
  if (!spfs_sb_read_u32(bh, pos, &out->capacity)) {
    return -EINVAL;
  }
  if (!spfs_sb_read_u32(bh, pos, &out->length)) {
    return -EINVAL;
  }

  return 0;
}

static int
spfs_make_file_block(struct buffer_head *bh, size_t *pos,
                     const struct spfs_file_block *in) {
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

/*
 * Used to retrieve data from the device.
 * A non-negative return value represents the number of bytes successfully
 * read.
 */
static ssize_t
spfs_read(struct file *file, char __user *in_buf, size_t in_len, loff_t *ppos) {

  int res;
  struct spfs_inode *inode;
  struct super_block *sb;
  struct spfs_super_block *sbi;

  loff_t pos;
  ssize_t read;

  if (!ppos) {
    return -EINVAL;
  }

  pos = *ppos;
  read = 0;

  printk(KERN_INFO "spfs_read()\n");

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
    mutex_lock(&inode->lock);
    start = inode->start;
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

      con = MIN(pos, spfs_sb_remaining(bh, bh_pos));
      bh_pos += con;
      pos -= con;

      if (pos == 0) {

        con = MIN(in_len, spfs_sb_remaining(bh, bh_pos));
        if (con > 0) {
          copy_to_user(/*dest*/ in_buf, /*src*/ bh->b_data + bh_pos, con);
          bh_pos += con;
          in_buf += con;
          in_len -= con;

          read += con;
        }
      }

      brelse(bh);

      if (in_len > 0) {
        start = file_block.next;
        goto Lit;
      }
    }
    mutex_unlock(&inode->lock);

    *ppos += read;
  }

  return read;
}

//=====================================
static bool
spfs_modify_start_cb(void *closure, struct spfs_inode *inode) {
  sector_t start = *((sector_t *)closure);

  BUG_ON(!S_ISREG(inode->i_inode.i_mode));
  BUG_ON(inode->start != 0);

  inode->start = start;

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
    int res;
    struct spfs_file_block desc = {
        /**/
        .next = 0,
        .capacity = blocks,
        .length = 0,
        /**/
    };
    size_t b_pos = 0;
    struct buffer_head *bh;

    bh = sb_bread(sb, result);
    BUG_ON(!bh);

    res = spfs_make_file_block(bh, &b_pos, &desc);
    if (res) {
      // cleanup
      return 0;
    }

    mark_buffer_dirty(bh); // XXX maybe sync?
    brelse(bh);
  }

  return result;
}

/*
 * Sends data to the device. If missing, -EINVAL is returned to the program
 * calling the write system call. The return value, if non-negative,
 * represents the number of bytes successfully written.
 *
 * On a regular file, if this incremented file offset is greater than the
 * length of the file, the length of the file shall be set to this file offset.
 */
static ssize_t
spfs_write(struct file *file, const char __user *in_buf, size_t in_len,
           loff_t *ppos) {
  int res;
  struct spfs_inode *inode;
  struct super_block *sb;
  struct spfs_super_block *sbi;

  ssize_t written = 0;
  loff_t pos;

  if (!ppos) {
    return -EINVAL;
  }

  /* XXX EFBIG            */

  pos = *ppos;

  printk(KERN_INFO "spfs_write()\n");

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

    // XXX interruptible
    mutex_lock(&inode->lock);
    start = inode->start;
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

      con = MIN(pos, spfs_sb_remaining(bh, bh_pos));
      pos -= con;
      bh_pos += con;
      if (pos == 0) {
        /* We have counted down to the desired position by traversing the file
         * blocks. Meaning we can now start to write from $in_buf to $bh
         */

        con = MIN(in_len, spfs_sb_remaining(bh, bh_pos));
        if (con > 0) {
          size_t ipos;

          copy_from_user(/*dest*/ bh->b_data + bh_pos, /*src*/ in_buf, con);
          in_buf += con;
          in_len -= con;
          bh_pos += con;
          block.length += con;
          written += con;

          /* Write back $block.length */
          ipos = 0;
          res = spfs_make_file_block(bh, &ipos, &block);
          if (res) {
            // cleanup
            return res;
          }

          block_dirty = true;
        }
      }

      if (in_len > 0) {
        /* We have handled current file block, onto the next.
         */
        if (block.next == 0) {
          size_t ipos;
          pos = 0;

          /* Allocate $block.next to make space for $in_buf */
          block.next = spfs_file_block_alloc(sb, in_len);
          if (!block.next) {
            // cleanup
            return -EINVAL;
          }

          /* Write back $block.next */
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
      /* We only get here we the initial $spfs_inode.start pointer is 0
       */
      start = spfs_file_block_alloc(sb, in_len);
      if (!start) {
        // cleanup
        return -EINVAL;
      }

      mutex_lock(&sbi->tree.lock); // XXX read lock
      res = spfs_btree_modify(&sbi->tree, inode->i_inode.i_ino, &start,
                              spfs_modify_start_cb);
      mutex_unlock(&sbi->tree.lock);

      if (res) {
        // cleanup
        return res;
      }

      inode->start = start;
      goto Lit;
    }

    BUG_ON(in_len != 0);

    if (written > 0) {
      loff_t sz = i_size_read(&inode->i_inode);
      i_size_write(&inode->i_inode, sz + written);
    }
    mutex_unlock(&inode->lock);
    *ppos += written;
  }

  return written;
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
  dir_emit(ctx, name, nlength, cur->i_inode.i_ino, DT_UNKNOWN);
  ++ctx->pos;

  return true;
}

static int
spfs_iterate(struct file *file, struct dir_context *ctx) {

  struct inode *parent;

  printk(KERN_INFO "spfs_iterate()\n");

  parent = file_inode(file);
  if (ctx->pos >= parent->i_size) {
    // TODO re-entrant pos how does it work?
    return 0;
  }

  spfs_for_all_children(SPFS_INODE(parent), ctx, spfs_iterate_cb);
  return 0;
}

static const struct file_operations spfs_dir_ops = {
    /**/
    .llseek = generic_file_llseek,
    .read = generic_read_dir,

    .iterate = spfs_iterate,
    /**/
};

//=====================================
static int
spfs_read_super_block(struct buffer_head *bh, struct spfs_super_block *super) {
  int res;
  size_t pos;
  unsigned long dummy;

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
  if (spfs_sb_read_u32(bh, &pos, &dummy)) {
    goto Lout;
  }

  if (spfs_sb_read_u32(bh, &pos, &super->id)) {
    goto Lout;
  }
  if (spfs_sb_read_u32(bh, &pos, &super->root_id)) {
    goto Lout;
  }

  if (spfs_sb_read_u32(bh, &pos, &super->btree_offset)) {
    goto Lout;
  }
  if (spfs_sb_read_u32(bh, &pos, &super->free_list_offset)) {
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
    printk(KERN_INFO "super->magic[%lu] != SPOOKY_FS_SUPER_MAGIC[%u]\n", //
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
  struct spfs_inode *root;
  struct spfs_super_block *sbi;
  int res;

  const sector_t super_start = 0;
  printk(KERN_INFO "spfs_kill_super_block()\n");

  sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
  if (!sbi) {
    return -ENOMEM;
  }

  /* Filesystem private info */
  sb->s_fs_info = sbi;
  /*  */
  /* sb->s_flags |= MS_NODIRATIME; */
  // doc
  sb->s_magic = SPOOKY_FS_SUPER_MAGIC;
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

  root = spfs_inode_by_id(sb, sbi->root_id);
  if (IS_ERR(root)) {
    root = spfs_new_inode(sb, NULL, S_IFDIR);
    if (IS_ERR(root)) {
      res = -ENOMEM;
      goto Lerr;
    }
    sbi->root_id = root->i_inode.i_ino;
  }

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

  memset(inode, 0, sizeof(*inode));
  mutex_init(&inode->lock);

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

  printk(KERN_INFO "spfs_write_inode()\n");

  sb = inode->i_sb;
  BUG_ON(!sb);

  sbi = sb->s_fs_info;
  BUG_ON(!sbi);

  mutex_lock(&sbi->tree.lock);
  res = spfs_btree_modify(&sbi->tree, inode->i_ino, inode, spfs_mod_inode_cb);
  mutex_unlock(&sbi->tree.lock);

  return res;
}

//=====================================
static struct file_system_type spfs_fs_type = {
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

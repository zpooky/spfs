#ifndef _SP_FS_UTIL_H
#define _SP_FS_UTIL_H

extern unsigned int
spfs_sb_remaining(struct buffer_head *bh, unsigned int pos);

extern bool
spfs_sb_read_u32(struct buffer_head *bh, unsigned int *pos, unsigned int *out);


extern bool
spfs_sb_write_u32(struct buffer_head *bh, unsigned int *pos, unsigned int val);

#endif

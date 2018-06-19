#ifndef _SP_FS_UTIL_H
#define _SP_FS_UTIL_H

#include <linux/buffer_head.h>

extern unsigned int
spfs_sb_remaining(struct buffer_head *bh, size_t pos);

extern bool
spfs_sb_read_u16(struct buffer_head *bh, size_t *pos, unsigned short *out);

extern bool
spfs_sb_write_u16(struct buffer_head *bh, size_t *pos, unsigned short val);

extern bool
spfs_sb_read_u32(struct buffer_head *bh, size_t *pos, unsigned long *out);

extern bool
spfs_sb_write_u32(struct buffer_head *bh, size_t *pos, unsigned long val);

extern bool
spfs_sb_read_u64(struct buffer_head *bh, size_t *pos, unsigned long long *out);

extern bool
spfs_sb_write_u64(struct buffer_head *bh, size_t *pos, unsigned long long val);

extern bool
spfs_sb_read_str(struct buffer_head *bh, size_t *pos, char *str, size_t len);

extern bool
spfs_sb_write_str(struct buffer_head *bh, size_t *pos, const char *str,
                  size_t len);

#endif

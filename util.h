#ifndef _SP_FS_UTIL_H
#define _SP_FS_UTIL_H

#include <linux/buffer_head.h>

extern unsigned int
spfs_sb_remaining(struct buffer_head *bh, unsigned int pos);

extern bool
spfs_sb_read_u32(struct buffer_head *bh, unsigned int *pos, unsigned int *out);

extern bool
spfs_sb_write_u32(struct buffer_head *bh, unsigned int *pos, unsigned int val);

extern bool
spfs_sb_read_sector_t(struct buffer_head *bh, unsigned int *pos, sector_t *out);

extern bool
spfs_sb_write_sector_t(struct buffer_head *bh, unsigned int *pos, sector_t val);

extern bool
spfs_sb_read_str(struct buffer_head *bh, unsigned int *pos, char *str,
                 size_t len);

extern bool
spfs_sb_write_str(struct buffer_head *bh, unsigned int *pos, const char *str,
                  size_t len);

#endif

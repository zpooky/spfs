#ifndef _SP_FS_UTIL_H
#define _SP_FS_UTIL_H

#include <linux/buffer_head.h>
#include <linux/types.h>

extern size_t
spfs_sb_remaining(struct buffer_head *bh, size_t pos);

extern bool
spfs_sb_read_u16(struct buffer_head *bh, size_t *pos, uint16_t *out);

extern bool
spfs_sb_write_u16(struct buffer_head *bh, size_t *pos, uint16_t val);

extern bool
spfs_sb_read_u32(struct buffer_head *bh, size_t *pos, uint32_t *out);

extern bool
spfs_sb_write_u32(struct buffer_head *bh, size_t *pos, uint32_t val);

extern bool
spfs_sb_read_u64(struct buffer_head *bh, size_t *pos, uint64_t *out);

extern bool
spfs_sb_write_u64(struct buffer_head *bh, size_t *pos, uint64_t val);

extern bool
spfs_sb_read_str(struct buffer_head *bh, size_t *pos, char *str, size_t len);

extern bool
spfs_sb_write_str(struct buffer_head *bh, size_t *pos, const char *str,
                  size_t len);

#endif

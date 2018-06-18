#include "util.h"

/* ===================================== */
unsigned int
spfs_sb_remaining(struct buffer_head *bh, unsigned int pos) {
  BUG_ON(pos > bh->b_size);
  return bh->b_size - pos;
}

/* ===================================== */
bool
spfs_sb_read_u32(struct buffer_head *bh, unsigned int *pos, unsigned int *out) {
  if (spfs_sb_remaining(bh, *pos) < sizeof(*out)) {
    return false;
  }

  memcpy(out, bh->b_data + *pos, sizeof(*out));
  *out = be32_to_cpu(*out);

  *pos += sizeof(*out);

  return true;
}

/* ===================================== */
bool
spfs_sb_write_u32(struct buffer_head *bh, unsigned int *pos, unsigned int val) {
  if (spfs_sb_remaining(bh, *pos) < sizeof(val)) {
    return false;
  }

  val = cpu_to_be32(val);
  memcpy(/*dest*/ bh->b_data + *pos, /*src*/ &val, sizeof(val));

  *pos += sizeof(val);

  return true;
}

/* ===================================== */
extern bool
spfs_sb_read_sector_t(struct buffer_head *bh, unsigned int *pos,
                      sector_t *out) {
  // TODO
  return true;
}

extern bool
spfs_sb_write_sector_t(struct buffer_head *bh, unsigned int *pos,
                       sector_t val) {
  // TODO
  return true;
}

/* ===================================== */
bool
spfs_sb_read_str(struct buffer_head *bh, unsigned int *pos, char *str,
                 size_t len) {
  // TODO
  return true;
}

/* ===================================== */
bool
spfs_sb_write_str(struct buffer_head *bh, unsigned int *pos, const char *str,
                  size_t len) {
  // TODO
  return true;
}

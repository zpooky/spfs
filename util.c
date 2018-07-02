#include "util.h"

/* ===================================== */
size_t
spfs_sb_remaining(struct buffer_head *bh, size_t pos) {
  BUG_ON(pos > bh->b_size);
  return bh->b_size - pos;
}

/* ===================================== */
extern bool
spfs_sb_read_u16(struct buffer_head *bh, size_t *pos, uint16_t *out) {
  if (spfs_sb_remaining(bh, *pos) < sizeof(*out)) {
    return false;
  }

  memcpy(out, bh->b_data + *pos, sizeof(*out));
  *out = be16_to_cpu(*out);

  *pos += sizeof(*out);

  return true;
}

extern bool
spfs_sb_write_u16(struct buffer_head *bh, size_t *pos, uint16_t val) {
  if (spfs_sb_remaining(bh, *pos) < sizeof(val)) {
    return false;
  }

  val = cpu_to_be16(val);
  memcpy(/*dest*/ bh->b_data + *pos, /*src*/ &val, sizeof(val));

  *pos += sizeof(val);

  return true;
}

/* ===================================== */
bool
spfs_sb_read_u32(struct buffer_head *bh, size_t *pos, uint32_t *out) {
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
spfs_sb_write_u32(struct buffer_head *bh, size_t *pos, uint32_t val) {
  if (spfs_sb_remaining(bh, *pos) < sizeof(val)) {
    return false;
  }

  val = cpu_to_be32(val);
  memcpy(/*dest*/ bh->b_data + *pos, /*src*/ &val, sizeof(val));

  *pos += sizeof(val);

  return true;
}

/* ===================================== */
bool
spfs_sb_read_u64(struct buffer_head *bh, size_t *pos, uint64_t *out) {
  if (spfs_sb_remaining(bh, *pos) < sizeof(*out)) {
    return false;
  }

  memcpy(out, bh->b_data + *pos, sizeof(*out));
  *out = be64_to_cpu(*out);

  *pos += sizeof(*out);

  return true;
}

bool
spfs_sb_write_u64(struct buffer_head *bh, size_t *pos, uint64_t val) {
  if (spfs_sb_remaining(bh, *pos) < sizeof(val)) {
    return false;
  }

  val = cpu_to_be64(val);
  memcpy(/*dest*/ bh->b_data + *pos, /*src*/ &val, sizeof(val));

  *pos += sizeof(val);

  return true;
}

/* ===================================== */
bool
spfs_sb_read_ino(struct buffer_head *bh, size_t *pos, unsigned long *out) {
  uint32_t dummy;
  if (spfs_sb_remaining(bh, *pos) < sizeof(*out)) {
    return false;
  }

  memcpy(&dummy, bh->b_data + *pos, sizeof(dummy));
  *out = be32_to_cpu(dummy);

  *pos += sizeof(dummy);

  return true;
}

bool
spfs_sb_write_ino(struct buffer_head *bh, size_t *pos, unsigned long val) {
  uint32_t dummy;
  if (spfs_sb_remaining(bh, *pos) < sizeof(val)) {
    return false;
  }

  dummy = cpu_to_be32(val);

  memcpy(/*dest*/ bh->b_data + *pos, /*src*/ &dummy, sizeof(dummy));

  *pos += sizeof(dummy);

  return true;
}

/* ===================================== */
bool
spfs_sb_read_sector(struct buffer_head *bh, size_t *pos, sector_t *out) {
  uint32_t dummy;
  if (spfs_sb_remaining(bh, *pos) < sizeof(*out)) {
    return false;
  }

  memcpy(/*dest*/ &dummy, bh->b_data + *pos, sizeof(dummy));
  *out = be32_to_cpu(dummy);

  *pos += sizeof(dummy);

  return true;
}

bool
spfs_sb_write_sector(struct buffer_head *bh, size_t *pos, sector_t val) {
  uint32_t dummy;
  if (spfs_sb_remaining(bh, *pos) < sizeof(val)) {
    return false;
  }

  dummy = val;
  dummy = cpu_to_be32(dummy);
  memcpy(/*dest*/ bh->b_data + *pos, /*src*/ &dummy, sizeof(dummy));

  *pos += sizeof(dummy);

  return true;
}

/* ===================================== */
bool
spfs_sb_read_str(struct buffer_head *bh, size_t *pos, char *out, size_t len) {
  if (spfs_sb_remaining(bh, *pos) < len) {
    return false;
  }

  memcpy(out, bh->b_data + *pos, len);
  *pos += len;

  return true;
}

bool
spfs_sb_write_str(struct buffer_head *bh, size_t *pos, const char *str,
                  size_t len) {
  if (spfs_sb_remaining(bh, *pos) < len) {
    return false;
  }

  memcpy(/*dest*/ bh->b_data + *pos, /*src*/ &str, len);
  *pos += len;

  return true;
}

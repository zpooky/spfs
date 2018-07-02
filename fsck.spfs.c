#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h> //memset
#include <sys/stat.h>
#include <sys/stat.h> //mode_t
#include <sys/types.h>
#include <unistd.h>

#include "mkfs.spfs.h"
#include "spfs.h"

static int
fsck_read_u32(int fd, uint32_t *data) {
  size_t len = sizeof(*data);
  ssize_t res;

  res = read(fd, data, len);
  if (res < 0) {
    return res;
  }
  *data = ntohl(*data);

  return len - res;
}

static void
print_super(struct spfs_super_block_wire *super) {
  printf("super:[magic:%u,version:%u,block_size:%u,id:%u,root_id:%u,btree_of:"
         "%u,free_of:%u]\n", //
         super->magic, super->version, super->block_size, super->id,
         super->root_id, super->btree_offset, super->free_list_offset);
}

static int
read_super_block(int fd, struct spfs_super_block_wire *super) {
  int res = 1;

  if (lseek(fd, 0, SEEK_SET) < 0) {
    fprintf(stderr, "failed super lseek(0), %s\n", strerror(errno));
    goto Lout;
  }

  if (fsck_read_u32(fd, &super->magic)) {
    fprintf(stderr, "failed super->magic\n");
    goto Lout;
  }

  if (fsck_read_u32(fd, &super->version)) {
    fprintf(stderr, "failed super->version\n");
    goto Lout;
  }
  if (fsck_read_u32(fd, &super->block_size)) {
    fprintf(stderr, "failed super->block_size\n");
    goto Lout;
  }
  if (fsck_read_u32(fd, &super->dummy)) {
    fprintf(stderr, "failed super->dummy\n");
    goto Lout;
  }

  if (fsck_read_u32(fd, &super->id)) {
    fprintf(stderr, "failed super->id\n");
    goto Lout;
  }
  if (fsck_read_u32(fd, &super->root_id)) {
    fprintf(stderr, "failed super->root_id\n");
    goto Lout;
  }

  if (fsck_read_u32(fd, &super->btree_offset)) {
    fprintf(stderr, "failed super->btree_offset\n");
    goto Lout;
  }
  if (fsck_read_u32(fd, &super->free_list_offset)) {
    fprintf(stderr, "failed super->free_list_offset\n");
    goto Lout;
  }

  print_super(super);

  res = 0;
Lout:
  return res;
}

static int
fsck_super_block(struct spfs_super_block_wire *super) {

  if (super->magic != SPOOKY_FS_SUPER_MAGIC) {
    fprintf(stderr, "incorrect super->magic[%u] != SPFS_MAGIC[%u]\n",
            super->magic, SPOOKY_FS_SUPER_MAGIC);
    return 1;
  }

  if (super->block_size < SPOOKY_FS_INITIAL_BLOCK_SIZE) {
    fprintf(stderr,
            "incorrect super->block_size[%u] < [%u] == false\n", //
            super->block_size, SPOOKY_FS_INITIAL_BLOCK_SIZE);
    return 1;
  }

  if (super->block_size % SPOOKY_FS_INITIAL_BLOCK_SIZE != 0) {
    fprintf(stderr,
            "incorrect super->block_size[%u] mod [%u] == 0\n", //
            super->block_size, SPOOKY_FS_INITIAL_BLOCK_SIZE);
    return 1;
  }

  return 0;
}

static int
read_free_list_header(int fd, struct spfs_free_list *header) {
  if (fsck_read_u32(fd, &header->magic)) {
    return 1;
  }
  if (fsck_read_u32(fd, &header->entries)) {
    return 1;
  }
  if (fsck_read_u32(fd, &header->next)) {
    return 1;
  }

  return 0;
}

static int
read_free_entry(int fd, struct spfs_free_entry *entry) {
  if (fsck_read_u32(fd, &entry->start)) {
    return 1;
  }
  if (fsck_read_u32(fd, &entry->blocks)) {
    return 1;
  }

  return 0;
}

static void
print_free_header(struct spfs_free_list *header) {
  printf("free_list:[magic:%u,entries:%u,next:%u]\n", //
         header->magic, header->entries, header->next);
}

static void
print_free_entry(struct spfs_free_entry *entry) {
  printf("\t- [start:%u,blocks:%u]\n", entry->start, entry->blocks);
}

static int
fsck_free_list(int fd, const struct spfs_super_block_wire *super) {
  size_t hdr;
  size_t i;
  off_t pos;
  uint32_t free_list = super->free_list_offset;

  hdr = 0;
  do {
    struct spfs_free_list header = {};

    pos = free_list * super->block_size;
    if (lseek(fd, pos, SEEK_SET) < 0) {
      fprintf(stderr, "failed super lseek(%jd), %s\n", pos, strerror(errno));
      return 1;
    }

    if (read_free_list_header(fd, &header)) {
      return 1;
    }

    if (header.magic != SPOOKY_FS_FL_MAGIC) {
      fprintf(stderr, "failed header.magic[%u] != SPOOKY_FS_FL_MAGIC[%u]\n", //
              header.magic, SPOOKY_FS_FL_MAGIC);
      return 1;
    }

    print_free_header(&header);

    for (i = 0; i < header.entries; ++i) {
      struct spfs_free_entry entry = {};
      if (read_free_entry(fd, &entry)) {
        return 1;
      }
      print_free_entry(&entry);
    }
    free_list = header.next;
    ++hdr;
  } while (free_list);

  return 0;
}

int
main(int argc, const char **args) {
  int res = 1;
  int fd = 0;
  if (argc > 1) {
    struct spfs_super_block_wire super;
    const char *device = args[1];

    fd = open(device, O_RDONLY);
    if (!fd) {
      fprintf(stderr, "failed to open('%s', O_RDONLY)\n", device);
      goto Ldone;
    }

    if (read_super_block(fd, &super)) {
      fprintf(stderr, "failed to read super block\n");
      goto Ldone;
    }
    if (fsck_super_block(&super)) {
      fprintf(stderr, "failed to parse super block\n");
      goto Ldone;
    }

    if (fsck_free_list(fd, &super)) {
      fprintf(stderr, "failed to parse free list\n");
      goto Ldone;
    }
    res = 0;
    goto Ldone;
  }

  res = 0;
  fprintf(stderr, "%s device\n", args[0]);
Ldone:
  if (fd) {
    close(fd);
  }
  return res;
}

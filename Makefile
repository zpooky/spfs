obj-m := spfs.o
spfs-objs := sp.o btree.o free_list.o util.o
ccflags-y := -DSPFS_DEBUG

MKFS := mkfs.spfs
FSCK := fsck.spfs
CXXFLAG := -std=gnu89 -Wall -Werror -Wextra

CFLAG_DEBUG := -g -DDEBUG

# CFLAGS_sp.o := -DDEBUG

all: $(MKFS) $(FSCK) ko_debug

ko:
	# make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	make -C $(HOME)/sources/linux M=$(PWD) modules

ko_debug:
	# make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	make -C $(HOME)/sources/linux M=$(PWD) modules
	EXTRA_CFLAGS="$(CFLAG_DEBUG)"

%.o: %.c
	$(CC) $(CXXFLAG) -c $< -o $@

$(MKFS): mkfs.spfs.o
	$(CC) $^ -o $@

$(FSCK): fsck.spfs.o
	$(CC) $^ -o $@

clean:
	rm $(MKFS)
	# make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	make -C $(HOME)/sources/linux M=$(PWD) clean


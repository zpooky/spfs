obj-m := spfs.o
spfs-objs := sp.o btree.o free_list.o util.o
ccflags-y := -DSPFS_DEBUG

MKFS := mkfs.spfs
OBJECTS := mkfs.spfs.o
CFLAGS := -std=gnu89 -Wall -Werror -Wextra

# CFLAGS_sp.o := -DDEBUG

all: $(MKFS) ko

ko:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(MKFS): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(MKFS)

clean:
	rm $(MKFS)
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean


obj-m := spfs.o
spfs-objs := sp.o btree.o
ccflags-y := -DSPFS_DEBUG

MKFS := mkfs.spfs
OBJECTS := mkfs.spfs.o
CXXFLAGS := -std=gnu89 -Wall -Werror -Wextra

all: $(MKFS) ko

ko:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

%.o: %.c
	$(CC) $(CXXFLAGS) -c $< -o $@

$(MKFS): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(MKFS)

clean:
	rm $(MKFS)
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

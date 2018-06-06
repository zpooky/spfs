obj-m := spfs.o
spfs-objs := simple.o
ccflags-y := -DSPFS_DEBUG

MKFS := mkfs.spfs
OBJECTS := mkfs.spfs.o
CXXFLAGS := -std=gnu89

all: ko $(MKFS)

ko:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

%.o: %.c
	$(CC) $(CXXFLAGS) -c $< -o $@

$(MKFS): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(MKFS)

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm mkfs-spfs

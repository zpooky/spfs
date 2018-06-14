# spfs

https://www.tldp.org/LDP/lkmpg/2.6/html/x569.html
https://github.com/cirosantilli/linux-kernel-module-cheat#setup-types
https://sysplay.in/blog/category/linux-device-drivers/

## hfs
super.c:hfs_fill_super()
  mdb.c:hfs_mdb_get(sb) # build fs, including btree & bitmaps

# example
- https://github.com/krinkinmu/aufs
  https://kukuruku.co/post/teaching-the-file-system-to-read/


#
Fiestel network inode gen?

# error ptr
```c
return ERR_PTR(-ENOMEM);
```

# QEMU shares
https://wiki.archlinux.org/index.php/QEMU#QEMU.27s_built-in_SMB_server

#debug

## enable build with debug symbols
make menuconfig
  ->kernel hacking
  ->Compile time checks and ...
  ->[]Compile the kernel with debug info(CONFIG_DEBUG_INFO)
https://mchehab.fedorapeople.org/kernel_docs/dev-tools/gdb-kernel-debugging.html

## disable Kernel address space layout randomization
qemu -append nokaslr

## gdb
```gdb
# to read debug symbols
gdb ./vmlinux # or in a running gdb instance
file vmlinux

info address init_uts_ns

# maybe remap where we look for src code
set substitute-path /build/linux-tt6jd0/linux-4.13.0 /home/alambert/kernel/home/alambert/kernel/source/linux-4.13.0
```
http://www.alexlambert.com/2017/12/18/kernel-debugging-for-newbies.html

##gdb
*linux-source*/scripts/gdb

##gdb2
./Documentation/dev-tools/gdb-kernel-debugging.rst

Note: Some distros may restrict auto-loading of gdb scripts to known safe
directories. In case gdb reports to refuse loading vmlinux-gdb.py, add::

```
add-auto-load-safe-path /path/to/linux-build
```

to ~/.gdbinit. See gdb help for more details.

```
(gdb) lx-symbols
loading vmlinux
scanning for modules in /home/user/linux/build
```

Dump the log buffer of the target kernel:
```
(gdb) lx-dmesg
[     0.000000] Initializing cgroup subsys cpuset
[     0.000000] Initializing cgroup subsys cpu
```


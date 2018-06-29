# spfs
https://www.tldp.org/LDP/lkmpg/2.6/html/x569.html
https://github.com/cirosantilli/linux-kernel-module-cheat#setup-types
https://sysplay.in/blog/category/linux-device-drivers/

# VFS
- https://kukuruku.co/post/writing-a-file-system-in-linux-kernel/

- https://github.com/krinkinmu/aufs
  https://kukuruku.co/post/teaching-the-file-system-to-read/

- https://www.win.tue.nl/~aeb/linux/lk/lk-8.html

- https://www.kernel.org/doc/html/latest/filesystems/index.html?highlight=vfs#the-linux-vfs
  https://www.kernel.org/doc/htmldocs/filesystems/vfs.html

- https://github.com/Harinlen/vvsfs/

# character device driver
- https://pete.akeo.ie/2011/08/writing-linux-device-driver-for-kernels.html
- http://derekmolloy.ie/writing-a-linux-kernel-module-part-1-introduction/
  http://derekmolloy.ie/writing-a-linux-kernel-module-part-2-a-character-device/

# device driver
- http://derekmolloy.ie/kernel-gpio-programming-buttons-and-leds/


#general
- https://github.com/gurugio/book_linuxkernel_blockdrv
- https://stackoverflow.com/questions/12589605/debug-info-for-loadable-kernel-modules "Debug-info for loadable kernel modules"
- https://opensourceforu.com/tag/linux-device-drivers-series/page/2/ "Linux Device Drivers Series"

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
gdb ./vmlinux
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


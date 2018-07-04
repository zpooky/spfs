#!/bin/sh

insmod spfs.ko
mount -t spfs -o loop spfs_simple.img mnt
ls -alh mnt

echo "cd mnt"
cd mnt

echo "touch 1"
touch 1

echo "asd" > asd

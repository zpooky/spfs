#!/bin/bash

the_dev="./fs_raw"
the_mount="./mnt"

the_ko_name="spfs"
the_ko="../${the_ko_name}.ko"

if [ ! -e "${the_dev}" ]; then
  dd if=/dev/zero of="${the_dev}" count=5 bs=1M
fi

echo "sudo insmod ${the_ko}"
sudo insmod "${the_ko}"
echo "sudo mount -t ${the_dev} ${the_dev} ${the_mount} -o loop"
sudo mount -t "${the_ko_name}" "${the_dev}" "${the_mount}" -o loop
echo "sudo ls ${the_mount}"
sudo ls "${the_mount}"
echo "sudo umount ${the_mount}"
sudo umount ${the_mount}
echo "sudo rmmod ${the_ko_name}"
sudo rmmod "${the_ko_name}"

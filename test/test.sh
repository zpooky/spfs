#!/bin/bash

the_dev="./fs_raw.img"
the_mount="./mnt"

the_ko_name="spfs"
the_ko="../${the_ko_name}.ko"

if [ ! -e "${the_dev}" ]; then
  echo "dd if=/dev/zero of=${the_dev} count=5 bs=1M"
  dd if=/dev/zero of="${the_dev}" count=5 bs=1M

  echo "\n../mkfs.spfs ${the_dev}"
  ../mkfs.spfs "${the_dev}"
fi

echo ""
echo "sudo insmod ${the_ko}"
sudo insmod "${the_ko}"

echo ""
echo "sudo mount -t ${the_ko_name} -o loop ${the_dev} ${the_mount}"
sudo mount -t "${the_ko_name}" -o loop "${the_dev}" "${the_mount}"

sleep 10

echo ""
echo "sudo ls ${the_mount}"
sudo ls "${the_mount}"

sleep 10

echo ""
echo "sudo umount ${the_mount}"
sudo umount ${the_mount}

echo ""
echo "sudo rmmod ${the_ko_name}"
sudo rmmod "${the_ko_name}"

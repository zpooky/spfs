#!/bin/bash

the_dev="./fs_raw"
the_mount="./mnt"

the_ko_name="spfs"
the_ko="../${the_ko_name}.ko"

if [ ! -e "${the_dev}" ]; then
  echo "dd if=/dev/zero of=${the_dev} count=5 bs=1M"
  dd if=/dev/zero of="${the_dev}" count=5 bs=1M

  echo "\n../mkfs.spfs ${the_dev}"
  ../mkfs.spfs "${the_dev}"
fi

echo "\nsudo insmod ${the_ko}"
sudo insmod "${the_ko}"

echo "\nsudo mount -t ${the_dev} ${the_dev} ${the_mount} -o loop"
sudo mount -t "${the_ko_name}" "${the_dev}" "${the_mount}" -o loop

echo "\nsudo ls ${the_mount}"
sudo ls "${the_mount}"

echo "\nsudo umount ${the_mount}"
sudo umount ${the_mount}

echo "\nsudo rmmod ${the_ko_name}"
sudo rmmod "${the_ko_name}"

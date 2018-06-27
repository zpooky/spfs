#!/bin/bash

# mount: /home/spooky/development/repos/spfs/test/mnt: wrong fs type, bad option,
# bad superblock on /dev/loop0, missing codepage or helper program, or other
# error.

the_dev="./fs_raw.img"
the_mount="./mnt"

the_ko_name="spfs"
the_ko="../${the_ko_name}.ko"

su_do=""
su_do="sudo"

if [ ! -e "${the_dev}" ]; then
  echo "dd if=/dev/zero of=${the_dev} count=5 bs=1M"
  dd if=/dev/zero of="${the_dev}" count=5 bs=1M

  echo "\n../mkfs.spfs ${the_dev}"
  ../mkfs.spfs "${the_dev}"
fi

echo ""
echo "${su_do} insmod ${the_ko}"
${su_do} insmod "${the_ko}"

echo ""
echo "${su_do} mount -t ${the_ko_name} -o loop ${the_dev} ${the_mount}"
${su_do} mount -t "${the_ko_name}" -o loop "${the_dev}" "${the_mount}"

sleep 10

echo ""
echo "${su_do} ls ${the_mount}"
${su_do} ls "${the_mount}"

sleep 10

echo ""
echo "${su_do} umount ${the_mount}"
${su_do} umount ${the_mount}

echo ""
echo "${su_do} rmmod ${the_ko_name}"
${su_do} rmmod "${the_ko_name}"

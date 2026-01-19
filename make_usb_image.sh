#!/bin/bash
set -e

# Variables
IMG=usb.img
MNT=mnt
SIZE_MB=64
KERNEL=build/kernel.elf
LIMINE_DIR=build  # Limine binaries are in the build directory

# 1. Create empty image
dd if=/dev/zero of=$IMG bs=1M count=$SIZE_MB

# 2. Format as FAT32
mkfs.fat -F 32 $IMG

# 3. Mount image
mkdir -p $MNT
sudo mount -o loop $IMG $MNT

# 4. Copy files
sudo cp $KERNEL $MNT/
sudo cp limine.conf $MNT/
sudo cp $LIMINE_DIR/limine.sys $MNT/
sudo cp $LIMINE_DIR/limine-cd.bin $MNT/
sudo cp $LIMINE_DIR/limine-efi.sys $MNT/
sudo mkdir -p $MNT/EFI/BOOT
sudo cp $LIMINE_DIR/BOOTX64.EFI $MNT/EFI/BOOT/

# 5. Unmount
sudo umount $MNT

# 6. Install Limine
$LIMINE_DIR/limine-install $IMG

echo "USB image $IMG is ready. Write it to your USB stick with:"
echo "sudo dd if=$IMG of=/dev/sdX bs=4M status=progress && sync"
echo "Or use Rufus on Windows to write $IMG to your USB stick."

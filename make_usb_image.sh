echo "USB image $IMG is ready. Write it to your USB stick with:"
echo "sudo dd if=$IMG of=/dev/sdX bs=4M status=progress && sync"
echo "Or use Rufus on Windows to write $IMG to your USB stick."

#!/bin/bash
set -e

# Ensure required build dependencies are installed
echo "Checking for required build dependencies..."
if command -v apt-get >/dev/null; then
    sudo apt-get update
    sudo apt-get install -y lld nasm mtools gcc make autoconf automake libtool pkg-config curl git
elif command -v dnf >/dev/null; then
    sudo dnf install -y lld nasm mtools gcc make autoconf automake libtool pkgconf-pkg-config curl git
elif command -v pacman >/dev/null; then
    sudo pacman -Sy --noconfirm lld nasm mtools gcc make autoconf automake libtool pkgconf curl git
elif command -v zypper >/dev/null; then
    sudo zypper install -y lld nasm mtools gcc make autoconf automake libtool pkgconf curl git
else
    echo "Please install lld nasm mtools gcc make autoconf automake libtool pkg-config curl git manually."
fi

#!/bin/bash
set -e

# Variables
IMG=usb.img
MNT=mnt
SIZE_MB=64
KERNEL=build/kernel.elf
LIMINE_VERSION=8.6.0
LIMINE_URL="https://github.com/limine-bootloader/limine/raw/v${LIMINE_VERSION}-binary/BOOTX64.EFI"
BOOTX64=build/BOOTX64.EFI

# Download Limine BOOTX64.EFI if missing
if [ ! -f "$BOOTX64" ]; then
	echo "Downloading Limine BOOTX64.EFI..."
	mkdir -p build
	curl -L -o "$BOOTX64" "$LIMINE_URL"
fi

# 1. Create empty image
dd if=/dev/zero of=$IMG bs=1M count=$SIZE_MB

# 2. Format as FAT32
mkfs.fat -F 32 $IMG

# 3. Mount image
mkdir -p $MNT
# Unmount if already mounted
if mount | grep -q "$PWD/$IMG on $PWD/$MNT"; then
	echo "$IMG is already mounted, unmounting..."
	sudo umount $MNT
fi
sudo mount -o loop $IMG $MNT

# 4. Copy files
sudo mkdir -p $MNT/EFI/BOOT
sudo cp "$BOOTX64" $MNT/EFI/BOOT/BOOTX64.EFI
sudo cp limine.conf $MNT/
sudo cp "$KERNEL" $MNT/
for f in build/user/*.elf; do
	sudo cp "$f" $MNT/
done

# 5. Unmount
sudo umount $MNT


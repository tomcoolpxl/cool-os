#!/bin/bash
set -e

IMG=usb.img

if [ ! -f "$IMG" ]; then
    echo "Error: $IMG not found. Run make_usb_image.sh first."
    exit 1
fi

# Use OVMF for UEFI boot
OVMF_CODE=/usr/share/edk2/x64/OVMF_CODE.4m.fd
OVMF_VARS=build/OVMF_VARS.4m.fd

# Copy OVMF_VARS if needed
if [ ! -f "$OVMF_VARS" ]; then
    cp /usr/share/edk2/x64/OVMF_VARS.4m.fd "$OVMF_VARS"
fi

qemu-system-x86_64 \
    -enable-kvm \
    -cpu host \
    -m 256M \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$OVMF_VARS" \
    -drive format=raw,file="$IMG",if=ide \
    -serial stdio \
    -display gtk \
    -boot order=d

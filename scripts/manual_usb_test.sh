#!/bin/bash
set -e

# Configuration
BUILD_DIR="build"
DIST_DIR="${BUILD_DIR}/dist"
IMG="${DIST_DIR}/cool-os-debug.img"
OVMF_CODE="${OVMF_CODE:-/usr/share/edk2/x64/OVMF_CODE.4m.fd}"
OVMF_VARS="${BUILD_DIR}/OVMF_VARS.4m.fd"

# Ensure OVMF_VARS exists
if [ ! -f "${OVMF_VARS}" ]; then
    if [ -f "/usr/share/edk2/x64/OVMF_VARS.4m.fd" ]; then
        cp /usr/share/edk2/x64/OVMF_VARS.4m.fd "${OVMF_VARS}"
    else
        echo "Error: OVMF_VARS not found"
        exit 1
    fi
fi

echo "Starting cool-os with USB Keyboard ONLY (PS/2 disabled)..."
echo "Please type a letter in the QEMU window."
echo "If USB is working, you will see the character echoed on screen."
echo "Close the QEMU window to finish the test."

qemu-system-x86_64 \
    -enable-kvm \
    -cpu host \
    -m 256M \
    -no-reboot \
    -no-shutdown \
    -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
    -drive if=pflash,format=raw,file="${OVMF_VARS}" \
    -drive format=raw,file="${IMG}" \
    -device qemu-xhci \
    -device usb-kbd \
    -serial stdio \
    -display gtk \
    -machine pc,i8042=off

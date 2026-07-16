#!/bin/bash
set -euo pipefail

FILE="disk.img"
TARGET_SIZE="100M"
MODE="${1:-bios}"

if [ ! -f "$FILE" ]; then
    qemu-img create -f raw "$FILE" "$TARGET_SIZE"
    echo "Created filesystem image"
else
    qemu-img resize -f raw "$FILE" "$TARGET_SIZE" >/dev/null
fi

case "$MODE" in
    bios)
        qemu-system-x86_64 -cdrom iso/output/mms-os.iso -drive format=raw,file="$FILE" -audiodev pa,id=speaker -machine pcspk-audiodev=speaker
        ;;
    uefi)
        if [ ! -f iso/output/mms-os-uefi-usb.img ]; then
            echo "Missing UEFI USB image. Run ./build.sh usb first." >&2
            exit 1
        fi
        OVMF_CODE="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}"
        if [ ! -f "$OVMF_CODE" ]; then
            echo "Missing OVMF firmware at $OVMF_CODE. Set OVMF_CODE=/path/to/OVMF_CODE.fd." >&2
            exit 1
        fi
        qemu-system-x86_64 \
            -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
            -drive format=raw,file=iso/output/mms-os-uefi-usb.img,if=virtio \
            -drive format=raw,file="$FILE" \
            -audiodev pa,id=speaker -machine pcspk-audiodev=speaker
        ;;
    *)
        echo "Usage: $0 [bios|uefi]" >&2
        exit 2
        ;;
esac

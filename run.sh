#!/bin/bash
set -e

FILE="disk.img"

if [ -f "$FILE" ]; then
    qemu-system-x86_64 -cdrom iso/output/mms-os.iso -drive format=raw,file=disk.img -audiodev pa,id=speaker -machine pcspk-audiodev=speaker
else
    qemu-img create -f raw disk.img 10M
    echo "Created filesystem image"
    qemu-system-x86_64 -cdrom iso/output/mms-os.iso -drive format=raw,file=disk.img -audiodev pa,id=speaker -machine pcspk-audiodev=speaker
fi
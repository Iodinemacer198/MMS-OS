#!/bin/bash
set -euo pipefail

BUILD_DIR="iso/build"
ISO_ROOT="iso/root"
OUT_DIR="iso/output"
KERNEL_BIN="$BUILD_DIR/kernel.bin"
USB_IMG="$OUT_DIR/mms-os-uefi-usb.img"

CC=${CC:-gcc}
LD=${LD:-ld}
NASM=${NASM:-nasm}

CFLAGS=(-m32 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -Ikernel/fs -Ikernel/commands -Ikernel/commands/calc -Ikernel/commands/login -Ikernel/commands/fs -Ikernel/commands/vgag -Ikernel/commands/wordle -Ikernel/commands/tcc -O2 -Wall)
LDFLAGS=(-m elf_i386 -T linker.ld -nostdlib)

require() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required tool: $1" >&2
        return 1
    fi
}

have() { command -v "$1" >/dev/null 2>&1; }

compile_kernel() {
    mkdir -p "$BUILD_DIR" "$ISO_ROOT/boot/grub" "$OUT_DIR"

    echo "Compiling kernel (32-bit freestanding Multiboot)..."
    "$CC" "${CFLAGS[@]}" -c kernel/kernel.c -o "$BUILD_DIR/kernel.o"

    echo "Compiling drivers and commands..."
    "$CC" "${CFLAGS[@]}" -c kernel/fs/ata.c -o "$BUILD_DIR/ata.o"
    "$CC" "${CFLAGS[@]}" -c kernel/fs/fs.c -o "$BUILD_DIR/fs.o"
    "$CC" "${CFLAGS[@]}" -c kernel/commands/calc/calc.c -o "$BUILD_DIR/calc.o"
    "$CC" "${CFLAGS[@]}" -c kernel/commands/login/login.c -o "$BUILD_DIR/login.o"
    "$CC" "${CFLAGS[@]}" -c kernel/commands/wordle/wordle.c -o "$BUILD_DIR/wordle.o"
    "$CC" "${CFLAGS[@]}" -c kernel/commands/fs/fsc.c -o "$BUILD_DIR/fsc.o"
    "$CC" "${CFLAGS[@]}" -c kernel/commands/vgag/vgag.c -o "$BUILD_DIR/vgag.o"
    "$CC" "${CFLAGS[@]}" -c kernel/commands/tcc/tinycc.c -o "$BUILD_DIR/tinycc.o"

    echo "Assembling Multiboot entry..."
    "$NASM" -f elf32 boot/boot.asm -o "$BUILD_DIR/boot.o"

    echo "Linking kernel..."
    "$LD" "${LDFLAGS[@]}" -o "$KERNEL_BIN" \
        "$BUILD_DIR/boot.o" "$BUILD_DIR/kernel.o" "$BUILD_DIR/ata.o" "$BUILD_DIR/fs.o" \
        "$BUILD_DIR/calc.o" "$BUILD_DIR/login.o" "$BUILD_DIR/wordle.o" "$BUILD_DIR/fsc.o" \
        "$BUILD_DIR/tinycc.o" "$BUILD_DIR/vgag.o"

    cp "$KERNEL_BIN" "$ISO_ROOT/boot/kernel.bin"
}

build_iso() {
    compile_kernel
    if have grub-mkrescue; then
        echo "Building BIOS/UEFI ISO..."
        grub-mkrescue -o "$OUT_DIR/mms-os.iso" "$ISO_ROOT"
    else
        echo "Skipping ISO: grub-mkrescue is not installed." >&2
    fi
}

build_uefi_usb() {
    compile_kernel
    require grub-mkstandalone
    require mformat
    require mcopy

    local esp="$BUILD_DIR/uefi-esp.img"
    local grub_efi="$BUILD_DIR/BOOTX64.EFI"
    local grub_cfg="$ISO_ROOT/boot/grub/grub.cfg"

    echo "Building standalone x86_64 UEFI GRUB loader..."
    grub-mkstandalone -O x86_64-efi -o "$grub_efi" \
        --locales="" --fonts="" \
        --modules="all_video boot configfile fat gfxterm multiboot normal part_gpt part_msdos search search_fs_file" \
        "boot/grub/grub.cfg=$grub_cfg"

    echo "Creating FAT32 USB image..."
    rm -f "$USB_IMG" "$esp"
    truncate -s 64M "$USB_IMG"
    cp "$USB_IMG" "$esp"
    mformat -i "$esp" -F ::
    mmd -i "$esp" ::/EFI ::/EFI/BOOT ::/boot ::/boot/grub
    mcopy -i "$esp" "$grub_efi" ::/EFI/BOOT/BOOTX64.EFI
    mcopy -i "$esp" "$ISO_ROOT/boot/kernel.bin" ::/boot/kernel.bin
    mcopy -i "$esp" "$grub_cfg" ::/boot/grub/grub.cfg
    mv "$esp" "$USB_IMG"
    echo "UEFI USB image ready: $USB_IMG"
    echo "Write it with: sudo dd if=$USB_IMG of=/dev/sdX bs=4M conv=fsync status=progress"
}

case "${1:-all}" in
    kernel) compile_kernel ;;
    iso) build_iso ;;
    usb|uefi-usb) build_uefi_usb ;;
    all) build_iso; if have grub-mkstandalone && have mformat && have mcopy; then build_uefi_usb; else echo "Skipping UEFI USB image: install grub-efi-amd64-bin and mtools." >&2; fi ;;
    clean) rm -rf "$BUILD_DIR" "$OUT_DIR" ;;
    *) echo "Usage: $0 [kernel|iso|usb|all|clean]" >&2; exit 2 ;;
esac

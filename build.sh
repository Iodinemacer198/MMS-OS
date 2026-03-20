#!/bin/bash
set -e

mkdir -p iso/build
mkdir -p iso/root/boot
mkdir -p iso/output

echo "Compiling kernel (32-bit)..."
gcc -m32 -ffreestanding -Ikernel/fs -Ikernel/commands -Ikernel/commands/calc -Ikernel/commands/login -Ikernel/commands/fs -Ikernel/commands/wordle -Ikernel/commands/tcc -c kernel/kernel.c -o iso/build/kernel.o -O2 -Wall

echo "Compiling file system drivers..."
gcc -m32 -ffreestanding -c kernel/fs/ata.c -o iso/build/ata.o -O2 -Wall
gcc -m32 -ffreestanding -c kernel/fs/fs.c -o iso/build/fs.o -O2 -Wall
gcc -m32 -ffreestanding -c kernel/commands/calc/calc.c -o iso/build/calc.o -O2 -Wall
gcc -m32 -ffreestanding -c kernel/commands/login/login.c -o iso/build/login.o -O2 -Wall
gcc -m32 -ffreestanding -c kernel/commands/wordle/wordle.c -o iso/build/wordle.o -O2 -Wall
gcc -m32 -ffreestanding -c kernel/commands/fs/fsc.c -o iso/build/fsc.o -O2 -Wall
gcc -m32 -ffreestanding -Ikernel/commands/tcc -c kernel/commands/tcc/tinycc.c -o iso/build/tinycc.o -O2 -Wall

echo "Assembling boot code..."
nasm -f elf32 boot/boot.asm -o iso/build/boot.o

echo "Linking kernel..."
ld -m elf_i386 -T linker.ld -o iso/build/kernel.bin \
iso/build/boot.o iso/build/kernel.o iso/build/ata.o iso/build/fs.o iso/build/calc.o iso/build/login.o iso/build/wordle.o iso/build/fsc.o iso/build/tinycc.o

echo "Copying kernel..."
cp iso/build/kernel.bin iso/root/boot/kernel.bin

echo "Building ISO..."
grub-mkrescue -o iso/output/mms-os.iso iso/root

echo "Build complete!"
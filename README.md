<img width="500" height="250" alt="mmslogo" src="https://github.com/user-attachments/assets/f37abe1a-a8dd-4546-957a-198538198ba6" />

Molecular Multiverse Services Operating System, or simply MMS-OS, is a WIP operating system made with C.

## Current Features & Overview
- Command line interface like setup
- Calculator
- Wordle game
- Basic file system with commands
- Login system
- A WIP VGA "graphics" (VGAG) system 
    - ┕> New login graphics
    - ┕> Welcome screen
    - ┕> Calculator
    - ┕> Bouncing ball simulation
    - ┕> Custom "program" support
    - ┕> Basic file explorer

## Boot architecture
MMS-OS now builds as a 32-bit freestanding Multiboot kernel that can be loaded by GRUB from either legacy BIOS or modern x86_64 UEFI firmware.

The boot path is:
1. Firmware starts GRUB (`BOOTX64.EFI` on UEFI USB media, or GRUB El Torito on BIOS ISO media).
2. GRUB loads `/boot/kernel.bin` through the Multiboot protocol.
3. `boot/boot.asm` enters the kernel, preserves the Multiboot magic and information pointer, and calls `kernel_main`.
4. The kernel runs in 32-bit protected mode, initializes the Multiboot framebuffer when UEFI/GRUB provides one, and uses the existing ATA/file-system and command architecture.

> Note: Secure Boot must be disabled unless you sign the generated `BOOTX64.EFI` yourself. MMS-OS now renders its main text console to the Multiboot framebuffer for UEFI boots, but some WIP VGAG paths and keyboard input still have legacy PC hardware assumptions.

## Use/Build instructions
**Build from source:** (recommended)
- Requires a Linux system or Windows Subsystem for Linux.
- Install build tools: `gcc` with 32-bit support, `nasm`, `binutils`, GRUB tools, and `mtools`.
  - Debian/Ubuntu example: `sudo apt install build-essential gcc-multilib nasm binutils grub-pc-bin grub-efi-amd64-bin xorriso mtools qemu-system-x86 ovmf`
- Git clone project to your preferred directory.
- Make sure you are in the project directory in your terminal.
- Run `chmod +x build.sh run.sh`.
- Run one of:
  - `./build.sh kernel` - compile/link only `iso/build/kernel.bin`.
  - `./build.sh iso` - build the BIOS/GRUB ISO at `iso/output/mms-os.iso`.
  - `./build.sh usb` - build a FAT32 UEFI USB image at `iso/output/mms-os-uefi-usb.img`.
  - `./build.sh all` - build every image that your installed tools support.

**Run in QEMU:**
- BIOS ISO: `./run.sh bios`
- UEFI USB image: `./run.sh uefi`
  - If OVMF is installed somewhere else, run `OVMF_CODE=/path/to/OVMF_CODE.fd ./run.sh uefi`.

**Write the UEFI USB image to real hardware:**
1. Build it with `./build.sh usb`.
2. Identify the USB block device carefully with `lsblk`.
3. Write the image, replacing `/dev/sdX` with the whole USB device, not a partition:
   ```sh
   sudo dd if=iso/output/mms-os-uefi-usb.img of=/dev/sdX bs=4M conv=fsync status=progress
   ```
4. Reboot, open your firmware boot menu, disable Secure Boot if needed, and choose the USB device's UEFI entry.


**Troubleshooting UEFI boot:**
- If GRUB reports `file /boot/kernel.bin not found`, rebuild the USB image with `./build.sh usb` so the image contains both `/EFI/BOOT/BOOTX64.EFI` and `/boot/kernel.bin`. The GRUB config searches the booted FAT volume for `/boot/kernel.bin` before starting MMS-OS.
- If the machine refuses to start `BOOTX64.EFI`, disable Secure Boot or sign the generated EFI binary.

**Use from release ISO:** (iso/output)
- Load ISO in your system of choice. It should work fine out the box.
- For real UEFI hardware, prefer the generated USB image over optical ISO emulation.

## Contact
This project is developed and maintained by the Molecular Multiverse Services team (just me so far, therealiodinemacer :D)
For any questions, suggestions, issues, etc, feel free to reach out to @therealiodinemacer on Discord or join our server [here](https://discord.gg/ZAx3NN5TJY)

## Roadmap
- ~~Better filesystem (preferably FAT)~~
- ~~Better command handling~~
- Possible basic graphics implementation (WIP!)

## Disclaimers
MMS-OS is a VERY work-in-progress project. Lag, bugs, etc are to be expected. As more of a hobby project, also expect slow progress and a lack of functionality. This project is just for fun, don't expect much :)

Some AI was used in the making of this project, especially with parts of the initial setup or other complicated pieces. I really don't know too much of C, and this is the easiest way for me to learn the language while also making progress. Outside of the initial setup and components that I needed help with, it is however my work. If you doubt that, check some of the scripts, there's no way anyone other than a stupid human could make some of those...

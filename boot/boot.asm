MBOOT_MAGIC equ 0x1BADB002
MBOOT_PAGE_ALIGN equ 1 << 0
MBOOT_MEMORY_INFO equ 1 << 1
MBOOT_VIDEO_MODE equ 1 << 2
MBOOT_FLAGS equ MBOOT_PAGE_ALIGN | MBOOT_MEMORY_INFO | MBOOT_VIDEO_MODE

section .multiboot
align 4
dd MBOOT_MAGIC
dd MBOOT_FLAGS
dd -(MBOOT_MAGIC + MBOOT_FLAGS)
dd 0 ; header_addr
dd 0 ; load_addr
dd 0 ; load_end_addr
dd 0 ; bss_end_addr
dd 0 ; entry_addr
dd 0 ; mode_type: linear graphics
dd 1024 ; width
dd 768 ; height
dd 32 ; depth

section .text
global _start
extern kernel_main

_start:
    push ebx ; Multiboot information pointer
    push eax ; Multiboot magic
    call kernel_main
    add esp, 8
hang:
    cli
    hlt
    jmp hang

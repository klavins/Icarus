; Multiboot header constants
MBALIGN  equ 1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ 1 << 1            ; provide memory map
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002        ; multiboot magic number
CHECKSUM equ -(MAGIC + FLAGS)

; Multiboot header (must be in first 8KB of kernel binary)
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; Reserve a 16KB stack
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; Entry point
section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top      ; set up the stack
    push ebx                ; push multiboot info pointer
    push eax                ; push multiboot magic number
    call kernel_main        ; call our C kernel
    cli                     ; disable interrupts
.hang:
    hlt                     ; halt the CPU
    jmp .hang               ; loop forever if we somehow wake up

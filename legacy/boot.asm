; boot.asm - Multiboot entry point and initial stack setup
;
; Copyright (C) 2026 Eric Klavins
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program. If not, see <https://www.gnu.org/licenses/>.

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

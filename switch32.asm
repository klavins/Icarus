; switch32.asm — Switch CPU from 64-bit long mode to 32-bit protected mode
;
; This code gets copied to address 0x1000 at runtime by uefi_boot64.c,
; then called there. This avoids PIC issues since we know our address.
;
; Entry: rdi = kernel entry address (32-bit), rsi = unused
; Uses System V AMD64 ABI.

bits 64

section .text
global switch_to_32bit
global switch_to_32bit_end

; This code will be copied to 0x1000 and executed there
switch_to_32bit:
    cli

    ; Save kernel entry address
    mov ebx, edi

    ; Load GDT (at known address: 0x1000 + gdt_offset)
    lgdt [0x1000 + (gdt_ptr - switch_to_32bit)]

    ; Far jump to compatibility mode
    ; Push segment:offset for retfq
    push 0x08
    push 0x1000 + (compat_mode - switch_to_32bit)
    retfq

bits 32
compat_mode:
    ; 32-bit compatibility mode
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Disable paging
    mov eax, cr0
    and eax, 0x7FFFFFFF
    mov cr0, eax

    ; Flush TLB
    xor eax, eax
    mov cr3, eax

    ; Disable PAE
    mov eax, cr4
    and eax, 0xFFFFFFDF
    mov cr4, eax

    ; Disable long mode
    mov ecx, 0xC0000080
    rdmsr
    and eax, 0xFFFFFEFF
    wrmsr

    ; Enable protected mode (no paging)
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to flush CS
    db 0xEA
    dd 0x1000 + (pm32 - switch_to_32bit)
    dw 0x08

pm32:
    ; Set up stack
    mov esp, 0x00090000

    ; kernel_main(0, NULL)
    push 0
    push 0
    jmp ebx

; ---- GDT ----
align 16
gdt_start:
    dq 0                        ; null
    dw 0xFFFF, 0x0000           ; 32-bit code: limit, base low
    db 0x00, 0x9A, 0xCF, 0x00  ; base mid, access, flags, base high
    dw 0xFFFF, 0x0000           ; 32-bit data: limit, base low
    db 0x00, 0x92, 0xCF, 0x00  ; base mid, access, flags, base high
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd 0x1000 + (gdt_start - switch_to_32bit)
    dd 0   ; padding for 64-bit lgdt

switch_to_32bit_end:
    nop

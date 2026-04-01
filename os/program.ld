/* Linker script for ICARUS user programs */
OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS {
    . = 0x800000;

    .text : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.* COMMON) }

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}

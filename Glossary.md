# Glossary

**[AHCI](https://en.wikipedia.org/wiki/Advanced_Host_Controller_Interface)** — Advanced Host Controller Interface. The standard interface for SATA disk controllers. Memory-mapped, unlike the older ATA PIO port-based interface.

**[ATA](https://en.wikipedia.org/wiki/Parallel_ATA)** — Advanced Technology Attachment. The legacy hard drive interface using I/O ports (0x1F0-0x1F7). Our disk driver uses ATA PIO (Programmed I/O) mode to read/write sectors.

**[BIOS](https://en.wikipedia.org/wiki/BIOS)** — Basic Input/Output System. The original PC firmware from 1981. Runs in 16-bit real mode, provides hardware initialization and boot services. Replaced by UEFI on modern systems.

**[BSS](https://en.wikipedia.org/wiki/.bss)** — Block Started by Symbol. The section of a binary that holds uninitialized global variables. Takes no space in the file — the loader allocates zeroed memory for it. In our project, the BSS holds the large static arrays: the shadow framebuffer (~9MB for double buffering), the framebuffer save buffer (~9MB for GRAPHICS mode switching), BASIC variable tables, the program store, and the keyboard/system status area.

**[EFI](https://en.wikipedia.org/wiki/UEFI)** — Extensible Firmware Interface. Apple's name for their UEFI implementation. Now generally synonymous with UEFI.

**[ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)** — Executable and Linkable Format. The standard binary format for Linux and Unix. Our BIOS kernel uses ELF. The UEFI build converts from ELF to PE.

**[GOP](https://en.wikipedia.org/wiki/UEFI#Graphics_features)** — Graphics Output Protocol. The UEFI service that provides a pixel framebuffer. Replaces VGA text mode on modern systems. Returns the framebuffer address, resolution, and pixel format.

**[GDT](https://en.wikipedia.org/wiki/Global_Descriptor_Table)** — Global Descriptor Table. An x86 CPU structure that defines memory segments (code, data). Must be set up before the CPU can handle interrupts.

**[IDT](https://en.wikipedia.org/wiki/Interrupt_descriptor_table)** — Interrupt Descriptor Table. An x86 CPU structure that maps interrupt numbers to handler functions. Entry 32 = timer, entry 33 = keyboard in our system.

**ISR** — Interrupt Service Routine. The function that runs when a hardware interrupt fires. Must save all registers, do its work quickly, send EOI to the PIC, and return with `iretq`.

**[PE](https://en.wikipedia.org/wiki/Portable_Executable)** — Portable Executable. The binary format used by Windows and UEFI. A `.efi` file is a PE executable. We convert from ELF to PE using `objcopy` for the UEFI build.

**[PIC](https://en.wikipedia.org/wiki/Intel_8259)** — Programmable Interrupt Controller (Intel 8259A). Hardware chip that routes device interrupts (keyboard, timer) to the CPU. Must be remapped so IRQs don't conflict with CPU exceptions.

**[PIT](https://en.wikipedia.org/wiki/Intel_8253)** — Programmable Interval Timer (Intel 8253/8254). Hardware chip that generates periodic timer interrupts. Runs at 1,193,182 Hz divided by a programmable divisor. We set it to 200 Hz (5ms per tick). Drives the DELAY command for game timing, maintains the system tick counter readable via PEEK, and also controls the PC speaker frequency for the SOUND command.

**[UEFI](https://en.wikipedia.org/wiki/UEFI)** — Unified Extensible Firmware Interface. The modern replacement for BIOS. Provides boot services, graphics (GOP), memory maps, and loads PE executables from FAT32 partitions.
- *QEMU:* [EDK II](https://en.wikipedia.org/wiki/TianoCore_EDK_II) (TianoCore) — Intel's open-source reference UEFI implementation, bundled with QEMU as OVMF firmware files.
- *Real hardware:* [American Megatrends (AMI)](https://en.wikipedia.org/wiki/American_Megatrends) — Commercial UEFI firmware on the ASUS motherboard with AMD Ryzen 5 2600X.

**[VGA](https://en.wikipedia.org/wiki/Video_Graphics_Array)** — Video Graphics Array. The legacy display standard from 1987. Text mode at 0xB8000 (80x25 characters) and Mode 13h at 0xA0000 (320x200 pixels). Not available on modern UEFI systems.

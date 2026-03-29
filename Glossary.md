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

**[Paging](https://en.wikipedia.org/wiki/Memory_paging)** — The CPU's memory translation system. Every memory access goes through a 4-level page table (PML4 → PDPT → PD → PT) that maps virtual addresses to physical addresses in 4KB chunks called pages. Required in x86-64 mode. UEFI sets up an identity map (virtual = physical) before handing control to us. Each page table entry has flag bits that control permissions and caching behavior.

**[PAT](https://en.wikipedia.org/wiki/Page_attribute_table)** — Page Attribute Table. The modern way to control memory caching types per-page via page table entries. An MSR holds 8 slots, each mapping to a memory type (WB, WC, UC, etc.). Three bits in each page table entry (PWT, PCD, PAT) select which slot to use. We replace slot 1 (normally Write-Through) with Write-Combining and set the corresponding bits on framebuffer pages. Works per-page with no alignment restrictions.

**[PCI](https://en.wikipedia.org/wiki/Peripheral_Component_Interconnect)** — Peripheral Component Interconnect. The bus standard that connects devices (GPU, disk controller, network card) to the CPU. We scan the PCI bus to find the AHCI SATA controller. Each device has a config space accessible via I/O ports 0xCF8/0xCFC.

**[TLB](https://en.wikipedia.org/wiki/Translation_lookaside_buffer)** — Translation Lookaside Buffer. A CPU cache of recent page table lookups. After modifying a page table entry, the TLB must be flushed with `invlpg` so the CPU sees the change. Without flushing, the CPU may continue using the old cached entry.

**[UEFI](https://en.wikipedia.org/wiki/UEFI)** — Unified Extensible Firmware Interface. The modern replacement for BIOS. Provides boot services, graphics (GOP), memory maps, and loads PE executables from FAT32 partitions.
- *QEMU:* [EDK II](https://en.wikipedia.org/wiki/TianoCore_EDK_II) (TianoCore) — Intel's open-source reference UEFI implementation, bundled with QEMU as OVMF firmware files.
- *Real hardware:* [American Megatrends (AMI)](https://en.wikipedia.org/wiki/American_Megatrends) — Commercial UEFI firmware on the ASUS motherboard with AMD Ryzen 5 2600X.

**BAR** — Base Address Register. A PCI concept — each device has up to 6 BARs that tell the system where the device's memory regions are mapped in the CPU's address space. On the GTX 1650, BAR0 maps MMIO control registers (16MB) and BAR1 maps a window into VRAM (256MB). BAR1 uses identity mapping on TU117 — no GPU page tables needed.

**DMA** — Direct Memory Access. Hardware reads or writes memory on its own, without the CPU doing each byte. The opposite of PIO (Programmed I/O). The NVIDIA window channel is a DMA channel — the display engine's DMA fetcher reads push buffer commands from VRAM independently. DMA channels require the full display engine init and instance memory to function.

**[EVO](https://nouveau.freedesktop.org/EVO.html)** — Evolution. NVIDIA's display engine command interface. The CPU writes method commands to a push buffer in VRAM, and the display engine firmware reads and executes them. Used for mode setting, surface configuration, and page flipping. Turing uses the NVDisplay variant with classes NVC57D (core) and NVC57E (window).

**[MMIO](https://en.wikipedia.org/wiki/Memory-mapped_I/O)** — Memory-Mapped I/O. Device registers accessed by reading/writing memory addresses instead of using I/O port instructions. The GPU's control registers are at BAR0 (0xF5000000 on our hardware). Every register read/write goes across the PCIe bus to the GPU.

**[MTRR](https://en.wikipedia.org/wiki/Memory_type_range_register)** — Memory Type Range Registers. CPU registers that control caching behavior for physical address ranges. The firmware uses them to mark RAM as write-back and everything else as uncacheable. MTRRs override PAT — if an MTRR says UC, PAT cannot promote to WC. On our Ryzen, BAR1 falls in the UC default region, preventing write-combining for full-frame GPU writes. We cannot modify MTRRs on this system (crashes).

**[PCIe](https://en.wikipedia.org/wiki/PCI_Express)** — PCI Express. The serial bus connecting the GPU to the CPU. All BAR0/BAR1 accesses go across PCIe. Without write-combining, each byte written to BAR1 is a separate PCIe transaction (~20MB/s). With WC, writes are batched into 64-byte bursts (~1GB/s).

**Push Buffer** — A region of VRAM where the CPU writes display commands for the GPU to execute. Each command is a header word (opcode, count, method offset) followed by data words. The CPU advances a PUT pointer to trigger execution; the GPU advances a GET pointer as it processes commands. On TU117, the core channel uses old NV50-style headers, while window channels use opcode 2 (bit 30) headers.

**[RAMHT](https://nouveau.freedesktop.org/RAMHT.html)** — RAM Hash Table. A lookup table in VRAM that maps 32-bit handles to GPU objects (DMA contexts). When the window channel executes SET_CONTEXT_DMA_ISO, the display engine hashes the handle to find the DMA context in the RAMHT. Required for the window channel to access framebuffer memory. Entries are 8 bytes: handle + context word (inst_offset << 9).

**[VRAM](https://en.wikipedia.org/wiki/Video_RAM)** — Video RAM. The GPU's private memory (4GB on GTX 1650). The CPU accesses it through BAR1 (identity-mapped) or the GOP address (0xF1000000, with write-combining from UEFI). The display engine scans it directly for pixel output. Push buffers, instance memory, RAMHT, and framebuffer pages all live in VRAM.

**[VGA](https://en.wikipedia.org/wiki/Video_Graphics_Array)** — Video Graphics Array. The legacy display standard from 1987. Text mode at 0xB8000 (80x25 characters) and Mode 13h at 0xA0000 (320x200 pixels). Not available on modern UEFI systems.

**[Write-Combining (WC)](https://en.wikipedia.org/wiki/Write-combining)** — A CPU memory type designed for framebuffers and device memory. Without WC, each byte written to the framebuffer is sent individually across the PCIe bus (uncacheable). With WC, the CPU accumulates writes in internal 64-byte buffers and sends them in bursts — dramatically faster for sequential writes like memcpy. We enable WC on the framebuffer pages via PAT, which makes `gfx_present()` and text scrolling much faster on real hardware. UEFI firmware marks the framebuffer as uncacheable by default, and the page table entries are write-protected, so enabling WC requires clearing CR0.WP temporarily to modify them.

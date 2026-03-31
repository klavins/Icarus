# ICARUS

Icarus is a bare-metal x86 operating system with a built-in BASIC interpreter. It boots on 64-bit UEFI systems with some GPU driver support. It has been tested on real hardware (AMD Ryzen 5 2600X, American Megatrends UEFI, GTX 1650).

## What is this?

ICARUS boots directly on PC hardware or in QEMU with no underlying operating system. It provides a BASIC programming environment reminiscent of home computers from the early 1980s, complete with graphics, sound, and disk storage.

When you power on, you get a prompt:

```
 ICARUS OS
 >
```

You can type BASIC commands, write programs, save them to disk, draw graphics, and edit text files with the built-in editor.

For the full BASIC language reference, see [Basic.md](Basic.md).

## Features

- GPU drivers: NVIDIA (EVO display engine), BGA (page flipping), VMware SVGA, GOP fallback
- PAT write-combining and shadow buffers for fast rendering
- 6 display modes: 2 text scales + 4 pixel graphics with integer scaling and square pixels
- AHCI SATA disk with simple flat filesystem
- PCI bus scanning with pluggable driver model
- Atari-inspired BASIC: floating point, trig, graphics, sound, PEEK/POKE, disk I/O
- Built-in text editor ([kilo](https://github.com/antirez/kilo) by Salvatore Sanfilippo) with VT100 terminal emulation
- Dynamic memory allocator (malloc/realloc/free) and freestanding C library (klib)

## Requirements

- Docker (for building the EFI binary on MacOS)
- `qemu-system-x86_64` (for simulation)
- OVMF firmware (UEFI for QEMU, typically installed with QEMU)
- A USB stick if you want to run it on hardware

## Building and Running

Build the EFI binary:

```
./util/build-efi64
```

### Simulation

Three GPU configurations are defined in the Makefile:

```
make sim-bga       # Bochs Graphics Adapter
make sim-vmware    # VMware SVGA — FIFO command submission, no tearing
make sim-gop       # No GPU driver — GOP framebuffer fallback
```

### Bootable USB stick (64-bit UEFI, for real hardware)

Build a properly partitioned GPT image:

```
./util/build-efi64
./util/make-usb-img
```

Write to USB (on macOS):

```
diskutil list                              # find your USB device
diskutil unmountDisk /dev/diskN
sudo dd if=icarus-usb.img of=/dev/rdiskN bs=1M
```

Write to USB (on Linux):

```
lsblk                                     # find your USB device
sudo dd if=icarus-usb.img of=/dev/sdX bs=1M
sync
```

Then boot from the USB stick via the UEFI boot menu (usually F8, F12, or DEL at power-on).

## Disk Utility

The `idu` tool manages files on the virtual disk image from the development environment.

```
idu list                              # list files
idu write "Ball.bas" examples/Ball.bas  # write a file to disk
idu read "Ball.bas"                   # read a file from disk
idu delete "Ball.bas"                 # delete a file
idu format                            # format the disk (erases all files)
```

Defaults to `disk.img` in the current directory. Use `-f path` for a different image.

Create a new disk image:

```
dd if=/dev/zero of=disk.img bs=1M count=2
idu format
```

Write all example programs at once:

```
./util/write-examples
```

Or write individual files:

```
idu write "Hello.bas" hello.bas
```

Then in ICARUS:

```
LOAD "Hello.bas"
RUN
```

### Editor

ICARUS includes a built-in text editor based on [kilo](https://github.com/antirez/kilo) by Salvatore Sanfilippo. Launch it from the BASIC prompt:

```
EDIT "myfile.txt"
```

Key bindings:

- **Ctrl-S** — Save to disk
- **Ctrl-Q** — Quit (press 3 times to discard unsaved changes)
- **Ctrl-F** — Search
- **Arrow keys** — Navigate
- **Page Up/Down** — Scroll by screen
- **Home/End** — Beginning/end of line

The editor supports C syntax highlighting for `.c` and `.h` files.

## Memory Map

When ICARUS boots via UEFI, the 64-bit EFI stub collects hardware information, locates free memory for the kernel heap, and passes control to the kernel. The following memory regions are used:

```
Address         Size        Description
─────────────── ─────────── ────────────────────────────────
0x00000000      640 KB      Low memory (conventional)
0x00070000      256 bytes   System status area
                             +0x000  Key state (128 bytes, 1=pressed per scancode)
                             +0x080  Last key scancode
                             +0x081  Last key ASCII
                             +0x084  Timer tick count (32-bit)
                             +0x088  Uptime seconds (32-bit)
0x00080000      ~100 bytes  UEFI boot info (framebuffer, memory, firmware, heap location)
0x00090000      64 KB       Kernel stack
0x00100000+     ~760 KB     Kernel code and data (loaded by UEFI)
                             .text    Code
                             .rodata  String literals, font data, VGA palette
                             .data    Small initialized globals
                             .bss     IDT, GDT, small static tables
0x01000000+     163+ MB     Kernel heap (bump allocator, size from UEFI memory map)
                             Text console shadow buffer (~3-9 MB depending on resolution)
                             Graphics shadow buffer (same size)
                             Graphics saved-screen buffer (same size)
                             malloc/free heap (512 KB for editor and C programs)
                             ── watermark ── (CLR resets to here)
                             BASIC program text (allocated per line)
                             BASIC variables and arrays (allocated on DIM)
                             BASIC string variable data (allocated on DIM)
0xE0000000+     256 MB      GPU BAR1 (NVIDIA VRAM window, UC)
0xF1000000      ~5 MB       GOP framebuffer (NVIDIA VRAM via UEFI, WC)
                             Used for all CPU framebuffer writes
```

The system status area at `0x70000` is readable from BASIC via `PEEK`. The interrupt handler updates key state and timer ticks here in real time. See `sysinfo.h` for the full layout.

Memory is managed by a bump allocator for BASIC and a first-fit free-list allocator (malloc/realloc/free) for the editor and future C programs. The UEFI boot stub finds the largest free memory region (163 MB in QEMU with 256 MB RAM, much larger on real hardware). Framebuffer buffers and the malloc heap are allocated first, then a watermark is set. BASIC data (program text, variables, arrays) is allocated above the watermark and freed on `CLR`.

## Hardware Notes

- **Keyboard**: A PS/2 keyboard is required. USB keyboards rely on UEFI PS/2 emulation which stops working after ExitBootServices. Most ASUS and similar motherboards have a PS/2 port on the back panel.
- **Display**: ICARUS probes PCI for a supported GPU. On NVIDIA GTX 1650 (TU117), it initializes the EVO display engine with core and window channels. On QEMU, BGA and VMware SVGA drivers provide page flipping. Falls back to UEFI GOP framebuffer on unsupported GPUs.
- **Disk**: The AHCI SATA driver works with modern SATA controllers. Falls back to ATA PIO for legacy/QEMU configurations. NVMe is not yet supported.


# ICARUS

A bare-metal x86 operating system with a built-in BASIC interpreter, inspired by the Atari 800. Boots on 64-bit UEFI systems with GPU driver support. Tested on real hardware (AMD Ryzen 5 2600X, American Megatrends UEFI).

## What is this?

ICARUS boots directly on PC hardware (or in QEMU) with no underlying operating system. It provides a BASIC programming environment reminiscent of home computers from the early 1980s, complete with graphics, sound, and disk storage.

When you power on, you get a prompt:

```
ICARUS BASIC
 >
```

Type BASIC commands, write programs, save them to disk, and draw graphics.

## Features

### Operating System
- 64-bit UEFI boot
- GPU driver framework with BGA and VMware SVGA drivers
- Page flipping for flicker-free graphics (BGA)
- GOP framebuffer fallback for unsupported GPUs
- PAT write-combining for fast framebuffer access
- Framebuffer text console with shadow buffer and 2x scaling on high-res displays
- Full-resolution pixel graphics with integer scaling (4 modes)
- PS/2 keyboard input with shift support
- PC speaker sound
- AHCI SATA and ATA PIO disk drivers
- PCI bus scanning and device detection
- Simple flat filesystem with save/load/directory

### BASIC Interpreter
- Immediate mode and stored program execution
- Line-number based program editing
- Multi-character variable names
- Double-precision floating point arithmetic with operator precedence and MOD
- String variables (DIM and $)
- 1D and 2D arrays
- Built-in functions: RND, ABS, INT, SQR, SIN, COS, LEN, PEEK
- Constants: PI, SCRW, SCRH
- Hex number literals (0xFF)
- Commands: PRINT, LET, DIM, IF/THEN, FOR/NEXT, GOTO, GOSUB/RETURN, ON GOTO/GOSUB, READ/DATA/RESTORE, INPUT, PAUSE, DELAY, POKE, REM
- Graphics: GRAPHICS (4 resolution modes), PLOT, DRAWTO, FILLTO, COLOR, POS, TEXT, SHOW, CLS
- Sound: SOUND (PC speaker, Atari BASIC syntax)
- Disk: SAVE, LOAD, DIR, DELETE, FORMAT, DOS
- Dynamic memory: bump allocator with watermark for program text, variables, and arrays
- System: RUN, LIST, CLR, QUIT

## Requirements

- Docker (for building the EFI binary)
- `qemu-system-x86_64` (for simulation)
- OVMF firmware (UEFI for QEMU, typically installed with QEMU)

### For real hardware (USB stick)
- Everything above
- A USB stick (any size)

## Building and Running

Build the EFI binary (runs in Docker):

```
./util/build-efi64
```

### Simulation

Three GPU configurations are available:

```
make sim-bga       # Bochs Graphics Adapter — page flipping, no shimmer
make sim-vmware    # VMware SVGA — FIFO command submission
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

### Bootable ISO (for USB sticks, legacy BIOS PCs)

```
make docker-iso
```

Write to USB with:

```
diskutil unmountDisk /dev/diskN
sudo dd if=icarus.iso of=/dev/rdiskN bs=1M
```

## Disk Utility

The `idu` tool manages files on the virtual disk image from the host.

```
idu list                          # list files
idu write MYPROG myprogram.bas    # write a file to disk
idu read MYPROG                   # read a file from disk
idu delete MYPROG                 # delete a file
idu format                        # format the disk (erases all files)
```

Defaults to `disk.img` in the current directory. Use `-f path` for a different image.

Create a new disk image:

```
dd if=/dev/zero of=disk.img bs=1M count=2
idu format
```

### Writing BASIC programs from the host

Create a text file with line numbers:

```
10 PRINT "HELLO WORLD"
20 FOR I = 1 TO 10
30 PRINT I
40 NEXT I
```

Write it to disk and run:

```
idu write HELLO hello.bas
make sim-bga
```

Then in ICARUS:

```
LOAD "HELLO"
RUN
```

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
0x00100000+     ~700 KB     Kernel code and data (loaded by UEFI)
                             .text    Code (~200 KB)
                             .rodata  String literals, font data, VGA palette (~50 KB)
                             .data    Small initialized globals
                             .bss     IDT, GDT, small static tables (~1 MB)
0x01000000+     163+ MB     Kernel heap (bump allocator, size from UEFI memory map)
                             Shadow framebuffer (allocated at boot, ~4 MB)
                             Framebuffer save buffer (allocated at boot, ~4 MB)
                             ── watermark ── (CLR resets to here)
                             BASIC program text (allocated per line)
                             BASIC variables and arrays (allocated on DIM)
                             BASIC string variable data (allocated on DIM)
0xFD000000+     varies      GPU framebuffer (address from PCI BAR or GOP)
                             Marked write-combining via PAT
                             Page-flipped (BGA) or update-rect (VMware)
```

The system status area at `0x70000` is readable from BASIC via `PEEK`. The interrupt handler updates key state and timer ticks here in real time. See `sysinfo.h` for the full layout.

Memory is managed by a bump allocator. The UEFI boot stub finds the largest free memory region (163 MB in QEMU with 256 MB RAM, much larger on real hardware). Framebuffer buffers are allocated first, then a watermark is set. BASIC data (program text, variables, arrays) is allocated above the watermark and freed on `CLR`. The `FREE` command (planned) will show remaining heap space.

## Project Structure

### Kernel
| File | Description |
|------|-------------|
| `kernel.c` | Main kernel entry, BASIC REPL loop |
| `boot_info.c/h` | Hardware detection (CPUID, memory, EFLAGS, ATA) |

### Display
| File | Description |
|------|-------------|
| `fbconsole.c` | Framebuffer text console with shadow buffer and 2x scaling |
| `font_8x16.h` | 8x16 bitmap font for framebuffer rendering |
| `graphics.c/h` | Pixel graphics with page flipping, integer scaling, and text overlay |
| `gpu.c/h` | GPU driver abstraction — probes drivers, manages framebuffer handoff |
| `bga.c/h` | Bochs Graphics Adapter driver — page flipping via Y offset |
| `vmware.c/h` | VMware SVGA II driver — FIFO command submission |
| `pat.c/h` | PAT write-combining for fast framebuffer writes |

### Drivers
| File | Description |
|------|-------------|
| `keyboard.c/h` | Interrupt-driven keyboard input via ring buffer |
| `interrupts.c/h` | IDT, GDT, PIC setup; timer and keyboard ISRs |
| `sysinfo.h` | System status area layout (key state, timer ticks) |
| `speaker.c/h` | PC speaker tone generation |
| `pci.c/h` | PCI bus scanning, device lookup by class or vendor/device ID |
| `ahci.c/h` | AHCI SATA disk driver |
| `ata.c/h` | Unified disk interface (AHCI with PIO fallback) |
| `io.h` | Port I/O, MSR access (inb, outb, rdmsr, wrmsr) |
| `math.c/h` | Freestanding math functions (sin, cos, sqrt) |

### Filesystem
| File | Description |
|------|-------------|
| `fs.c/h` | Simple flat filesystem (format, save, load, delete, list) |

### BASIC Interpreter
| File | Description |
|------|-------------|
| `basic.c/h` | Public API, program store, serialization, DOS menu |
| `basic_internal.h` | Shared constants, structs, and internal function prototypes |
| `basic_token.c` | Tokenizer and keyword lookup |
| `basic_expr.c` | Expression parser, RNG, built-in functions |
| `basic_exec.c` | Statement dispatcher (switch-based), runtime stacks |
| `basic_vars.c` | Variable storage, bump allocator |

### C Library
| File | Description |
|------|-------------|
| `klib.c/h` | Freestanding libc: memset, memcpy, strlen, strcmp, sprintf, etc. |

### UEFI Boot
| File | Description |
|------|-------------|
| `uefi_boot.c` | 32-bit UEFI entry point (for QEMU i386) |
| `uefi_boot64.c` | 64-bit UEFI entry point (for real hardware and QEMU x86_64) |
| `uefi.h` | Minimal UEFI type definitions |
| `uefi_stubs.c` | MinGW runtime stubs for freestanding build |
| `Dockerfile.efi` | Docker image for UEFI cross-compilation |

### Tools
| File | Description |
|------|-------------|
| `util/idu` | Host-side disk image utility (Python) |
| `util/build-efi` | 32-bit UEFI build script (runs in Docker) |
| `util/build-efi64` | 64-bit UEFI build script (runs in Docker) |
| `util/make-usb-img` | Create GPT-partitioned USB boot image |

For the full BASIC language reference, see [Basic.md](Basic.md).

## Running on Real Hardware

### Legacy BIOS PC

Any PC from before ~2015 with a VGA port and Legacy/CSM boot support will work. Dell Optiplex, HP ProDesk, and Lenovo ThinkCentre desktops from that era are ideal. Build the ISO with `make docker-iso` and write it to a USB stick.

### Modern UEFI PC

Build a bootable USB with `./util/build-efi64` and `./util/make-usb-img`, then write `icarus-usb.img` to a USB stick. This produces a 64-bit UEFI application with a proper GPT partition table.

Successfully tested on:
- AMD Ryzen 5 2600X / ASUS motherboard / American Megatrends UEFI
- Display via GOP framebuffer (no NVIDIA driver yet)
- AHCI SATA disk (1TB WDC)
- 64GB RAM detected

### Hardware Notes

- **Keyboard**: A PS/2 keyboard is required. USB keyboards rely on UEFI PS/2 emulation which stops working after ExitBootServices. Most ASUS and similar motherboards have a PS/2 port on the back panel.
- **Display**: ICARUS probes PCI for a supported GPU (BGA, VMware SVGA) and falls back to the UEFI GOP framebuffer. On real hardware without a supported GPU driver, GOP provides basic display output. On multi-GPU systems, make sure your monitor is connected to the GPU marked as `boot_vga` (check with `cat /sys/bus/pci/devices/*/boot_vga` from Linux).
- **Disk**: The AHCI SATA driver works with modern SATA controllers. Falls back to ATA PIO for legacy/QEMU configurations. NVMe is not yet supported.
- **BIOS Setup**: If you can't enter BIOS setup, try clearing CMOS by removing the motherboard coin battery for 30 seconds with power disconnected.

## Documentation

See `Basic.md` for the full BASIC language reference with detailed examples.

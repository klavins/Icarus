# ICARUS

A bare-metal x86 operating system with a built-in BASIC interpreter, inspired by the Atari 800. Boots on both legacy BIOS and modern 64-bit UEFI systems. Tested on real hardware (AMD Ryzen 5 2600X, NVIDIA GPUs, American Megatrends UEFI).

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
- Boots via Multiboot (GRUB/QEMU), 32-bit UEFI, or 64-bit UEFI
- 32-bit kernel for BIOS/Multiboot, 64-bit kernel for UEFI
- VGA text mode (80x25) for BIOS boot
- GOP framebuffer rendering with bitmap font for UEFI boot
- VGA Mode 13h graphics (320x200, 256 colors) for BIOS boot
- Full-resolution pixel graphics for UEFI boot
- PS/2 keyboard input with shift support
- PC speaker sound
- ATA PIO disk driver
- Simple flat filesystem with save/load/directory

### BASIC Interpreter
- Immediate mode and stored program execution
- Line-number based program editing
- Multi-character variable names
- Integer arithmetic with operator precedence
- String variables (DIM and $)
- 1D and 2D arrays
- Commands: PRINT, LET, DIM, IF/THEN, FOR/NEXT, GOTO, GOSUB/RETURN, ON GOTO/GOSUB, READ/DATA/RESTORE, INPUT, PAUSE
- Graphics: GRAPHICS, PLOT, DRAWTO, COLOR
- Sound: SOUND (PC speaker, Atari BASIC syntax)
- Disk: SAVE, LOAD, DIR, DELETE, FORMAT, DOS
- System: RUN, LIST, CLR, QUIT

## Requirements

### For BIOS boot (make sim)
- `nasm` (assembler)
- `i686-elf-gcc` (cross-compiler)
- `qemu-system-i386`

### For UEFI boot (make sim-uefi / make sim-uefi64)
- Everything above
- Docker (for building the EFI binary)

### For real hardware (USB stick)
- Everything above
- A USB stick (any size)

### Cross-compiler

The `i686-elf-gcc` cross-compiler must be built from source or installed via your package manager. On macOS, follow the [OSDev GCC Cross-Compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler).

## Building and Running

### BIOS text mode (classic VGA)

```
make clean && make sim
```

### BIOS framebuffer mode (320x200 pixel rendering)

```
make clean && make sim-fb
```

### UEFI mode — 32-bit (for QEMU i386)

First build the Docker image (one time):

```
docker build --platform linux/amd64 -t icarus-efi -f Dockerfile.efi .
```

Then build and run:

```
./util/build-efi
make sim-uefi
```

### UEFI mode — 64-bit (for QEMU x86_64 and real hardware)

```
./util/build-efi64
make sim-uefi64
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
make sim-uefi
```

Then in ICARUS:

```
LOAD "HELLO"
RUN
```

## Project Structure

### Kernel
| File | Description |
|------|-------------|
| `boot.asm` | Multiboot entry point and stack setup |
| `kernel.c` | Main kernel entry, BASIC REPL loop |
| `linker.ld` | Linker script (kernel loads at 1MB) |
| `multiboot.h` | Multiboot header struct and flag definitions |
| `boot_info.c/h` | Hardware detection (CPUID, memory, EFLAGS, ATA) |

### Display
| File | Description |
|------|-------------|
| `vga.c/h` | VGA text mode driver (80x25) |
| `fbconsole.c` | Framebuffer text renderer (Mode 13h and UEFI GOP) |
| `font_8x16.h` | 8x16 bitmap font for framebuffer rendering |
| `graphics.c/h` | Pixel graphics mode (PLOT, DRAWTO, line drawing) |

### Drivers
| File | Description |
|------|-------------|
| `keyboard.c/h` | PS/2 keyboard polling with shift support |
| `speaker.c/h` | PC speaker tone generation |
| `ata.c/h` | ATA PIO disk read/write with timeouts |
| `io.h` | Port I/O functions (inb, outb, inw, outw) |

### Filesystem
| File | Description |
|------|-------------|
| `fs.c/h` | Simple flat filesystem (format, save, load, delete, list) |

### BASIC Interpreter
| File | Description |
|------|-------------|
| `basic.c/h` | Tokenizer, expression parser, and command executor |

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

## BASIC Quick Reference

### Immediate mode

```
PRINT "HELLO"
PRINT 2 + 3 * 4
LET X = 42
PRINT X
```

### Stored programs

```
10 FOR I = 1 TO 10
20 PRINT I * I
30 NEXT I
LIST
RUN
```

### Strings

```
DIM N$(20)
LET N$ = "ICARUS"
PRINT N$
```

### Arrays

```
DIM A(10)
DIM M(3,3)
LET A(0) = 42
LET M(1,2) = 99
```

### Graphics

```
GRAPHICS 1              (320x200, chunky pixels)
GRAPHICS 2              (640x400)
GRAPHICS 3              (800x600)
GRAPHICS 4              (native resolution)
COLOR 4
PLOT 160, 100
DRAWTO 300, 50
POS 10, 10
TEXT "HELLO"
PRINT SCRW, SCRH        (screen dimensions)
PAUSE
GRAPHICS 0              (back to text)
```

### Sound

```
SOUND 0, 100, 0, 8
SOUND 0, 0, 0, 0
```

### Disk

```
SAVE "MYPROG"
LOAD "MYPROG"
DIR
DELETE "MYPROG"
DOS
```

### Control flow

```
IF X > 10 THEN PRINT "BIG"
GOTO 100
GOSUB 500
ON X GOTO 100, 200, 300
ON X GOSUB 100, 200, 300
```

### Data

```
10 DATA 10, 20, 30, "HELLO"
20 READ X
30 READ Y
40 READ Z
50 DIM S$(20)
60 READ S$
70 RESTORE
```

## Running on Real Hardware

### Legacy BIOS PC

Any PC from before ~2015 with a VGA port and Legacy/CSM boot support will work. Dell Optiplex, HP ProDesk, and Lenovo ThinkCentre desktops from that era are ideal. Build the ISO with `make docker-iso` and write it to a USB stick.

### Modern UEFI PC

Build a bootable USB with `./util/build-efi64` and `./util/make-usb-img`, then write `icarus-usb.img` to a USB stick. This produces a 64-bit UEFI application with a proper GPT partition table.

Successfully tested on:
- AMD Ryzen 5 2600X / ASUS motherboard / NVIDIA GPUs / American Megatrends UEFI
- Display: 1024x768 via GOP framebuffer
- 64GB RAM detected

### Hardware Notes

- **Keyboard**: A PS/2 keyboard is required. USB keyboards rely on UEFI PS/2 emulation which stops working after ExitBootServices. Most ASUS and similar motherboards have a PS/2 port on the back panel.
- **Display**: ICARUS uses the UEFI GOP framebuffer. On multi-GPU systems, make sure your monitor is connected to the GPU marked as `boot_vga` (check with `cat /sys/bus/pci/devices/*/boot_vga` from Linux).
- **Disk**: The ATA PIO driver works with legacy PATA/IDE drives and QEMU virtual disks. SATA and NVMe drives on real hardware are not yet supported — BASIC works fine without a disk, you just can't save/load programs.
- **BIOS Setup**: If you can't enter BIOS setup, try clearing CMOS by removing the motherboard coin battery for 30 seconds with power disconnected.

## Documentation

See `Basic.md` for the full BASIC language reference with detailed examples.

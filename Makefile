AS      = nasm
CC      = i686-elf-gcc
LD      = i686-elf-gcc

ASFLAGS = -f elf32
CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra
LDFLAGS = -T linker.ld -ffreestanding -O2 -nostdlib -lgcc

BUILD   = build
OBJS    = $(BUILD)/boot.o $(BUILD)/klib.o $(BUILD)/vga.o $(BUILD)/boot_info.o \
          $(BUILD)/keyboard.o $(BUILD)/speaker.o $(BUILD)/ata.o $(BUILD)/fs.o \
          $(BUILD)/graphics.o $(BUILD)/basic.o $(BUILD)/kernel.o
KERNEL  = icarus.bin
ISO     = icarus.iso

.PHONY: all clean run iso sim docker-iso

all: $(ISO)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) -c $(CFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(ISO)

$(ISO): $(KERNEL) grub.cfg
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/
	cp grub.cfg isodir/boot/grub/
	grub-mkrescue -o $@ isodir

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO)

sim:
	qemu-system-i386 -kernel icarus.bin -m 256 -smp 1 -display cocoa,zoom-to-fit=on -drive file=disk.img,format=raw -machine pcspk-audiodev=snd -audiodev coreaudio,id=snd,timer-period=1000,out.buffer-count=2 -full-screen

docker-iso:
	docker run --rm --platform linux/amd64 -v "$(CURDIR)":/src -w /src ubuntu:latest bash -c \
		"apt-get update -qq && apt-get install -y -qq nasm gcc-i686-linux-gnu make grub-pc-bin xorriso >/dev/null 2>&1 && make clean && make iso CC=i686-linux-gnu-gcc LD=i686-linux-gnu-gcc"

clean:
	rm -rf $(BUILD) $(KERNEL) $(ISO) isodir

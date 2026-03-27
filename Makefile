AS      = nasm
CC      = i686-elf-gcc
LD      = i686-elf-gcc

ASFLAGS = -f elf32
CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra
LDFLAGS = -T linker.ld -ffreestanding -O2 -nostdlib -lgcc

BUILD   = build
COMMON  = $(BUILD)/boot.o $(BUILD)/klib.o $(BUILD)/boot_info.o \
          $(BUILD)/keyboard.o $(BUILD)/speaker.o $(BUILD)/ata.o $(BUILD)/fs.o \
          $(BUILD)/graphics.o $(BUILD)/interrupts.o $(BUILD)/math.o \
          $(BUILD)/basic_vars.o $(BUILD)/basic_token.o $(BUILD)/basic_expr.o \
          $(BUILD)/basic_exec.o $(BUILD)/basic.o $(BUILD)/kernel.o
OBJS    = $(COMMON) $(BUILD)/vga.o
OBJS_FB = $(COMMON) $(BUILD)/fbconsole.o
KERNEL  = icarus.bin
KERNEL_FB = icarus-fb.bin
ISO     = icarus.iso

# Source files that affect UEFI builds
SRCS    = $(wildcard *.c *.h *.asm)

.PHONY: all clean run iso sim sim-fb sim-uefi sim-uefi64 docker-iso

all: $(ISO)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) -c $(CFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(KERNEL_FB): $(OBJS_FB) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS_FB)

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

sim-fb: $(KERNEL_FB)
	qemu-system-i386 -kernel $(KERNEL_FB) -m 256 -smp 1 -display cocoa,zoom-to-fit=on -drive file=disk.img,format=raw -machine pcspk-audiodev=snd -audiodev coreaudio,id=snd,timer-period=1000,out.buffer-count=2 -full-screen

OVMF_CODE = /opt/homebrew/share/qemu/edk2-i386-code.fd
OVMF_VARS = /opt/homebrew/share/qemu/edk2-i386-vars.fd

sim-uefi: icarus-uefi.img
	cp $(OVMF_VARS) /tmp/ovmf-vars.fd
	qemu-system-i386 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=/tmp/ovmf-vars.fd \
		-drive format=raw,file=icarus-uefi.img,if=none,id=boot -device ide-hd,drive=boot,bus=ide.1 \
		-drive file=disk.img,format=raw,if=none,id=data -device ide-hd,drive=data,bus=ide.0 \
		-m 256 -smp 1 -net none \
		-display cocoa,zoom-to-fit=on -full-screen \
		-machine pcspk-audiodev=snd \
		-audiodev coreaudio,id=snd,timer-period=1000,out.buffer-count=2

icarus-uefi.img: $(SRCS)
	./util/build-efi

sim-uefi64: icarus-uefi64.img
	cp $(OVMF_VARS) /tmp/ovmf-vars.fd
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=/opt/homebrew/share/qemu/edk2-x86_64-code.fd \
		-drive if=pflash,format=raw,file=/tmp/ovmf-vars.fd \
		-drive format=raw,file=icarus-uefi64.img,if=none,id=boot -device ide-hd,drive=boot,bus=ide.1 \
		-drive file=disk.img,format=raw,if=none,id=data -device ide-hd,drive=data,bus=ide.0 \
		-m 256 -smp 1 -net none \
		-display cocoa,zoom-to-fit=on -full-screen \
		-machine pcspk-audiodev=snd \
		-audiodev coreaudio,id=snd,timer-period=1000,out.buffer-count=2

icarus-uefi64.img: $(SRCS)
	./util/build-efi64

docker-iso:
	docker run --rm --platform linux/amd64 -v "$(CURDIR)":/src -w /src ubuntu:latest bash -c \
		"apt-get update -qq && apt-get install -y -qq nasm gcc-i686-linux-gnu make grub-pc-bin xorriso >/dev/null 2>&1 && make clean && make iso CC=i686-linux-gnu-gcc LD=i686-linux-gnu-gcc"

clean:
	rm -rf $(BUILD) build-efi build-efi64 $(KERNEL) $(KERNEL_FB) icarus-uefi.img icarus-uefi64.img $(ISO) isodir

DOCKER = docker run --rm --platform linux/amd64 -v "$(CURDIR)/..":/src -w /src icarus-efi
CC     = $(DOCKER) gcc
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
         -fno-stack-protector -fno-pic -mno-red-zone -fcf-protection=none \
         -I include -I os -I lib
LDFLAGS = -nostdlib -T os/program.ld -lgcc
IDU    = ../util/idu

SRCS = $(wildcard *.c)
BINS = $(patsubst %.c,../build-programs/%.bin,$(SRCS))

all: $(BINS)

../build-programs:
	mkdir -p ../build-programs

../build-programs/crt0.o: ../os/crt0.c ../os/icarus_api.h | ../build-programs
	$(CC) -c $(CFLAGS) os/crt0.c -o build-programs/crt0.o

../build-programs/%.bin: %.c ../build-programs/crt0.o ../os/program.ld | ../build-programs
	$(CC) -c $(CFLAGS) examples/$*.c -o build-programs/$*.o
	$(CC) $(LDFLAGS) build-programs/crt0.o build-programs/$*.o -o build-programs/$*.bin
	cd .. && util/idu write "$*" build-programs/$*.bin
	@echo "  $* -> disk"

clean:
	rm -rf ../build-programs

.PHONY: all clean

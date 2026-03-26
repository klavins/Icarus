#include <stdint.h>
#include "multiboot.h"
#include "vga.h"
#include "boot_info.h"
#include "keyboard.h"
#include "basic.h"
#include "fs.h"
#include "ata.h"

#define LINE_MAX 80
#define MBOOT_MAGIC 0x2BADB002

struct uefi_boot_info {
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
    uint32_t total_memory_kb;
    char     firmware_vendor[64];
    uint32_t firmware_revision;
    uint32_t pixel_format;
};
extern struct uefi_boot_info uefi_info __attribute__((weak));

static inline void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                          uint32_t *ecx, uint32_t *edx) {
    asm volatile ("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf));
}

static void print_cpu_info(void) {
    uint32_t eax, ebx, ecx, edx;

    cpuid(0, &eax, &ebx, &ecx, &edx);
    char vendor[13];
    *(uint32_t *)&vendor[0] = ebx;
    *(uint32_t *)&vendor[4] = edx;
    *(uint32_t *)&vendor[8] = ecx;
    vendor[12] = '\0';
    terminal_printf(" CPU: %s", vendor);

    cpuid(1, &eax, &ebx, &ecx, &edx);
    uint32_t family = (eax >> 8) & 0xF;
    uint32_t model  = (eax >> 4) & 0xF;
    if (family == 0xF)
        family += (eax >> 20) & 0xFF;
    if (family == 0x6 || family == 0xF)
        model += ((eax >> 16) & 0xF) << 4;
    terminal_printf(" family %d model %d\n", family, model);
}

static void print_disk_info(void) {
    uint32_t sectors = ata_get_total_sectors();
    if (sectors > 0) {
        uint32_t kb = sectors / 2;
        if (kb >= 1024)
            terminal_printf(" Disk: %d MB (%d sectors)\n", kb / 1024, sectors);
        else
            terminal_printf(" Disk: %d KB (%d sectors)\n", kb, sectors);
    } else {
        terminal_print(" Disk: not detected\n");
    }
}

static void print_uefi_info(void) {
    terminal_setcolor(VGA_GREEN, VGA_BLACK);
    terminal_print(" UEFI Boot\n");
    terminal_setcolor(VGA_LCYAN, VGA_BLACK);
    terminal_printf(" Firmware: %s (rev %d)\n",
                    uefi_info.firmware_vendor,
                    uefi_info.firmware_revision);
    terminal_printf(" Display: %dx%d, %d bpp\n",
                    uefi_info.fb_width,
                    uefi_info.fb_height,
                    uefi_info.fb_bpp * 8);
    terminal_printf(" Memory: %d KB (%d MB)\n",
                    uefi_info.total_memory_kb,
                    uefi_info.total_memory_kb / 1024);
    print_cpu_info();
    print_disk_info();
}

void kernel_main(uint32_t magic, struct multiboot_info *mboot) {

    terminal_init();
    if (magic == MBOOT_MAGIC)
        boot_info_print(magic, mboot);
    else if (&uefi_info)
        print_uefi_info();

    fs_init();
    basic_init();
    terminal_setcolor(VGA_LGREEN, VGA_BLACK);
    terminal_print("\n ICARUS BASIC\n");
    terminal_print(" > ");

    char line[LINE_MAX];
    int pos = 0;

    for (;;) {
        char c = keyboard_poll();
        if (c == '\n') {
            line[pos] = '\0';
            terminal_putchar('\n');
            if (pos > 0)
                basic_exec(line);
            pos = 0;
            terminal_print(" > ");
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                terminal_putchar('\b');
            }
        } else if (c && pos < LINE_MAX - 1) {
            line[pos++] = c;
            terminal_putchar(c);
        }
    }

}

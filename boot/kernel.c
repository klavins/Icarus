/*
 * kernel.c - Kernel entry point and system initialization
 *
 * Copyright (C) 2026 Eric Klavins
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include "multiboot.h"
#include "vga.h"
#include "boot_info.h"
#include "keyboard.h"
#include "basic.h"
#include "basic_internal.h"
#include "graphics.h"
#include "fs.h"
#include "ata.h"
#include "interrupts.h"
#include "pci.h"
#include "gpu.h"
#include "pat.h"
#include "os.h"
#include "malloc.h"
#include "klib.h"

#define LINE_MAX 80
#define MBOOT_MAGIC 0x2BADB002

struct uefi_boot_info {
    uint32_t fb_addr;           /* 32-bit UEFI: pointer; 64-bit UEFI: low 32 bits */
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
    uint32_t total_memory_kb;
    char     firmware_vendor[64];
    uint32_t firmware_revision;
    uint32_t pixel_format;
    uint32_t heap_base;
    uint32_t heap_size;
};
extern struct uefi_boot_info uefi_info __attribute__((weak));

/* 64-bit UEFI boot stores info at this fixed address */
#define UEFI_INFO_FIXED ((volatile struct uefi_boot_info *)0x00080000)

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
    /* Find the boot info — either from weak symbol or fixed address */
    volatile struct uefi_boot_info *info = 0;
    if (&uefi_info && uefi_info.fb_width > 0)
        info = (volatile struct uefi_boot_info *)&uefi_info;
    else if (UEFI_INFO_FIXED->fb_width > 0)
        info = UEFI_INFO_FIXED;

    terminal_setcolor(VGA_GREEN, VGA_BLACK);
    terminal_print(" UEFI Boot\n");
    terminal_setcolor(VGA_LCYAN, VGA_BLACK);

    if (info) {
        terminal_printf(" Firmware: %s (rev %d)\n",
                        (const char *)info->firmware_vendor,
                        info->firmware_revision);
        terminal_printf(" Display: %dx%d, %d bpp\n",
                        info->fb_width,
                        info->fb_height,
                        info->fb_bpp * 8);
        terminal_printf(" Memory: %d KB (%d MB)\n",
                        info->total_memory_kb,
                        info->total_memory_kb / 1024);
    }
    print_cpu_info();
}

void kernel_main(uint32_t magic, struct multiboot_info *mboot) {

    basic_heap_init();
    pat_init();
    terminal_init();
    if (magic == MBOOT_MAGIC)
        boot_info_print(magic, mboot);
    else
        print_uefi_info();

    graphics_alloc_init();

    /* Initialize malloc heap — 512KB carved from the bump allocator */
    {
        #define MALLOC_HEAP_SIZE (512 * 1024)
        void *mheap = basic_alloc(MALLOC_HEAP_SIZE);
        if (mheap) heap_init(mheap, MALLOC_HEAP_SIZE);
    }

    basic_alloc_set_watermark();

    gpu_init();

    /* If no GPU driver took over, set up WC on the GOP framebuffer */
    if (!gpu) {
        uint8_t *fb;
        uint32_t w, h, p, b;
        terminal_get_fb(&fb, &w, &h, &p, &b);
        if (fb)
            pat_set_write_combining((uint64_t)(uintptr_t)fb, p * b * h);
    }
    ata_init();
    print_disk_info();
    fs_init();
    nvidia_save_dump();
    interrupts_init();
    basic_init();
    terminal_setcolor(VGA_LGRAY, VGA_BLACK);
    terminal_printf(" ICARUS OS v%d\n", OS_VERSION);
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
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

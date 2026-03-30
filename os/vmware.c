/*
 * vmware.c - VMware SVGA II virtual GPU driver
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

#include "vmware.h"
#include "gpu.h"
#include "pci.h"
#include "io.h"
#include "vga.h"

/* PCI IDs */
#define VMWARE_PCI_VENDOR 0x15AD
#define VMWARE_PCI_DEVICE 0x0405  /* SVGA II */

/* Register indices */
#define SVGA_REG_ID             0
#define SVGA_REG_ENABLE         1
#define SVGA_REG_WIDTH          2
#define SVGA_REG_HEIGHT         3
#define SVGA_REG_BITS_PER_PIXEL 7
#define SVGA_REG_BYTES_PER_LINE 12
#define SVGA_REG_FB_START       13
#define SVGA_REG_FB_OFFSET      14
#define SVGA_REG_VRAM_SIZE      15
#define SVGA_REG_FB_SIZE        16
#define SVGA_REG_CAPABILITIES   17
#define SVGA_REG_MEM_START      18
#define SVGA_REG_MEM_SIZE       19
#define SVGA_REG_CONFIG_DONE    20
#define SVGA_REG_SYNC           21
#define SVGA_REG_BUSY           22

/* FIFO registers (dword offsets into FIFO memory) */
#define SVGA_FIFO_MIN      0
#define SVGA_FIFO_MAX      1
#define SVGA_FIFO_NEXT_CMD 2
#define SVGA_FIFO_STOP     3

/* FIFO commands */
#define SVGA_CMD_UPDATE 1  /* 4 args: x, y, width, height */

/* Version IDs */
#define SVGA_ID_2 0x90000002

static uint8_t  vm_bus, vm_slot, vm_func;
static uint16_t io_base;       /* I/O port base from BAR0 */
static uint8_t *fb_base;       /* framebuffer from BAR1 */
static volatile uint32_t *fifo;/* FIFO memory from BAR2 */
static uint32_t fb_width;
static uint32_t fb_height;
static uint32_t fb_pitch;
static uint32_t fb_offset;     /* offset to visible region */
static uint32_t vram_size;
static int      detected;
static int      can_flip;

static void svga_write(uint32_t reg, uint32_t val) {
    outl(io_base + 0, reg);
    outl(io_base + 1, val);
}

static uint32_t svga_read(uint32_t reg) {
    outl(io_base + 0, reg);
    return inl(io_base + 1);
}

static void fifo_write(uint32_t cmd, uint32_t *args, int nargs) {
    /* Simple synchronous FIFO write */
    uint32_t min = fifo[SVGA_FIFO_MIN];
    uint32_t max = fifo[SVGA_FIFO_MAX];
    uint32_t next = fifo[SVGA_FIFO_NEXT_CMD];

    /* Write command */
    fifo[next / 4] = cmd;
    next += 4;
    if (next >= max) next = min;

    /* Write arguments */
    for (int i = 0; i < nargs; i++) {
        fifo[next / 4] = args[i];
        next += 4;
        if (next >= max) next = min;
    }

    fifo[SVGA_FIFO_NEXT_CMD] = next;
}

static void svga_update(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t args[4] = { x, y, w, h };
    fifo_write(SVGA_CMD_UPDATE, args, 4);
}

static void svga_sync(void) {
    svga_write(SVGA_REG_SYNC, 1);
    while (svga_read(SVGA_REG_BUSY))
        ;
}

int vmware_detect(void) {
    if (!pci_find_vendor(VMWARE_PCI_VENDOR, VMWARE_PCI_DEVICE,
                         &vm_bus, &vm_slot, &vm_func))
        return 0;

    /* Enable PCI I/O and memory access before touching registers */
    pci_enable_device(vm_bus, vm_slot, vm_func);

    /* Read I/O base from BAR0 */
    uint32_t bar0 = pci_read_bar(vm_bus, vm_slot, vm_func, 0);
    io_base = bar0 & 0xFFFC;

    /* Negotiate version */
    svga_write(SVGA_REG_ID, SVGA_ID_2);
    if (svga_read(SVGA_REG_ID) != SVGA_ID_2) return 0;

    /* Read framebuffer and FIFO addresses */
    uint32_t bar1 = pci_read_bar(vm_bus, vm_slot, vm_func, 1);
    fb_base = (uint8_t *)(uintptr_t)(bar1 & 0xFFFFFFF0);

    uint32_t bar2 = pci_read_bar(vm_bus, vm_slot, vm_func, 2);
    fifo = (volatile uint32_t *)(uintptr_t)(bar2 & 0xFFFFFFF0);

    vram_size = svga_read(SVGA_REG_VRAM_SIZE);

    detected = 1;
    terminal_printf(" VMSVGA: %d:%d.%d fb=0x%x vram=%dMB\n",
                    vm_bus, vm_slot, vm_func,
                    (uint32_t)(uintptr_t)fb_base,
                    vram_size / (1024 * 1024));
    return 1;
}

void vmware_init(uint32_t width, uint32_t height) {
    if (!detected) return;

    /* Set mode */
    svga_write(SVGA_REG_WIDTH, width);
    svga_write(SVGA_REG_HEIGHT, height);
    svga_write(SVGA_REG_BITS_PER_PIXEL, 32);
    svga_write(SVGA_REG_ENABLE, 1);

    /* Read back actual parameters */
    fb_width  = svga_read(SVGA_REG_WIDTH);
    fb_height = svga_read(SVGA_REG_HEIGHT);
    fb_pitch  = svga_read(SVGA_REG_BYTES_PER_LINE);
    fb_offset = svga_read(SVGA_REG_FB_OFFSET);

    /* Check if we have room for double buffering */
    uint32_t page_size = fb_pitch * fb_height;
    can_flip = (vram_size >= (fb_offset + page_size * 2));

    /* Initialize FIFO */
    uint32_t fifo_size = svga_read(SVGA_REG_MEM_SIZE);
    fifo[SVGA_FIFO_MIN]      = 16;
    fifo[SVGA_FIFO_MAX]      = fifo_size;
    fifo[SVGA_FIFO_NEXT_CMD] = 16;
    fifo[SVGA_FIFO_STOP]     = 16;
    svga_write(SVGA_REG_CONFIG_DONE, 1);

    terminal_printf(" VMSVGA: %dx%d 32bpp pitch=%d", fb_width, fb_height, fb_pitch);
    if (can_flip)
        terminal_print(", page flip\n");
    else
        terminal_print(", no flip\n");
}

uint8_t *vmware_framebuffer(void) {
    return fb_base + fb_offset;
}

uint32_t vmware_width(void) {
    return fb_width;
}

uint32_t vmware_height(void) {
    return fb_height;
}

uint32_t vmware_pitch(void) {
    return fb_pitch;
}

int vmware_can_flip(void) {
    return can_flip;
}

void vmware_set_page(int page) {
    svga_update(0, 0, fb_width, fb_height);
    svga_sync();
}

static void vmware_update(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    svga_update(x, y, w, h);
    svga_sync();
}

uint8_t *vmware_page_addr(int page) {
    uint32_t page_size = fb_pitch * fb_height;
    return fb_base + fb_offset + (page ? page_size : 0);
}

struct gpu_driver vmware_gpu_driver = {
    .name        = "VMSVGA",
    .detect      = vmware_detect,
    .init        = vmware_init,
    .framebuffer = vmware_framebuffer,
    .width       = vmware_width,
    .height      = vmware_height,
    .pitch       = vmware_pitch,
    .can_flip    = vmware_can_flip,
    .page_addr   = vmware_page_addr,
    .set_page    = vmware_set_page,
    .update      = vmware_update,
};

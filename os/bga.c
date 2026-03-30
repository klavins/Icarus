/*
 * bga.c - Bochs Graphics Adapter (BGA) display driver
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

#include "bga.h"
#include "gpu.h"
#include "pci.h"
#include "io.h"
#include "vga.h"

/* BGA VBE register ports */
#define VBE_INDEX 0x01CE
#define VBE_DATA  0x01CF

/* BGA register indices */
#define VBE_ID          0x00
#define VBE_XRES        0x01
#define VBE_YRES        0x02
#define VBE_BPP         0x03
#define VBE_ENABLE      0x04
#define VBE_BANK        0x05
#define VBE_VIRT_WIDTH  0x06
#define VBE_VIRT_HEIGHT 0x07
#define VBE_X_OFFSET    0x08
#define VBE_Y_OFFSET    0x09

/* Enable flags */
#define VBE_DISABLED    0x00
#define VBE_ENABLED     0x01
#define VBE_LFB_ENABLED 0x40
#define VBE_NOCLEARMEM  0x80

/* PCI vendor/device for BGA */
#define BGA_PCI_VENDOR 0x1234
#define BGA_PCI_DEVICE 0x1111

static uint8_t  bga_bus, bga_slot, bga_func;
static uint8_t *fb_base;
static uint32_t fb_width;
static uint32_t fb_height;
static uint32_t fb_pitch;
static int      detected;
static int      can_flip;

static void vbe_write(uint16_t reg, uint16_t val) {
    outw(VBE_INDEX, reg);
    outw(VBE_DATA, val);
}

static uint16_t vbe_read(uint16_t reg) {
    outw(VBE_INDEX, reg);
    return inw(VBE_DATA);
}

int bga_detect(void) {
    if (!pci_find_vendor(BGA_PCI_VENDOR, BGA_PCI_DEVICE,
                         &bga_bus, &bga_slot, &bga_func))
        return 0;

    /* Verify VBE ID */
    uint16_t ver = vbe_read(VBE_ID);
    if (ver < 0xB0C0) return 0;

    /* Read framebuffer base from BAR0 */
    uint32_t bar0 = pci_read_bar(bga_bus, bga_slot, bga_func, 0);
    fb_base = (uint8_t *)(uintptr_t)(bar0 & 0xFFFFFFF0);

    detected = 1;
    terminal_printf(" BGA: v%x at %d:%d.%d fb=0x%x\n",
                    ver, bga_bus, bga_slot, bga_func,
                    (uint32_t)(uintptr_t)fb_base);
    return 1;
}

void bga_init(uint32_t width, uint32_t height) {
    if (!detected) return;

    /* Enable PCI memory access */
    pci_enable_device(bga_bus, bga_slot, bga_func);

    /* Disable display while configuring */
    vbe_write(VBE_ENABLE, VBE_DISABLED);

    /* Set resolution and color depth */
    vbe_write(VBE_XRES, width);
    vbe_write(VBE_YRES, height);
    vbe_write(VBE_BPP, 32);

    /* Virtual height = 2x for double buffering (page flipping) */
    vbe_write(VBE_VIRT_WIDTH, width);
    vbe_write(VBE_VIRT_HEIGHT, height * 2);
    vbe_write(VBE_X_OFFSET, 0);
    vbe_write(VBE_Y_OFFSET, 0);

    /* Enable with linear framebuffer, don't clear memory */
    vbe_write(VBE_ENABLE, VBE_ENABLED | VBE_LFB_ENABLED | VBE_NOCLEARMEM);

    fb_width = width;
    fb_height = height;
    fb_pitch = width * 4; /* 32bpp = 4 bytes per pixel */

    /* Verify BGA accepted our settings */
    uint16_t actual_vh = vbe_read(VBE_VIRT_HEIGHT);
    can_flip = (actual_vh >= height * 2);

    terminal_printf(" BGA: %dx%d 32bpp", width, height);
    if (can_flip)
        terminal_print(", page flip\n");
    else
        terminal_printf(", no flip (virt %d < %d)\n", actual_vh, height * 2);
}

uint8_t *bga_framebuffer(void) {
    return fb_base;
}

uint32_t bga_width(void) {
    return fb_width;
}

uint32_t bga_height(void) {
    return fb_height;
}

uint32_t bga_pitch(void) {
    return fb_pitch;
}

int bga_can_flip(void) {
    return can_flip;
}

void bga_set_page(int page) {
    /* Flip by changing the Y offset the display scans from */
    vbe_write(VBE_Y_OFFSET, page ? fb_height : 0);
}

uint8_t *bga_page_addr(int page) {
    /* Page 0 starts at fb_base, page 1 at fb_base + one screen */
    return fb_base + (page ? fb_height * fb_pitch : 0);
}

struct gpu_driver bga_gpu_driver = {
    .name        = "BGA",
    .detect      = bga_detect,
    .init        = bga_init,
    .framebuffer = bga_framebuffer,
    .width       = bga_width,
    .height      = bga_height,
    .pitch       = bga_pitch,
    .can_flip    = bga_can_flip,
    .page_addr   = bga_page_addr,
    .set_page    = bga_set_page,
};

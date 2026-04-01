/*
 * graphics.c - High-level graphics drawing API with page flipping
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

#include "graphics.h"
#include "os.h"
#include "klib.h"
#include "malloc.h"
#include "vga.h"
#include "font_8x16.h"
#include "gpu.h"

static int current_mode;
static uint32_t draw_color;
static int cursor_x;
static int cursor_y;

/* Physical framebuffer (always 32bpp on UEFI) */
static uint8_t  *gfx_fb_addr;
static uint32_t  gfx_fb_width;
static uint32_t  gfx_fb_height;
static uint32_t  gfx_fb_pitch;

/* Shadow buffer — all drawing goes here */
static uint8_t *shadow_fb;
static uint8_t *draw_buf;      /* points to shadow_fb */
static uint32_t shadow_fb_size;

/* GPU page flipping state */
static int use_page_flip;      /* 1 if GPU page flipping is active */
static int draw_page;          /* which GPU page the display is showing (0 or 1) */

/* Dirty region tracking — only used for memcpy path (no page flip) */
static uint32_t dirty_y0;
static uint32_t dirty_y1;  /* exclusive */

static void dirty_mark(uint32_t y0, uint32_t y1) {
    if (y0 < dirty_y0) dirty_y0 = y0;
    if (y1 > dirty_y1) dirty_y1 = y1;
}

static void dirty_reset(void) {
    dirty_y0 = gfx_fb_height;
    dirty_y1 = 0;
}

/* Virtual resolution (what BASIC programs see) */
static uint32_t virt_width;
static uint32_t virt_height;

/* Uniform scaling: integer scale factor + centering offsets */
static uint32_t pixel_scale;   /* integer scale factor (1, 2, 3, ...) */
static uint32_t offset_x;     /* centering offset in physical pixels */
static uint32_t offset_y;

/* Target widths for pixel graphics modes 2-5 */
static const uint32_t mode_target_width[] = { 0, 0, 320, 640, 800, 0 };

static uint32_t vga_to_rgb32(int color);

/* Saved framebuffer for restoring text screen after graphics mode */
static uint8_t *saved_fb;
static uint32_t saved_fb_size;

/* ---- Public API ---- */

static void setup_virtual_res(int mode) {
    if (mode == 5 || mode_target_width[mode] == 0) {
        /* Native resolution — 1:1 mapping */
        virt_width  = gfx_fb_width;
        virt_height = gfx_fb_height;
        pixel_scale = 1;
    } else {
        /* Derive scale from physical width / target width */
        pixel_scale = gfx_fb_width / mode_target_width[mode];
        if (pixel_scale < 1) pixel_scale = 1;
        /* Virtual resolution = physical / scale — fills entire screen */
        virt_width  = gfx_fb_width / pixel_scale;
        virt_height = gfx_fb_height / pixel_scale;
    }
    offset_x = 0;
    offset_y = 0;
}

void graphics_alloc_init(void) {
    uint8_t *addr;
    uint32_t w, h, pitch, bpp;
    terminal_get_fb(&addr, &w, &h, &pitch, &bpp);
    if (addr && w > 0) {
        uint32_t size = pitch * h;
        shadow_fb = malloc(size);
        saved_fb = malloc(size);
        shadow_fb_size = size;
        saved_fb_size = size;
        /* Initialize virtual resolution for text mode SCRW/SCRH */
        virt_width = w;
        virt_height = h;
    }
}

void gfx_set_mode(int mode) {
    if (mode < 0 || mode > 5) return;

    {
        uint32_t bpp;
        terminal_get_fb(&gfx_fb_addr, &gfx_fb_width, &gfx_fb_height,
                        &gfx_fb_pitch, &bpp);
    }

    int was_graphics = (current_mode >= 2);
    int want_graphics = (mode >= 2);

    if (mode <= 1 && !was_graphics) {
        /* Switching between text modes (0=1x, 1=2x) */
        terminal_set_scale(mode == 1 ? 2 : 1);
        terminal_clear();
        current_mode = mode;
        virt_width = gfx_fb_width;
        virt_height = gfx_fb_height;
    } else if (want_graphics && !was_graphics) {
        /* Entering pixel graphics from text — save current screen */
        uint32_t needed = gfx_fb_pitch * gfx_fb_height;
        if (saved_fb && needed <= saved_fb_size) {
            uint8_t *src = gpu ? gpu->page_addr(0) : gfx_fb_addr;
            memcpy(saved_fb, src, needed);
        }

        /* Set up drawing target */
        if (gpu && gpu->can_flip() && shadow_fb) {
            use_page_flip = 1;
            draw_page = 0;
            uint32_t page_size = gfx_fb_pitch * gfx_fb_height;
            memset(gpu->page_addr(0), 0, page_size);
            memset(gpu->page_addr(1), 0, page_size);
            gpu->set_page(0);
            draw_buf = shadow_fb;
        } else if (shadow_fb) {
            use_page_flip = 0;
            draw_buf = shadow_fb;
            dirty_reset();
        } else {
            return;
        }

        setup_virtual_res(mode);
        current_mode = mode;
        draw_color = vga_to_rgb32(15);
        cursor_x = 0;
        cursor_y = 0;
        gfx_clear();
    } else if (want_graphics && was_graphics) {
        /* Switching between pixel graphics modes */
        setup_virtual_res(mode);
        current_mode = mode;
        draw_color = vga_to_rgb32(15);
        cursor_x = 0;
        cursor_y = 0;
        gfx_clear();
        gfx_present();
    } else if (mode <= 1 && was_graphics) {
        /* Return to text from pixel graphics — restore saved screen */
        use_page_flip = 0;
        terminal_set_scale(mode == 1 ? 2 : 1);
        if (saved_fb && saved_fb_size > 0) {
            uint32_t needed = gfx_fb_pitch * gfx_fb_height;
            if (needed <= saved_fb_size) {
                /* Restore to both the GPU/framebuffer and the terminal shadow */
                uint8_t *dst = gpu ? gpu->page_addr(0) : gfx_fb_addr;
                memcpy(dst, saved_fb, needed);
                terminal_restore_shadow(saved_fb, needed);
            }
        }
        if (gpu) {
            gpu->set_page(0);
            gpu_update(0, 0, gfx_fb_width, gfx_fb_height);
        }
        current_mode = mode;
        virt_width = gfx_fb_width;
        virt_height = gfx_fb_height;
    }
}

int gfx_get_mode(void) {
    return current_mode;
}

int gfx_width(void) {
    return virt_width;
}

int gfx_height(void) {
    return virt_height;
}

void gfx_clear(void) {
    if (current_mode < 2 || !draw_buf) return;
    uint32_t size = gfx_fb_pitch * gfx_fb_height;
    memset(draw_buf, 0, size);
    if (!use_page_flip) {
        dirty_y0 = 0;
        dirty_y1 = gfx_fb_height;
    }
}

/* Standard VGA DAC palette — first 64 entries cover the most-used colors.
   Each entry is the 6-bit DAC value (0-63) for R, G, B. */
static const uint8_t vga_dac[256][3] = {
    /* 0x00-0x0F: standard 16 colors */
    { 0, 0, 0},{  0, 0,42},{ 0,42, 0},{ 0,42,42},
    {42, 0, 0},{42, 0,42},{42,21, 0},{42,42,42},
    {21,21,21},{21,21,63},{21,63,21},{21,63,63},
    {63,21,21},{63,21,63},{63,63,21},{63,63,63},
    /* 0x10-0x1F: grayscale ramp */
    { 0, 0, 0},{ 5, 5, 5},{ 8, 8, 8},{11,11,11},
    {14,14,14},{17,17,17},{20,20,20},{24,24,24},
    {28,28,28},{32,32,32},{36,36,36},{40,40,40},
    {45,45,45},{50,50,50},{56,56,56},{63,63,63},
    /* 0x20-0x2F */
    { 0, 0,63},{16, 0,63},{31, 0,63},{47, 0,63},
    {63, 0,63},{63, 0,47},{63, 0,31},{63, 0,16},
    {63, 0, 0},{63,16, 0},{63,31, 0},{63,47, 0},
    {63,63, 0},{47,63, 0},{31,63, 0},{16,63, 0},
    /* 0x30-0x3F */
    { 0,63, 0},{ 0,63,16},{ 0,63,31},{ 0,63,47},
    { 0,63,63},{ 0,47,63},{ 0,31,63},{ 0,16,63},
    {31,31,63},{39,31,63},{47,31,63},{55,31,63},
    {63,31,63},{63,31,55},{63,31,47},{63,31,39},
    /* 0x40-0x4F */
    {63,31,31},{63,39,31},{63,47,31},{63,55,31},
    {63,63,31},{55,63,31},{47,63,31},{39,63,31},
    {31,63,31},{31,63,39},{31,63,47},{31,63,55},
    {31,63,63},{31,55,63},{31,47,63},{31,39,63},
    /* 0x50-0x5F */
    {45,45,63},{49,45,63},{54,45,63},{58,45,63},
    {63,45,63},{63,45,58},{63,45,54},{63,45,49},
    {63,45,45},{63,49,45},{63,54,45},{63,58,45},
    {63,63,45},{58,63,45},{54,63,45},{49,63,45},
    /* 0x60-0x6F */
    {45,63,45},{45,63,49},{45,63,54},{45,63,58},
    {45,63,63},{45,58,63},{45,54,63},{45,49,63},
    { 0, 0,28},{ 7, 0,28},{14, 0,28},{21, 0,28},
    {28, 0,28},{28, 0,21},{28, 0,14},{28, 0, 7},
    /* 0x70-0x7F */
    {28, 0, 0},{28, 7, 0},{28,14, 0},{28,21, 0},
    {28,28, 0},{21,28, 0},{14,28, 0},{ 7,28, 0},
    { 0,28, 0},{ 0,28, 7},{ 0,28,14},{ 0,28,21},
    { 0,28,28},{ 0,21,28},{ 0,14,28},{ 0, 7,28},
    /* 0x80-0x8F */
    {14,14,28},{17,14,28},{21,14,28},{24,14,28},
    {28,14,28},{28,14,24},{28,14,21},{28,14,17},
    {28,14,14},{28,17,14},{28,21,14},{28,24,14},
    {28,28,14},{24,28,14},{21,28,14},{17,28,14},
    /* 0x90-0x9F */
    {14,28,14},{14,28,17},{14,28,21},{14,28,24},
    {14,28,28},{14,24,28},{14,21,28},{14,17,28},
    {20,20,28},{22,20,28},{24,20,28},{26,20,28},
    {28,20,28},{28,20,26},{28,20,24},{28,20,22},
    /* 0xA0-0xAF */
    {28,20,20},{28,22,20},{28,24,20},{28,26,20},
    {28,28,20},{26,28,20},{24,28,20},{22,28,20},
    {20,28,20},{20,28,22},{20,28,24},{20,28,26},
    {20,28,28},{20,26,28},{20,24,28},{20,22,28},
    /* 0xB0-0xBF */
    { 0, 0,16},{ 4, 0,16},{ 8, 0,16},{12, 0,16},
    {16, 0,16},{16, 0,12},{16, 0, 8},{16, 0, 4},
    {16, 0, 0},{16, 4, 0},{16, 8, 0},{16,12, 0},
    {16,16, 0},{12,16, 0},{ 8,16, 0},{ 4,16, 0},
    /* 0xC0-0xCF */
    { 0,16, 0},{ 0,16, 4},{ 0,16, 8},{ 0,16,12},
    { 0,16,16},{ 0,12,16},{ 0, 8,16},{ 0, 4,16},
    { 8, 8,16},{10, 8,16},{12, 8,16},{14, 8,16},
    {16, 8,16},{16, 8,14},{16, 8,12},{16, 8,10},
    /* 0xD0-0xDF */
    {16, 8, 8},{16,10, 8},{16,12, 8},{16,14, 8},
    {16,16, 8},{14,16, 8},{12,16, 8},{10,16, 8},
    { 8,16, 8},{ 8,16,10},{ 8,16,12},{ 8,16,14},
    { 8,16,16},{ 8,14,16},{ 8,12,16},{ 8,10,16},
    /* 0xE0-0xEF */
    {11,11,16},{12,11,16},{13,11,16},{15,11,16},
    {16,11,16},{16,11,15},{16,11,13},{16,11,12},
    {16,11,11},{16,12,11},{16,13,11},{16,15,11},
    {16,16,11},{15,16,11},{13,16,11},{12,16,11},
    /* 0xF0-0xFF */
    {11,16,11},{11,16,12},{11,16,13},{11,16,15},
    {11,16,16},{11,15,16},{11,13,16},{11,12,16},
    { 0, 0, 0},{ 0, 0, 0},{ 0, 0, 0},{ 0, 0, 0},
    { 0, 0, 0},{ 0, 0, 0},{ 0, 0, 0},{ 0, 0, 0},
};

/* Convert VGA 8-bit palette index to 32-bit BGRA */
static uint32_t vga_to_rgb32(int color) {
    if (color < 0 || color > 255) color = 0;
    /* VGA DAC values are 6-bit (0-63), scale to 8-bit (0-255) */
    uint32_t r = (vga_dac[color][0] * 255) / 63;
    uint32_t g = (vga_dac[color][1] * 255) / 63;
    uint32_t b = (vga_dac[color][2] * 255) / 63;
    return (r << 16) | (g << 8) | b;
}

void gfx_set_color(int color) {
    draw_color = vga_to_rgb32(color);
}

/* Draw a single physical pixel (no scaling, always 32bpp) */
static void raw_pixel(uint32_t px, uint32_t py, uint32_t color) {
    if (px >= gfx_fb_width || py >= gfx_fb_height)
        return;
    uint32_t *p = (uint32_t *)(draw_buf + py * gfx_fb_pitch + px * 4);
    *p = color;
    if (!use_page_flip)
        dirty_mark(py, py + 1);
}

void gfx_pixel(int x, int y, int color) {
    if (x < 0 || (uint32_t)x >= virt_width || y < 0 || (uint32_t)y >= virt_height)
        return;

    uint32_t px0 = offset_x + x * pixel_scale;
    uint32_t py0 = offset_y + y * pixel_scale;
    for (uint32_t py = py0; py < py0 + pixel_scale; py++)
        for (uint32_t px = px0; px < px0 + pixel_scale; px++)
            raw_pixel(px, py, color);
}

void gfx_plot(int x, int y) {
    gfx_pixel(x, y, draw_color);
    cursor_x = x;
    cursor_y = y;
}

/* Bresenham's line algorithm */
void gfx_drawto(int x1, int y1) {
    int x0 = cursor_x;
    int y0 = cursor_y;
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int err = dx - dy;

    for (;;) {
        gfx_pixel(x0, y0, draw_color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }

    cursor_x = x1;
    cursor_y = y1;
}

void gfx_fillto(int x1, int y1) {
    int x0 = cursor_x;
    int y0 = cursor_y;
    /* Sort coordinates */
    int left   = (x0 < x1) ? x0 : x1;
    int right  = (x0 > x1) ? x0 : x1;
    int top    = (y0 < y1) ? y0 : y1;
    int bottom = (y0 > y1) ? y0 : y1;
    for (int y = top; y <= bottom; y++)
        for (int x = left; x <= right; x++)
            gfx_pixel(x, y, draw_color);
    cursor_x = x1;
    cursor_y = y1;
}

void gfx_present(void) {
    if (current_mode < 2) return;

    if (use_page_flip && gpu) {
        /* Copy shadow buffer to the back page, then flip */
        int back = 1 - draw_page;
        uint32_t size = gfx_fb_pitch * gfx_fb_height;
        memcpy(gpu->page_addr(back), shadow_fb, size);
        gpu->set_page(back);
        draw_page = back;
    } else {
        if (!gfx_fb_addr || !shadow_fb_size) return;
        if (dirty_y0 >= dirty_y1) return; /* nothing changed */
        /* Only copy the dirty rows */
        uint32_t offset = dirty_y0 * gfx_fb_pitch;
        uint32_t bytes  = (dirty_y1 - dirty_y0) * gfx_fb_pitch;
        memcpy(gfx_fb_addr + offset, draw_buf + offset, bytes);
        gpu_update(0, dirty_y0, gfx_fb_width, dirty_y1 - dirty_y0);
        dirty_reset();
    }
}

void gfx_pos(int x, int y) {
    cursor_x = x;
    cursor_y = y;
}

void gfx_text(const char *str) {
    while (*str) {
        unsigned char ch = (unsigned char)*str;
        if (ch >= 128) ch = 0;
        const uint8_t *glyph = font_8x16[ch];
        for (int row = 0; row < 16; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col))
                    gfx_pixel(cursor_x + col, cursor_y + row, draw_color);
            }
        }
        cursor_x += 8;
        str++;
    }
}

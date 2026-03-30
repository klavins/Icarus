/*
 * fbconsole.c - Framebuffer-based text console with bitmap font rendering
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

#include "vga.h"
#include "klib.h"
#include "io.h"
#include "font_8x16.h"
#include "gpu.h"
#include <stdarg.h>

extern void *basic_alloc(size_t size);

#define FONT_W 8
#define FONT_H 16

/* Text scaling factor — set at init based on resolution */
static int text_scale = 1;
#define CELL_W (FONT_W * text_scale)
#define CELL_H (FONT_H * text_scale)

/* Framebuffer state — configured by terminal_init */
static uint8_t *fb_addr;      /* real (MMIO) framebuffer */
static uint8_t *fb_shadow;    /* RAM shadow — all reads/writes go here */
static uint32_t fb_width;
static uint32_t fb_height;
static uint32_t fb_pitch;   /* bytes per scan line */
static uint32_t fb_bpp;     /* bytes per pixel: 1 = 8-bit indexed, 4 = 32-bit */
static uint32_t fb_cols;
static uint32_t fb_rows;

static size_t cursor_row;
static size_t cursor_col;
static uint32_t fg_color;
static uint32_t bg_color;
static int raw_fg;
static int raw_bg;

/* UEFI framebuffer info — set by uefi_boot.c if UEFI, zero otherwise */
struct framebuffer_info {
    uint32_t *addr;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;
    uint32_t  bpp;
};
extern struct framebuffer_info fb_info __attribute__((weak));

/* Fixed-address struct from 64-bit UEFI boot */
struct framebuffer_info_fixed {
    uint32_t addr_lo;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t total_memory_kb;
    char     firmware_vendor[64];
    uint32_t firmware_revision;
    uint32_t pixel_format;
    uint32_t heap_base;
    uint32_t heap_size;
};
#define FB_INFO_FIXED ((volatile struct framebuffer_info_fixed *)0x00080000)

/* 32-bit BGRA colors for UEFI GOP */
static const uint32_t color32_map[16] = {
    0x00000000,  /* black */
    0x000000AA,  /* blue */
    0x0000AA00,  /* green */
    0x0000AAAA,  /* cyan */
    0x00AA0000,  /* red */
    0x00AA00AA,  /* magenta */
    0x00AA5500,  /* brown */
    0x00AAAAAA,  /* light gray */
    0x00555555,  /* dark gray */
    0x005555FF,  /* light blue */
    0x0055FF55,  /* light green */
    0x0055FFFF,  /* light cyan */
    0x00FF5555,  /* light red */
    0x00FF55FF,  /* light magenta */
    0x00FFFF55,  /* yellow */
    0x00FFFFFF,  /* white */
};

/* 8-bit palette indices for Mode 13h */
static const uint8_t color8_map[16] = {
    0, 1, 2, 3, 4, 5, 20, 7,
    56, 57, 58, 59, 60, 61, 62, 15,
};

/* Write to shadow buffer (fast RAM), not directly to MMIO framebuffer */
static inline void fb_pixel(uint32_t x, uint32_t y, uint32_t color) {
    uint8_t *buf = fb_shadow ? fb_shadow : fb_addr;
    if (x >= fb_width || y >= fb_height)
        return;
    if (fb_bpp == 4) {
        uint32_t *p = (uint32_t *)(buf + y * fb_pitch + x * 4);
        *p = color;
    } else {
        buf[y * fb_pitch + x] = (uint8_t)color;
    }
}

/* Flush a rectangular region from shadow to real framebuffer */
static void fb_flush_region(uint32_t y0, uint32_t rows) {
    if (!fb_shadow) return;
    uint32_t offset = y0 * fb_pitch;
    uint32_t bytes  = rows * fb_pitch;
    memcpy(fb_addr + offset, fb_shadow + offset, bytes);
    gpu_update(0, y0, fb_width, rows);
}

static void fb_flush_all(void) {
    if (!fb_shadow) return;
    memcpy(fb_addr, fb_shadow, fb_pitch * fb_height);
    gpu_update(0, 0, fb_width, fb_height);
}

static void draw_cursor(int show) {
    uint32_t x0 = cursor_col * CELL_W;
    uint32_t y0 = cursor_row * CELL_H + CELL_H - 2 * text_scale;
    uint32_t color = show ? fg_color : bg_color;
    for (int y = 0; y < 2 * text_scale; y++)
        for (int x = 0; x < CELL_W; x++)
            fb_pixel(x0 + x, y0 + y, color);
}

static void fb_draw_char(uint32_t col, uint32_t row, unsigned char ch,
                          uint32_t fg, uint32_t bg) {
    uint32_t x0 = col * CELL_W;
    uint32_t y0 = row * CELL_H;
    const uint8_t *glyph = (ch < 128) ? font_8x16[ch] : font_8x16[0];

    for (int y = 0; y < FONT_H; y++) {
        uint8_t bits = glyph[y];
        for (int x = 0; x < FONT_W; x++) {
            uint32_t color = (bits & (0x80 >> x)) ? fg : bg;
            for (int sy = 0; sy < text_scale; sy++)
                for (int sx = 0; sx < text_scale; sx++)
                    fb_pixel(x0 + x * text_scale + sx,
                             y0 + y * text_scale + sy, color);
        }
    }
}

static void fb_scroll(void) {
    uint8_t *buf = fb_shadow ? fb_shadow : fb_addr;
    uint32_t row_bytes = fb_pitch * CELL_H;
    uint32_t total = fb_pitch * (fb_height - CELL_H);

    /* Move everything up by one character row (in fast RAM) */
    memcpy(buf, buf + row_bytes, total);

    /* Clear last row */
    uint8_t *last = buf + fb_pitch * (fb_height - CELL_H);
    if (fb_bpp == 4) {
        uint32_t *p = (uint32_t *)last;
        uint32_t pixels = fb_width * CELL_H;
        for (uint32_t i = 0; i < pixels; i++)
            p[i] = bg_color;
    } else {
        memset(last, (uint8_t)bg_color, fb_pitch * CELL_H);
    }

    /* One write to the real framebuffer */
    fb_flush_all();
}

/* Mode 13h register tables */
static const uint8_t mode13h_misc = 0x63;
static const uint8_t mode13h_seq[] = { 0x03, 0x01, 0x0F, 0x00, 0x0E };
static const uint8_t mode13h_crtc[] = {
    0x5F,0x4F,0x50,0x82,0x54,0x80,0xBF,0x1F,
    0x00,0x41,0x00,0x00,0x00,0x00,0x00,0x00,
    0x9C,0x0E,0x8F,0x28,0x40,0x96,0xB9,0xA3,0xFF
};
static const uint8_t mode13h_gc[] = {
    0x00,0x00,0x00,0x00,0x00,0x40,0x05,0x0F,0xFF
};
static const uint8_t mode13h_ac[] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x41,0x00,0x0F,0x00,0x00
};

static void init_mode13h(void) {
    outb(0x3C2, mode13h_misc);
    for (int i = 0; i < 5; i++) { outb(0x3C4, i); outb(0x3C5, mode13h_seq[i]); }
    outb(0x3D4, 0x03); outb(0x3D5, inb(0x3D5) | 0x80);
    outb(0x3D4, 0x11); outb(0x3D5, inb(0x3D5) & ~0x80);
    for (int i = 0; i < 25; i++) { outb(0x3D4, i); outb(0x3D5, mode13h_crtc[i]); }
    for (int i = 0; i < 9; i++) { outb(0x3CE, i); outb(0x3CF, mode13h_gc[i]); }
    inb(0x3DA);
    for (int i = 0; i < 21; i++) { outb(0x3C0, i); outb(0x3C0, mode13h_ac[i]); }
    outb(0x3C0, 0x20);

    fb_addr   = (uint8_t *)0xA0000;
    fb_width  = 320;
    fb_height = 200;
    fb_pitch  = 320;
    fb_bpp    = 1;
}

void terminal_init(void) {
    /* Check if 32-bit UEFI provided a framebuffer (weak symbol) */
    if (&fb_info && fb_info.addr && fb_info.width > 0) {
        fb_addr   = (uint8_t *)fb_info.addr;
        fb_width  = fb_info.width;
        fb_height = fb_info.height;
        fb_pitch  = fb_info.pitch * fb_info.bpp;
        fb_bpp    = fb_info.bpp;
    /* Check if 64-bit UEFI stored fb info at fixed address */
    } else if (FB_INFO_FIXED->addr_lo != 0 && FB_INFO_FIXED->width > 0) {
        fb_addr   = (uint8_t *)(uintptr_t)FB_INFO_FIXED->addr_lo;
        fb_width  = FB_INFO_FIXED->width;
        fb_height = FB_INFO_FIXED->height;
        fb_pitch  = FB_INFO_FIXED->pitch * FB_INFO_FIXED->bpp;
        fb_bpp    = FB_INFO_FIXED->bpp;
    } else {
        /* Fall back to VGA Mode 13h */
        init_mode13h();
    }

    text_scale = 1;
    fb_cols = fb_width / CELL_W;
    fb_rows = fb_height / CELL_H;

    /* Allocate RAM shadow buffer — avoids slow MMIO reads during scroll */
    uint32_t fb_size = fb_pitch * fb_height;
    fb_shadow = basic_alloc(fb_size);

    fg_color = (fb_bpp == 4) ? color32_map[VGA_WHITE] : color8_map[VGA_WHITE];
    bg_color = (fb_bpp == 4) ? color32_map[VGA_BLACK] : color8_map[VGA_BLACK];
    cursor_row = 0;
    cursor_col = 0;
    terminal_clear();
}

void terminal_setcolor(enum vga_color fg, enum vga_color bg) {
    raw_fg = fg & 0x0F;
    raw_bg = bg & 0x0F;
    if (fb_bpp == 4) {
        fg_color = color32_map[raw_fg];
        bg_color = color32_map[raw_bg];
    } else {
        fg_color = color8_map[raw_fg];
        bg_color = color8_map[raw_bg];
    }
}

void terminal_getcolor(int *fg, int *bg) {
    *fg = raw_fg;
    *bg = raw_bg;
}

void terminal_clear(void) {
    uint8_t *buf = fb_shadow ? fb_shadow : fb_addr;
    if (fb_bpp == 4) {
        uint32_t *p = (uint32_t *)buf;
        uint32_t total = fb_width * fb_height;
        for (uint32_t i = 0; i < total; i++)
            p[i] = bg_color;
    } else {
        memset(buf, (uint8_t)bg_color, fb_width * fb_height);
    }
    cursor_row = 0;
    cursor_col = 0;
    draw_cursor(1);
    fb_flush_all();
}

void terminal_putchar(char c) {
    draw_cursor(0); /* erase cursor */
    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            fb_draw_char(cursor_col, cursor_row, ' ', fg_color, bg_color);
        }
        draw_cursor(1);
        fb_flush_region(cursor_row * CELL_H, CELL_H);
        return;
    }
    if (c == '\n') {
        uint32_t old_row = cursor_row;
        cursor_col = 0;
        if (++cursor_row >= fb_rows) {
            fb_scroll(); /* does its own flush_all */
            cursor_row = fb_rows - 1;
        } else {
            /* Flush old cursor row + new cursor row */
            fb_flush_region(old_row * CELL_H, CELL_H);
            fb_flush_region(cursor_row * CELL_H, CELL_H);
        }
        draw_cursor(1);
        fb_flush_region(cursor_row * CELL_H, CELL_H);
        return;
    }
    uint32_t char_row = cursor_row;
    fb_draw_char(cursor_col, cursor_row, (unsigned char)c, fg_color, bg_color);
    if (++cursor_col >= fb_cols) {
        cursor_col = 0;
        if (++cursor_row >= fb_rows) {
            fb_scroll(); /* does its own flush_all */
            cursor_row = fb_rows - 1;
        }
    }
    draw_cursor(1);
    /* Flush the row(s) we touched */
    fb_flush_region(char_row * CELL_H, CELL_H);
    if (cursor_row != char_row)
        fb_flush_region(cursor_row * CELL_H, CELL_H);
}

void terminal_print(const char *s) {
    while (*s)
        terminal_putchar(*s++);
}

void terminal_printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    terminal_print(buf);
}

size_t terminal_getcol(void) {
    return cursor_col;
}

void terminal_get_fb(uint8_t **addr, uint32_t *width, uint32_t *height,
                     uint32_t *pitch, uint32_t *bpp) {
    *addr   = fb_addr;
    *width  = fb_width;
    *height = fb_height;
    *pitch  = fb_pitch;
    *bpp    = fb_bpp;
}

void terminal_set_fb(uint8_t *addr, uint32_t width, uint32_t height,
                     uint32_t pitch, uint32_t bpp) {
    int same_layout = (width == fb_width && height == fb_height &&
                       pitch == fb_pitch && bpp == fb_bpp);

    fb_addr   = addr;
    fb_width  = width;
    fb_height = height;
    fb_pitch  = pitch;
    fb_bpp    = bpp;

    /* Recalculate text grid — keep current scale */
    fb_cols = fb_width / CELL_W;
    fb_rows = fb_height / CELL_H;

    /* Update colors for new bpp */
    fg_color = (fb_bpp == 4) ? color32_map[raw_fg] : color8_map[raw_fg];
    bg_color = (fb_bpp == 4) ? color32_map[raw_bg] : color8_map[raw_bg];

    if (same_layout && fb_shadow) {
        /* Same resolution — just flush the shadow to the new framebuffer */
        fb_flush_all();
    } else {
        /* Resolution changed — need a new shadow buffer */
        fb_shadow = basic_alloc(fb_pitch * fb_height);
        terminal_clear();
    }
}

void terminal_set_scale(int scale) {
    if (scale < 1) scale = 1;
    text_scale = scale;
    fb_cols = fb_width / CELL_W;
    fb_rows = fb_height / CELL_H;
    if (cursor_row >= fb_rows) cursor_row = fb_rows - 1;
    if (cursor_col >= fb_cols) cursor_col = fb_cols - 1;
}

void terminal_restore_shadow(const uint8_t *data, uint32_t size) {
    if (fb_shadow) {
        uint32_t max = fb_pitch * fb_height;
        if (size > max) size = max;
        memcpy(fb_shadow, data, size);
    }
}

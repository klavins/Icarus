#include "graphics.h"
#include "io.h"
#include "klib.h"
#include "vga.h"

static int current_mode;
static uint32_t draw_color;
static int cursor_x;
static int cursor_y;

/* Framebuffer state for pixel drawing */
static uint8_t  *gfx_fb_addr;
static uint32_t  gfx_fb_width;
static uint32_t  gfx_fb_height;
static uint32_t  gfx_fb_pitch;
static uint32_t  gfx_fb_bpp;

/* Is the console a framebuffer (UEFI) or VGA text? */
static int using_fb_console;

/* ---- VGA Mode 13h support (BIOS boot only) ---- */

#define VGA_FRAMEBUFFER ((volatile uint8_t *)0xA0000)
#define VGA_TEXT_BUF    ((volatile uint16_t *)0xB8000)
#define VGA_TEXT_SIZE   (80 * 25)

static uint8_t  saved_font[256 * 32];
static uint16_t saved_text[VGA_TEXT_SIZE];

/* Saved framebuffer for UEFI mode (up to 1024x768x4 = 3MB) */
#define MAX_SAVED_FB (1024 * 768 * 4)
static uint8_t saved_fb[MAX_SAVED_FB];
static uint32_t saved_fb_size;

/* VGA register tables for Mode 13h */
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

/* Mode 03h register tables */
static const uint8_t mode03h_misc = 0x67;
static const uint8_t mode03h_seq[] = { 0x03, 0x00, 0x03, 0x00, 0x02 };
static const uint8_t mode03h_crtc[] = {
    0x5F,0x4F,0x50,0x82,0x55,0x81,0xBF,0x1F,
    0x00,0x4F,0x0D,0x0E,0x00,0x00,0x00,0x50,
    0x9C,0x0E,0x8F,0x28,0x1F,0x96,0xB9,0xA3,0xFF
};
static const uint8_t mode03h_gc[] = {
    0x00,0x00,0x00,0x00,0x00,0x10,0x0E,0x00,0xFF
};
static const uint8_t mode03h_ac[] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x0C,0x00,0x0F,0x08,0x00
};

#define VGA_SEQ_INDEX  0x3C4
#define VGA_SEQ_DATA   0x3C5
#define VGA_GC_INDEX   0x3CE
#define VGA_GC_DATA    0x3CF
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA  0x3D5

static void write_regs(uint8_t misc, const uint8_t *seq, int seq_count,
                       const uint8_t *crtc, int crtc_count,
                       const uint8_t *gc, int gc_count,
                       const uint8_t *ac, int ac_count) {
    outb(0x3C2, misc);
    for (int i = 0; i < seq_count; i++) { outb(0x3C4, i); outb(0x3C5, seq[i]); }
    outb(0x3D4, 0x03); outb(0x3D5, inb(0x3D5) | 0x80);
    outb(0x3D4, 0x11); outb(0x3D5, inb(0x3D5) & ~0x80);
    for (int i = 0; i < crtc_count; i++) { outb(0x3D4, i); outb(0x3D5, crtc[i]); }
    for (int i = 0; i < gc_count; i++) { outb(0x3CE, i); outb(0x3CF, gc[i]); }
    inb(0x3DA);
    for (int i = 0; i < ac_count; i++) { outb(0x3C0, i); outb(0x3C0, ac[i]); }
    outb(0x3C0, 0x20);
}

static void expose_plane2(void) {
    outb(0x3C4, 0x00); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);
    outb(0x3C4, 0x04); outb(0x3C5, 0x07);
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3CE, 0x04); outb(0x3CF, 0x02);
    outb(0x3CE, 0x05); outb(0x3CF, 0x00);
    outb(0x3CE, 0x06); outb(0x3CF, 0x00);
}

static void restore_text_access(void) {
    outb(0x3C4, 0x00); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);
    outb(0x3C4, 0x04); outb(0x3C5, 0x03);
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3CE, 0x04); outb(0x3CF, 0x00);
    outb(0x3CE, 0x05); outb(0x3CF, 0x10);
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E);
}

static void save_font(void) {
    expose_plane2();
    volatile uint8_t *fm = (volatile uint8_t *)0xA0000;
    for (int i = 0; i < 256 * 32; i++) saved_font[i] = fm[i];
    restore_text_access();
}

static void restore_font(void) {
    expose_plane2();
    volatile uint8_t *fm = (volatile uint8_t *)0xA0000;
    for (int i = 0; i < 256 * 32; i++) fm[i] = saved_font[i];
    restore_text_access();
}

/* ---- Public API ---- */

void gfx_set_mode(int mode) {
    /* Check if console is framebuffer-based */
    terminal_get_fb(&gfx_fb_addr, &gfx_fb_width, &gfx_fb_height,
                    &gfx_fb_pitch, &gfx_fb_bpp);
    using_fb_console = (gfx_fb_addr != 0);

    if (mode == 1 && current_mode != 1) {
        if (using_fb_console) {
            /* Save the text framebuffer */
            saved_fb_size = gfx_fb_pitch * gfx_fb_height;
            if (saved_fb_size > MAX_SAVED_FB)
                saved_fb_size = MAX_SAVED_FB;
            memcpy(saved_fb, gfx_fb_addr, saved_fb_size);
        } else {
            /* VGA text mode — save screen, switch to Mode 13h */
            for (int i = 0; i < VGA_TEXT_SIZE; i++)
                saved_text[i] = VGA_TEXT_BUF[i];
            save_font();
            write_regs(mode13h_misc, mode13h_seq, 5, mode13h_crtc, 25,
                       mode13h_gc, 9, mode13h_ac, 21);
            gfx_fb_addr   = (uint8_t *)0xA0000;
            gfx_fb_width  = 320;
            gfx_fb_height = 200;
            gfx_fb_pitch  = 320;
            gfx_fb_bpp    = 1;
        }
        current_mode = 1;
        draw_color = (gfx_fb_bpp == 4) ? 0x00FFFFFF : 15;
        cursor_x = 0;
        cursor_y = 0;
        gfx_clear();
    } else if (mode == 0 && current_mode != 0) {
        if (using_fb_console) {
            /* Restore the text framebuffer */
            if (saved_fb_size > 0)
                memcpy(gfx_fb_addr, saved_fb, saved_fb_size);
        } else {
            /* Restore VGA text mode */
            write_regs(mode03h_misc, mode03h_seq, 5, mode03h_crtc, 25,
                       mode03h_gc, 9, mode03h_ac, 21);
            restore_font();
            for (int i = 0; i < VGA_TEXT_SIZE; i++)
                VGA_TEXT_BUF[i] = saved_text[i];
        }
        current_mode = 0;
    }
}

int gfx_get_mode(void) {
    return current_mode;
}

int gfx_width(void) {
    return gfx_fb_width;
}

int gfx_height(void) {
    return gfx_fb_height;
}

void gfx_clear(void) {
    if (current_mode != 1) return;
    if (gfx_fb_bpp == 4) {
        uint32_t *p = (uint32_t *)gfx_fb_addr;
        for (uint32_t i = 0; i < gfx_fb_width * gfx_fb_height; i++)
            p[i] = 0;
    } else {
        memset(gfx_fb_addr, 0, gfx_fb_width * gfx_fb_height);
    }
}

void gfx_set_color(int color) {
    draw_color = color;
}

void gfx_pixel(int x, int y, int color) {
    if (x < 0 || (uint32_t)x >= gfx_fb_width || y < 0 || (uint32_t)y >= gfx_fb_height)
        return;
    if (gfx_fb_bpp == 4) {
        uint32_t *p = (uint32_t *)(gfx_fb_addr + y * gfx_fb_pitch + x * 4);
        *p = (uint32_t)color;
    } else {
        gfx_fb_addr[y * gfx_fb_pitch + x] = (uint8_t)color;
    }
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

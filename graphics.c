#include "graphics.h"
#include "io.h"
#include "klib.h"
#include "vga.h"
#include "font_8x16.h"

static int current_mode;
static uint32_t draw_color;
static int cursor_x;
static int cursor_y;

/* Physical framebuffer */
static uint8_t  *gfx_fb_addr;
static uint32_t  gfx_fb_width;
static uint32_t  gfx_fb_height;
static uint32_t  gfx_fb_pitch;
static uint32_t  gfx_fb_bpp;

/* Virtual resolution (what BASIC programs see) */
static uint32_t virt_width;
static uint32_t virt_height;
static uint32_t scale_x;  /* physical pixels per virtual pixel (x256 fixed point) */
static uint32_t scale_y;

/* Virtual resolution table */
static const uint32_t mode_widths[]  = { 0, 320, 640, 800, 0 };
static const uint32_t mode_heights[] = { 0, 200, 400, 600, 0 };

/* Is the console a framebuffer (UEFI) or VGA text? */
static int using_fb_console;

static uint32_t vga_to_rgb32(int color);

/* ---- VGA Mode 13h support (BIOS boot only) ---- */

#define VGA_FRAMEBUFFER ((volatile uint8_t *)0xA0000)
#define VGA_TEXT_BUF    ((volatile uint16_t *)0xB8000)
#define VGA_TEXT_SIZE   (80 * 25)

static uint8_t  saved_font[256 * 32];
static uint16_t saved_text[VGA_TEXT_SIZE];

/* Saved framebuffer for UEFI mode (up to 1024x768x4 = 3MB) */
#define MAX_SAVED_FB (1920 * 1200 * 4)
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

static void setup_virtual_res(int mode) {
    if (mode == 4 || mode_widths[mode] == 0) {
        /* Native resolution */
        virt_width  = gfx_fb_width;
        virt_height = gfx_fb_height;
    } else {
        virt_width  = mode_widths[mode];
        virt_height = mode_heights[mode];
    }
    /* Fixed-point scale factors (x256) */
    scale_x = (gfx_fb_width  * 256) / virt_width;
    scale_y = (gfx_fb_height * 256) / virt_height;
}

void gfx_set_mode(int mode) {
    if (mode < 0 || mode > 4) return;

    /* Check if console is framebuffer-based */
    terminal_get_fb(&gfx_fb_addr, &gfx_fb_width, &gfx_fb_height,
                    &gfx_fb_pitch, &gfx_fb_bpp);
    using_fb_console = (gfx_fb_addr != 0);

    if (mode >= 1 && current_mode == 0) {
        /* Entering graphics from text */
        if (using_fb_console) {
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
        setup_virtual_res(mode);
        current_mode = mode;
        draw_color = (gfx_fb_bpp == 4) ? vga_to_rgb32(15) : 15;
        cursor_x = 0;
        cursor_y = 0;
        gfx_clear();
    } else if (mode >= 1 && current_mode >= 1) {
        /* Switching between graphics modes */
        setup_virtual_res(mode);
        current_mode = mode;
        draw_color = (gfx_fb_bpp == 4) ? vga_to_rgb32(15) : 15;
        cursor_x = 0;
        cursor_y = 0;
        gfx_clear();
    } else if (mode == 0 && current_mode != 0) {
        /* Return to text */
        if (using_fb_console) {
            if (saved_fb_size > 0)
                memcpy(gfx_fb_addr, saved_fb, saved_fb_size);
        } else {
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
    if (current_mode >= 1) return virt_width;
    /* In text mode, return the physical framebuffer size */
    uint8_t *addr;
    uint32_t w, h, p, b;
    terminal_get_fb(&addr, &w, &h, &p, &b);
    return w ? w : 320;
}

int gfx_height(void) {
    if (current_mode >= 1) return virt_height;
    uint8_t *addr;
    uint32_t w, h, p, b;
    terminal_get_fb(&addr, &w, &h, &p, &b);
    return h ? h : 200;
}

void gfx_clear(void) {
    if (current_mode < 1) return;
    if (gfx_fb_bpp == 4) {
        uint32_t *p = (uint32_t *)gfx_fb_addr;
        for (uint32_t i = 0; i < gfx_fb_width * gfx_fb_height; i++)
            p[i] = 0;
    } else {
        memset(gfx_fb_addr, 0, gfx_fb_width * gfx_fb_height);
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
    if (gfx_fb_bpp == 4)
        draw_color = vga_to_rgb32(color);
    else
        draw_color = color;
}

/* Draw a single physical pixel (no scaling) */
static void raw_pixel(uint32_t px, uint32_t py, uint32_t color) {
    if (px >= gfx_fb_width || py >= gfx_fb_height)
        return;
    if (gfx_fb_bpp == 4) {
        uint32_t *p = (uint32_t *)(gfx_fb_addr + py * gfx_fb_pitch + px * 4);
        *p = color;
    } else {
        gfx_fb_addr[py * gfx_fb_pitch + px] = (uint8_t)color;
    }
}

void gfx_pixel(int x, int y, int color) {
    if (x < 0 || (uint32_t)x >= virt_width || y < 0 || (uint32_t)y >= virt_height)
        return;

    /* Scale virtual coords to physical */
    uint32_t px0 = (x * scale_x) >> 8;
    uint32_t py0 = (y * scale_y) >> 8;
    uint32_t px1 = ((x + 1) * scale_x) >> 8;
    uint32_t py1 = ((y + 1) * scale_y) >> 8;

    /* Fill the scaled pixel block */
    for (uint32_t py = py0; py < py1; py++)
        for (uint32_t px = px0; px < px1; px++)
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

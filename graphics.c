#include "graphics.h"
#include "io.h"
#include "klib.h"
#include "vga.h"

#define VGA_FRAMEBUFFER ((volatile uint8_t *)0xA0000)

static int current_mode;
static int draw_color;
static int cursor_x;

/* Saved font data: 256 chars x 32 bytes each = 8192 bytes */
static uint8_t saved_font[256 * 32];

/* Saved text buffer: 80 x 25 x 2 bytes = 4000 bytes */
#define VGA_TEXT_BUF ((volatile uint16_t *)0xB8000)
#define VGA_TEXT_SIZE (80 * 25)
static uint16_t saved_text[VGA_TEXT_SIZE];
static int cursor_y;

/* ---- VGA register tables ---- */
/* These program the VGA into Mode 13h (320x200x256) or back to Mode 03h (80x25 text) */

/* Miscellaneous output register */
#define VGA_MISC_WRITE  0x3C2
#define VGA_MISC_READ   0x3CC

/* Sequencer */
#define VGA_SEQ_INDEX   0x3C4
#define VGA_SEQ_DATA    0x3C5

/* CRT Controller */
#define VGA_CRTC_INDEX  0x3D4
#define VGA_CRTC_DATA   0x3D5

/* Graphics Controller */
#define VGA_GC_INDEX    0x3CE
#define VGA_GC_DATA     0x3CF

/* Attribute Controller */
#define VGA_AC_INDEX    0x3C0
#define VGA_AC_WRITE    0x3C0
#define VGA_AC_READ     0x3C1
#define VGA_INSTAT_READ 0x3DA

/* Mode 13h register values */
static const uint8_t mode13h_misc = 0x63;

static const uint8_t mode13h_seq[] = {
    0x03, 0x01, 0x0F, 0x00, 0x0E
};

static const uint8_t mode13h_crtc[] = {
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
    0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
    0xFF
};

static const uint8_t mode13h_gc[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
    0xFF
};

static const uint8_t mode13h_ac[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x41, 0x00, 0x0F, 0x00, 0x00
};

/* Mode 03h (80x25 text) register values */
static const uint8_t mode03h_misc = 0x67;

static const uint8_t mode03h_seq[] = {
    0x03, 0x00, 0x03, 0x00, 0x02
};

static const uint8_t mode03h_crtc[] = {
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
    0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
    0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
    0xFF
};

static const uint8_t mode03h_gc[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00,
    0xFF
};

static const uint8_t mode03h_ac[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x0C, 0x00, 0x0F, 0x08, 0x00
};

static void write_regs(uint8_t misc, const uint8_t *seq, int seq_count,
                       const uint8_t *crtc, int crtc_count,
                       const uint8_t *gc, int gc_count,
                       const uint8_t *ac, int ac_count) {
    /* Misc register */
    outb(VGA_MISC_WRITE, misc);

    /* Sequencer */
    for (int i = 0; i < seq_count; i++) {
        outb(VGA_SEQ_INDEX, i);
        outb(VGA_SEQ_DATA, seq[i]);
    }

    /* Unlock CRTC */
    outb(VGA_CRTC_INDEX, 0x03);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);

    /* CRTC */
    for (int i = 0; i < crtc_count; i++) {
        outb(VGA_CRTC_INDEX, i);
        outb(VGA_CRTC_DATA, crtc[i]);
    }

    /* Graphics Controller */
    for (int i = 0; i < gc_count; i++) {
        outb(VGA_GC_INDEX, i);
        outb(VGA_GC_DATA, gc[i]);
    }

    /* Attribute Controller */
    inb(VGA_INSTAT_READ);  /* reset AC flip-flop */
    for (int i = 0; i < ac_count; i++) {
        outb(VGA_AC_INDEX, i);
        outb(VGA_AC_WRITE, ac[i]);
    }
    /* Enable display */
    outb(VGA_AC_INDEX, 0x20);
}

static void set_mode_13h(void) {
    write_regs(mode13h_misc,
               mode13h_seq,  sizeof(mode13h_seq),
               mode13h_crtc, sizeof(mode13h_crtc),
               mode13h_gc,   sizeof(mode13h_gc),
               mode13h_ac,   sizeof(mode13h_ac));
}

static void expose_plane2(void) {
    /* Set sequencer and GC to expose plane 2 at 0xA0000 */
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x01); /* sync reset */
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x04); /* write plane 2 */
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x07); /* sequential access */
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03); /* end reset */

    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x02); /* read plane 2 */
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x00); /* disable odd/even */
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x00); /* map at 0xA0000 */
}

static void restore_text_access(void) {
    /* Restore sequencer and GC for normal text mode */
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x01); /* sync reset */
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x03); /* planes 0 & 1 */
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x03); /* odd/even */
    outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03); /* end reset */

    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x00); /* read plane 0 */
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x10); /* odd/even */
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x0E); /* map at 0xB8000 */
}

static void save_font(void) {
    expose_plane2();
    volatile uint8_t *font_mem = (volatile uint8_t *)0xA0000;
    for (int i = 0; i < 256 * 32; i++)
        saved_font[i] = font_mem[i];
    restore_text_access();
}

static void restore_font(void) {
    expose_plane2();
    volatile uint8_t *font_mem = (volatile uint8_t *)0xA0000;
    for (int i = 0; i < 256 * 32; i++)
        font_mem[i] = saved_font[i];
    restore_text_access();
}

static void set_mode_03h(void) {
    write_regs(mode03h_misc,
               mode03h_seq,  sizeof(mode03h_seq),
               mode03h_crtc, sizeof(mode03h_crtc),
               mode03h_gc,   sizeof(mode03h_gc),
               mode03h_ac,   sizeof(mode03h_ac));
}

/* ---- Public API ---- */

void gfx_set_mode(int mode) {
    if (mode == 1 && current_mode != 1) {
        /* Save text screen and font */
        for (int i = 0; i < VGA_TEXT_SIZE; i++)
            saved_text[i] = VGA_TEXT_BUF[i];
        save_font();
        set_mode_13h();
        current_mode = 1;
        draw_color = 15; /* white */
        cursor_x = 0;
        cursor_y = 0;
        gfx_clear();
    } else if (mode == 0 && current_mode != 0) {
        set_mode_03h();
        restore_font();
        /* Restore text screen */
        for (int i = 0; i < VGA_TEXT_SIZE; i++)
            VGA_TEXT_BUF[i] = saved_text[i];
        current_mode = 0;
    }
}

int gfx_get_mode(void) {
    return current_mode;
}

void gfx_clear(void) {
    if (current_mode == 1)
        memset((void *)0xA0000, 0, GFX_WIDTH * GFX_HEIGHT);
}

void gfx_set_color(int color) {
    draw_color = color & 0xFF;
}

void gfx_pixel(int x, int y, int color) {
    if (x >= 0 && x < GFX_WIDTH && y >= 0 && y < GFX_HEIGHT)
        VGA_FRAMEBUFFER[y * GFX_WIDTH + x] = (uint8_t)color;
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

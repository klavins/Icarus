#include "vga.h"
#include "klib.h"
#include "io.h"
#include <stdarg.h>

#define VGA_MEMORY      ((volatile uint16_t *)0xB8000)
#define VGA_CRTC_INDEX  0x3D4
#define VGA_CRTC_DATA   0x3D5
#define CRTC_CURSOR_LO  0x0F
#define CRTC_CURSOR_HI  0x0E

static size_t cursor_row;
static size_t cursor_col;
static uint8_t terminal_color;

static uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

static void vga_update_cursor(void) {
    uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;
    outb(VGA_CRTC_INDEX, CRTC_CURSOR_LO);
    outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
    outb(VGA_CRTC_INDEX, CRTC_CURSOR_HI);
    outb(VGA_CRTC_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

static void terminal_scroll(void) {
    /* Move every row up by one */
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
    /* Clear the last row */
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
}

void terminal_init(void) {
    /* Text mode — nothing to do, VGA is already in mode 03h */
}

void terminal_setcolor(enum vga_color fg, enum vga_color bg) {
    terminal_color = fg | bg << 4;
}

void terminal_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    cursor_row = 0;
    cursor_col = 0;
    vga_update_cursor();
}

void terminal_putchar(char c) {
    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', terminal_color);
            vga_update_cursor();
        }
        return;
    }
    if (c == '\n') {
        cursor_col = 0;
        if (++cursor_row == VGA_HEIGHT) {
            terminal_scroll();
            cursor_row = VGA_HEIGHT - 1;
        }
        vga_update_cursor();
        return;
    }
    VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, terminal_color);
    if (++cursor_col == VGA_WIDTH) {
        cursor_col = 0;
        if (++cursor_row == VGA_HEIGHT) {
            terminal_scroll();
            cursor_row = VGA_HEIGHT - 1;
        }
    }
    vga_update_cursor();
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
    /* VGA text mode — no framebuffer to share */
    *addr   = 0;
    *width  = 0;
    *height = 0;
    *pitch  = 0;
    *bpp    = 0;
}

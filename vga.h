#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

enum vga_color {
    VGA_BLACK,   VGA_BLUE,      VGA_GREEN,  VGA_CYAN,
    VGA_RED,     VGA_MAGENTA,   VGA_BROWN,  VGA_LGRAY,
    VGA_DGRAY,   VGA_LBLUE,     VGA_LGREEN, VGA_LCYAN,
    VGA_LRED,    VGA_LMAGENTA,  VGA_YELLOW, VGA_WHITE,
};

void terminal_setcolor(enum vga_color fg, enum vga_color bg);
void terminal_clear(void);
void terminal_putchar(char c);
void terminal_print(const char *s);
void terminal_printf(const char *fmt, ...);
size_t terminal_getcol(void);

#endif

/*
 * vga.h - Terminal and VGA display API declarations
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

void terminal_init(void);
void terminal_setcolor(enum vga_color fg, enum vga_color bg);
void terminal_getcolor(int *fg, int *bg);
void terminal_clear(void);
void terminal_putchar(char c);
void terminal_print(const char *s);
void terminal_printf(const char *fmt, ...);
size_t terminal_getcol(void);
size_t terminal_getrow(void);
size_t terminal_getcols(void);
size_t terminal_getrows(void);
void   terminal_setcursor(size_t row, size_t col);
void   terminal_clear_to_eol(void);
void   terminal_show_cursor(int show);
void   terminal_flush_lock(void);
void   terminal_flush_unlock(void);

/* Framebuffer access for graphics module */
void    terminal_get_fb(uint8_t **addr, uint32_t *width, uint32_t *height,
                        uint32_t *pitch, uint32_t *bpp);

/* Switch to a new framebuffer (e.g. after BGA init). Redraws from shadow. */
void    terminal_set_fb(uint8_t *addr, uint32_t width, uint32_t height,
                        uint32_t pitch, uint32_t bpp);

/* Set text scale factor (1 = 8x16, 2 = 16x32). */
void    terminal_set_scale(int scale);

/* Restore shadow buffer contents (e.g. when returning from graphics mode) */
void    terminal_restore_shadow(const uint8_t *data, uint32_t size);

#endif

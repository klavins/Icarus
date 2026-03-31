/*
 * os.h - ICARUS OS public service API declarations
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

#ifndef OS_H
#define OS_H

/*
 * ICARUS OS API
 *
 * This is the public interface for applications and interpreters.
 * Include only this header to access all OS services.
 */

#include <stdint.h>
#include <stddef.h>

#define OS_VERSION 11

/* ---- Display (text mode) ---- */

void os_print(const char *s);
void os_putchar(char c);
void os_printf(const char *fmt, ...);
void os_set_color(int fg, int bg);
void os_get_color(int *fg, int *bg);
void os_clear_screen(void);
size_t os_cursor_col(void);
size_t os_cursor_row(void);
size_t os_screen_cols(void);
size_t os_screen_rows(void);
void os_set_cursor(int row, int col);
void os_clear_to_eol(void);
void os_show_cursor(int show);
void os_write(const char *buf, int len);

/* ---- Input ---- */

char os_read_key(void);           /* blocking — waits for keypress */
int  os_read_key_ext(void);      /* blocking — returns ASCII or special key code */
int  os_key_state(int scancode);  /* non-blocking — 1 if held, 0 if not */
int  os_last_key_ascii(void);     /* last key pressed (ASCII), 0 if none */
void os_flush_keys(void);

/* Special key codes returned by os_read_key_ext */
#define OS_KEY_ARROW_UP    128
#define OS_KEY_ARROW_DOWN  129
#define OS_KEY_ARROW_LEFT  130
#define OS_KEY_ARROW_RIGHT 131
#define OS_KEY_HOME        132
#define OS_KEY_END         133
#define OS_KEY_PAGE_UP     134
#define OS_KEY_PAGE_DOWN   135
#define OS_KEY_DELETE      136
#define OS_KEY_INSERT      137

/* ---- Sound ---- */

void os_tone(int freq);
void os_tone_off(void);

/* ---- Disk ---- */

int  os_format_disk(void);
int  os_list_files(void);
int  os_save_file(const char *name, const void *data, uint32_t size);
int  os_load_file(const char *name, void *data, uint32_t max_size);
int  os_delete_file(const char *name);
int  os_file_exists(const char *name);

/* ---- Memory ---- */

void  os_heap_init(void);
void *os_alloc(size_t size);
void  os_alloc_reset(void);
void  os_alloc_set_watermark(void);
size_t os_heap_free(void);

/* ---- Timer ---- */

void os_delay_ms(int ms);
uint32_t os_ticks(void);

/* ---- System ---- */

void os_shutdown(void);

/* Color constants */
#define OS_BLACK        0
#define OS_BLUE         1
#define OS_GREEN        2
#define OS_CYAN         3
#define OS_RED          4
#define OS_MAGENTA      5
#define OS_BROWN        6
#define OS_LGRAY        7
#define OS_DGRAY        8
#define OS_LBLUE        9
#define OS_LGREEN      10
#define OS_LCYAN       11
#define OS_LRED        12
#define OS_LMAGENTA    13
#define OS_YELLOW      14
#define OS_WHITE       15

#endif

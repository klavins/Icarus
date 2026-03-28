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

/* ---- Display (text mode) ---- */

void os_print(const char *s);
void os_putchar(char c);
void os_printf(const char *fmt, ...);
void os_set_color(int fg, int bg);
void os_get_color(int *fg, int *bg);
void os_clear_screen(void);
size_t os_cursor_col(void);

/* ---- Input ---- */

char os_read_key(void);           /* blocking — waits for keypress */
int  os_key_state(int scancode);  /* non-blocking — 1 if held, 0 if not */
int  os_last_key_ascii(void);     /* last key pressed (ASCII), 0 if none */
void os_flush_keys(void);

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

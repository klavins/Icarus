/*
 * icarus.h - Convenience header for ICARUS user programs
 *
 * Include this instead of os.h. It provides the same function names
 * via macros that call through the API lookup table.
 *
 * Usage:
 *   #include "icarus.h"
 *
 *   int main(icarus_lookup_fn api) {
 *       icarus_init(api);
 *       os_print("Hello!\n");
 *       return 0;
 *   }
 */

#ifndef ICARUS_H
#define ICARUS_H

#include "icarus_api.h"

/* Global lookup function, defined in crt0.c, set by _start */
extern icarus_lookup_fn _icarus_api;

/* ---- Display ---- */
#define os_print(s)             ((void (*)(const char *))_icarus_api(API_PRINT))(s)
#define os_putchar(c)           ((void (*)(char))_icarus_api(API_PUTCHAR))(c)
#define os_set_color(fg, bg)    ((void (*)(int, int))_icarus_api(API_SET_COLOR))(fg, bg)
#define os_get_color(fg, bg)    ((void (*)(int*, int*))_icarus_api(API_GET_COLOR))(fg, bg)
#define os_clear_screen()       ((void (*)(void))_icarus_api(API_CLEAR_SCREEN))()
#define os_write(b, n)          ((void (*)(const char*, int))_icarus_api(API_WRITE))(b, n)
#define os_set_cursor(r, c)     ((void (*)(int, int))_icarus_api(API_SET_CURSOR))(r, c)
#define os_screen_cols()        ((size_t (*)(void))_icarus_api(API_SCREEN_COLS))()
#define os_screen_rows()        ((size_t (*)(void))_icarus_api(API_SCREEN_ROWS))()
#define os_clear_to_eol()       ((void (*)(void))_icarus_api(API_CLEAR_TO_EOL))()
#define os_show_cursor(s)       ((void (*)(int))_icarus_api(API_SHOW_CURSOR))(s)

/* ---- Input ---- */
#define os_read_key()           ((char (*)(void))_icarus_api(API_READ_KEY))()
#define os_read_key_ext()       ((int (*)(void))_icarus_api(API_READ_KEY_EXT))()
#define os_key_state(sc)        ((int (*)(int))_icarus_api(API_KEY_STATE))(sc)
#define os_flush_keys()         ((void (*)(void))_icarus_api(API_FLUSH_KEYS))()

/* ---- Graphics ---- */
#define gfx_set_mode(m)         ((void (*)(int))_icarus_api(API_GFX_SET_MODE))(m)
#define gfx_get_mode()          ((int (*)(void))_icarus_api(API_GFX_GET_MODE))()
#define gfx_width()             ((int (*)(void))_icarus_api(API_GFX_WIDTH))()
#define gfx_height()            ((int (*)(void))_icarus_api(API_GFX_HEIGHT))()
#define gfx_clear()             ((void (*)(void))_icarus_api(API_GFX_CLEAR))()
#define gfx_set_color(c)        ((void (*)(int))_icarus_api(API_GFX_SET_COLOR))(c)
#define gfx_plot(x, y)          ((void (*)(int, int))_icarus_api(API_GFX_PLOT))(x, y)
#define gfx_drawto(x, y)        ((void (*)(int, int))_icarus_api(API_GFX_DRAWTO))(x, y)
#define gfx_fillto(x, y)        ((void (*)(int, int))_icarus_api(API_GFX_FILLTO))(x, y)
#define gfx_pixel(x, y, c)      ((void (*)(int, int, int))_icarus_api(API_GFX_PIXEL))(x, y, c)
#define gfx_pos(x, y)           ((void (*)(int, int))_icarus_api(API_GFX_POS))(x, y)
#define gfx_text(s)             ((void (*)(const char*))_icarus_api(API_GFX_TEXT))(s)
#define gfx_present()           ((void (*)(void))_icarus_api(API_GFX_PRESENT))()

/* ---- Memory ---- */
#define malloc(s)               ((void *(*)(size_t))_icarus_api(API_MALLOC))(s)
#define realloc(p, s)           ((void *(*)(void*, size_t))_icarus_api(API_REALLOC))(p, s)
#define free(p)                 ((void (*)(void*))_icarus_api(API_FREE))(p)
#define calloc(n, s)            ((void *(*)(size_t, size_t))_icarus_api(API_CALLOC))(n, s)
#define os_heap_free()          ((size_t (*)(void))_icarus_api(API_HEAP_FREE))()

/* ---- String/memory ---- */
#define strlen(s)               ((size_t (*)(const char*))_icarus_api(API_STRLEN))(s)
#define strcmp(a, b)            ((int (*)(const char*, const char*))_icarus_api(API_STRCMP))(a, b)
#define strncmp(a, b, n)        ((int (*)(const char*, const char*, size_t))_icarus_api(API_STRNCMP))(a, b, n)
#define strcpy(d, s)            ((char *(*)(char*, const char*))_icarus_api(API_STRCPY))(d, s)
#define strncpy(d, s, n)        ((char *(*)(char*, const char*, size_t))_icarus_api(API_STRNCPY))(d, s, n)
#define strdup(s)               ((char *(*)(const char*))_icarus_api(API_STRDUP))(s)
#define strchr(s, c)            ((char *(*)(const char*, int))_icarus_api(API_STRCHR))(s, c)
#define strstr(h, n)            ((char *(*)(const char*, const char*))_icarus_api(API_STRSTR))(h, n)
#define memset(d, v, n)         ((void *(*)(void*, int, size_t))_icarus_api(API_MEMSET))(d, v, n)
#define memcpy(d, s, n)         ((void *(*)(void*, const void*, size_t))_icarus_api(API_MEMCPY))(d, s, n)
#define memmove(d, s, n)        ((void *(*)(void*, const void*, size_t))_icarus_api(API_MEMMOVE))(d, s, n)
#define memcmp(a, b, n)         ((int (*)(const void*, const void*, size_t))_icarus_api(API_MEMCMP))(a, b, n)

/* ---- Sound ---- */
#define os_tone(f)              ((void (*)(int))_icarus_api(API_TONE))(f)
#define os_tone_off()           ((void (*)(void))_icarus_api(API_TONE_OFF))()

/* ---- Disk ---- */
#define os_save_file(n, d, s)   ((int (*)(const char*, const void*, uint32_t))_icarus_api(API_SAVE_FILE))(n, d, s)
#define os_load_file(n, d, m)   ((int (*)(const char*, void*, uint32_t))_icarus_api(API_LOAD_FILE))(n, d, m)
#define os_delete_file(n)       ((int (*)(const char*))_icarus_api(API_DELETE_FILE))(n)
#define os_file_exists(n)       ((int (*)(const char*))_icarus_api(API_FILE_EXISTS))(n)
#define os_list_files()         ((int (*)(void))_icarus_api(API_LIST_FILES))()

/* ---- Timer ---- */
#define os_delay_ms(ms)         ((void (*)(int))_icarus_api(API_DELAY_MS))(ms)
#define os_ticks()              ((uint32_t (*)(void))_icarus_api(API_TICKS))()

/* ---- System ---- */
#define os_shutdown()           ((void (*)(void))_icarus_api(API_SHUTDOWN))()

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

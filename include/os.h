/*
 * os.h - Display, input, sound, disk, timer, and system functions
 */
#ifndef ICARUS_USER_OS_H
#define ICARUS_USER_OS_H

#include "base.h"

/* Display */
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

/* Input */
#define os_read_key()           ((char (*)(void))_icarus_api(API_READ_KEY))()
#define os_read_key_ext()       ((int (*)(void))_icarus_api(API_READ_KEY_EXT))()
#define os_key_state(sc)        ((int (*)(int))_icarus_api(API_KEY_STATE))(sc)
#define os_flush_keys()         ((void (*)(void))_icarus_api(API_FLUSH_KEYS))()

/* Sound */
#define os_tone(f)              ((void (*)(int))_icarus_api(API_TONE))(f)
#define os_tone_off()           ((void (*)(void))_icarus_api(API_TONE_OFF))()

/* Disk */
#define os_save_file(n, d, s)   ((int (*)(const char*, const void*, uint32_t))_icarus_api(API_SAVE_FILE))(n, d, s)
#define os_load_file(n, d, m)   ((int (*)(const char*, void*, uint32_t))_icarus_api(API_LOAD_FILE))(n, d, m)
#define os_delete_file(n)       ((int (*)(const char*))_icarus_api(API_DELETE_FILE))(n)
#define os_file_exists(n)       ((int (*)(const char*))_icarus_api(API_FILE_EXISTS))(n)
#define os_list_files()         ((int (*)(void))_icarus_api(API_LIST_FILES))()

/* Timer */
#define os_delay_ms(ms)         ((void (*)(int))_icarus_api(API_DELAY_MS))(ms)
#define os_ticks()              ((uint32_t (*)(void))_icarus_api(API_TICKS))()

/* System */
#define os_shutdown()           ((void (*)(void))_icarus_api(API_SHUTDOWN))()
#define os_heap_free()          ((size_t (*)(void))_icarus_api(API_HEAP_FREE))()

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

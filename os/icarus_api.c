/*
 * icarus_api.c - API lookup table and program loader
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

#include "icarus_api.h"
#include "os.h"
#include "graphics.h"
#include "klib.h"
#include "malloc.h"

/* Version returned by API_VERSION */
static int api_version(void) { return OS_VERSION; }

/* Lookup function — maps API IDs to function pointers */
void *icarus_lookup(int id) {
    switch (id) {
    /* Display */
    case API_PRINT:         return (void *)os_print;
    case API_PUTCHAR:       return (void *)os_putchar;
    case API_PRINTF:        return (void *)os_printf;
    case API_SET_COLOR:     return (void *)os_set_color;
    case API_GET_COLOR:     return (void *)os_get_color;
    case API_CLEAR_SCREEN:  return (void *)os_clear_screen;
    case API_WRITE:         return (void *)os_write;
    case API_SET_CURSOR:    return (void *)os_set_cursor;
    case API_SCREEN_COLS:   return (void *)os_screen_cols;
    case API_SCREEN_ROWS:   return (void *)os_screen_rows;
    case API_CURSOR_COL:    return (void *)os_cursor_col;
    case API_CURSOR_ROW:    return (void *)os_cursor_row;
    case API_CLEAR_TO_EOL:  return (void *)os_clear_to_eol;
    case API_SHOW_CURSOR:   return (void *)os_show_cursor;

    /* Input */
    case API_READ_KEY:      return (void *)os_read_key;
    case API_READ_KEY_EXT:  return (void *)os_read_key_ext;
    case API_KEY_STATE:     return (void *)os_key_state;
    case API_FLUSH_KEYS:    return (void *)os_flush_keys;

    /* Graphics */
    case API_GFX_SET_MODE:  return (void *)gfx_set_mode;
    case API_GFX_GET_MODE:  return (void *)gfx_get_mode;
    case API_GFX_WIDTH:     return (void *)gfx_width;
    case API_GFX_HEIGHT:    return (void *)gfx_height;
    case API_GFX_CLEAR:     return (void *)gfx_clear;
    case API_GFX_SET_COLOR: return (void *)gfx_set_color;
    case API_GFX_PLOT:      return (void *)gfx_plot;
    case API_GFX_DRAWTO:    return (void *)gfx_drawto;
    case API_GFX_FILLTO:    return (void *)gfx_fillto;
    case API_GFX_PIXEL:     return (void *)gfx_pixel;
    case API_GFX_POS:       return (void *)gfx_pos;
    case API_GFX_TEXT:      return (void *)gfx_text;
    case API_GFX_PRESENT:   return (void *)gfx_present;

    /* Memory */
    case API_MALLOC:        return (void *)malloc;
    case API_REALLOC:       return (void *)realloc;
    case API_FREE:          return (void *)free;
    case API_CALLOC:        return (void *)calloc;
    case API_HEAP_FREE:     return (void *)os_heap_free;

    /* String/memory */
    case API_STRLEN:        return (void *)strlen;
    case API_STRCMP:         return (void *)strcmp;
    case API_STRNCMP:       return (void *)strncmp;
    case API_STRCPY:        return (void *)strcpy;
    case API_STRNCPY:       return (void *)strncpy;
    case API_STRDUP:        return (void *)strdup;
    case API_STRCHR:        return (void *)strchr;
    case API_STRSTR:        return (void *)strstr;
    case API_MEMSET:        return (void *)memset;
    case API_MEMCPY:        return (void *)memcpy;
    case API_MEMMOVE:       return (void *)memmove;
    case API_MEMCMP:        return (void *)memcmp;
    case API_SNPRINTF:      return (void *)snprintf;

    /* Sound */
    case API_TONE:          return (void *)os_tone;
    case API_TONE_OFF:      return (void *)os_tone_off;

    /* Disk */
    case API_SAVE_FILE:     return (void *)os_save_file;
    case API_LOAD_FILE:     return (void *)os_load_file;
    case API_DELETE_FILE:   return (void *)os_delete_file;
    case API_FILE_EXISTS:   return (void *)os_file_exists;
    case API_LIST_FILES:    return (void *)os_list_files;

    /* Timer */
    case API_DELAY_MS:      return (void *)os_delay_ms;
    case API_TICKS:         return (void *)os_ticks;

    /* System */
    case API_SHUTDOWN:      return (void *)os_shutdown;
    case API_VERSION:       return (void *)api_version;

    default:                return (void *)0;
    }
}

/* Load and execute a C program from disk */
#define EXEC_LOAD_ADDR 0x800000

int exec_program(const char *filename) {
    int size = os_load_file(filename, (void *)EXEC_LOAD_ADDR, 32768);
    if (size <= 0) {
        os_print("?FILE NOT FOUND\n");
        return -1;
    }

    icarus_entry_fn entry = (icarus_entry_fn)EXEC_LOAD_ADDR;
    return entry(icarus_lookup);
}

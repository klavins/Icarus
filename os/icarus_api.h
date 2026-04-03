/*
 * icarus_api.h - ICARUS OS API for user programs
 *
 * User programs call os_lookup(ID) to get function pointers.
 * IDs are stable — new functions get new numbers, old numbers
 * never change.
 *
 * Copyright (C) 2026 Eric Klavins
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef ICARUS_API_H
#define ICARUS_API_H

#include <stdint.h>
#include <stddef.h>

/* API function IDs — never reorder or reuse, only append */

/* Display */
#define API_PRINT           1
#define API_PUTCHAR         2
#define API_PRINTF          3
#define API_SET_COLOR       4
#define API_GET_COLOR       5
#define API_CLEAR_SCREEN    6
#define API_WRITE           7
#define API_SET_CURSOR      8
#define API_SCREEN_COLS     9
#define API_SCREEN_ROWS     10
#define API_CURSOR_COL      11
#define API_CURSOR_ROW      12
#define API_CLEAR_TO_EOL    13
#define API_SHOW_CURSOR     14

/* Input */
#define API_READ_KEY        20
#define API_READ_KEY_EXT    21
#define API_KEY_STATE       22
#define API_FLUSH_KEYS      23

/* Graphics */
#define API_GFX_SET_MODE    30
#define API_GFX_GET_MODE    31
#define API_GFX_WIDTH       32
#define API_GFX_HEIGHT      33
#define API_GFX_CLEAR       34
#define API_GFX_SET_COLOR   35
#define API_GFX_PLOT        36
#define API_GFX_DRAWTO      37
#define API_GFX_FILLTO      38
#define API_GFX_PIXEL       39
#define API_GFX_POS         40
#define API_GFX_TEXT        41
#define API_GFX_PRESENT     42

/* Memory */
#define API_MALLOC          50
#define API_REALLOC         51
#define API_FREE            52
#define API_CALLOC          53
#define API_HEAP_FREE       54

/* String/memory */
#define API_STRLEN          60
#define API_STRCMP           61
#define API_STRNCMP         62
#define API_STRCPY          63
#define API_STRNCPY         64
#define API_STRDUP          65
#define API_STRCHR          66
#define API_STRSTR          67
#define API_MEMSET          68
#define API_MEMCPY          69
#define API_MEMMOVE         70
#define API_MEMCMP          71
#define API_SNPRINTF        72

/* Sound */
#define API_TONE            80
#define API_TONE_OFF        81

/* Disk */
#define API_SAVE_FILE       90
#define API_LOAD_FILE       91
#define API_DELETE_FILE     92
#define API_FILE_EXISTS     93
#define API_LIST_FILES      94

/* Timer */
#define API_DELAY_MS        100
#define API_TICKS           101

/* Math */
#define API_SIN             105
#define API_COS             106
#define API_SQRT            107

/* System */
#define API_SHUTDOWN        110
#define API_VERSION         111

/*
 * User programs receive a pointer to the lookup function as their
 * first argument. Call it once per function at startup:
 *
 *   typedef void (*print_fn)(const char *);
 *   print_fn my_print = (print_fn)api(API_PRINT);
 *   my_print("Hello!\n");
 */
typedef void *(*icarus_lookup_fn)(int id);

/* Entry point called by the kernel — receives API, argc, argv */
typedef int (*icarus_entry_fn)(icarus_lookup_fn api, int argc, char **argv);

#endif

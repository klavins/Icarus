/*
 * fs.h - Filesystem structures and API declarations
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

#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_MAGIC       0x49434152  /* "ICAR" */
#define FS_MAX_FILES   32
#define FS_NAME_LEN    12
#define FS_DATA_START  10          /* first data sector */

struct __attribute__((packed)) fs_header {
    uint32_t magic;
    uint32_t file_count;
    uint8_t  reserved[504];
};

struct __attribute__((packed)) fs_entry {
    char     name[FS_NAME_LEN];
    uint32_t start_sector;
    uint32_t size_bytes;
    uint8_t  reserved[512 - FS_NAME_LEN - 8];
};

void fs_init(void);
int  fs_format(void);
int  fs_list(void);
int  fs_save(const char *name, const void *data, uint32_t size);
int  fs_load(const char *name, void *data, uint32_t max_size);
int  fs_delete(const char *name);
int  fs_exists(const char *name);

#endif

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

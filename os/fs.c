#include "fs.h"
#include "ata.h"
#include "klib.h"
#include "vga.h"

static uint8_t sector_buf[SECTOR_SIZE];
static uint32_t total_sectors;
static uint32_t find_free_sector(struct fs_header *hdr);

void fs_init(void) {
    total_sectors = ata_get_total_sectors();
}

static int read_header(struct fs_header *hdr) {
    if (ata_read_sector(0, hdr) < 0) return -1;
    if (hdr->magic != FS_MAGIC) return -1;
    return 0;
}

static int write_header(struct fs_header *hdr) {
    return ata_write_sector(0, hdr);
}

static int read_entry(int idx, struct fs_entry *ent) {
    /* Each directory entry is one sector, starting at sector 1 */
    return ata_read_sector(1 + idx, ent);
}

static int write_entry(int idx, struct fs_entry *ent) {
    return ata_write_sector(1 + idx, ent);
}

int fs_format(void) {
    struct fs_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = FS_MAGIC;
    hdr.file_count = 0;
    if (write_header(&hdr) < 0) return -1;

    /* Clear directory entries */
    struct fs_entry ent;
    memset(&ent, 0, sizeof(ent));
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (write_entry(i, &ent) < 0) return -1;
    }

    return 0;
}

int fs_list(void) {
    struct fs_header hdr;
    if (read_header(&hdr) < 0) {
        terminal_print("?DISK NOT FORMATTED\n");
        return -1;
    }

    if (hdr.file_count == 0) {
        terminal_print(" (no files)\n");
    }

    struct fs_entry ent;
    for (uint32_t i = 0; i < hdr.file_count; i++) {
        if (read_entry(i, &ent) < 0) return -1;
        terminal_printf(" %s", ent.name);
        /* Pad to 12 chars */
        for (int pad = strlen(ent.name); pad < 12; pad++)
            terminal_putchar(' ');
        terminal_printf(" %d bytes\n", ent.size_bytes);
    }

    /* Show free space */
    uint32_t next_free = find_free_sector(&hdr);
    uint32_t free_sectors = (total_sectors > next_free) ? total_sectors - next_free : 0;
    uint32_t free_mb = free_sectors / 2048;  /* sectors / (1048576/512) */
    if (free_mb >= 1024)
        terminal_printf("\n %d files, %d GB free\n",
                        hdr.file_count, free_mb / 1024);
    else
        terminal_printf("\n %d files, %d MB free\n",
                        hdr.file_count, free_mb);

    return 0;
}

/* Find the next free data sector after all existing files */
static uint32_t find_free_sector(struct fs_header *hdr) {
    uint32_t next = FS_DATA_START;
    struct fs_entry ent;
    for (uint32_t i = 0; i < hdr->file_count; i++) {
        if (read_entry(i, &ent) < 0) continue;
        uint32_t end = ent.start_sector + (ent.size_bytes + SECTOR_SIZE - 1) / SECTOR_SIZE;
        if (end > next)
            next = end;
    }
    return next;
}

int fs_save(const char *name, const void *data, uint32_t size) {
    struct fs_header hdr;
    if (read_header(&hdr) < 0) {
        terminal_print("?DISK NOT FORMATTED\n");
        return -1;
    }

    /* Check if file already exists — overwrite by deleting first */
    fs_delete(name);

    /* Re-read header after possible delete */
    if (read_header(&hdr) < 0) return -1;

    if (hdr.file_count >= FS_MAX_FILES) {
        terminal_print("?DIRECTORY FULL\n");
        return -1;
    }

    uint32_t start = find_free_sector(&hdr);
    uint32_t sectors_needed = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;

    /* Write data sectors */
    const uint8_t *p = (const uint8_t *)data;
    for (uint32_t s = 0; s < sectors_needed; s++) {
        memset(sector_buf, 0, SECTOR_SIZE);
        uint32_t chunk = size - s * SECTOR_SIZE;
        if (chunk > SECTOR_SIZE) chunk = SECTOR_SIZE;
        memcpy(sector_buf, p + s * SECTOR_SIZE, chunk);
        if (ata_write_sector(start + s, sector_buf) < 0) return -1;
    }

    /* Write directory entry */
    struct fs_entry ent;
    memset(&ent, 0, sizeof(ent));
    strncpy(ent.name, name, FS_NAME_LEN - 1);
    ent.name[FS_NAME_LEN - 1] = '\0';
    ent.start_sector = start;
    ent.size_bytes = size;
    if (write_entry(hdr.file_count, &ent) < 0) return -1;

    /* Update header */
    hdr.file_count++;
    return write_header(&hdr);
}

int fs_load(const char *name, void *data, uint32_t max_size) {
    struct fs_header hdr;
    if (read_header(&hdr) < 0) {
        terminal_print("?DISK NOT FORMATTED\n");
        return -1;
    }

    struct fs_entry ent;
    for (uint32_t i = 0; i < hdr.file_count; i++) {
        if (read_entry(i, &ent) < 0) return -1;
        if (strcmp(ent.name, name) == 0) {
            uint32_t size = ent.size_bytes;
            if (size > max_size) size = max_size;
            uint32_t sectors = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
            uint8_t *p = (uint8_t *)data;
            for (uint32_t s = 0; s < sectors; s++) {
                if (ata_read_sector(ent.start_sector + s, sector_buf) < 0)
                    return -1;
                uint32_t chunk = size - s * SECTOR_SIZE;
                if (chunk > SECTOR_SIZE) chunk = SECTOR_SIZE;
                memcpy(p + s * SECTOR_SIZE, sector_buf, chunk);
            }
            return (int)size;
        }
    }

    terminal_print("?FILE NOT FOUND\n");
    return -1;
}

int fs_delete(const char *name) {
    struct fs_header hdr;
    if (read_header(&hdr) < 0) return -1;

    struct fs_entry ent;
    for (uint32_t i = 0; i < hdr.file_count; i++) {
        if (read_entry(i, &ent) < 0) return -1;
        if (strcmp(ent.name, name) == 0) {
            /* Shift remaining entries down */
            for (uint32_t j = i; j < hdr.file_count - 1; j++) {
                struct fs_entry next;
                if (read_entry(j + 1, &next) < 0) return -1;
                if (write_entry(j, &next) < 0) return -1;
            }
            hdr.file_count--;
            return write_header(&hdr);
        }
    }

    return 0; /* not found is OK */
}

int fs_exists(const char *name) {
    struct fs_header hdr;
    if (read_header(&hdr) < 0) return 0;

    struct fs_entry ent;
    for (uint32_t i = 0; i < hdr.file_count; i++) {
        if (read_entry(i, &ent) < 0) return 0;
        if (strcmp(ent.name, name) == 0)
            return 1;
    }
    return 0;
}

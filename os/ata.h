#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define SECTOR_SIZE 512

void     ata_init(void);
int      ata_read_sector(uint32_t lba, void *buf);
int      ata_write_sector(uint32_t lba, const void *buf);
uint32_t ata_get_total_sectors(void);

#endif

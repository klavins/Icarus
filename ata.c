#include "ata.h"
#include "io.h"

#define ATA_DATA    0x1F0
#define ATA_ERROR   0x1F1
#define ATA_COUNT   0x1F2
#define ATA_LBA_LO  0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HI  0x1F5
#define ATA_DRIVE   0x1F6
#define ATA_CMD     0x1F7
#define ATA_STATUS  0x1F7

#define ATA_CMD_READ  0x20
#define ATA_CMD_WRITE 0x30

#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

#define ATA_TIMEOUT 1000000

static int ata_wait_ready(void) {
    for (int i = 0; i < ATA_TIMEOUT; i++) {
        if (!(inb(ATA_STATUS) & ATA_SR_BSY))
            return 0;
    }
    return -1;
}

static int ata_wait_drq(void) {
    for (int i = 0; i < ATA_TIMEOUT; i++) {
        uint8_t s = inb(ATA_STATUS);
        if (s & ATA_SR_ERR) return -1;
        if (s & ATA_SR_DRQ) return 0;
    }
    return -1;
}

int ata_read_sector(uint32_t lba, void *buf) {
    if (ata_wait_ready() < 0) return -1;

    outb(ATA_DRIVE,   0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_COUNT,   1);
    outb(ATA_LBA_LO,  (uint8_t)(lba));
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI,  (uint8_t)(lba >> 16));
    outb(ATA_CMD,     ATA_CMD_READ);

    if (ata_wait_drq() < 0)
        return -1;

    uint16_t *p = (uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        p[i] = inw(ATA_DATA);

    return 0;
}

int ata_write_sector(uint32_t lba, const void *buf) {
    if (ata_wait_ready() < 0) return -1;

    outb(ATA_DRIVE,   0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_COUNT,   1);
    outb(ATA_LBA_LO,  (uint8_t)(lba));
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI,  (uint8_t)(lba >> 16));
    outb(ATA_CMD,     ATA_CMD_WRITE);

    if (ata_wait_drq() < 0)
        return -1;

    const uint16_t *p = (const uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        outw(ATA_DATA, p[i]);

    /* Flush */
    if (ata_wait_ready() < 0) return -1;

    return 0;
}

uint32_t ata_get_total_sectors(void) {
    #define ATA_CMD_IDENTIFY 0xEC

    if (ata_wait_ready() < 0) return 0;

    outb(ATA_DRIVE, 0xA0);
    outb(ATA_CMD, ATA_CMD_IDENTIFY);

    if (inb(ATA_STATUS) == 0) return 0; /* no drive */
    if (ata_wait_drq() < 0) return 0;

    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = inw(ATA_DATA);

    /* Words 60-61: total LBA28 sectors */
    return (uint32_t)id[60] | ((uint32_t)id[61] << 16);
}

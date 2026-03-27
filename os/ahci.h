#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>

#define AHCI_MAX_PORTS 32

/* Initialize AHCI — find controller, detect drives */
int ahci_init(void);

/* Read/write a sector via AHCI */
int ahci_read_sector(int port, uint64_t lba, void *buf);
int ahci_write_sector(int port, uint64_t lba, const void *buf);

/* Get total sectors for a port (-1 if no drive) */
uint64_t ahci_get_total_sectors(int port);

/* Get number of detected ports with drives */
int ahci_port_count(void);

/* Get the first port with a drive, or -1 */
int ahci_first_port(void);

#endif

/*
 * ahci.h - AHCI driver API declarations
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

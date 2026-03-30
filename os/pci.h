/*
 * pci.h - PCI bus access API declarations
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

#ifndef PCI_H
#define PCI_H

#include <stdint.h>

/* PCI configuration space access */
uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);

/* Find a PCI device by class/subclass. Returns 1 if found, fills bus/slot/func. */
int pci_find_device(uint8_t class, uint8_t subclass,
                    uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func);

/* Find a PCI device by vendor/device ID. Returns 1 if found, fills bus/slot/func. */
int pci_find_vendor(uint16_t vendor, uint16_t device,
                    uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func);

/* Read a BAR (Base Address Register) */
uint32_t pci_read_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar);

/* Enable bus mastering and memory access for a device */
void pci_enable_device(uint8_t bus, uint8_t slot, uint8_t func);

/* Print all PCI devices found on the bus */
void pci_scan_print(void);

#endif

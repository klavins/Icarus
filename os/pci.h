#ifndef PCI_H
#define PCI_H

#include <stdint.h>

/* PCI configuration space access */
uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);

/* Find a PCI device by class/subclass. Returns 1 if found, fills bus/slot/func. */
int pci_find_device(uint8_t class, uint8_t subclass,
                    uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func);

/* Read a BAR (Base Address Register) */
uint32_t pci_read_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar);

/* Enable bus mastering and memory access for a device */
void pci_enable_device(uint8_t bus, uint8_t slot, uint8_t func);

/* Print all PCI devices found on the bus */
void pci_scan_print(void);

#endif

#include "pci.h"
#include "io.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static uint32_t pci_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (1 << 31)                /* enable bit */
         | ((uint32_t)bus << 16)
         | ((uint32_t)slot << 11)
         | ((uint32_t)func << 8)
         | (offset & 0xFC);
}

uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDR, pci_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

void pci_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    outl(PCI_CONFIG_ADDR, pci_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

int pci_find_device(uint8_t class, uint8_t subclass,
                    uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint32_t id = pci_read(bus, slot, func, 0);
                if ((id & 0xFFFF) == 0xFFFF) continue; /* no device */

                uint32_t class_reg = pci_read(bus, slot, func, 0x08);
                uint8_t dev_class = (class_reg >> 24) & 0xFF;
                uint8_t dev_subclass = (class_reg >> 16) & 0xFF;

                if (dev_class == class && dev_subclass == subclass) {
                    *out_bus = bus;
                    *out_slot = slot;
                    *out_func = func;
                    return 1;
                }

                /* If not multi-function, skip remaining functions */
                if (func == 0) {
                    uint32_t header = pci_read(bus, slot, 0, 0x0C);
                    if (!((header >> 16) & 0x80))
                        break;
                }
            }
        }
    }
    return 0;
}

uint32_t pci_read_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar) {
    return pci_read(bus, slot, func, 0x10 + bar * 4);
}

void pci_enable_device(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t cmd = pci_read(bus, slot, func, 0x04);
    cmd |= (1 << 1) | (1 << 2); /* memory space + bus master */
    pci_write(bus, slot, func, 0x04, cmd);
}

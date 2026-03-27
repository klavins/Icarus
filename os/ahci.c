#include "ahci.h"
#include "pci.h"
#include "io.h"
#include "klib.h"
#include <stdint.h>

/* ---- AHCI register structures ---- */

/* HBA Memory Registers (at BAR5) */
typedef volatile struct {
    uint32_t cap;        /* Host Capabilities */
    uint32_t ghc;        /* Global Host Control */
    uint32_t is;         /* Interrupt Status */
    uint32_t pi;         /* Ports Implemented */
    uint32_t vs;         /* Version */
    uint32_t ccc_ctl;
    uint32_t ccc_ports;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  reserved[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
} HBA_MEM;

/* Port registers (one per port, starting at BAR5 + 0x100) */
typedef volatile struct {
    uint32_t clb;        /* Command List Base Address */
    uint32_t clbu;       /* Command List Base Address Upper */
    uint32_t fb;         /* FIS Base Address */
    uint32_t fbu;        /* FIS Base Address Upper */
    uint32_t is;         /* Interrupt Status */
    uint32_t ie;         /* Interrupt Enable */
    uint32_t cmd;        /* Command and Status */
    uint32_t reserved0;
    uint32_t tfd;        /* Task File Data */
    uint32_t sig;        /* Signature */
    uint32_t ssts;       /* SATA Status */
    uint32_t sctl;       /* SATA Control */
    uint32_t serr;       /* SATA Error */
    uint32_t sact;       /* SATA Active */
    uint32_t ci;         /* Command Issue */
    uint32_t sntf;
    uint32_t fbs;
    uint8_t  reserved1[0x70 - 0x44];
    uint8_t  vendor[0x80 - 0x70];
} HBA_PORT;

/* Command header (in command list) */
typedef struct {
    uint16_t flags;      /* CFL (command FIS length in dwords), ATAPI, Write, Prefetchable, etc. */
    uint16_t prdtl;      /* Physical Region Descriptor Table Length */
    uint32_t prdbc;      /* PRD Byte Count */
    uint32_t ctba;       /* Command Table Base Address */
    uint32_t ctbau;      /* Command Table Base Address Upper */
    uint32_t reserved[4];
} __attribute__((packed)) HBA_CMD_HEADER;

/* Physical Region Descriptor Table entry */
typedef struct {
    uint32_t dba;        /* Data Base Address */
    uint32_t dbau;       /* Data Base Address Upper */
    uint32_t reserved;
    uint32_t dbc;        /* Data Byte Count (bit 0 must be 1, max 4MB) */
} __attribute__((packed)) HBA_PRDT_ENTRY;

/* Command table */
typedef struct {
    uint8_t  cfis[64];   /* Command FIS */
    uint8_t  acmd[16];   /* ATAPI command */
    uint8_t  reserved[48];
    HBA_PRDT_ENTRY prdt[1]; /* at least one entry */
} __attribute__((packed)) HBA_CMD_TBL;

/* FIS Register H2D (host to device) */
typedef struct {
    uint8_t  fis_type;   /* 0x27 = Register H2D */
    uint8_t  flags;      /* bit 7 = command/control */
    uint8_t  command;
    uint8_t  feature_lo;
    uint8_t  lba0, lba1, lba2;
    uint8_t  device;
    uint8_t  lba3, lba4, lba5;
    uint8_t  feature_hi;
    uint16_t count;
    uint8_t  icc;
    uint8_t  control;
    uint32_t reserved;
} __attribute__((packed)) FIS_REG_H2D;

/* ---- AHCI constants ---- */

#define SATA_SIG_ATA    0x00000101
#define SATA_SIG_ATAPI  0xEB140101

#define HBA_PORT_DET_PRESENT 3
#define HBA_PORT_IPM_ACTIVE  1

#define HBA_GHC_AE      (1 << 31)  /* AHCI Enable */
#define HBA_PORT_CMD_ST  (1 << 0)   /* Start */
#define HBA_PORT_CMD_FRE (1 << 4)   /* FIS Receive Enable */
#define HBA_PORT_CMD_FR  (1 << 14)  /* FIS Receive Running */
#define HBA_PORT_CMD_CR  (1 << 15)  /* Command List Running */

#define ATA_CMD_READ_DMA_EX  0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY     0xEC

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ  0x08

/* ---- Static state ---- */

static HBA_MEM *hba;
static HBA_PORT *ports[AHCI_MAX_PORTS];
static int port_active[AHCI_MAX_PORTS];
static uint64_t port_sectors[AHCI_MAX_PORTS];
static int num_active_ports;

/* Per-port DMA buffers — aligned to 1024 bytes */
/* We allocate these from low memory to ensure 32-bit addressable */
#define CMD_LIST_SIZE  1024
#define FIS_RCV_SIZE   256
#define CMD_TBL_SIZE   256

/* Use a static buffer area for DMA */
static uint8_t dma_area[AHCI_MAX_PORTS * 2048] __attribute__((aligned(4096)));

static HBA_CMD_HEADER *get_cmd_list(int port) {
    return (HBA_CMD_HEADER *)(dma_area + port * 2048);
}

static uint8_t *get_fis_area(int port) {
    return dma_area + port * 2048 + CMD_LIST_SIZE;
}

static HBA_CMD_TBL *get_cmd_tbl(int port) {
    return (HBA_CMD_TBL *)(dma_area + port * 2048 + CMD_LIST_SIZE + FIS_RCV_SIZE);
}

/* Sector buffer for DMA — must be physically contiguous and 32-bit addressable */
static uint8_t sector_buf[512] __attribute__((aligned(512)));

/* ---- Helpers ---- */

static void port_stop(HBA_PORT *p) {
    p->cmd &= ~HBA_PORT_CMD_ST;
    p->cmd &= ~HBA_PORT_CMD_FRE;
    while (p->cmd & (HBA_PORT_CMD_FR | HBA_PORT_CMD_CR))
        ;
}

static void port_start(HBA_PORT *p) {
    while (p->cmd & HBA_PORT_CMD_CR)
        ;
    p->cmd |= HBA_PORT_CMD_FRE;
    p->cmd |= HBA_PORT_CMD_ST;
}

static int port_wait(HBA_PORT *p, int timeout) {
    while (timeout-- > 0) {
        if (!(p->ci & 1))
            return 0;
        for (volatile int i = 0; i < 1000; i++) ;
    }
    return -1; /* timeout */
}

static int port_has_drive(HBA_PORT *p) {
    uint32_t ssts = p->ssts;
    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    return (det == HBA_PORT_DET_PRESENT && ipm == HBA_PORT_IPM_ACTIVE);
}

/* ---- Port initialization ---- */

static void init_port(int idx) {
    HBA_PORT *p = ports[idx];

    port_stop(p);

    /* Set command list and FIS base addresses */
    HBA_CMD_HEADER *cmdlist = get_cmd_list(idx);
    uint8_t *fisarea = get_fis_area(idx);

    memset(cmdlist, 0, CMD_LIST_SIZE);
    memset(fisarea, 0, FIS_RCV_SIZE);

    uint64_t cl_addr = (uint64_t)(uintptr_t)cmdlist;
    uint64_t fb_addr = (uint64_t)(uintptr_t)fisarea;

    p->clb  = (uint32_t)cl_addr;
    p->clbu = (uint32_t)(cl_addr >> 32);
    p->fb   = (uint32_t)fb_addr;
    p->fbu  = (uint32_t)(fb_addr >> 32);

    /* Set up command header 0 to point to command table */
    HBA_CMD_TBL *cmdtbl = get_cmd_tbl(idx);
    memset(cmdtbl, 0, CMD_TBL_SIZE);

    uint64_t ct_addr = (uint64_t)(uintptr_t)cmdtbl;
    cmdlist[0].ctba  = (uint32_t)ct_addr;
    cmdlist[0].ctbau = (uint32_t)(ct_addr >> 32);

    /* Clear error register */
    p->serr = 0xFFFFFFFF;
    p->is   = 0xFFFFFFFF;

    port_start(p);
}

/* ---- Issue a command ---- */

static int issue_cmd(int idx, uint8_t command, uint64_t lba, int write) {
    HBA_PORT *p = ports[idx];
    HBA_CMD_HEADER *cmdlist = get_cmd_list(idx);
    HBA_CMD_TBL *cmdtbl = get_cmd_tbl(idx);

    /* Wait for port to be ready */
    int spin = 100000;
    while ((p->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin--)
        ;
    if (spin <= 0) return -1;

    /* Set up command header */
    cmdlist[0].flags = sizeof(FIS_REG_H2D) / 4; /* CFL = 5 dwords */
    if (write) cmdlist[0].flags |= (1 << 6);    /* Write bit */
    cmdlist[0].prdtl = 1;
    cmdlist[0].prdbc = 0;

    /* Set up PRDT */
    uint64_t buf_addr = (uint64_t)(uintptr_t)sector_buf;
    cmdtbl->prdt[0].dba  = (uint32_t)buf_addr;
    cmdtbl->prdt[0].dbau = (uint32_t)(buf_addr >> 32);
    cmdtbl->prdt[0].dbc  = 511; /* 512 bytes - 1 */

    /* Set up command FIS */
    FIS_REG_H2D *fis = (FIS_REG_H2D *)cmdtbl->cfis;
    memset(fis, 0, sizeof(FIS_REG_H2D));
    fis->fis_type = 0x27;  /* H2D */
    fis->flags    = 0x80;  /* Command */
    fis->command  = command;
    fis->device   = (1 << 6); /* LBA mode */
    fis->lba0 = (uint8_t)(lba);
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->count = 1;

    /* Issue command */
    p->ci = 1;

    /* Wait for completion */
    return port_wait(p, 500000);
}

/* ---- Public API ---- */

int ahci_init(void) {
    uint8_t bus, slot, func;

    /* Find AHCI controller: class 01 (mass storage), subclass 06 (SATA) */
    if (!pci_find_device(0x01, 0x06, &bus, &slot, &func))
        return -1;

    /* Enable PCI device */
    pci_enable_device(bus, slot, func);

    /* Read BAR5 — AHCI Base Memory Register */
    uint32_t bar5 = pci_read_bar(bus, slot, func, 5);
    bar5 &= ~0xF; /* mask lower bits */
    if (bar5 == 0) return -1;

    hba = (HBA_MEM *)(uintptr_t)bar5;

    /* Enable AHCI mode */
    hba->ghc |= HBA_GHC_AE;

    /* Scan ports */
    uint32_t pi = hba->pi;
    num_active_ports = 0;

    for (int i = 0; i < 32; i++) {
        if (!(pi & (1 << i))) continue;

        HBA_PORT *p = (HBA_PORT *)((uint8_t *)hba + 0x100 + i * 0x80);
        ports[i] = p;

        if (port_has_drive(p)) {
            init_port(i);

            /* Send IDENTIFY to get drive info */
            memset(sector_buf, 0, 512);
            if (issue_cmd(i, ATA_CMD_IDENTIFY, 0, 0) == 0) {
                uint16_t *id = (uint16_t *)sector_buf;
                port_sectors[i] = (uint64_t)id[60] | ((uint64_t)id[61] << 16);
                /* Check for 48-bit LBA */
                if (id[83] & (1 << 10)) {
                    port_sectors[i] = (uint64_t)id[100]
                                    | ((uint64_t)id[101] << 16)
                                    | ((uint64_t)id[102] << 32)
                                    | ((uint64_t)id[103] << 48);
                }
                port_active[i] = 1;
                num_active_ports++;
            }
        }
    }

    return num_active_ports > 0 ? 0 : -1;
}

int ahci_read_sector(int port, uint64_t lba, void *buf) {
    if (port < 0 || port >= 32 || !port_active[port]) return -1;
    memset(sector_buf, 0, 512);
    if (issue_cmd(port, ATA_CMD_READ_DMA_EX, lba, 0) < 0) return -1;
    memcpy(buf, sector_buf, 512);
    return 0;
}

int ahci_write_sector(int port, uint64_t lba, const void *buf) {
    if (port < 0 || port >= 32 || !port_active[port]) return -1;
    memcpy(sector_buf, buf, 512);
    if (issue_cmd(port, ATA_CMD_WRITE_DMA_EX, lba, 1) < 0) return -1;
    return 0;
}

uint64_t ahci_get_total_sectors(int port) {
    if (port < 0 || port >= 32 || !port_active[port]) return 0;
    return port_sectors[port];
}

int ahci_port_count(void) {
    return num_active_ports;
}

int ahci_first_port(void) {
    for (int i = 0; i < 32; i++)
        if (port_active[i]) return i;
    return -1;
}

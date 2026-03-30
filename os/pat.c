/*
 * pat.c - Page Attribute Table setup for write-combining memory
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

#include "pat.h"
#include "io.h"
#include "vga.h"

/* PAT MSR address */
#define PAT_MSR 0x277

/* Memory types */
#define PAT_WC 0x01

/* Page table entry bits */
#define PTE_PRESENT   (1ULL << 0)
#define PTE_PWT       (1ULL << 3)
#define PTE_PCD       (1ULL << 4)
#define PTE_PS        (1ULL << 7)   /* Large page (2MB in PD, 1GB in PDPT) */
#define PTE_PAT_4K    (1ULL << 7)   /* PAT bit for 4KB page table entries */
#define PTE_PAT_LARGE (1ULL << 12)  /* PAT bit for 2MB/1GB large pages */
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

static inline uint64_t read_cr3(void) {
    uint64_t val;
    asm volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr0(void) {
    uint64_t val;
    asm volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint64_t val) {
    asm volatile ("mov %0, %%cr0" : : "r"(val));
}

static inline void invlpg(uint64_t addr) {
    asm volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

#define CR0_WP (1ULL << 16)

void pat_init(void) {
    /*
     * Replace PAT slot 1 (Write-Through) with Write-Combining.
     * To mark a page as WC: set PWT=1, PCD=0, PAT=0 in its page table entry.
     */
    uint64_t pat = rdmsr(PAT_MSR);
    pat &= ~(0xFFULL << 8);         /* clear slot 1 */
    pat |= ((uint64_t)PAT_WC << 8); /* set slot 1 = WC */
    wrmsr(PAT_MSR, pat);
}

/*
 * Set PAT slot 1 (WC) bits on a page table entry.
 * Slot 1 = {PAT=0, PCD=0, PWT=1}
 */
static void set_wc_4k(uint64_t *entry) {
    *entry |= PTE_PWT;
    *entry &= ~PTE_PCD;
    *entry &= ~PTE_PAT_4K;
}

static void set_wc_large(uint64_t *entry) {
    *entry |= PTE_PWT;
    *entry &= ~PTE_PCD;
    *entry &= ~PTE_PAT_LARGE;
}

void pat_set_write_combining(uint64_t phys_addr, uint64_t size) {
    uint64_t cr3 = read_cr3();
    uint64_t *pml4 = (uint64_t *)(cr3 & PTE_ADDR_MASK);
    uint64_t end = phys_addr + size;

    /* Disable write protection so we can modify page table entries */
    uint64_t cr0 = read_cr0();
    write_cr0(cr0 & ~CR0_WP);

    int pages_modified = 0;

    for (uint64_t addr = phys_addr; addr < end; ) {
        int pml4_idx = (addr >> 39) & 0x1FF;
        int pdpt_idx = (addr >> 30) & 0x1FF;
        int pd_idx   = (addr >> 21) & 0x1FF;
        int pt_idx   = (addr >> 12) & 0x1FF;

        /* Level 4: PML4 */
        if (!(pml4[pml4_idx] & PTE_PRESENT)) {
            addr = (addr + (1ULL << 39)) & ~((1ULL << 39) - 1);
            continue;
        }
        uint64_t *pdpt = (uint64_t *)(pml4[pml4_idx] & PTE_ADDR_MASK);

        /* Level 3: PDPT */
        if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
            addr = (addr + (1ULL << 30)) & ~((1ULL << 30) - 1);
            continue;
        }
        if (pdpt[pdpt_idx] & PTE_PS) {
            set_wc_large(&pdpt[pdpt_idx]);
            invlpg(addr);
            pages_modified++;
            addr = (addr + (1ULL << 30)) & ~((1ULL << 30) - 1);
            continue;
        }
        uint64_t *pd = (uint64_t *)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

        /* Level 2: PD */
        if (!(pd[pd_idx] & PTE_PRESENT)) {
            addr = (addr + (1ULL << 21)) & ~((1ULL << 21) - 1);
            continue;
        }
        if (pd[pd_idx] & PTE_PS) {
            set_wc_large(&pd[pd_idx]);
            invlpg(addr);
            pages_modified++;
            addr = (addr + (1ULL << 21)) & ~((1ULL << 21) - 1);
            continue;
        }

        uint64_t *pt = (uint64_t *)(pd[pd_idx] & PTE_ADDR_MASK);

        /* Level 1: PT (4KB page) */
        if (pt[pt_idx] & PTE_PRESENT) {
            set_wc_4k(&pt[pt_idx]);
            invlpg(addr);
            pages_modified++;
        }

        addr += 1ULL << 12;
    }

    /* Restore write protection */
    write_cr0(cr0);

    terminal_printf(" Framebuffer WC: %d pages at 0x%x (%d KB)\n",
                    pages_modified, (uint32_t)phys_addr,
                    (uint32_t)(size / 1024));
}

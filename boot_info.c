#include "boot_info.h"
#include "vga.h"
#include "io.h"

static inline void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                          uint32_t *ecx, uint32_t *edx) {
    asm volatile ("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf));
}

static inline uint32_t read_eflags(void) {
    unsigned long flags;
#ifdef __x86_64__
    asm volatile ("pushfq; popq %0" : "=r"(flags));
#else
    asm volatile ("pushfl; popl %0" : "=r"(flags));
#endif
    return (uint32_t)flags;
}

static int check_multiboot(uint32_t magic) {
    if (magic != MBOOT_MAGIC) {
        terminal_setcolor(VGA_RED, VGA_BLACK);
        terminal_printf(" WARNING: Bad multiboot magic: 0x%x\n", magic);
        return 0;
    }
    terminal_setcolor(VGA_GREEN, VGA_BLACK);
    terminal_print(" Multiboot OK\n");
    return 1;
}

static void print_multiboot(struct multiboot_info *mboot) {
    if (mboot->flags & MBOOT_FLAG_MEM) {
        terminal_printf(" Lower memory: %d KB\n", mboot->mem_lower);
        terminal_printf(" Upper memory: %d KB (%d MB)\n",
                        mboot->mem_upper, mboot->mem_upper / 1024);
    }

    if (mboot->flags & MBOOT_FLAG_LOADER)
        terminal_printf(" Boot loader: %s\n",
                        (const char *)mboot->boot_loader_name);

    if (mboot->flags & MBOOT_FLAG_CMDLINE)
        terminal_printf(" Command line: %s\n",
                        (const char *)mboot->cmdline);

    if (mboot->flags & MBOOT_FLAG_BOOTDEV) {
        uint32_t bd = mboot->boot_device;
        terminal_printf(" Boot device: drive=0x%x, part=%d/%d/%d\n",
                        (bd >> 24) & 0xFF,
                        (bd >> 16) & 0xFF,
                        (bd >> 8) & 0xFF,
                        bd & 0xFF);
    }
}

static void print_cpuid(void) {
    uint32_t eax, ebx, ecx, edx;

    cpuid(0, &eax, &ebx, &ecx, &edx);
    char vendor[13];
    *(uint32_t *)&vendor[0] = ebx;
    *(uint32_t *)&vendor[4] = edx;
    *(uint32_t *)&vendor[8] = ecx;
    vendor[12] = '\0';
    terminal_printf(" CPU vendor: %s\n", vendor);

    cpuid(1, &eax, &ebx, &ecx, &edx);
    uint32_t family = (eax >> 8) & 0xF;
    uint32_t model  = (eax >> 4) & 0xF;
    uint32_t stepping = eax & 0xF;
    if (family == 0xF)
        family += (eax >> 20) & 0xFF;
    if (family == 0x6 || family == 0xF)
        model += ((eax >> 16) & 0xF) << 4;
    terminal_printf(" CPU family=%d model=%d stepping=%d\n",
                    family, model, stepping);
    terminal_printf(" CPU features: edx=0x%x ecx=0x%x\n", edx, ecx);
}

static void print_eflags(void) {
    uint32_t eflags = read_eflags();
    terminal_printf(" EFLAGS: 0x%x", eflags);
    terminal_print(" [");
    if (eflags & (1 << 0))  terminal_print(" CF");
    if (eflags & (1 << 6))  terminal_print(" ZF");
    if (eflags & (1 << 7))  terminal_print(" SF");
    if (eflags & (1 << 9))  terminal_print(" IF");
    if (eflags & (1 << 10)) terminal_print(" DF");
    if (eflags & (1 << 11)) terminal_print(" OF");
    if (eflags & (1 << 21)) terminal_print(" ID");
    terminal_print(" ]\n");
}

static void print_ata(void) {
    /* Probe primary master (ports 0x1F0-0x1F7) */
    /* Select drive 0 */
    outb(0x1F6, 0xA0);
    /* Send IDENTIFY command */
    outb(0x1F7, 0xEC);

    /* Read status — 0 means no drive */
    uint8_t status = inb(0x1F7);
    if (status == 0) {
        terminal_print(" ATA primary master: not present\n");
        return;
    }

    /* Wait for BSY to clear */
    while (inb(0x1F7) & 0x80)
        ;

    /* Check for errors (could be ATAPI/SATA) */
    if (inb(0x1F4) || inb(0x1F5)) {
        terminal_print(" ATA primary master: not ATA\n");
        return;
    }

    /* Wait for DRQ or ERR */
    while (1) {
        status = inb(0x1F7);
        if (status & 0x08) break;  /* DRQ — data ready */
        if (status & 0x01) {       /* ERR */
            terminal_print(" ATA primary master: error\n");
            return;
        }
    }

    /* Read 256 words of IDENTIFY data */
    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = inw(0x1F0);

    /* Words 60-61: total number of LBA28 sectors */
    uint32_t sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    uint32_t size_kb = sectors / 2;

    /* Words 27-46: model string (40 ASCII chars, byte-swapped) */
    char model[41];
    for (int i = 0; i < 20; i++) {
        model[i * 2]     = (char)(id[27 + i] >> 8);
        model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
    }
    model[40] = '\0';
    /* Trim trailing spaces */
    for (int i = 39; i >= 0 && model[i] == ' '; i--)
        model[i] = '\0';

    terminal_printf(" ATA disk: %s\n", model);
    terminal_printf(" ATA size: %d sectors (%d KB)\n", sectors, size_kb);
}

void boot_info_print(uint32_t magic, struct multiboot_info *mboot) {
    
    terminal_setcolor(VGA_YELLOW, VGA_BLACK);
    terminal_clear();
    terminal_print("\n");
    terminal_print(" =================================\n");
    terminal_print("   ICARUS\n");
    terminal_print("   32-bit x86 kernel\n");
    terminal_print(" =================================\n");
    terminal_print("\n");

    if (!check_multiboot(magic))
        return;

    terminal_setcolor(VGA_CYAN, VGA_BLACK);
    print_multiboot(mboot);
    print_cpuid();
    print_eflags();
    print_ata();

}

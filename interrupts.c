#include "interrupts.h"
#include "sysinfo.h"
#include "io.h"
#include "klib.h"
#include <stdint.h>

/* ---- PIC ports ---- */
#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define PIC_EOI    0x20

/* ---- PS/2 keyboard ---- */
#define KB_DATA_PORT 0x60

/* Scancode to ASCII lookups (unshifted and shifted) */
static const char scancode_lower[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,  ' ',
};

static const char scancode_upper[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,  ' ',
};

/* Key buffer — ring buffer filled by ISR, read by keyboard_poll */
#define KEYBUF_SIZE 64
static volatile char keybuf[KEYBUF_SIZE];
static volatile int keybuf_head;
static volatile int keybuf_tail;

static int shift_held;

static void keybuf_put(char c) {
    int next = (keybuf_head + 1) % KEYBUF_SIZE;
    if (next != keybuf_tail) { /* not full */
        keybuf[keybuf_head] = c;
        keybuf_head = next;
    }
}

/* ---- IDT ---- */

#ifdef __x86_64__

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

static void idt_set_gate(int n, uint64_t handler) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = 0x08;  /* code segment */
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;  /* present, ring 0, interrupt gate */
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].reserved    = 0;
}

#else /* 32-bit */

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

static void idt_set_gate(int n, uint32_t handler) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = 0x08;
    idt[n].zero        = 0;
    idt[n].type_attr   = 0x8E;
    idt[n].offset_high = (handler >> 16) & 0xFFFF;
}

#endif

/* ---- PIC initialization ---- */

static void pic_remap(void) {
    /* Save masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* ICW1: begin initialization */
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);

    /* ICW2: remap IRQs */
    outb(PIC1_DATA, 0x20);  /* IRQ 0-7  -> INT 32-39 */
    outb(PIC2_DATA, 0x28);  /* IRQ 8-15 -> INT 40-47 */

    /* ICW3: cascading */
    outb(PIC1_DATA, 0x04);  /* slave on IRQ2 */
    outb(PIC2_DATA, 0x02);  /* cascade identity */

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    /* Mask all except IRQ0 (timer) and IRQ1 (keyboard) */
    outb(PIC1_DATA, 0xFC);  /* 11111100 = allow IRQ0 and IRQ1 */
    outb(PIC2_DATA, 0xFF);  /* mask all on slave */

    (void)mask1;
    (void)mask2;
}

/* ---- PIT configuration ---- */

#define PIT_HZ     200    /* 200 ticks per second (5ms per tick) */
#define PIT_DIVISOR (1193182 / PIT_HZ)

static void pit_init(void) {
    outb(0x43, 0x36);  /* channel 0, lo/hi byte, square wave */
    outb(0x40, (uint8_t)(PIT_DIVISOR & 0xFF));
    outb(0x40, (uint8_t)((PIT_DIVISOR >> 8) & 0xFF));
}

/* ---- Interrupt handlers ---- */

/* Timer interrupt (IRQ0 -> INT 32) */
void timer_handler(void) {
    volatile uint32_t *ticks = SYS_TICKS_PTR;
    (*ticks)++;
    if ((*ticks) % PIT_HZ == 0) {
        volatile uint32_t *secs = SYS_SECONDS_PTR;
        (*secs)++;
    }
    outb(PIC1_CMD, PIC_EOI);
}

/* Keyboard interrupt (IRQ1 -> INT 33) */
void keyboard_handler(void) {
    uint8_t scancode = inb(KB_DATA_PORT);
    volatile uint8_t *keystate = SYS_KEYSTATE_PTR;

    /* Track shift state */
    if (scancode == 0x2A || scancode == 0x36) { shift_held = 1; goto eoi; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_held = 0; goto eoi; }

    if (scancode & 0x80) {
        /* Key release */
        keystate[scancode & 0x7F] = 0;
    } else {
        /* Key press */
        keystate[scancode] = 1;
        *((volatile uint8_t *)SYS_LASTKEY) = scancode;
        if (scancode < sizeof(scancode_lower)) {
            const char *map = shift_held ? scancode_upper : scancode_lower;
            char ascii = map[scancode];
            *((volatile uint8_t *)SYS_LASTASCII) = (uint8_t)ascii;
            if (ascii)
                keybuf_put(ascii);
        }
    }
eoi:
    outb(PIC1_CMD, PIC_EOI);
}

/* ---- ISR stubs (save/restore registers, call C handler) ---- */

#ifdef __x86_64__


/* Minimal ISR: just save/restore all GPRs, no segment manipulation */
#define ISR_SAVE \
    "cld\n" \
    "push %rax\n push %rbx\n push %rcx\n push %rdx\n" \
    "push %rbp\n push %rsi\n push %rdi\n" \
    "push %r8\n push %r9\n push %r10\n push %r11\n" \
    "push %r12\n push %r13\n push %r14\n push %r15\n"

#define ISR_RESTORE \
    "pop %r15\n pop %r14\n pop %r13\n pop %r12\n" \
    "pop %r11\n pop %r10\n pop %r9\n pop %r8\n" \
    "pop %rdi\n pop %rsi\n pop %rbp\n" \
    "pop %rdx\n pop %rcx\n pop %rbx\n pop %rax\n"

__attribute__((naked)) static void isr_timer(void) {
    asm volatile (ISR_SAVE "call timer_handler\n" ISR_RESTORE "iretq\n");
}

__attribute__((naked)) static void isr_keyboard(void) {
    asm volatile (ISR_SAVE "call keyboard_handler\n" ISR_RESTORE "iretq\n");
}

#else /* 32-bit */

__attribute__((naked)) static void isr_timer(void) {
    asm volatile (
        "pusha\n"
        "call timer_handler\n"
        "popa\n"
        "iret\n"
    );
}

__attribute__((naked)) static void isr_keyboard(void) {
    asm volatile (
        "pusha\n"
        "call keyboard_handler\n"
        "popa\n"
        "iret\n"
    );
}

#endif

/* ---- Public API ---- */

void sysinfo_init(void) {
    memset((void *)SYS_BASE, 0, 256);
    keybuf_head = 0;
    keybuf_tail = 0;
}

char keybuf_read_blocking(void) {
    while (keybuf_head == keybuf_tail)
        asm volatile ("hlt"); /* sleep until next interrupt */
    char c = keybuf[keybuf_tail];
    keybuf_tail = (keybuf_tail + 1) % KEYBUF_SIZE;
    return c;
}

void keybuf_flush(void) {
    keybuf_tail = keybuf_head;
}

/* ---- GDT ---- */
/* We need our own GDT with known selectors since the UEFI GDT is unknown */

#ifdef __x86_64__

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit;
    uint8_t  base_high;
} __attribute__((packed));

static struct gdt_entry gdt[3];

struct gdt_ptr_s {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt_ptr_s gdtp;

static void gdt_init(void) {
    /* Null descriptor */
    memset(&gdt[0], 0, sizeof(struct gdt_entry));

    /* Code segment: selector 0x08 */
    gdt[1].limit_low   = 0xFFFF;
    gdt[1].base_low    = 0;
    gdt[1].base_mid    = 0;
    gdt[1].access      = 0x9A;  /* present, ring 0, code, exec/read */
    gdt[1].flags_limit = 0xAF;  /* 64-bit, 4KB granularity, limit high */
    gdt[1].base_high   = 0;

    /* Data segment: selector 0x10 */
    gdt[2].limit_low   = 0xFFFF;
    gdt[2].base_low    = 0;
    gdt[2].base_mid    = 0;
    gdt[2].access      = 0x92;  /* present, ring 0, data, read/write */
    gdt[2].flags_limit = 0xCF;  /* 32-bit, 4KB granularity, limit high */
    gdt[2].base_high   = 0;

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base = (uint64_t)gdt;
    asm volatile (
        "lgdt %0\n"
        /* Reload CS via far return */
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        /* Reload data segments */
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        : : "m"(gdtp) : "rax", "memory"
    );
}

#else

static void gdt_init(void) {
    /* For 32-bit BIOS boot, the Multiboot loader already set up a GDT
       with code=0x08, data=0x10. Nothing to do. */
}

#endif

void interrupts_init(void) {
    sysinfo_init();

    /* Set up our own GDT with known selectors */
    gdt_init();

    /* Clear IDT */
    memset(idt, 0, sizeof(idt));

    /* Set up timer and keyboard gates */
#ifdef __x86_64__
    idt_set_gate(32, (uint64_t)isr_timer);
    idt_set_gate(33, (uint64_t)isr_keyboard);
#else
    idt_set_gate(32, (uint32_t)isr_timer);
    idt_set_gate(33, (uint32_t)isr_keyboard);
#endif

    /* Load IDT */
    idtp.limit = sizeof(idt) - 1;
#ifdef __x86_64__
    idtp.base = (uint64_t)idt;
#else
    idtp.base = (uint32_t)idt;
#endif
    asm volatile ("lidt %0" : : "m"(idtp));

    /* Remap PIC */
    pic_remap();
    pit_init();

    /* Enable interrupts */
    asm volatile ("sti");
}

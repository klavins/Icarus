#include "os.h"
#include "vga.h"
#include "keyboard.h"
#include "interrupts.h"
#include "sysinfo.h"
#include "speaker.h"
#include "fs.h"
#include "io.h"
#include "klib.h"
#include <stdarg.h>

/* ---- Display ---- */

void os_print(const char *s) { terminal_print(s); }
void os_putchar(char c) { terminal_putchar(c); }

void os_printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    terminal_print(buf);
}

void os_set_color(int fg, int bg) { terminal_setcolor(fg, bg); }
void os_get_color(int *fg, int *bg) { terminal_getcolor(fg, bg); }
void os_clear_screen(void) { terminal_clear(); }
size_t os_cursor_col(void) { return terminal_getcol(); }

/* ---- Input ---- */

char os_read_key(void) { return keyboard_poll(); }

int os_key_state(int scancode) {
    if (scancode < 0 || scancode > 127) return 0;
    return SYS_KEYSTATE_PTR[scancode];
}

int os_last_key_ascii(void) {
    return *((volatile uint8_t *)SYS_LASTASCII);
}

void os_flush_keys(void) { keybuf_flush(); }

/* ---- Sound ---- */

void os_tone(int freq) { speaker_on(freq); }
void os_tone_off(void) { speaker_off(); }

/* ---- Disk ---- */

int os_format_disk(void) { return fs_format(); }
int os_list_files(void) { return fs_list(); }
int os_save_file(const char *name, const void *data, uint32_t size) { return fs_save(name, data, size); }
int os_load_file(const char *name, void *data, uint32_t max_size) { return fs_load(name, data, max_size); }
int os_delete_file(const char *name) { return fs_delete(name); }
int os_file_exists(const char *name) { return fs_exists(name); }

/* ---- Memory ---- */

/* These are defined in basic_vars.c — forward to them */
extern void   basic_heap_init(void);
extern void  *basic_alloc(size_t size);
extern void   basic_alloc_reset(void);
extern void   basic_alloc_set_watermark(void);
extern size_t basic_heap_free(void);

void   os_heap_init(void) { basic_heap_init(); }
void  *os_alloc(size_t size) { return basic_alloc(size); }
void   os_alloc_reset(void) { basic_alloc_reset(); }
void   os_alloc_set_watermark(void) { basic_alloc_set_watermark(); }
size_t os_heap_free(void) { return basic_heap_free(); }

/* ---- Timer ---- */

void os_delay_ms(int ms) {
    if (ms <= 0) return;
    volatile uint32_t *ticks = SYS_TICKS_PTR;
    uint32_t wait_ticks = (uint32_t)ms / 5;
    if (wait_ticks < 1) wait_ticks = 1;
    uint32_t start = *ticks;
    while ((*ticks - start) < wait_ticks)
        asm volatile ("hlt");
}

uint32_t os_ticks(void) {
    return *SYS_TICKS_PTR;
}

/* ---- System ---- */

void os_shutdown(void) {
    /* Try ACPI shutdown (works in QEMU/Bochs) */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    /* If still running, we're on real hardware — halt the CPU */
    os_print("\n IT IS NOW SAFE TO TURN OFF YOUR COMPUTER.\n");
    asm volatile ("cli");
    for (;;)
        asm volatile ("hlt");
}

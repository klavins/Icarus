#include <stdint.h>
#include "multiboot.h"
#include "vga.h"
#include "boot_info.h"
#include "keyboard.h"
#include "basic.h"
#include "fs.h"

#define LINE_MAX 80

void kernel_main(uint32_t magic, struct multiboot_info *mboot) {

    boot_info_print(magic, mboot);

    fs_init();
    basic_init();
    terminal_setcolor(VGA_LGREEN, VGA_BLACK);
    terminal_print("\n ICARUS BASIC\n");
    terminal_print(" > ");

    char line[LINE_MAX];
    int pos = 0;

    for (;;) {
        char c = keyboard_poll();
        if (c == '\n') {
            line[pos] = '\0';
            terminal_putchar('\n');
            if (pos > 0)
                basic_exec(line);
            pos = 0;
            terminal_print(" > ");
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                terminal_putchar('\b');
            }
        } else if (c && pos < LINE_MAX - 1) {
            line[pos++] = c;
            terminal_putchar(c);
        }
    }

}

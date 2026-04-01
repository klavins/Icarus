/*
 * hello.c - Hello World for ICARUS OS
 */

#include "icarus.h"

int main(void) {
    os_clear_screen();
    os_set_color(OS_GREEN, OS_BLACK);
    os_print("Hello, ICARUS!\n\n");

    os_set_color(OS_LGRAY, OS_BLACK);
    os_print("This is a C program running on bare metal.\n");

    os_print("\nPress any key to return to BASIC.\n");
    os_read_key();

    os_clear_screen();
    return 0;
}

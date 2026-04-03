/*
 * hello.c - Hello World for ICARUS OS
 */

#include "os.h"

int main(int argc, char **argv) {
    os_clear_screen();
    os_set_color(OS_GREEN, OS_BLACK);
    os_print("Hello, ICARUS!\n\n");

    os_set_color(OS_LGRAY, OS_BLACK);
    if (argc > 1) {
        os_print("Arguments:\n");
        for (int i = 1; i < argc; i++) {
            os_print("  ");
            os_print(argv[i]);
            os_print("\n");
        }
    } else {
        os_print("No arguments. Try: EXEC hello \"world\" 42\n");
    }

    os_print("\nPress any key to return to BASIC.\n");
    os_read_key();

    os_clear_screen();
    return 0;
}

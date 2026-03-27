#include "keyboard.h"
#include "interrupts.h"
#include "sysinfo.h"

char keyboard_poll(void) {
    return keybuf_read_blocking();
}

int keyboard_escape_pressed(void) {
    /* Check if ESC key is currently held (scancode 1) */
    return SYS_KEYSTATE_PTR[1];
}

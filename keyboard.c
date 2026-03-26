#include "keyboard.h"
#include "io.h"

#define KB_DATA_PORT   0x60
#define KB_STATUS_PORT 0x64

#define SC_ESCAPE        0x01
#define SC_LSHIFT_PRESS  0x2A
#define SC_RSHIFT_PRESS  0x36
#define SC_LSHIFT_RELEASE 0xAA
#define SC_RSHIFT_RELEASE 0xB6

static int shift_held;

/* Scancode set 1 -> ASCII (unshifted) */
static const char scancode_lower[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,  ' ',
};

/* Scancode set 1 -> ASCII (shifted) */
static const char scancode_upper[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,  ' ',
};

char keyboard_poll(void) {
    for (;;) {
        while (!(inb(KB_STATUS_PORT) & 0x01))
            ;

        uint8_t scancode = inb(KB_DATA_PORT);

        /* Track shift state */
        if (scancode == SC_LSHIFT_PRESS || scancode == SC_RSHIFT_PRESS) {
            shift_held = 1;
            continue;
        }
        if (scancode == SC_LSHIFT_RELEASE || scancode == SC_RSHIFT_RELEASE) {
            shift_held = 0;
            continue;
        }

        /* Ignore other key releases */
        if (scancode & 0x80)
            continue;

        if (scancode < sizeof(scancode_lower)) {
            const char *map = shift_held ? scancode_upper : scancode_lower;
            return map[scancode];
        }
    }
}

int keyboard_escape_pressed(void) {
    if (!(inb(KB_STATUS_PORT) & 0x01))
        return 0;
    uint8_t scancode = inb(KB_DATA_PORT);
    /* Track shift state even during non-blocking checks */
    if (scancode == SC_LSHIFT_PRESS || scancode == SC_RSHIFT_PRESS)
        shift_held = 1;
    if (scancode == SC_LSHIFT_RELEASE || scancode == SC_RSHIFT_RELEASE)
        shift_held = 0;
    return scancode == SC_ESCAPE;
}

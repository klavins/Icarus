#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Poll for a keypress. Blocks until a key is pressed.
   Returns the ASCII character, or 0 for non-printable keys. */
char keyboard_poll(void);

/* Non-blocking check if Escape was pressed. Returns 1 if so. */
int keyboard_escape_pressed(void);

#endif

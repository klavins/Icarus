#ifndef INTERRUPTS_H
#define INTERRUPTS_H

void interrupts_init(void);

/* Read a character from the key buffer, blocking until one is available. */
char keybuf_read_blocking(void);

/* Flush all pending keys from the buffer. */
void keybuf_flush(void);

#endif

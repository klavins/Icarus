#ifndef SYSINFO_H
#define SYSINFO_H

#include <stdint.h>

/*
 * System status area — a fixed memory region readable via PEEK.
 * Base address: 0x00070000 (safe conventional memory)
 *
 * Layout:
 *   0x70000 - 0x7007F  Key state (128 bytes, 1=pressed, 0=released)
 *   0x70080            Last key pressed (scancode)
 *   0x70081            Last key ASCII value
 *   0x70084 - 0x70087  Timer tick count (32-bit, increments ~18.2 times/sec)
 *   0x70088 - 0x7008B  Uptime in seconds (approximate)
 */

#define SYS_BASE         0x00070000
#define SYS_KEYSTATE     (SYS_BASE + 0x000)  /* 128 bytes */
#define SYS_LASTKEY      (SYS_BASE + 0x080)  /* 1 byte: last scancode */
#define SYS_LASTASCII    (SYS_BASE + 0x081)  /* 1 byte: last ASCII */
#define SYS_TICKS        (SYS_BASE + 0x084)  /* 4 bytes: tick count */
#define SYS_SECONDS      (SYS_BASE + 0x088)  /* 4 bytes: uptime seconds */

#define SYS_KEYSTATE_PTR ((volatile uint8_t *)SYS_KEYSTATE)
#define SYS_TICKS_PTR    ((volatile uint32_t *)SYS_TICKS)
#define SYS_SECONDS_PTR  ((volatile uint32_t *)SYS_SECONDS)

void sysinfo_init(void);

#endif

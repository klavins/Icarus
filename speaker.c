#include "speaker.h"
#include "io.h"

#define PIT_FREQ     1193182
#define PIT_CMD      0x43
#define PIT_CH2_DATA 0x42
#define PIT_CH2_MODE 0xB6   /* channel 2, square wave, lo/hi byte */
#define SYS_CTRL     0x61   /* system control port */
#define SPK_ENABLE   0x03   /* bits 0+1: PIT gate + speaker enable */
#define SPK_GATE     0x01   /* bit 0: PIT channel 2 gate */
#define SPK_DATA     0x02   /* bit 1: speaker data (direct toggle) */

void speaker_on(int freq) {
    if (freq <= 0) return;
    uint16_t div = PIT_FREQ / freq;
    outb(PIT_CMD, PIT_CH2_MODE);
    outb(PIT_CH2_DATA, (uint8_t)(div & 0xFF));
    outb(PIT_CH2_DATA, (uint8_t)((div >> 8) & 0xFF));
    uint8_t val = inb(SYS_CTRL);
    outb(SYS_CTRL, val | SPK_ENABLE);
}

void speaker_off(void) {
    uint8_t val = inb(SYS_CTRL);
    outb(SYS_CTRL, val & ~SPK_ENABLE);
}

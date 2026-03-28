#ifndef BGA_H
#define BGA_H

#include <stdint.h>

/* Bochs Graphics Adapter driver */

/* Detect BGA on PCI bus. Returns 1 if found. */
int bga_detect(void);

/* Initialize display: set resolution, bpp, double-buffered virtual height */
void bga_init(uint32_t width, uint32_t height);

/* Get framebuffer info */
uint8_t *bga_framebuffer(void);
uint32_t bga_width(void);
uint32_t bga_height(void);
uint32_t bga_pitch(void);

/* Page flipping: 0 = front, 1 = back */
int     bga_can_flip(void);    /* 1 if double buffering is available */
void    bga_set_page(int page);
uint8_t *bga_page_addr(int page);

/* GPU driver struct for gpu.h abstraction */
struct gpu_driver;
extern struct gpu_driver bga_gpu_driver;

#endif

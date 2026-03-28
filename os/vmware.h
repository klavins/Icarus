#ifndef VMWARE_H
#define VMWARE_H

#include <stdint.h>

/* VMware SVGA II driver */

int  vmware_detect(void);
void vmware_init(uint32_t width, uint32_t height);

uint8_t *vmware_framebuffer(void);
uint32_t vmware_width(void);
uint32_t vmware_height(void);
uint32_t vmware_pitch(void);

int      vmware_can_flip(void);
uint8_t *vmware_page_addr(int page);
void     vmware_set_page(int page);

/* GPU driver struct */
struct gpu_driver;
extern struct gpu_driver vmware_gpu_driver;

#endif

/*
 * vmware.h - VMware SVGA II driver API declarations
 *
 * Copyright (C) 2026 Eric Klavins
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

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

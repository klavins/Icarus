/*
 * gpu.h - GPU driver abstraction interface and structures
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

#ifndef GPU_H
#define GPU_H

#include <stdint.h>

struct gpu_driver {
    const char *name;
    int      (*detect)(void);
    void     (*init)(uint32_t width, uint32_t height);
    uint8_t *(*framebuffer)(void);
    uint32_t (*width)(void);
    uint32_t (*height)(void);
    uint32_t (*pitch)(void);
    int      (*can_flip)(void);
    uint8_t *(*page_addr)(int page);
    void     (*set_page)(int page);
    void     (*update)(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
};

/* The active GPU driver, or NULL if none */
extern struct gpu_driver *gpu;

/* Probe for a supported GPU and set the global driver */
void gpu_init(void);

/* Tell the GPU a region of the framebuffer changed (no-op if not needed) */
static inline void gpu_update(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (gpu && gpu->update)
        gpu->update(x, y, w, h);
}

#endif

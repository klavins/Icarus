/*
 * gpu.c - GPU driver abstraction and auto-detection layer
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

#include "gpu.h"
#include "nvidia.h"
#include "bga.h"
#include "vmware.h"
#include "pat.h"
#include "vga.h"

struct gpu_driver *gpu = 0;

/* List of GPU drivers to probe, in order of preference */
static struct gpu_driver *drivers[] = {
    &nvidia_gpu_driver,
    &bga_gpu_driver,
    &vmware_gpu_driver,
    0
};

void gpu_init(void) {
    /* Get current resolution from terminal (set by GOP) */
    uint8_t *addr;
    uint32_t w, h, p, b;
    terminal_get_fb(&addr, &w, &h, &p, &b);

    /* Try each driver until one works */
    for (int i = 0; drivers[i]; i++) {
        if (!drivers[i]->detect())
            continue;

        drivers[i]->init(w, h);

        /* Only take over display if the driver provides a framebuffer */
        if (!drivers[i]->framebuffer())
            continue;

        /* Set up write-combining for the GPU framebuffer */
        pat_set_write_combining(
            (uint64_t)(uintptr_t)drivers[i]->framebuffer(),
            drivers[i]->pitch() * drivers[i]->height() * 2);

        /* Switch terminal to GPU framebuffer */
        terminal_set_fb(drivers[i]->framebuffer(),
                        drivers[i]->width(),
                        drivers[i]->height(),
                        drivers[i]->pitch(), 4);

        gpu = drivers[i];
        terminal_printf(" Display: GOP -> %s\n", gpu->name);
        return;
    }

    /* No GPU driver found — stay on GOP */
    terminal_print(" Display: GOP (no GPU driver)\n");
}

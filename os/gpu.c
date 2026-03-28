#include "gpu.h"
#include "bga.h"
#include "vmware.h"
#include "pat.h"
#include "vga.h"

struct gpu_driver *gpu = 0;

/* List of GPU drivers to probe, in order of preference */
static struct gpu_driver *drivers[] = {
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

#include "gpu.h"
#include "bga.h"
#include "pat.h"
#include "vga.h"

struct gpu_driver *gpu = 0;

void gpu_init(void) {
    /* Try each driver in order of preference */
    if (bga_detect()) {
        /* Get current resolution from terminal (set by GOP) */
        uint8_t *addr;
        uint32_t w, h, p, b;
        terminal_get_fb(&addr, &w, &h, &p, &b);

        bga_init(w, h);

        /* Set up write-combining for the BGA framebuffer */
        pat_set_write_combining((uint64_t)(uintptr_t)bga_framebuffer(),
                                bga_pitch() * bga_height() * 2);

        /* Switch terminal to BGA framebuffer */
        terminal_set_fb(bga_framebuffer(), bga_width(), bga_height(),
                        bga_pitch(), 4);

        gpu = &bga_gpu_driver;
        terminal_printf(" Display: GOP -> %s\n", gpu->name);
        return;
    }

    /* No GPU driver found — stay on GOP */
    terminal_print(" Display: GOP (no GPU driver)\n");
}

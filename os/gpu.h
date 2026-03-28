#ifndef GPU_H
#define GPU_H

#include <stdint.h>

struct gpu_driver {
    const char *name;
    uint8_t *(*framebuffer)(void);
    uint32_t (*width)(void);
    uint32_t (*height)(void);
    uint32_t (*pitch)(void);
    int      (*can_flip)(void);
    uint8_t *(*page_addr)(int page);
    void     (*set_page)(int page);
};

/* The active GPU driver, or NULL if none */
extern struct gpu_driver *gpu;

/* Probe for a supported GPU and set the global driver */
void gpu_init(void);

#endif

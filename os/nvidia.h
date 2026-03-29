#ifndef NVIDIA_H
#define NVIDIA_H

#include <stdint.h>

/* NVIDIA GPU driver — detection and info only for now */

int  nvidia_detect(void);

/* Save display register dump to disk (call after fs_init) */
void nvidia_save_dump(void);

/* GPU driver struct */
struct gpu_driver;
extern struct gpu_driver nvidia_gpu_driver;

#endif

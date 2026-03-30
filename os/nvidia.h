/*
 * nvidia.h - NVIDIA GPU driver API declarations
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

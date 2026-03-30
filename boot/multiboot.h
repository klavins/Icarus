/*
 * multiboot.h - Multiboot specification structures and constants
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

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MBOOT_MAGIC 0x2BADB002

/* Flags bits indicating which fields in multiboot_info are valid */
#define MBOOT_FLAG_MEM      (1 << 0)   /* mem_lower, mem_upper */
#define MBOOT_FLAG_BOOTDEV  (1 << 1)   /* boot_device */
#define MBOOT_FLAG_CMDLINE  (1 << 2)   /* cmdline */
#define MBOOT_FLAG_MODS     (1 << 3)   /* mods_count, mods_addr */
#define MBOOT_FLAG_SYMS_A   (1 << 4)   /* a.out symbol table */
#define MBOOT_FLAG_SYMS_E   (1 << 5)   /* ELF section header */
#define MBOOT_FLAG_MMAP     (1 << 6)   /* mmap_length, mmap_addr */
#define MBOOT_FLAG_DRIVES   (1 << 7)   /* drives_length, drives_addr */
#define MBOOT_FLAG_CONFIG   (1 << 8)   /* config_table */
#define MBOOT_FLAG_LOADER   (1 << 9)   /* boot_loader_name */
#define MBOOT_FLAG_APM      (1 << 10)  /* apm_table */
#define MBOOT_FLAG_VBE      (1 << 11)  /* VBE fields */

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
};

#endif

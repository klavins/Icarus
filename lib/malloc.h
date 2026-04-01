/*
 * malloc.h - Dynamic memory allocator declarations
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

#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>

/* Initialize the heap with a given memory region */
void heap_init(void *base, size_t size);

/* Standard allocation functions */
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

/* Convenience */
void *calloc(size_t count, size_t size);

/* Diagnostics */
size_t heap_free_total(void);
size_t heap_used_total(void);

#endif

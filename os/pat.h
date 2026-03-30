/*
 * pat.h - Page Attribute Table API declarations
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

#ifndef PAT_H
#define PAT_H

#include <stdint.h>

/* Set up PAT with Write-Combining in slot 1 */
void pat_init(void);

/* Mark a physical address range as Write-Combining in the page tables */
void pat_set_write_combining(uint64_t phys_addr, uint64_t size);

#endif

/*
 * uefi_stubs.c - MinGW runtime stubs for freestanding UEFI builds
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

/* MinGW runtime stubs for freestanding UEFI build */

/* Stack probe — not needed in freestanding, just return */
void __attribute__((naked)) ___chkstk_ms(void) {
    asm volatile ("ret");
}

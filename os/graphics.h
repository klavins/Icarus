/*
 * graphics.h - Graphics drawing API declarations
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

#ifndef GRAPHICS_H
#define GRAPHICS_H

/*
 * Graphics modes:
 *   0 = text mode (1x font)
 *   1 = text mode (2x font)
 *   2 = ~320px wide (chunky pixels, Atari-style)
 *   3 = ~640px wide (medium)
 *   4 = ~800px wide (high)
 *   5 = native resolution
 */
void graphics_alloc_init(void);
void gfx_set_mode(int mode);
int  gfx_get_mode(void);
int  gfx_width(void);
int  gfx_height(void);
void gfx_clear(void);
void gfx_set_color(int color);
void gfx_plot(int x, int y);
void gfx_drawto(int x, int y);
void gfx_fillto(int x, int y);
void gfx_pixel(int x, int y, int color);
void gfx_pos(int x, int y);
void gfx_text(const char *str);
void gfx_present(void);

#endif

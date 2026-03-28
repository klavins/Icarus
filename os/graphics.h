#ifndef GRAPHICS_H
#define GRAPHICS_H

/*
 * Graphics modes (virtual resolution derived from physical by integer scaling):
 *   0 = text mode
 *   1 = ~320px wide (chunky pixels, Atari-style)
 *   2 = ~640px wide (medium)
 *   3 = ~800px wide (high)
 *   4 = native resolution
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

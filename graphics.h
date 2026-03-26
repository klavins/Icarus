#ifndef GRAPHICS_H
#define GRAPHICS_H

#define GFX_WIDTH  320
#define GFX_HEIGHT 200

void gfx_set_mode(int mode);  /* 0 = text, 1 = 320x200x256 */
int  gfx_get_mode(void);
void gfx_clear(void);
void gfx_set_color(int color);
void gfx_plot(int x, int y);
void gfx_drawto(int x, int y);
void gfx_pixel(int x, int y, int color);

#endif

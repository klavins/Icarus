#ifndef GRAPHICS_H
#define GRAPHICS_H

void gfx_set_mode(int mode);  /* 0 = text, 1 = pixel graphics */
int  gfx_get_mode(void);
int  gfx_width(void);
int  gfx_height(void);
void gfx_clear(void);
void gfx_set_color(int color);
void gfx_plot(int x, int y);
void gfx_drawto(int x, int y);
void gfx_pixel(int x, int y, int color);

#endif

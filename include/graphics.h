/*
 * graphics.h - Pixel graphics functions
 */
#ifndef ICARUS_USER_GRAPHICS_H
#define ICARUS_USER_GRAPHICS_H

#include "base.h"

#define gfx_set_mode(m)         ((void (*)(int))_icarus_api(API_GFX_SET_MODE))(m)
#define gfx_get_mode()          ((int (*)(void))_icarus_api(API_GFX_GET_MODE))()
#define gfx_width()             ((int (*)(void))_icarus_api(API_GFX_WIDTH))()
#define gfx_height()            ((int (*)(void))_icarus_api(API_GFX_HEIGHT))()
#define gfx_clear()             ((void (*)(void))_icarus_api(API_GFX_CLEAR))()
#define gfx_set_color(c)        ((void (*)(int))_icarus_api(API_GFX_SET_COLOR))(c)
#define gfx_plot(x, y)          ((void (*)(int, int))_icarus_api(API_GFX_PLOT))(x, y)
#define gfx_drawto(x, y)        ((void (*)(int, int))_icarus_api(API_GFX_DRAWTO))(x, y)
#define gfx_fillto(x, y)        ((void (*)(int, int))_icarus_api(API_GFX_FILLTO))(x, y)
#define gfx_pixel(x, y, c)      ((void (*)(int, int, int))_icarus_api(API_GFX_PIXEL))(x, y, c)
#define gfx_pos(x, y)           ((void (*)(int, int))_icarus_api(API_GFX_POS))(x, y)
#define gfx_text(s)             ((void (*)(const char*))_icarus_api(API_GFX_TEXT))(s)
#define gfx_present()           ((void (*)(void))_icarus_api(API_GFX_PRESENT))()

#endif

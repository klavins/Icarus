/*
 * demo.c - ICARUS graphics mode demo
 * Shows all four pixel graphics modes with border, circle, and text.
 */

#include "os.h"
#include "graphics.h"
#include "math.h"

static void draw_circle(int cx, int cy, int r) {
    int steps = 36;
    double da = 2.0 * PI / steps;
    gfx_plot(cx + r, cy);
    for (int i = 1; i <= steps; i++) {
        int px = cx + (int)(r * cos(i * da));
        int py = cy + (int)(r * sin(i * da));
        gfx_drawto(px, py);
    }
}

static void demo_mode(int mode, const char *label, const char *res, const char *desc) {
    gfx_set_mode(mode);
    int w = gfx_width() - 1;
    int h = gfx_height() - 1;
    int cx = gfx_width() / 2;
    int cy = gfx_height() / 2;

    /* Border */
    gfx_set_color(4);  /* red */
    gfx_plot(0, 0);
    gfx_drawto(w, 0);
    gfx_drawto(w, h);
    gfx_drawto(0, h);
    gfx_drawto(0, 0);

    /* Circle */
    gfx_set_color(10); /* light green */
    draw_circle(cx, cy, gfx_height() / 4);

    /* Text */
    gfx_set_color(15); /* white */
    gfx_pos(cx - 44, cy - 50);
    gfx_text(label);

    gfx_set_color(14); /* yellow */
    gfx_pos(cx - 36, cy - 30);
    gfx_text(res);

    gfx_set_color(2);  /* green */
    gfx_pos(cx - 56, cy - 10);
    gfx_text(desc);

    gfx_set_color(1);  /* blue */
    gfx_pos(cx - 52, cy + 10);
    gfx_text("PRESS ANY KEY");

    gfx_present();
    os_read_key();
}

int main(void) {
    demo_mode(2, "GRAPHICS 2", "320 x 200",  "CHUNKY PIXELS");
    demo_mode(3, "GRAPHICS 3", "640 x 400",  "MEDIUM RESOLUTION");
    demo_mode(4, "GRAPHICS 4", "800 x 600",  "HIGH RESOLUTION");
    demo_mode(5, "GRAPHICS 5", "NATIVE RES",  "FULL DETAIL");

    gfx_set_mode(0);
    return 0;
}

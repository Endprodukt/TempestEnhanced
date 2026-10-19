#include "render/draw.h"

#include <math.h>

typedef struct pen {
    vp_dvg *d;
    int     have_pos, x, y;
    int     have_col;
    uint8_t col[3];
} pen;

void tp_draw_lines(vp_dvg *d, const tp_config *cfg, const tp_line *lines, int count)
{
    pen p = { d, 0, 0, 0, 0, { 0, 0, 0 } };
    for (int i = 0; i < count; i++) {
        const tp_line *l = &lines[i];
        float k = powf((float)l->intensity / 15.0f, cfg->gamma) * cfg->brightness;
        if (k > 1.0f) k = 1.0f;
        uint8_t r = (uint8_t)lroundf(l->r * k), g = (uint8_t)lroundf(l->g * k), b = (uint8_t)lroundf(l->b * k);
        if (!(r | g | b)) continue;
        int x0 = (int)lroundf(l->x0), y0 = (int)lroundf(l->y0), x1 = (int)lroundf(l->x1), y1 = (int)lroundf(l->y1);
        if (!p.have_col || r != p.col[0] || g != p.col[1] || b != p.col[2]) {
            vp_color(d, r, g, b);
            p.col[0] = r; p.col[1] = g; p.col[2] = b;
            p.have_col = 1;
        }
        if (!p.have_pos || x0 != p.x || y0 != p.y) vp_moveto(d, x0, y0);
        if (x0 == x1 && y0 == y1) x1 += 1;      /* a dot: the beam has to move to show */
        vp_lineto(d, x1, y1);
        p.x = x1; p.y = y1;
        p.have_pos = 1;
    }
}

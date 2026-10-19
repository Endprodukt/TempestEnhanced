#include "render/svg.h"

#include <stdio.h>

int tp_svg_write(const char *path, const tp_line *lines, int count)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    /* MAME swaps the generator's axes and turns the monitor back (ROT270):
     * together the generator's x is the screen's, its y points down. */
    fprintf(f, "<svg xmlns='http://www.w3.org/2000/svg' viewBox='-300 -300 600 600' width='600' height='600'>\n");
    fprintf(f, "<rect x='-300' y='-300' width='600' height='600' fill='black'/>\n");
    for (int i = 0; i < count; i++) {
        const tp_line *ln = &lines[i];
        double a = ln->intensity / 15.0;
        fprintf(f, "<line x1='%.1f' y1='%.1f' x2='%.1f' y2='%.1f' stroke='rgb(%d,%d,%d)' stroke-opacity='%.2f' "
                   "stroke-width='1.5' stroke-linecap='round'/>\n",
                ln->x0, ln->y0, ln->x1, ln->y1, ln->r, ln->g, ln->b, a);
        if (ln->x0 == ln->x1 && ln->y0 == ln->y1)
            fprintf(f, "<circle cx='%.1f' cy='%.1f' r='1.2' fill='rgb(%d,%d,%d)' fill-opacity='%.2f'/>\n",
                    ln->x0, ln->y0, ln->r, ln->g, ln->b, a);
    }
    fprintf(f, "</svg>\n");
    fclose(f);
    return 0;
}

int tp_svg_write_board(const char *path, const tp_line *lines, int count, float aspect)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    double h = 600.0, w = 600.0 * aspect;
    fprintf(f, "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %.0f %.0f' width='%.0f' height='%.0f'>\n", w, h, w, h);
    fprintf(f, "<rect x='0' y='0' width='%.0f' height='%.0f' fill='black'/>\n", w, h);
    for (int i = 0; i < count; i++) {
        const tp_line *ln = &lines[i];
        double a = ln->intensity / 15.0;
        double x0 = ln->x0 / 4096.0 * w, y0 = (4096.0 - ln->y0) / 4096.0 * h;
        double x1 = ln->x1 / 4096.0 * w, y1 = (4096.0 - ln->y1) / 4096.0 * h;
        fprintf(f, "<line x1='%.1f' y1='%.1f' x2='%.1f' y2='%.1f' stroke='rgb(%d,%d,%d)' stroke-opacity='%.2f' "
                   "stroke-width='1.6' stroke-linecap='round'/>\n", x0, y0, x1, y1, ln->r, ln->g, ln->b, a);
    }
    fprintf(f, "</svg>\n");
    fclose(f);
    return 0;
}

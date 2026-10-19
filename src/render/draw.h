/* draw.h - lines in board units (render/layout.h) onto the VectorPie
 * device layer, each in the colour its colour RAM entry gives (as MAME
 * shows it), scaled by its intensity and the picture's brightness.
 */
#ifndef TP_DRAW_H
#define TP_DRAW_H

#include "avg/avg.h"
#include "render/config.h"

#include <vectorpie/vp_dvg.h>

/* Draw the lines in list order, chaining strokes and setting the colour
 * only when it changes. */
void tp_draw_lines(vp_dvg *d, const tp_config *cfg, const tp_line *lines, int count);

#endif

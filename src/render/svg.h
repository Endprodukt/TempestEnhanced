/* svg.h - a picture of the vector generator's lines as an SVG file, for
 * looking at what the game drew without a screen (tools/tpshot,
 * tempest --shot). */
#ifndef TP_SVG_H
#define TP_SVG_H

#include "avg/avg.h"

/* Lines in generator units, as the cabinet shows them. Returns 0, or -1
 * if the file could not be written. */
int tp_svg_write(const char *path, const tp_line *lines, int count);

/* Lines in board units (render/layout.h), on a screen of `aspect` (width
 * over height: 4/3 a horizontal tube, 3/4 an upright one). */
int tp_svg_write_board(const char *path, const tp_line *lines, int count, float aspect);

#endif

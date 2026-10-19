/* logo.h - the attract mode's title in the marquee's lettering.
 *
 * The game flies its title in as a stack of copies of one shape (3F4E,
 * 34 lines, in the text sub-list), each the logo come all the way in
 * scaled about the middle of the field, in changing colours. Each copy is
 * drawn instead as the TEMPEST of the cabinet's marquee (traced from the
 * picture by tools/logo/logotrace.py: render/logo_shape.h), the same
 * width, as tall as its proportions make it, from the same top down: the
 * copies behind as the letters' outlines in the game's colours, the front
 * one also in the marquee's colours, with the cream of the letters' sides.
 */
#ifndef TP_LOGO_H
#define TP_LOGO_H

#include "avg/avg.h"
#include "render/config.h"

typedef struct tp_logo {
    tp_avg_list out;
} tp_logo;

void tp_logo_init(tp_logo *lg);
void tp_logo_free(tp_logo *lg);

/* The picture with the title's copies in the marquee's lettering (pic
 * itself when TP_FX_LOGO is off or there is no title in it). */
const tp_avg_list *tp_logo_picture(tp_logo *lg, const tp_config *cfg, const tp_avg_list *pic);

/* Where the title ends below, come all the way in (generator units, y
 * down): the marquee's lettering's bottom with TP_FX_LOGO, the game's
 * logo's without. */
float tp_logo_bottom(const tp_config *cfg);

#endif

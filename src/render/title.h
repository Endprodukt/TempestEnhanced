/* title.h - "ENHANCED" under the game's title in the attract mode.
 *
 * The attract mode flies the game's title in (a picture of text only, the
 * text sub-list at 2004): a stack of copies of it in changing colours,
 * coming out of the depth below it, which is then taken apart layer by
 * layer from the back until the title stands alone (its letters x
 * -195..203, y -146..-98, the copyright line at 84..96). Then the port
 * flies ENHANCED in directly under it the same way, only much faster: a
 * stack of copies out of the depth, taken apart from the back. It is
 * drawn in the VectorPie SDK's own vector lettering and glows gently
 * until the title is gone.
 */
#ifndef TP_TITLE_H
#define TP_TITLE_H

#include "avg/avg.h"
#include "render/config.h"

#include <vectorpie/vp_dvg.h>

/* 1 while the picture is the title, come all the way in and standing
 * alone (its stack of copies taken apart). */
int  tp_title_complete(const tp_avg_list *pic);

/* ENHANCED where the last picture laid out (render/layout.h) puts the
 * space under the title; shown: seconds since the title stood alone. */
void tp_title_draw(vp_dvg *d, const tp_config *cfg, double shown);

#endif

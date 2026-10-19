/* nextlevel.h - the next level's playfield known before it is flown into.
 *
 * At a level's end the claw flies down the tube (state 20), then the next
 * level's tube comes out of the depth and grows (state 18) until play
 * goes on (state 04). The horizontal layout (render/layout.h) fits the
 * playfield to a tube at rest, so without help it would keep the old
 * level's scale and place through the flight and then slide to the new
 * one's. Instead, when the flight begins, a copy of the machine runs on
 * ahead, a slice each picture, until the next level is played (in a
 * game only: the attract mode's demo ends after its flight); its tube
 * gives the scale and place, set when the new tube first shows (states 0E,
 * 18, and the tube shown no larger than the next one at rest: for a few
 * frames after the flight down the game still draws the old level, its tube
 * all round the screen, which stays where it was); during the whole flight
 * the layout holds.
 */
#ifndef TP_NEXTLEVEL_H
#define TP_NEXTLEVEL_H

#include "avg/avg.h"
#include "core/emu.h"

typedef struct tp_nextlevel {
    tp_machine *copy;           /* the machine run ahead (allocated once) */
    tp_emu     *copy_emu;
    int         stage;          /* NL_xxx of nextlevel.c */
    uint64_t    started;        /* the copy's cycle when it began */
    tp_avg_list pic;            /* the next level's picture at rest */
} tp_nextlevel;

void tp_nextlevel_init(tp_nextlevel *n);
void tp_nextlevel_free(tp_nextlevel *n);

/* Once a picture shown, before it is laid out: m and e the game, shown
 * the picture. */
void tp_nextlevel_update(tp_nextlevel *n, const tp_machine *m, const tp_emu *e, const tp_avg_list *shown);

#endif

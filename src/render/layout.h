/* layout.h - where the game's picture goes on the screen.
 *
 * The vector generator's lines (avg/avg.h: center 0,0, x right, y down as
 * the cabinet shows them, its mirror bits applied) go to the board's
 * 0..4095 square:
 *
 *   vertical     native: the picture as the generator draws it, for a
 *                monitor mounted upright as on the cabinet (the tube does
 *                the rest, as it did there)
 *   horizontal   for a horizontal 4:3 screen: the playfield (the tube of
 *                the level, with room for the claw below its rim) as large
 *                as the height and the side strips allow, in the
 *                proportions the upright tube gives it (its 3:4 over the
 *                generator's square field, as MAME shows it); the text the
 *                cabinet shows above and below it (score, lives, high
 *                score, level, credits) moved row by row into the strips
 *                beside it (in a game; in the attract mode it stays where
 *                the cabinet has it, over the playfield). The scale eases from level to level and holds
 *                while the tube is flown through; the stars of the flight
 *                to the next level are spread out to the screen's edges. A
 *                picture of text only
 *                (the logo flying in, the high score table, the choice of
 *                level) as large as the screen takes it, the scale capped
 *                so that the logo, small at first, still flies in, and
 *                eased so that nothing jumps.
 *
 * The text is the game's sub-list at 2004 (its letters and digits), the
 * tube the one at 240A. Text rows above or below the playfield go to the
 * side their middle is on.
 * Size, place and mirroring (tp_config) apply to either.
 */
#ifndef TP_LAYOUT_H
#define TP_LAYOUT_H

#include "avg/avg.h"
#include "render/config.h"

#define TP_TEXT_SUB 0x2004      /* the game's sub-list of letters and digits */

/* Whether the game is in its attract mode (RAM 0005 D7 clear): then the
 * text above and below the playfield is not moved to the side strips but
 * stays where the cabinet shows it, over the playfield, as the high score
 * table has it; the playfield takes the width. */
void tp_layout_set_attract(int on);

/* Whether initials are entered (state 12): then the copyright is left out
 * and the text below does not grow (the instructions are there). */
void tp_layout_set_entry(int on);

/* Whether the player chooses the level (state 16): then that picture has
 * a layout of its own, as wide as the playfield, large and fixed. */
void tp_layout_set_rating(int on);

/* During a flight from one level to the next (render/nextlevel.h) the
 * playfield's scale and place hold, whatever the tube does. */
void tp_layout_set_flight(int on);

/* The playfield's scale and place at once as for this picture (the next
 * level at rest, run ahead), so that its tube, flown into, is already
 * where it will stay. 0 if the picture holds no tube at rest. */
int  tp_layout_aim(const tp_line *lines, int count);

#define TP_LAYOUT_EXTRA 16      /* lines of the port's own tp_layout may add */

/* The lines in board units (0..4095, y up), written to out (room for count
 * + TP_LAYOUT_EXTRA; out may be lines). Returns how many. In a game on a
 * horizontal screen the text in the corners comes last, corner by corner,
 * and the word LEVEL is added before the level's number at the bottom
 * right: out then holds the lines in another order, and more. */
int  tp_layout(const tp_config *cfg, const tp_line *lines, int count, tp_line *out);

/* A point of the native picture (generator units) in board units, for
 * pictures of the port's own laid over the game's. */
void tp_layout_point(const tp_config *cfg, float x, float y, int *bx, int *by);

/* A point of the horizontal screen (u across -2/3..2/3, v down -1/2..1/2,
 * a unit as tall as the screen) in board units. */
void tp_layout_screen(const tp_config *cfg, float u, float v, int *bx, int *by);

/* A point of the game's picture (generator units) where the last picture
 * laid out puts such a point (its playfield, or the whole of a picture of
 * text only): for drawing something of the port's own into it. */
void tp_layout_map(const tp_config *cfg, float x, float y, int *bx, int *by);

#endif

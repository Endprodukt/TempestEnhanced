/* frames.h - the game's pictures as they come, and the next one ahead.
 *
 * The game rewrites its sub-lists in place, once a frame, as the last
 * thing its main loop does; a picture is complete when the loop starts
 * its next frame (tp_emu.frames). While the jump at 2000 goes to a HALT
 * (the game does that while it changes over), or the list is empty (as
 * between the flight down the tube and the next level), there is no
 * picture and the last stays. With smooth motion
 * (render/smooth.h) a copy of the machine then runs on to the next
 * picture, with the controls as they are, and the shown picture moves
 * towards it.
 */
#ifndef TP_FRAMES_H
#define TP_FRAMES_H

#include "core/emu.h"
#include "render/smooth.h"

typedef struct tp_frames {
    uint32_t    frame;          /* the game's frame of the last picture (tp_emu.frames) */
    tp_avg_list pic;            /* the last picture (generator units, the board's mirror bits applied) */
    tp_avg_list ahead;          /* the next, from the copy */
    uint64_t    t;              /* the cycle it came */
    uint32_t    pictures;       /* pictures so far */
    tp_smooth   sm;
    tp_machine *copy;           /* the machine run ahead (allocated once) */
    tp_emu     *copy_emu;
} tp_frames;

void tp_frames_init(tp_frames *f);
void tp_frames_free(tp_frames *f);

/* After the game ran a while: take a new picture if there is one (1),
 * and if smooth, run ahead to the next. (The next picture depends only on
 * what came in before this one: the game gathers the spinner and the
 * buttons in its interrupt and takes them at the start of a frame. So the
 * copy needs no guess at what the player does meanwhile.) */
int  tp_frames_check(tp_frames *f, tp_machine *m, tp_emu *e, int smooth);

/* The picture to show at cycle now. */
void tp_frames_picture(tp_frames *f, uint64_t now, int smooth, tp_avg_list *out);

/* Where 2000 jumps, 0 if not to a list (a HALT there, or none). */
uint16_t tp_frames_target(const uint8_t *mem);

#endif

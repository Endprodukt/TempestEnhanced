/* smooth.h - pictures between the game's frames.
 *
 * The game draws about 26 pictures a second; the screen shows 50 or 60.
 * The program runs from the ROM, so no picture of an in-between moment
 * can be drawn by the game itself (as the C ports of Battlezone and Star
 * Wars do). Instead, when a picture is new, a copy of the machine runs
 * ahead to the next one (with the controls as they are); each line of the
 * picture then moves towards the same line of that next one as the time
 * between them passes. The picture is never behind the game.
 *
 * Which line is the same: the game's list calls its sub-lists, each of
 * which draws its objects one after another (avg/avg.h: tp_line.sub,
 * item, ikey, in). An object is a call of a shape, or a figure of the
 * sub-list's own vectors joined end to start (enemies are drawn so, line
 * by line). If a sub-list holds the same objects in both pictures, they
 * are paired in order; if something came or went (or a shot turned to
 * its next shape), each with the nearest of the same size, of its own
 * shape before another, so that the others do not shift onto their
 * neighbours. Then the lines of paired objects by their number. Lines
 * without a partner, and partners too far apart (a jump), change over
 * halfway.
 */
#ifndef TP_SMOOTH_H
#define TP_SMOOTH_H

#include "avg/avg.h"

#include <stdint.h>

typedef struct tp_smooth {
    tp_avg_list cur;        /* the picture shown now (generator units) */
    tp_avg_list next;       /* the one expected next */
    uint64_t    t_cur;      /* CPU cycle the shown one came */
    uint32_t    span;       /* CPU cycles until the next one is expected; 0: none */
    int        *pair;       /* for each line of cur: its line in next, or -1 */
    uint8_t    *taken;      /* for each line of next: it has a partner */
    int         cap_pair, cap_taken;
} tp_smooth;

void tp_smooth_init(tp_smooth *s);
void tp_smooth_free(tp_smooth *s);

/* A new picture came at cycle t; next: the one expected `span` cycles
 * later (NULL or span 0: none known). Both are copied. */
void tp_smooth_set(tp_smooth *s, const tp_avg_list *cur, uint64_t t, const tp_avg_list *next, uint32_t span);

/* The picture at cycle now, into out (grown as needed). */
void tp_smooth_picture(tp_smooth *s, uint64_t now, tp_avg_list *out);

#endif

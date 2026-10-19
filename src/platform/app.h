/* app.h - the program's outer loop on the VectorPie SDK.
 *
 * vp_run is not used: it hands draw() no position between updates and
 * runs updates at a whole number of Hz, while this game needs 246.09 Hz
 * interrupts and pictures between its frames. This loop keeps vp_run's conventions
 * (output chosen the same way, --dvg/--window/--windowed/--vertical,
 * ui_cancel and Ctrl+C stop it, the marquee on the HDMI screen while the
 * board draws, VP_STATS=1) and adds what the game needs: a real-time clock
 * for the game and frame pacing at any rate.
 */
#ifndef TP_APP_H
#define TP_APP_H

#include <stdint.h>

#include <vectorpie/vp.h>

typedef struct tp_app {
    const char *title;
    vp_dvg     *dvg;
    vp_input   *in;
    int         on_board;     /* drawing on the vector board */
    double      frame_hz;     /* frames sent per second */
    int         quit;
    int         hold_cancel;  /* 1: ui_cancel does not stop the program (it means something else just now) */
    int         stats;        /* VP_STATS=1 */

    /* internal */
    uint64_t    perf_freq;
    uint64_t    last_clock;
    double      next_frame;   /* seconds, on the app clock */
    double      clock;        /* seconds since open */
    double      last_poll;    /* the clock at the last tp_app_poll */
    uint64_t    frame_t0;
    double      stat_since;
    int         stat_frames;
    double      stat_sum[5], stat_peak[5];
} tp_app;

/* Options understood (others are left to the program): --dvg [PORT],
 * --window, --windowed, --vertical, --hz N (frame rate; default 60 in a
 * window, board_hz on the board: the board redraws its last frame at its
 * own refresh rate, so this has to be that rate, or frames are shown
 * twice and skipped by turns). Returns 0, or -1 with a message printed. */
int  tp_app_open(tp_app *a, const char *title, int argc, char **argv, double board_hz);
void tp_app_close(tp_app *a);

/* Read the controls; false when the program should stop. Returns the real
 * seconds elapsed since the previous call in *dt. */
int  tp_app_poll(tp_app *a, double *dt);

void tp_app_begin_frame(tp_app *a);
/* Send the frame, then sleep until the next one is due. -1 if the board
 * failed (the program should stop). */
int  tp_app_end_frame(tp_app *a);

/* The value of an option "--name VALUE" in argv, or NULL. */
const char *tp_arg(int argc, char **argv, const char *name);
int         tp_flag(int argc, char **argv, const char *name);

#endif

#include "render/nextlevel.h"

#include "render/frames.h"
#include "render/layout.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* RAM of the program (revision 3) */
#define RAM_QSTATE   0x0000
#define ST_PLAY      0x04
#define ST_DOWN      0x20       /* the claw flies down the tube at a level's end */
#define ST_BLANK     0x0e       /* (one picture between the two flights) */
#define ST_INTO      0x18       /* the next level's tube comes out of the depth */
#define TUBE_SUB     0x240a     /* the tube's sub-list */

#define SLICE_CYCLES 200000     /* run ahead a picture: an eighth of a second of the game */
#define MAX_AHEAD    (5 * 1512000)  /* given up after this much of the game */
#define RAM_QSTATUS  0x0005
#define ST_GAME      0x80       /* D7 of QSTATUS: a game (clear: the attract mode) */

enum { NL_IDLE, NL_RUNNING, NL_READY, NL_DONE };

void tp_nextlevel_init(tp_nextlevel *n)
{
    memset(n, 0, sizeof *n);
}

void tp_nextlevel_free(tp_nextlevel *n)
{
    free(n->copy);
    free(n->copy_emu);
    tp_avg_list_free(&n->pic);
    memset(n, 0, sizeof *n);
}

static int start(tp_nextlevel *n, const tp_machine *m, const tp_emu *e)
{
    if (!n->copy) {
        n->copy = (tp_machine *)malloc(sizeof *n->copy);
        n->copy_emu = (tp_emu *)malloc(sizeof *n->copy_emu);
        if (!n->copy || !n->copy_emu) {
            free(n->copy); free(n->copy_emu);
            n->copy = NULL; n->copy_emu = NULL;
            return -1;
        }
    }
    /* (as render/frames.c copies the machine) */
    memcpy(n->copy, m, sizeof *n->copy);
    *n->copy_emu = *e;
    n->copy_emu->m = n->copy;
    n->copy_emu->cpu.user = n->copy_emu;
    n->copy->read_hook = NULL;
    n->copy->sound_write = NULL;
    n->copy->vg.trace = NULL;
    n->started = n->copy->cycle;
    return 0;
}

/* The width of a picture's tube (generator units), 0 without one. */
static float tube_width(const tp_avg_list *pic)
{
    float x0 = 1e9f, x1 = -1e9f;
    for (int i = 0; i < pic->count; i++) {
        const tp_line *q = &pic->lines[i];
        if (q->sub != TUBE_SUB) continue;
        x0 = fminf(x0, fminf(q->x0, q->x1));
        x1 = fmaxf(x1, fmaxf(q->x0, q->x1));
    }
    return x1 > x0 ? x1 - x0 : 0.0f;
}

/* The copy a slice further; NL_READY once the next level is played. */
static int run(tp_nextlevel *n)
{
    tp_machine *c = n->copy;
    uint64_t end = c->cycle + SLICE_CYCLES;
    while (c->cycle < end && !n->copy_emu->cpu.jammed) {
        tp_emu_run(n->copy_emu, 20000);
        if (c->mem[RAM_QSTATE] == ST_PLAY && tp_frames_target(c->mem)) {
            tp_avg_run(c->mem, c->colram, TP_VGRAM, c->coinout & 0x08, c->coinout & 0x10, &n->pic, 0);
            return NL_READY;
        }
    }
    if (n->copy_emu->cpu.jammed || c->cycle - n->started > MAX_AHEAD) return NL_DONE;     /* (given up) */
    return NL_RUNNING;
}

void tp_nextlevel_update(tp_nextlevel *n, const tp_machine *m, const tp_emu *e, const tp_avg_list *shown)
{
    uint8_t st = m->mem[RAM_QSTATE];
    int flying = st == ST_DOWN || st == ST_BLANK || st == ST_INTO;
    tp_layout_set_flight(flying);
    if (!flying) {
        n->stage = NL_IDLE;
        return;
    }
    /* (the attract mode's demo ends after its flight: nothing to run ahead to) */
    if (n->stage == NL_IDLE)
        n->stage = (m->mem[RAM_QSTATUS] & ST_GAME) && start(n, m, e) == 0 ? NL_RUNNING : NL_DONE;
    if (n->stage == NL_RUNNING) n->stage = run(n);
    /* the new tube shows: from now on the next level's scale and place (not
     * while the picture is still the old level's: for a few frames after
     * the flight down the game draws it on, its tube all round the screen,
     * and laid out in the new level's place it, its spikes among it, would
     * jump) */
    if (n->stage == NL_READY && st != ST_DOWN && tube_width(shown) <= tube_width(&n->pic) * 1.05f) {
        tp_layout_aim(n->pic.lines, n->pic.count);
        n->stage = NL_DONE;
    }
}

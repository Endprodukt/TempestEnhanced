#include "render/frames.h"

#include <stdlib.h>
#include <string.h>

#define AHEAD_MAX_CYCLES 400000     /* a picture comes every 60000 or so */

void tp_frames_init(tp_frames *f)
{
    memset(f, 0, sizeof *f);
    tp_smooth_init(&f->sm);
}

void tp_frames_free(tp_frames *f)
{
    tp_avg_list_free(&f->pic);
    tp_avg_list_free(&f->ahead);
    tp_smooth_free(&f->sm);
    free(f->copy);
    free(f->copy_emu);
    memset(f, 0, sizeof *f);
}

uint16_t tp_frames_target(const uint8_t *mem)
{
    unsigned w = mem[0x2000] | (mem[0x2001] << 8);
    if ((w >> 13) != 7) return 0;                       /* not a JMPL */
    unsigned t = 0x2000 + ((w & 0x1fff) << 1);
    unsigned first = mem[t & 0xffff] | (mem[(t + 1) & 0xffff] << 8);
    if ((first >> 13) == 1) return 0;                   /* HALT: the buffers are being swapped */
    return (uint16_t)t;
}

static void picture_of(const tp_machine *m, tp_avg_list *out)
{
    tp_avg_run(m->mem, m->colram, TP_VGRAM, m->coinout & 0x08, m->coinout & 0x10, out, 0);
}

int tp_frames_check(tp_frames *f, tp_machine *m, tp_emu *e, int smooth)
{
    if (e->frames == f->frame) return 0;
    f->frame = e->frames;
    if (!tp_frames_target(m->mem)) return 0;
    /* an empty picture (the game shows one as it changes from the flight
     * down the tube to the next level's): the last stays, the screen does
     * not go dark for it */
    static tp_avg_list fresh;
    picture_of(m, &fresh);
    if (!fresh.count) return 0;
    tp_avg_list keep = f->pic;
    f->pic = fresh;
    fresh = keep;
    f->t = m->cycle;
    f->pictures++;
    if (!smooth) {
        tp_smooth_set(&f->sm, &f->pic, f->t, NULL, 0);
        return 1;
    }

    /* a copy runs on to the next picture */
    if (!f->copy) {
        f->copy = (tp_machine *)malloc(sizeof *f->copy);
        f->copy_emu = (tp_emu *)malloc(sizeof *f->copy_emu);
        if (!f->copy || !f->copy_emu) {
            free(f->copy); free(f->copy_emu);
            f->copy = NULL; f->copy_emu = NULL;
            tp_smooth_set(&f->sm, &f->pic, f->t, NULL, 0);
            return 1;
        }
    }
    tp_machine *c = f->copy;
    tp_emu *ce = f->copy_emu;
    memcpy(c, m, sizeof *c);
    *ce = *e;
    ce->m = c;
    ce->cpu.user = ce;
    c->read_hook = NULL;
    c->sound_write = NULL;
    c->vg.trace = NULL;
    uint64_t start = c->cycle;
    uint32_t span = 0, frame = ce->frames;
    while (c->cycle - start < AHEAD_MAX_CYCLES && !ce->cpu.jammed) {
        tp_emu_step(ce);
        if (ce->frames != frame) {
            span = (uint32_t)(c->cycle - start);
            break;
        }
    }
    if (span && tp_frames_target(c->mem)) {
        picture_of(c, &f->ahead);
        tp_smooth_set(&f->sm, &f->pic, f->t, &f->ahead, span);
    } else {
        tp_smooth_set(&f->sm, &f->pic, f->t, NULL, 0);
    }
    return 1;
}

void tp_frames_picture(tp_frames *f, uint64_t now, int smooth, tp_avg_list *out)
{
    if (!smooth) {
        /* (copy, so that out never aliases the picture kept here) */
        tp_smooth s0;
        memset(&s0, 0, sizeof s0);
        s0.cur = f->pic;
        tp_smooth_picture(&s0, now, out);
        return;
    }
    tp_smooth_picture(&f->sm, now, out);
}

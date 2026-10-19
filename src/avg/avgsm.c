#include "avg/avgsm.h"

#include <string.h>

#define VGSLICE 10000           /* MAME: master cycles a batch runs at least */

#define OP0(v) ((v)->op & 1)
#define OP1(v) (((v)->op >> 1) & 1)
#define OP2(v) (((v)->op >> 2) & 1)
#define ST3(v) (((v)->state_latch >> 3) & 1)

void tp_avgsm_init(tp_avgsm *v, const uint8_t *prom)
{
    memset(v, 0, sizeof *v);
    if (prom) {
        memcpy(v->prom, prom, 256);
        v->have_prom = 1;
    }
    v->run_at = TP_AVGSM_NEVER;
    v->halt_at = TP_AVGSM_NEVER;
}

static uint8_t state_addr(const tp_avgsm *v)
{
    return (uint8_t)((((v->state_latch >> 4) ^ 1) << 7) | (v->op << 4) | (v->state_latch & 0xf));
}

/* The strobes. Only what decides the timing and the flow is kept; the
 * beam's position is the renderer's (avg/avg.h). */
static int latch0(tp_avgsm *v) { v->dvy = (uint16_t)((v->dvy & 0x1f00) | v->data); v->pc++; return 0; }

static int latch1(tp_avgsm *v)
{
    v->dvy12 = (v->data >> 4) & 1;
    v->op = v->data >> 5;
    v->int_latch = 0;
    v->dvy = (uint16_t)((v->dvy12 << 12) | ((v->data & 0xf) << 8));
    v->dvx = 0;
    v->pc++;
    return 0;
}

static int latch2(tp_avgsm *v) { v->dvx = (uint16_t)((v->dvx & 0x1f00) | v->data); v->pc++; return 0; }

static int latch3(tp_avgsm *v)
{
    v->int_latch = v->data >> 4;
    v->dvx = (uint16_t)(((v->int_latch & 1) << 12) | ((v->data & 0xf) << 8) | (v->dvx & 0xff));
    v->pc++;
    return 0;
}

static int strobe0(tp_avgsm *v)
{
    if (OP0(v)) {
        v->stack[v->sp & 3] = v->pc;
    } else {
        int i = 0;
        while ((((v->dvy ^ (v->dvy << 1)) & 0x1000) == 0) && (((v->dvx ^ (v->dvx << 1)) & 0x1000) == 0) && (i++ < 16)) {
            v->dvy = (uint16_t)((v->dvy & 0x1000) | ((v->dvy << 1) & 0x1fff));
            v->dvx = (uint16_t)((v->dvx & 0x1000) | ((v->dvx << 1) & 0x1fff));
            v->timer >>= 1;
            v->timer |= (uint16_t)(0x4000 | (OP1(v) << 7));
        }
        if (OP1(v)) v->timer &= 0xff;
    }
    return 0;
}

static int strobe1(tp_avgsm *v)
{
    if (!OP2(v)) {
        for (int i = v->bin_scale; i > 0; i--) {
            v->timer >>= 1;
            v->timer |= (uint16_t)(0x4000 | (OP1(v) << 7));
        }
        if (OP1(v)) v->timer &= 0xff;
    }
    if (OP2(v)) {
        if (OP1(v)) v->sp = (uint16_t)((v->sp - 1) & 0xf);
        else        v->sp = (uint16_t)((v->sp + 1) & 0xf);
    }
    return 0;
}

static int strobe2(tp_avgsm *v)
{
    /* Tempest: STAT sets the colour with bit 11, else the intensity */
    if (!OP2(v) && !v->dvy12) {
        if (v->dvy & 0x800) v->color = (uint8_t)(v->dvy & 0xf);
        else v->intensity = (uint8_t)((v->dvy >> 4) & 0xf);
    }
    if (OP2(v)) {
        if (OP0(v)) v->pc = (uint16_t)(v->dvy << 1);
        else v->pc = v->stack[v->sp & 3];
    } else if (v->dvy12) {
        v->scale = (uint8_t)(v->dvy & 0xff);
        v->bin_scale = (uint8_t)((v->dvy >> 8) & 7);
    }
    return 0;
}

static int strobe3(tp_avgsm *v)
{
    int cycles = 0;
    v->halt = OP0(v);
    if (!OP0(v) && !OP2(v)) {
        cycles = OP1(v) ? 0x100 - (v->timer & 0xff) : 0x8000 - v->timer;
        v->timer = 0;
    }
    if (OP2(v)) {
        cycles = 0x8000 - v->timer;
        v->timer = 0;
    }
    return cycles;
}

/* One batch at master time t (MAME: run_state_machine). */
static void run_batch(tp_avgsm *v, const uint8_t *mem, uint64_t t)
{
    int cycles = 0;
    v->batches++;
    while (cycles < VGSLICE) {
        v->state_latch = (uint8_t)((v->state_latch & 0x10) | (v->prom[state_addr(v)] & 0xf));
        if (ST3(v)) {
            v->data = mem[(0x2000 + (v->pc ^ 1)) & 0xffff];
            switch (v->state_latch & 7) {
            case 0: cycles += latch0(v); break;
            case 1: cycles += latch1(v); break;
            case 2: cycles += latch2(v); break;
            case 3: cycles += latch3(v); break;
            case 4: cycles += strobe0(v); break;
            case 5: cycles += strobe1(v); break;
            case 6: cycles += strobe2(v); break;
            case 7: cycles += strobe3(v); break;
            }
        }
        /* a new halt: visible after the cycles so far (the CPU catches up first) */
        if (v->halt && !(v->state_latch & 0x10)) {
            v->halt_at = t + (uint64_t)cycles;
            if (v->trace) fprintf(v->trace, "    vg batch at %llu: HALT reached at pc %04X, visible at %llu\n",
                                  (unsigned long long)t, 0x2000 + v->pc, (unsigned long long)v->halt_at);
        }
        v->state_latch = (uint8_t)((v->halt << 4) | (v->state_latch & 0xf));
        cycles += 8;
    }
    v->run_at = t + (uint64_t)cycles;
}

void tp_avgsm_advance(tp_avgsm *v, const uint8_t *mem, uint64_t t)
{
    for (;;) {
        uint64_t next = v->run_at < v->halt_at ? v->run_at : v->halt_at;
        if (next == TP_AVGSM_NEVER || next > t) return;
        if (v->halt_at <= v->run_at) {
            v->halt = 1;
            v->sync_halt = 1;
            if (v->trace) fprintf(v->trace, "    vg halted at %llu\n", (unsigned long long)v->halt_at);
            v->halt_at = TP_AVGSM_NEVER;
        } else {
            run_batch(v, mem, v->run_at);
        }
    }
}

void tp_avgsm_go(tp_avgsm *v, const uint8_t *mem, uint64_t t)
{
    tp_avgsm_advance(v, mem, t);
    v->pc = 0;
    v->sp = 0;
    v->halt = 0;
    v->sync_halt = 0;
    if (v->trace) fprintf(v->trace, "    vg go at %llu (2000: %02X%02X)\n", (unsigned long long)t, mem[0x2001], mem[0x2000]);
    /* the batch runs at once (MAME: the run timer set to now) */
    run_batch(v, mem, t);
}

void tp_avgsm_reset(tp_avgsm *v)
{
    if (v->trace) fprintf(v->trace, "    vg reset\n");
    v->state_latch = 0;
    v->bin_scale = 0;
    v->scale = 0;
    v->color = 0;
    v->halt = 1;
    v->sync_halt = 1;
}

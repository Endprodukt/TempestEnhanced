/* avgsm.h - the vector generator's state machine, clock by clock, for its
 * timing: when it halts (IN0 bit 6), so that the program sees the bit
 * exactly when it would.
 *
 * The board's state machine is a 256x4 PROM (136002-125) behind a latch,
 * clocked at 1.512 MHz; three bits of the state strobe the parts of the
 * generator. This follows MAME's avg_device and avg_tempest_device
 * (src/devices/video/avgdvg.cpp, BSD-3-Clause, Mathis Rosenhauer) down to
 * how MAME runs it: in batches of at least 10000 master clock cycles,
 * each reading vector RAM as it is at the batch's start, the next batch
 * due when the time the batch took has passed. The halt becomes visible
 * after the cycles it took within its batch (MAME: vg_halt_timer).
 *
 * Times are in master clock cycles (12.096 MHz, eight per CPU cycle).
 */
#ifndef TP_AVGSM_H
#define TP_AVGSM_H

#include <stdint.h>
#include <stdio.h>

#define TP_AVGSM_NEVER (~(uint64_t)0)

typedef struct tp_avgsm {
    uint8_t  prom[256];
    int      have_prom;
    uint8_t  state_latch, op, data, int_latch, dvy12, halt, sync_halt;
    uint16_t pc, sp, stack[4], dvx, dvy, timer;
    uint8_t  scale, bin_scale, color, intensity;
    uint64_t run_at;        /* next batch; TP_AVGSM_NEVER: none due */
    uint64_t halt_at;       /* the pending halt becomes visible; TP_AVGSM_NEVER: none */
    uint32_t batches;
    FILE    *trace;         /* not NULL: every batch and halt is noted here */
} tp_avgsm;

void tp_avgsm_init(tp_avgsm *v, const uint8_t *prom);

/* Bring the generator up to master time `t`: run every batch and fire
 * every halt due at or before it. mem: the address space (it reads
 * 2000-3FFF). */
void tp_avgsm_advance(tp_avgsm *v, const uint8_t *mem, uint64_t t);

void tp_avgsm_go(tp_avgsm *v, const uint8_t *mem, uint64_t t);    /* a write to 4800 at time t */
void tp_avgsm_reset(tp_avgsm *v);                                   /* a write to 5800 */

static inline int tp_avgsm_halted(const tp_avgsm *v) { return v->sync_halt; }

#endif

/* emu.h - the original program on the board.
 *
 * The 6502 (cpu/m6502.h) runs the game from the owner's ROMs against
 * tp_machine. The interrupt line is raised every 6144 CPU cycles (246.09
 * a second, MAME: CLOCK_3KHZ / 12) and stays up until the program writes
 * to 5000. The game's main loop draws a picture each time the interrupt
 * has counted its frame timer to 9 or more.
 */
#ifndef TP_EMU_H
#define TP_EMU_H

#include "core/machine.h"
#include "cpu/m6502.h"

typedef struct tp_emu {
    tp_machine *m;
    m6502       cpu;
    uint64_t    next_irq;   /* cycle the interrupt line is raised next */
    uint64_t    irqs;       /* interrupts taken */
    uint32_t    frames;     /* frames the main loop started (it passed TP_MAIN_STA) */

    /* Steady frames (tp_emu_steady): the game's frames on a fixed beat,
     * the cabinet's average one, without its stumbles. */
    int         steady;
    uint32_t    speed;      /* the CPU runs this many times as fast meanwhile */
    uint32_t    frac;       /* its cycles not yet a whole cycle of time */
    uint64_t    release_at; /* the next frame may start from this cycle on */
    double      period;     /* the beat: CPU cycles from frame to frame */
    int         in_irq;     /* inside the interrupt routine */
    uint64_t    work, irq_work;     /* the frame's main line and interrupt cycles, at the cabinet's speed */
    uint64_t    irqs_at_frame;
} tp_emu;

/* The machine must be initialized and have its ROMs. Resets the CPU. */
void tp_emu_init(tp_emu *e, tp_machine *m);

/* One instruction (or the taking of the interrupt). Returns its cycles. */
int tp_emu_step(tp_emu *e);

/* 1 if the next step takes the interrupt rather than an instruction. */
int tp_emu_irq_due(const tp_emu *e);

/* Run until `cycles` more CPU cycles have passed. */
void tp_emu_run(tp_emu *e, uint64_t cycles);

/* Run until the main loop starts its next frame (or max_cycles pass).
 * Returns the cycles it took, -1 if no frame came. */
int64_t tp_emu_frame(tp_emu *e, uint64_t max_cycles);

/* Steady frames on or off (off: the cabinet's timing, exactly).
 *
 * On the cabinet a frame starts when the interrupt has counted the frame
 * timer to 9 and the frame before is done: after 9 interrupts mostly, 10
 * to 12 when there is much to draw, so the pace stumbles. Steady, the CPU
 * runs four times as fast (a frame is always done in time), and the main
 * loop's wait for the frame timer is held until the beat says go. The
 * beat is what the cabinet would take: each frame's work at the
 * cabinet's speed tells how many interrupts it would have needed (9, or
 * as many as the work fills), averaged over about two seconds. The
 * interrupt, the sound and the coins keep real time. */
void tp_emu_steady(tp_emu *e, int on);

#endif

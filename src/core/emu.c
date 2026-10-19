#include "core/emu.h"

#include <math.h>
#include <string.h>

/* The cycle of the access: an instruction reads or writes its operand in
 * its last cycle. */
static void access_time(tp_emu *e)
{
    uint8_t n = e->cpu.op_cycles;
    e->m->now = e->m->cycle + (n ? (uint64_t)(n - 1) : 0);
}

static uint8_t bus_read(void *user, uint16_t a)
{
    tp_emu *e = (tp_emu *)user;
    access_time(e);
    return tp_rd8(e->m, a);
}

static void bus_write(void *user, uint16_t a, uint8_t v)
{
    tp_emu *e = (tp_emu *)user;
    access_time(e);
    tp_wr8(e->m, a, v);
}

void tp_emu_init(tp_emu *e, tp_machine *m)
{
    memset(e, 0, sizeof *e);
    e->m = m;
    m6502_init(&e->cpu, bus_read, bus_write, e);
    m6502_reset(&e->cpu);
    m->cycle = 0;
    e->next_irq = TP_CYCLES_PER_IRQ;
    e->speed = 1;
}

#define STEADY_SPEED   4
#define WAIT_LO        0xc7a7       /* the main loop's wait for the frame timer: LDA, CMP #9, BCC */
#define WAIT_HI        0xc7ab
#define FRAME_TIMER_GO 9
#define AVG_FRAMES     50.0         /* about two seconds */
/* The interrupts a frame needs, guessed from its work as below, come out
 * 1.05 % more on average than the cabinet's (measured over four minutes of
 * play: 9.263 against 9.167): taken out. */
#define GUESS_FIX      (9.1665 / 9.2629)

void tp_emu_steady(tp_emu *e, int on)
{
    e->steady = on;
    e->speed = on ? STEADY_SPEED : 1;
    e->frac = 0;
    if (e->period <= 0.0) e->period = FRAME_TIMER_GO * TP_CYCLES_PER_IRQ;
    e->release_at = e->m->cycle;
}

/* A frame starts: the interrupts the one before would have needed on the
 * cabinet, into the beat, and the next start planned. */
static void steady_frame(tp_emu *e)
{
    uint64_t irqs = e->irqs - e->irqs_at_frame;
    if (irqs) {
        double avail = TP_CYCLES_PER_IRQ - (double)e->irq_work / (double)irqs;
        double need = ceil((double)e->work / (avail > 1000.0 ? avail : 1000.0));
        if (need < FRAME_TIMER_GO) need = FRAME_TIMER_GO;
        e->period += (need * GUESS_FIX * TP_CYCLES_PER_IRQ - e->period) / AVG_FRAMES;
    }
    e->release_at += (uint64_t)e->period;
    if (e->release_at < e->m->cycle) e->release_at = e->m->cycle + (uint64_t)(e->period * 0.5);
    e->work = e->irq_work = 0;
    e->irqs_at_frame = e->irqs;
}

int tp_emu_step(tp_emu *e)
{
    tp_machine *m = e->m;
    if (m->cycle >= e->next_irq) {
        m->irq = 1;
        e->next_irq += TP_CYCLES_PER_IRQ;
    }
    e->cpu.irq = m->irq;
    uint16_t pc = e->cpu.pc;
    int took_irq = m->irq && !(e->cpu.p & M6502_I);
    /* a frame starts when the main loop's store into the frame timer runs
     * (an interrupt that comes just before it runs first, and belongs to
     * the frame before, as on the cabinet) */
    if (pc == TP_MAIN_STA && !took_irq) {
        e->frames++;
        if (e->steady) steady_frame(e);
    }
    /* steady: the wait goes on until the beat, and ends on it (the frame
     * timer counts only at interrupts: left alone, the frame would start at
     * the next one) */
    if (e->steady && pc >= WAIT_LO && pc <= WAIT_HI) {
        if (m->cycle < e->release_at) {
            if (m->mem[TP_FRTIMR] >= FRAME_TIMER_GO) m->mem[TP_FRTIMR] = FRAME_TIMER_GO - 1;
        } else if (m->mem[TP_FRTIMR] < FRAME_TIMER_GO) {
            m->mem[TP_FRTIMR] = FRAME_TIMER_GO;
        }
    }
    int rti = e->steady && m->mem[pc] == 0x40;
    uint64_t before = e->cpu.cycles;
    int cy = m6502_step(&e->cpu);
    if (took_irq && e->cpu.pc == TP_IRQ_ENTRY) {
        e->irqs++;
        e->in_irq = 1;
    }
    uint32_t done = (uint32_t)(e->cpu.cycles - before);
    if (!e->steady) {
        m->cycle += done;
        return cy;
    }
    if (e->in_irq) e->irq_work += done;
    else if (!(pc >= WAIT_LO && pc <= WAIT_HI)) e->work += done;
    if (rti && !took_irq) e->in_irq = 0;
    e->frac += done;
    m->cycle += e->frac / e->speed;
    e->frac %= e->speed;
    return cy;
}

int tp_emu_irq_due(const tp_emu *e)
{
    int line = e->m->irq || e->m->cycle >= e->next_irq;
    return line && !(e->cpu.p & M6502_I);
}

void tp_emu_run(tp_emu *e, uint64_t cycles)
{
    uint64_t end = e->m->cycle + cycles;
    while (e->m->cycle < end) tp_emu_step(e);
}

int64_t tp_emu_frame(tp_emu *e, uint64_t max_cycles)
{
    uint64_t start = e->m->cycle;
    uint32_t f = e->frames;
    while (e->m->cycle - start < max_cycles) {
        tp_emu_step(e);
        if (e->frames != f) return (int64_t)(e->m->cycle - start);
    }
    return -1;
}

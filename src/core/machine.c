#include "core/machine.h"

#include "avg/avg.h"

#include <stdio.h>
#include <string.h>

void tp_machine_init(tp_machine *m)
{
    memset(m, 0, sizeof *m);
    m->in.in0 = 0x3f;       /* nothing closed (active low) */
    m->in.dsw1 = 0x00;      /* MAME's defaults: 1 coin 1 play */
    m->in.dsw2 = 0x00;      /* 1 credit minimum, English, bonus at 20000, 3 lives */
    m->in.diff = 3;         /* Medium1 */
    m->in.rating = 1;       /* levels 1, 3, 5, 7, 9 */
    tp_mathbox_reset(&m->mb);
    tp_earom_init(&m->earom);
    tp_pokeyio_init(&m->pk[0]);
    tp_pokeyio_init(&m->pk[1]);
    m->vg_halt_at = ~(uint64_t)0;  /* MAME: neither halted nor running until the program starts it */
    m->clk_phase = -1;             /* MAME's cycle count, whose bit 8 is the 3 kHz bit (tools/tplockstep) */
    tp_avgsm_init(&m->vg, NULL);
}

void tp_machine_set_avg_prom(tp_machine *m, const uint8_t *prom)
{
    tp_avgsm_init(&m->vg, prom);
}

static uint64_t vg_time(const tp_machine *m)
{
    return m->now * 8 + (uint64_t)(int64_t)m->vg_phase;
}

void tp_machine_set_rom(tp_machine *m, const uint8_t *vgrom3000, const uint8_t *prog9000)
{
    memcpy(m->mem + TP_VGROM, vgrom3000, 0x1000);
    memcpy(m->mem + TP_ROM_LO, prog9000, 0x5000);
    memcpy(m->mem + TP_ROM_MIRROR, prog9000 + 0x4000, 0x1000);
    m->have_rom = 1;
}

/* The switches as the pots see them. A pot reads "done" at once (0) when
 * its port bit is set, and counts to 228 when it is clear (MAME:
 * input_port_N_bit_r); pokeyio takes a bit per pot that counts. */
void tp_machine_update_pots(tp_machine *m)
{
    const tp_inputs *in = &m->in;
    int p = (m->leds & 0x04) ? 1 : 0;      /* the player the cocktail's flip selects */
    uint8_t in1 = (uint8_t)((in->spinner[p] & 0x0f) | (in->cocktail ? 0 : 0x10) | 0xe0);
    uint8_t in2 = (uint8_t)((in->diff & 3) | (in->rating ? 0x04 : 0) |
                            (in->zap[p] ? 0 : 0x08) | (in->fire[p] ? 0 : 0x10) |
                            (in->start1 ? 0 : 0x20) | (in->start2 ? 0 : 0x40) | 0x80);
    m->pk[0].inputs = (uint8_t)~in1;
    m->pk[1].inputs = (uint8_t)~in2;
}

/* Start the vector generator: it halts after its list, or goes round
 * it until it passes 2000 while 2000 leads to a HALT. */
static void vg_go(tp_machine *m)
{
    int halts = 0;
    uint32_t c = tp_avg_cycles(m->mem, TP_VGRAM, &halts) / 8;
    m->vg_go_at = m->cycle;
    m->vg_round = 0;
    if (halts) {
        m->vg_halt_at = m->cycle + c;
    } else {
        m->vg_halt_at = ~(uint64_t)0;
        m->vg_round = c ? c : 1;
    }
}

static int vg_halted(tp_machine *m)
{
    if (m->vg.have_prom) {
        tp_avgsm_advance(&m->vg, m->mem, vg_time(m));
        return tp_avgsm_halted(&m->vg);
    }
    if (m->cycle >= m->vg_halt_at) return 1;
    if (m->vg_round && m->cycle - m->vg_go_at >= m->vg_round) {
        /* it has come round to 2000 again: does it go on? */
        int halts = 0;
        uint32_t c = tp_avg_cycles(m->mem, TP_VGRAM, &halts) / 8;
        if (halts) {
            m->vg_halt_at = m->cycle + c;
            m->vg_round = 0;
            return c == 0;
        }
        m->vg_go_at = m->cycle;
        m->vg_round = c ? c : 1;
    }
    return 0;
}

static uint8_t io_read(tp_machine *m, uint16_t a)
{
    if (a >= TP_POKEY1 && a < TP_POKEY1 + 0x20) {
        int k = (a - TP_POKEY1) >> 4;
        return tp_pokeyio_read(&m->pk[k], a & 15, m->now);
    }
    switch (a) {
    case TP_IN0:
        return (uint8_t)((m->in.in0 & 0x3f) | (vg_halted(m) ? TP_IN0_HALT : 0) |
                         (((m->now + (uint64_t)(int64_t)m->clk_phase) & 0x100) ? TP_IN0_3KHZ : 0));
    case TP_DSW1:   return m->in.dsw1;
    case TP_DSW2:   return m->in.dsw2;
    case TP_EACTL:  return 0x00;                        /* math box status: always done */
    case TP_EAREAD: return tp_earom_read(&m->earom);
    case TP_MBLO:   return tp_mathbox_lo(&m->mb);
    case TP_MBHI:   return tp_mathbox_hi(&m->mb);
    default:        return 0x00;                        /* nothing there (MAME: unmapped reads 0) */
    }
}

static int is_io(uint16_t a)
{
    return (a >= 0x0800 && a < 0x2000) || (a >= 0x4000 && a < 0x9000);
}

uint8_t tp_rd8(tp_machine *m, uint16_t a)
{
    if (a <= TP_RAM_END) return m->mem[a];
    if (a >= 0x2000 && a < 0x4000) return m->mem[a];
    if (a >= TP_ROM_LO && a <= TP_ROM_HI) return m->mem[a];
    if (a >= TP_ROM_MIRROR) return m->mem[a];
    if (a >= 0xe000) return 0x00;                       /* E000-EFFF: nothing */
    if (!is_io(a)) return 0x00;
    if ((a >= TP_POKEY1 && a < TP_POKEY1 + 0x20)) tp_machine_update_pots(m);
    uint8_t v = io_read(m, a);
    if (m->read_hook) v = m->read_hook(m->read_user, a, v);
    return v;
}

void tp_wr8(tp_machine *m, uint16_t a, uint8_t v)
{
    if (a <= TP_RAM_END) { m->mem[a] = v; return; }
    if (a >= TP_VGRAM && a < TP_VGROM) {
        /* the generator's batches due by now read vector RAM as it was */
        if (m->vg.have_prom) tp_avgsm_advance(&m->vg, m->mem, vg_time(m));
        if (m->vg.trace && a < TP_VGRAM + 2)
            fprintf(m->vg.trace, "    cpu writes %04X=%02X at %llu (vg %llu)\n", a, v, (unsigned long long)m->now,
                    (unsigned long long)vg_time(m));
        m->mem[a] = v;
        return;
    }
    if (a >= TP_COLRAM && a < TP_COLRAM + 0x10) { m->colram[a & 15] = v; return; }
    if (a >= TP_EAWRITE && a < TP_EAWRITE + 0x40) { tp_earom_latch(&m->earom, (uint8_t)(a & 0x3f), v); return; }
    if (a >= TP_MBGO && a < TP_MBGO + 0x20) { tp_mathbox_go(&m->mb, a - TP_MBGO, v); return; }
    if (a >= TP_POKEY1 && a < TP_POKEY1 + 0x20) {
        int k = (a - TP_POKEY1) >> 4;
        if ((a & 15) == TP_PK_POTGO) tp_machine_update_pots(m);
        tp_pokeyio_write(&m->pk[k], a & 15, v, m->now);
        if (m->sound_write) m->sound_write(m->sound_user, a, v);
        return;
    }
    switch (a) {
    case TP_COINOUT: m->coinout = v; return;
    case TP_VGGO:
        m->vg_starts++;
        if (m->vg.have_prom) tp_avgsm_go(&m->vg, m->mem, vg_time(m));
        else vg_go(m);
        return;
    case TP_WDCLR:  m->irq = 0; m->watchdog++; return;
    case TP_VGRST:
        if (m->vg.have_prom) {
            tp_avgsm_advance(&m->vg, m->mem, vg_time(m));
            tp_avgsm_reset(&m->vg);
        }
        m->vg_halt_at = m->cycle;
        m->vg_round = 0;
        return;
    case TP_EACTL:  tp_earom_control(&m->earom, v); return;
    case TP_LEDS:   m->leds = v; return;
    default: return;
    }
}

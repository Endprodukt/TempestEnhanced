#include "sound/pokey.h"

#include <math.h>
#include <string.h>

/* The polynomial counters, one bit per clock tick, made once. */
static uint8_t poly4[15], poly5[31], poly9[511], poly17[131071];
static int polys_made;

static void make_poly(uint8_t *out, int len, int bits, int tap_a, int tap_b)
{
    uint32_t r = (1u << bits) - 1;
    for (int i = 0; i < len; i++) {
        out[i] = (uint8_t)(r & 1);
        uint32_t fb = ((r >> tap_a) ^ (r >> tap_b)) & 1;
        r = (r >> 1) | (fb << (bits - 1));
    }
}

static void make_polys(void)
{
    make_poly(poly4, 15, 4, 0, 1);
    make_poly(poly5, 31, 5, 0, 2);
    make_poly(poly9, 511, 9, 0, 4);
    make_poly(poly17, 131071, 17, 0, 5);
    polys_made = 1;
}

static float v0_tab[65536];
static void make_rc(void);
static int rc_made;

void pokey_reset(pokey *p)
{
    if (!polys_made) make_polys();
    if (!rc_made) make_rc();
    memset(p, 0, sizeof *p);
    for (int i = 0; i < 4; i++) p->counter[i] = 1;
    for (int g = 0; g < 3; g++) p->filt[g] = v0_tab[0];     /* (silent: the pin at its pull-up's level) */
}

/* Clock ticks from one run-out of channel ch's divider to the next. */
static uint32_t period(const pokey *p, int ch)
{
    uint32_t base = (p->audctl & 0x01) ? 114 : 28;     /* 15 kHz or 64 kHz from 1.79 MHz */
    switch (ch) {
    case 0:
        return (p->audctl & 0x40) ? (uint32_t)p->audf[0] + 4 : ((uint32_t)p->audf[0] + 1) * base;
    case 1:
        if (p->audctl & 0x10) {                         /* 1 and 2 joined: 16 bits */
            uint32_t n = ((uint32_t)p->audf[1] << 8) | p->audf[0];
            return (p->audctl & 0x40) ? n + 7 : (n + 1) * base;
        }
        return ((uint32_t)p->audf[1] + 1) * base;
    case 2:
        return (p->audctl & 0x20) ? (uint32_t)p->audf[2] + 4 : ((uint32_t)p->audf[2] + 1) * base;
    default:
        if (p->audctl & 0x08) {                         /* 3 and 4 joined */
            uint32_t n = ((uint32_t)p->audf[3] << 8) | p->audf[2];
            return (p->audctl & 0x20) ? n + 7 : (n + 1) * base;
        }
        return ((uint32_t)p->audf[3] + 1) * base;
    }
}

void pokey_write(pokey *p, unsigned reg, uint8_t value)
{
    reg &= 15;
    if (reg < 8) {
        if (reg & 1) p->audc[reg >> 1] = value;
        else p->audf[reg >> 1] = value;
    } else if (reg == 8) {
        p->audctl = value;
    } else if (reg == 9) {                              /* STIMER: the dividers start over */
        for (int i = 0; i < 4; i++) p->counter[i] = period(p, i);
    }
}

/* The channel's divider has run out at clock tick t. */
static void clock_channel(pokey *p, int ch, uint64_t t)
{
    uint8_t c = p->audc[ch];
    if (!(c & 0x80) && !poly5[t % 31]) return;          /* gated by the 5 bit counter */
    if (c & 0x20) p->out[ch] ^= 1;                      /* pure tone */
    else if (c & 0x40) p->out[ch] = poly4[t % 15];
    else p->out[ch] = (p->audctl & 0x80) ? poly9[t % 511] : poly17[t % 131071];
}

float pokey_run(pokey *p, unsigned ticks)
{
    if (ticks == 0) return 0.0f;
    uint32_t sum = 0;
    p->level[0] = p->level[1] = p->level[2] = p->level[3] = 0.0f;
    for (int ch = 0; ch < 4; ch++) {
        uint8_t c = p->audc[ch];
        uint32_t vol = (p->mute >> ch & 1) ? 0 : c & 0x0f;
        if (c & 0x10) {                                 /* volume only: the level itself */
            sum += vol * ticks;
            p->level[ch] = (float)vol;
            /* the divider keeps running, unheard */
            uint32_t per = period(p, ch), left = ticks;
            while (p->counter[ch] <= left) { left -= p->counter[ch]; p->counter[ch] = per; }
            p->counter[ch] -= left;
            continue;
        }
        if (vol == 0) {
            uint32_t per = period(p, ch), left = ticks;
            uint64_t t = p->ticks;
            while (p->counter[ch] <= left) {
                left -= p->counter[ch];
                t += p->counter[ch];
                p->counter[ch] = per;
                clock_channel(p, ch, t);
            }
            p->counter[ch] -= left;
            continue;
        }
        uint32_t per = period(p, ch), left = ticks, high = 0;
        uint64_t t = p->ticks;
        while (p->counter[ch] <= left) {
            if (p->out[ch]) high += p->counter[ch];
            left -= p->counter[ch];
            t += p->counter[ch];
            p->counter[ch] = per;
            clock_channel(p, ch, t);
        }
        if (p->out[ch]) high += left;
        p->counter[ch] -= left;
        sum += vol * high;
        p->level[ch] = (float)(vol * high) / (float)ticks;
    }
    p->ticks += ticks;
    return (float)sum / (float)ticks;
}

/* --- the output as it is (pokey_run_rc) ------------------------------------------------ */

#define PULL_UP   10000.0
#define CAP       0.015e-6
#define V_REF     5.0
#define CLOCK_HZ  1512000.0

static float mult_tab[65536];

/* MAME's vol_init and RC_LOWPASS, made into tables: for every set of four
 * channel volumes, the voltage the pin goes towards, and how far it gets
 * there in one clock tick. */
static void make_rc(void)
{
    static const double bits[4] = { 90000, 26500, 8050, 3400 };
    const double r_off = 8e6;
    double r_chan[16];
    for (int j = 0; j < 16; j++) {
        double g = 1.0 / 1e12;
        for (int i = 0; i < 4; i++) g += (j & (1 << i)) ? 1.0 / bits[i] : 1.0 / r_off;
        r_chan[j] = 1.0 / g;
    }
    for (int j = 0; j < 65536; j++) {
        double g = 0.0;
        for (int i = 0; i < 4; i++) g += 1.0 / r_chan[(j >> (i * 4)) & 0x0f];
        double r = 1.0 / g;
        v0_tab[j] = (float)(r / (r + PULL_UP) * V_REF);
        mult_tab[j] = (float)(1.0 - exp(-(r + PULL_UP) / (CAP * PULL_UP * r) / CLOCK_HZ));
    }
    rc_made = 1;
}

void pokey_run_rc(pokey *p, unsigned ticks, const uint8_t group[4], double v[3])
{
    if (!rc_made) make_rc();
    double sum[3] = { 0.0, 0.0, 0.0 };
    uint32_t per[4];
    for (int ch = 0; ch < 4; ch++) per[ch] = period(p, ch);
    for (unsigned k = 0; k < ticks; k++) {
        uint64_t t = p->ticks + k + 1;
        unsigned raw[3] = { 0, 0, 0 };
        for (int ch = 0; ch < 4; ch++) {
            if (--p->counter[ch] == 0) {
                p->counter[ch] = per[ch];
                clock_channel(p, ch, t);
            }
            uint8_t c = p->audc[ch];
            unsigned vol = (p->mute >> ch & 1) ? 0 : c & 0x0f;
            unsigned on = (c & 0x10) ? vol : (p->out[ch] ? vol : 0);
            raw[group[ch] % 3] |= on << (ch * 4);
        }
        for (int g = 0; g < 3; g++) {
            p->filt[g] += ((double)v0_tab[raw[g]] - p->filt[g]) * mult_tab[raw[g]];
            sum[g] += p->filt[g];
        }
    }
    p->ticks += ticks;
    for (int g = 0; g < 3; g++) v[g] = ticks ? (double)v0_tab[0] - sum[g] / ticks : 0.0;
}

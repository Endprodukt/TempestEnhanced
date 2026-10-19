#include "core/pokeyio.h"

#include "core/tp_map.h"

#include <string.h>

#define SK_RESET  0x03
#define SK_PADDLE 0x04
#define POLY9     0x80
#define DIV_15    114           /* clocks per step of a slow pot scan */

/* The 17 and 9 bit polynomial counters as MAME tabulates them. */
static uint32_t g_poly17[0x1ffff];
static uint32_t g_poly9[0x1ff];
static int g_ready;

static void tables(void)
{
    if (g_ready) return;
    uint32_t lfsr = 0x1ffff;
    for (uint32_t i = 0; i < 0x1ffff; i++) {
        uint32_t in8 = ((lfsr >> 8) ^ (lfsr >> 13)) & 1;
        uint32_t in = lfsr & 1;
        lfsr >>= 1;
        lfsr = (lfsr & 0xff7f) | (in8 << 7);
        lfsr = (in << 16) | lfsr;
        g_poly17[i] = lfsr;
    }
    lfsr = 0x1ff;
    for (uint32_t i = 0; i < 0x1ff; i++) {
        uint32_t in = (lfsr ^ (lfsr >> 5)) & 1;
        lfsr = (lfsr >> 1) | (in << 8);
        g_poly9[i] = lfsr;
    }
    g_ready = 1;
}

void tp_pokeyio_init(tp_pokeyio *p)
{
    tables();
    memset(p, 0, sizeof *p);
    for (int i = 0; i < 8; i++) p->potx[i] = 228;
    p->inputs = 0xff;
}

/* Which pots are done at `cycle`: those that read 0 at once, the others
 * when the scan's counter has passed their value (all at 228). */
static uint8_t pots_done(const tp_pokeyio *p, uint64_t cycle)
{
    if (!(p->skctl & SK_RESET)) return p->allpot;
    uint64_t el = cycle - p->potgo_at;
    uint64_t counter = (p->skctl & SK_PADDLE) ? el : el / DIV_15;
    if (counter > 228) counter = 228;
    uint8_t done = p->allpot;
    for (int i = 0; i < 8; i++)
        if (p->potx[i] < counter || counter == 228) done |= (uint8_t)(1u << i);
    return done;
}

void tp_pokeyio_write(tp_pokeyio *p, unsigned reg, uint8_t v, uint64_t cycle)
{
    switch (reg & 15) {
    case 0x08:
        p->audctl = v;
        break;
    case TP_PK_POTGO:
        if (!(p->skctl & SK_RESET)) break;
        p->potgo_at = cycle;
        p->allpot = 0;
        for (int i = 0; i < 8; i++) {
            p->potx[i] = (p->inputs >> i) & 1 ? 228 : 0;
            if (p->potx[i] == 0) p->allpot |= (uint8_t)(1u << i);
        }
        break;
    case TP_PK_SKCTL:
        if (v == p->skctl) break;
        if ((v & SK_RESET) && !(p->skctl & SK_RESET)) p->poly_base = cycle;
        p->skctl = v;
        break;
    default:
        break;
    }
}

uint8_t tp_pokeyio_read(tp_pokeyio *p, unsigned reg, uint64_t cycle)
{
    switch (reg & 15) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: {
        uint8_t done = pots_done(p, cycle);
        if (done & (1u << (reg & 7))) return p->potx[reg & 7];
        uint64_t el = cycle - p->potgo_at;
        uint64_t counter = (p->skctl & SK_PADDLE) ? el : el / DIV_15;
        return (uint8_t)(counter > 228 ? 228 : counter);
    }
    case TP_PK_ALLPOT:
        if (!(p->skctl & SK_RESET)) return p->allpot;
        return (uint8_t)(pots_done(p, cycle) ^ 0xff);
    case TP_PK_RANDOM: {
        if (!(p->skctl & SK_RESET)) return (uint8_t)(g_poly17[0] >> 8);
        uint64_t el = cycle - p->poly_base;
        if (p->audctl & POLY9) return (uint8_t)g_poly9[el % 0x1ff];
        return (uint8_t)(g_poly17[el % 0x1ffff] >> 8);
    }
    default:
        return 0;
    }
}

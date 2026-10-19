#include "sound/sound.h"

#include "core/tp_map.h"

#include <math.h>
#include <string.h>

#define TICKS_PER_SAMPLE (TP_CPU_HZ / TP_SOUND_RATE)
/* At volume 1 as loud as MAME's -wavwrite of the same run (tools/tpsoundcmp:
 * 1.6 was 6.33 dB louder) */
#define OUT_GAIN 0.772

void tp_sound_init(tp_sound *s)
{
    memset(s, 0, sizeof *s);
    pokey_reset(&s->pk[0]);
    pokey_reset(&s->pk[1]);
    s->volume = 1.0f;
}

void tp_sound_write(void *user, uint16_t addr, uint8_t value)
{
    tp_sound *s = (tp_sound *)user;
    if (addr >= TP_POKEY1 && addr < TP_POKEY1 + 0x20)
        pokey_write(&s->pk[(addr - TP_POKEY1) >> 4], addr & 15, value);
}

void tp_sound_run(tp_sound *s, int cpu_cycles)
{
    static const uint8_t one_group[4] = { 0, 0, 0, 0 };
    s->tick_frac += cpu_cycles;
    while (s->tick_frac >= TICKS_PER_SAMPLE) {
        unsigned ticks = (unsigned)TICKS_PER_SAMPLE;
        s->tick_frac -= ticks;
        /* (31.5 ticks a sample: every other sample takes the half tick) */
        if (s->tick_frac >= 1.0 && (s->ring_w & 1)) { ticks++; s->tick_frac -= 1.0; }
        double v1[3], v2[3];
        pokey_run_rc(&s->pk[0], ticks, one_group, v1);
        pokey_run_rc(&s->pk[1], ticks, one_group, v2);
        /* each chip pulls its pin down by up to ~3.5 V; half of each to the speaker */
        double x = 0.5 * (v1[0] + v2[0]) / 3.5;
        s->dc += (x - s->dc) * 0.0005;      /* about 4 Hz: the coupling capacitor */
        x = (x - s->dc) * OUT_GAIN * s->volume;
        if (x > 1.0) x = 1.0;
        if (x < -1.0) x = -1.0;
        if (fabs(x) > s->peak) s->peak = (float)fabs(x);
        uint32_t w = s->ring_w;
        if (w - s->ring_r < TP_SOUND_RING) {
            int16_t v = (int16_t)(x * 30000.0);
            s->ring[(w % TP_SOUND_RING) * 2] = v;
            s->ring[(w % TP_SOUND_RING) * 2 + 1] = v;
            s->ring_w = w + 1;
        }
    }
}

int tp_sound_read(tp_sound *s, int16_t *out, int n)
{
    uint32_t r = s->ring_r, have = s->ring_w - r;
    if ((uint32_t)n > have) n = (int)have;
    for (int i = 0; i < n; i++) {
        out[2 * i] = s->ring[((r + (uint32_t)i) % TP_SOUND_RING) * 2];
        out[2 * i + 1] = s->ring[((r + (uint32_t)i) % TP_SOUND_RING) * 2 + 1];
    }
    s->ring_r = r + (uint32_t)n;
    return n;
}

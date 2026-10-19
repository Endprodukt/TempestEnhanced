/* pokey.h - the sound of an Atari POKEY (C012294).
 *
 * Tempest has two (60C0 and 60D0). Only the tone generators
 * are here (AUDF1-4, AUDC1-4, AUDCTL with the 1.79 MHz clocks, the 15 kHz
 * base clock and the joined 16-bit channels, the 4, 5, 9 and 17 bit
 * polynomial counters). The high-pass filters, pots, keyboard, serial port
 * and timer interrupts are not.
 */
#ifndef TP_POKEY_H
#define TP_POKEY_H

#include <stdint.h>

typedef struct pokey {
    uint8_t  audf[4], audc[4], audctl;
    uint32_t counter[4];    /* clock ticks until the channel's divider runs out */
    uint8_t  out[4];        /* the channel's flip-flop */
    uint64_t ticks;         /* clock ticks since reset: where the polynomial counters are */
    uint8_t  mute;          /* a bit per channel: runs on, unheard (sound/samples.h) */
    float    level[4];      /* of the last pokey_run: each channel's part of what it returned */
    double   filt[3];       /* pokey_run_rc: the RC filter behind the output, for each group of channels */
} pokey;

void pokey_reset(pokey *p);
void pokey_write(pokey *p, unsigned reg, uint8_t value);

/* Advance by `ticks` cycles of the chip's clock (1.512 MHz here) and return
 * the output averaged over them, 0..60 (four channels of volume 0..15). */
float pokey_run(pokey *p, unsigned ticks);

/* The same, with the chip's output as it really is (after MAME's POKEY,
 * set_output_rc): each channel's output pulls the pin down through a
 * resistor network of its volume's bits (90k, 26.5k, 8.05k, 3.4k), a
 * pull-up of 10k holds it at 5 V, and a capacitor of 0.015 uF to ground
 * smooths it, with the time constant of the pull-up and the network side
 * by side. So it is not linear in the volume, and louder is brighter.
 * Worked out clock tick by clock tick. The channels are heard in groups
 * (group[ch]: 0..2, for where they are heard, sound/sound.h); for each
 * group, what its channels alone pull the pin down by, in volts, goes to
 * v[group] (0: silent). */
void pokey_run_rc(pokey *p, unsigned ticks, const uint8_t group[4], double v[3]);

#endif

/* sound.h - Tempest's sound: two POKEYs on one speaker.
 *
 * The program writes the POKEYs' registers (60C0-60CF, 60D0-60DF); the
 * machine's sound_write hook delivers them here. Each chip's output is
 * the RC network MAME gives it (set_output_rc: 10k, 0.015 uF, 5 V,
 * sound/pokey.h pokey_run_rc), both mixed at half each, as in MAME's
 * driver. tp_sound_run makes the samples of the CPU time that has passed
 * into a ring the sound device reads from another thread (platform/audio.h).
 * The cabinet has one speaker: both channels of the output carry it.
 */
#ifndef TP_SOUND_H
#define TP_SOUND_H

#include <stdint.h>

#include "sound/pokey.h"

#define TP_SOUND_RATE 48000
#define TP_SOUND_RING 16384     /* a third of a second, in pairs of left and right */

typedef struct tp_sound {
    pokey    pk[2];
    double   tick_frac;         /* POKEY clock ticks owed */
    double   dc;                /* the output's coupling capacitor (takes the DC away) */
    float    volume;            /* 0..1 */
    float    peak;              /* the loudest sample made (0..1) */

    int16_t  ring[TP_SOUND_RING * 2];
    volatile uint32_t ring_w, ring_r;
} tp_sound;

void tp_sound_init(tp_sound *s);
/* The machine's sound_write hook (user: the tp_sound). */
void tp_sound_write(void *user, uint16_t addr, uint8_t value);
/* Make the samples of `cpu_cycles` cycles at 1.512 MHz. */
void tp_sound_run(tp_sound *s, int cpu_cycles);
/* For the sound device: up to n pairs; returns how many there were. */
int  tp_sound_read(tp_sound *s, int16_t *out, int n);

#endif

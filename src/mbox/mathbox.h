/* mathbox.h - the Atari "math box" (Battlezone, Red Baron, Tempest): four 2901 bit slices run
 * by 256 words of micro-code (MBUCOD.V05, MBUDOC.DOC).
 *
 * The 6502 writes to one of 32 addresses (1860-187F); the mapping PROM
 * turns the address into the start of a micro-routine, which runs until it
 * stalls with a 16-bit result at the output (read at 1810 and 1818):
 *
 *   00-0A  load register halves (A, B, E, F, X, Y low)
 *   0B     Y high, then  X' = (X-E)*A - (Y-F)*B               (YSTRT1)
 *   0C     N, the number of quotient bits
 *   0D-10  Z, the 32-bit dividend
 *   11     Y high, then  (X*B + Y*A + F) / (X*A - Y*B + E)    (YSTRT2)
 *   12     Y' = (X-E)*B + (Y-F)*A                             (RSTRT1)
 *   13     Y'/X'      14  Z/X'                                (ZDIV)
 *   15-16  X'         17-19  show X', Y' low, Y' high
 *   1A-1B  Y'         1C  clip
 *   1D     distance between (A,B) and (E,F)                   (DSTNCE)
 *   1E     the same from |dx|, |dy| already in E, F           (DST2)
 *
 * This is the routine-level model of MAME (src/mame/atari/mathbox.cpp,
 * BSD-3-Clause, Eric Smith), the reference this port is checked against.
 * It finishes at once; the board took up to a few hundred microseconds,
 * which the program waits out (WAIT, or a few wasted instructions).
 */
#ifndef TP_MATHBOX_H
#define TP_MATHBOX_H

#include <stdint.h>

typedef struct tp_mathbox {
    int16_t reg[16];
    int16_t result;
} tp_mathbox;

void tp_mathbox_reset(tp_mathbox *mb);
void tp_mathbox_go(tp_mathbox *mb, unsigned offset, uint8_t data);

static inline uint8_t tp_mathbox_lo(const tp_mathbox *mb) { return (uint8_t)mb->result; }
static inline uint8_t tp_mathbox_hi(const tp_mathbox *mb) { return (uint8_t)((uint16_t)mb->result >> 8); }

#endif

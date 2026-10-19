/* avg.h - Tempest's analog vector generator (with colour), as a
 * display-list interpreter.
 *
 * 16-bit words, low byte first; the list starts at 2000, subroutines may
 * be in vector ROM (3000-3FFF). Opcode in bits 15-13:
 *
 *   000 VCTR  two words: DY (13-bit signed), then Z<<13 | DX (13-bit signed)
 *   001 HALT
 *   010 SVEC  one word: DY/2 in bits 12-8, Z in 7-5, DX/2 in 4-0
 *   011 STAT  bit 12 clear: bit 11 set: colour (bits 3-0, an index into
 *             colour RAM 0800-080F); bit 11 clear: intensity (bits 7-4)
 *       SCAL  bit 12 set: binary scale in bits 10-8, linear scale in 7-0
 *   100 CNTR  beam to the center
 *   101 JSRL  call word address (bits 12-0, from 2000), 4-deep stack
 *   110 RTSL
 *   111 JMPL
 *
 * Z (0..7) is the vector's brightness: 0 a move, 1 "as STAT set it",
 * otherwise twice itself. A colour RAM entry: bit 3 clear green, bit 2
 * clear blue, bits 1 and 0 clear red (strong and weak).
 *
 * Arithmetic and colours after MAME's avg_tempest_device (src/devices/
 * video/avgdvg.cpp, BSD-3-Clause, Mathis Rosenhauer): the deltas are
 * normalized as the board does it and the beam is kept in 16.16 fixed
 * point. Positions are the generator's own, center 0,0, x right, y up;
 * the cabinet's monitor is turned (MAME: ROT270), which the renderer does.
 */
#ifndef TP_AVG_H
#define TP_AVG_H

#include <stdint.h>

typedef struct tp_line {
    float    x0, y0, x1, y1;
    uint8_t  intensity;     /* 1..15 */
    uint8_t  z;             /* the vector's own 1..7 */
    uint8_t  color;         /* index into colour RAM */
    uint8_t  r, g, b;       /* as MAME shows that entry */
    uint16_t src;           /* address of the list's instruction that drew it (or called the routine that did) */
    uint16_t sub;           /* the vector ROM routine that instruction called, or 0 */
    uint16_t n;             /* the vector's number within that call (moves counted too) */
    /* What the line belongs to, for matching it with the same line of the
     * next picture (render/smooth.h): the game's list calls its sub-lists
     * (sub), each draws its objects one after another: an object (item) is
     * one instruction of the sub-list, a vector of its own or a call of a
     * shape; ikey is that shape's address (0: a vector of the sub-list),
     * in the vector's number within the object. */
    uint16_t item, ikey, in;
} tp_line;

enum tp_avg_status {
    TP_AVG_OK = 0,          /* reached HALT */
    TP_AVG_LIMIT,           /* instruction limit: a runaway or endless list */
};

typedef struct tp_avg_list {
    tp_line *lines;
    int      count;
    int      cap;
    int      instructions;  /* executed by the last run */
    int      status;
} tp_avg_list;

void tp_avg_list_free(tp_avg_list *l);
int  tp_avg_list_reserve(tp_avg_list *l, int n);

/* Run the display list at `start` (2000) of the address space `mem`
 * (64K) until HALT or until it jumps back to `start` (Tempest keeps the
 * generator going round its list; 2000 jumps to the buffer shown), with
 * the colour RAM `colram` (16 entries). Lines are
 * appended to out (cleared first). flip_x, flip_y: the board's mirror
 * bits (4000 D3, D4). max_instructions <= 0 uses a default. Returns
 * out->status. */
int tp_avg_run(const uint8_t *mem, const uint8_t *colram, uint16_t start, int flip_x, int flip_y,
               tp_avg_list *out, int max_instructions);

/* How long the board takes for one round of the list, until HALT or back
 * at `start`, in cycles of the 12.096 MHz master clock. *halts (if not
 * NULL): 1 if the round ends in HALT (IN0 bit 6 tells the program). */
uint32_t tp_avg_cycles(const uint8_t *mem, uint16_t start, int *halts);

#endif

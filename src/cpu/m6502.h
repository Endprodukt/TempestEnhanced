/* m6502.h - a MOS 6502 (NMOS), instruction by instruction, with its cycles.
 *
 * Runs the original Tempest program from the owner's ROMs: the game
 * itself, on the board the rest of src/ builds around it. All documented
 * instructions, decimal mode, NMI and IRQ. Cycle counts are the data
 * sheet's (page crossings and taken branches included): the game's pace
 * depends on them.
 */
#ifndef TP_CPU_M6502_H
#define TP_CPU_M6502_H

#include <stdint.h>

typedef uint8_t (*m6502_read_fn)(void *user, uint16_t addr);
typedef void    (*m6502_write_fn)(void *user, uint16_t addr, uint8_t value);

typedef struct m6502 {
    uint8_t  a, x, y, s, p;
    uint16_t pc;
    uint8_t  nmi_pending;       /* set by m6502_nmi (edge) */
    uint8_t  irq;               /* level: 1 asserted */
    uint8_t  jammed;            /* an undocumented opcode was met: the CPU stands */
    /* While an instruction works on its operand: the cycles it takes (its
     * operand is read or written in the last of them); 0 while it fetches
     * itself. For a board that needs the cycle of an access. */
    uint8_t  op_cycles;
    uint64_t cycles;            /* total run */
    m6502_read_fn  read;
    m6502_write_fn write;
    void    *user;
} m6502;

#define M6502_C 0x01
#define M6502_Z 0x02
#define M6502_I 0x04
#define M6502_D 0x08
#define M6502_B 0x10
#define M6502_U 0x20
#define M6502_V 0x40
#define M6502_N 0x80

void m6502_init(m6502 *c, m6502_read_fn rd, m6502_write_fn wr, void *user);
void m6502_reset(m6502 *c);         /* PC from FFFC, I set */
void m6502_nmi(m6502 *c);

/* One instruction, or the taking of an interrupt. Returns the cycles it
 * took. */
int m6502_step(m6502 *c);

#endif

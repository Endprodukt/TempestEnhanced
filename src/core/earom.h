/* earom.h - the GI ER2055: 64 bytes that keep the high scores and the
 * bookkeeping with the power off.
 *
 * Behaviour after MAME's er2055.cpp (BSD-3-Clause, Aaron Giles): the
 * program latches address and data with a write to 6000-603F, then works
 * the control lines through 6040 (D0 clock, D1 C2, D2 /C1, D3 CS1). A
 * read takes a falling clock edge with C1 high; a write ANDs the latched
 * data into the cell (so it has to be erased first); an erase sets it to
 * FF.
 */
#ifndef TP_EAROM_H
#define TP_EAROM_H

#include <stdint.h>

typedef struct tp_earom {
    uint8_t rom[64];
    uint8_t address;
    uint8_t data;       /* latched for a write, or read out */
    uint8_t control;    /* EA_xxx of earom.c */
    uint8_t dirty;      /* a cell changed since the owner last saved */
} tp_earom;

void tp_earom_init(tp_earom *e);                    /* all cells FF, as a new chip */
void tp_earom_latch(tp_earom *e, uint8_t offset, uint8_t data);
void tp_earom_control(tp_earom *e, uint8_t data);   /* a write to 6040 */
static inline uint8_t tp_earom_read(const tp_earom *e) { return e->data; }

#endif

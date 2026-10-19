#include "core/earom.h"

#include <string.h>

enum { EA_CK = 1, EA_C1 = 2, EA_C2 = 4, EA_CS1 = 8, EA_CS2 = 16 };

void tp_earom_init(tp_earom *e)
{
    memset(e, 0, sizeof *e);
    memset(e->rom, 0xff, sizeof e->rom);
}

void tp_earom_latch(tp_earom *e, uint8_t offset, uint8_t data)
{
    e->address = offset & 0x3f;
    e->data = data;
}

/* What the chip does in its current mode, after a change of the control
 * lines or a falling clock edge. */
static void update(tp_earom *e)
{
    switch (e->control & (EA_C1 | EA_C2)) {
    case 0:                 /* write: only clears bits, as without an erase */
        if ((e->rom[e->address] & e->data) != e->rom[e->address]) e->dirty = 1;
        e->rom[e->address] &= e->data;
        break;
    case EA_C2:             /* erase */
        if (e->rom[e->address] != 0xff) e->dirty = 1;
        e->rom[e->address] = 0xff;
        break;
    default:                /* read */
        break;
    }
}

void tp_earom_control(tp_earom *e, uint8_t data)
{
    /* The board: CK = D0, C1 = /D2, C2 = D1, CS1 = D3, /CS2 tied low. */
    uint8_t old = e->control;
    uint8_t lines = (uint8_t)((old & EA_CK) |
                              (!(data & 4) ? EA_C1 : 0) |
                              ((data & 2) ? EA_C2 : 0) |
                              ((data & 8) ? EA_CS1 : 0) |
                              EA_CS2);
    e->control = lines;
    if ((lines & (EA_CS1 | EA_CS2)) == (EA_CS1 | EA_CS2) && lines != old) update(e);

    /* then the clock line */
    old = e->control;
    if (data & 1) e->control |= EA_CK;
    else          e->control &= (uint8_t)~EA_CK;
    if ((e->control & (EA_CS1 | EA_CS2)) == (EA_CS1 | EA_CS2) && e->control != old && !(data & 1)) {
        if (e->control & EA_C1) e->data = e->rom[e->address];
        update(e);
    }
}

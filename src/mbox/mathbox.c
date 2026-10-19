/* After MAME's mathbox.cpp (BSD-3-Clause, copyright Eric Smith): the same
 * arithmetic, step for step, so that the two agree bit for bit. */
#include "mbox/mathbox.h"

#include <string.h>

#define REG0 mb->reg[0x00]
#define REG1 mb->reg[0x01]
#define REG2 mb->reg[0x02]
#define REG3 mb->reg[0x03]
#define REG4 mb->reg[0x04]
#define REG5 mb->reg[0x05]
#define REG6 mb->reg[0x06]
#define REG7 mb->reg[0x07]
#define REG8 mb->reg[0x08]
#define REG9 mb->reg[0x09]
#define REGa mb->reg[0x0a]
#define REGb mb->reg[0x0b]
#define REGc mb->reg[0x0c]
#define REGd mb->reg[0x0d]
#define REGe mb->reg[0x0e]
#define REGf mb->reg[0x0f]

/* 16-bit arithmetic as the C++ original does it on int16_t members: the
 * result of every operation wraps to 16 bits when it is stored. */
#define S16(x) ((int16_t)(uint16_t)(x))

void tp_mathbox_reset(tp_mathbox *mb)
{
    memset(mb, 0, sizeof *mb);
}

void tp_mathbox_go(tp_mathbox *mb, unsigned offset, uint8_t data)
{
    int32_t mb_temp;    /* temp 32-bit multiply results */
    int16_t mb_q;       /* temp used in division */
    int msb;

    switch (offset & 0x1f) {
    case 0x00: mb->result = REG0 = S16((REG0 & 0xff00) | data);        break;
    case 0x01: mb->result = REG0 = S16((REG0 & 0x00ff) | (data << 8)); break;
    case 0x02: mb->result = REG1 = S16((REG1 & 0xff00) | data);        break;
    case 0x03: mb->result = REG1 = S16((REG1 & 0x00ff) | (data << 8)); break;
    case 0x04: mb->result = REG2 = S16((REG2 & 0xff00) | data);        break;
    case 0x05: mb->result = REG2 = S16((REG2 & 0x00ff) | (data << 8)); break;
    case 0x06: mb->result = REG3 = S16((REG3 & 0xff00) | data);        break;
    case 0x07: mb->result = REG3 = S16((REG3 & 0x00ff) | (data << 8)); break;
    case 0x08: mb->result = REG4 = S16((REG4 & 0xff00) | data);        break;
    case 0x09: mb->result = REG4 = S16((REG4 & 0x00ff) | (data << 8)); break;

    case 0x0a: mb->result = REG5 = S16((REG5 & 0xff00) | data);        break;
        /* note: no function loads low part of REG5 without performing a computation */

    case 0x0c: mb->result = REG6 = data; break;
        /* note: no function loads high part of REG6 */

    case 0x15: mb->result = REG7 = S16((REG7 & 0xff00) | data);        break;
    case 0x16: mb->result = REG7 = S16((REG7 & 0x00ff) | (data << 8)); break;

    case 0x1a: mb->result = REG8 = S16((REG8 & 0xff00) | data);        break;
    case 0x1b: mb->result = REG8 = S16((REG8 & 0x00ff) | (data << 8)); break;

    case 0x0d: mb->result = REGa = S16((REGa & 0xff00) | data);        break;
    case 0x0e: mb->result = REGa = S16((REGa & 0x00ff) | (data << 8)); break;
    case 0x0f: mb->result = REGb = S16((REGb & 0xff00) | data);        break;
    case 0x10: mb->result = REGb = S16((REGb & 0x00ff) | (data << 8)); break;

    case 0x17: mb->result = REG7; break;
    case 0x19: mb->result = REG8; break;
    case 0x18: mb->result = REG9; break;

    case 0x0b:
        REG5 = S16((REG5 & 0x00ff) | (data << 8));

        REGf = S16(0xffff);
        REG4 = S16(REG4 - REG2);
        REG5 = S16(REG5 - REG3);

    step_048:
        mb_temp = ((int32_t)REG0) * ((int32_t)REG4);
        REGc = S16(mb_temp >> 16);
        REGe = S16(mb_temp & 0xffff);

        mb_temp = ((int32_t)-REG1) * ((int32_t)REG5);
        REG7 = S16(mb_temp >> 16);
        mb_q = S16(mb_temp & 0xffff);

        REG7 = S16(REG7 + REGc);

        /* rounding */
        REGe = S16((REGe >> 1) & 0x7fff);
        REGc = S16((mb_q >> 1) & 0x7fff);
        mb_q = S16(REGc + REGe);
        if (mb_q < 0)
            REG7 = S16(REG7 + 1);

        mb->result = REG7;

        if (REGf < 0)
            break;

        REG7 = S16(REG7 + REG2);

        /* fall into command 12 */
        /* fall through */

    case 0x12:
        mb_temp = ((int32_t)REG1) * ((int32_t)REG4);
        REGc = S16(mb_temp >> 16);
        REG9 = S16(mb_temp & 0xffff);

        mb_temp = ((int32_t)REG0) * ((int32_t)REG5);
        REG8 = S16(mb_temp >> 16);
        mb_q = S16(mb_temp & 0xffff);

        REG8 = S16(REG8 + REGc);

        /* rounding */
        REG9 = S16((REG9 >> 1) & 0x7fff);
        REGc = S16((mb_q >> 1) & 0x7fff);
        REG9 = S16(REG9 + REGc);
        if (REG9 < 0)
            REG8 = S16(REG8 + 1);
        REG9 = S16((uint16_t)REG9 << 1);  /* why? only to get the desired load address? */

        mb->result = REG8;

        if (REGf < 0)
            break;

        REG8 = S16(REG8 + REG3);

        REG9 = S16(REG9 & 0xff00);

        /* fall into command 13 */
        /* fall through */

    case 0x13:
        REGc = REG9;
        mb_q = REG8;
        goto step_0bf;

    case 0x14:
        REGc = REGa;
        mb_q = REGb;

    step_0bf:
        REGe = S16(REG7 ^ mb_q);  /* save sign of result */
        REGd = mb_q;
        if (mb_q >= 0)
            mb_q = REGc;
        else {
            REGd = S16(-mb_q - 1);
            mb_q = S16(-REGc - 1);
            if ((mb_q < 0) && ((mb_q + 1) < 0))
                REGd = S16(REGd + 1);
            mb_q = S16(mb_q + 1);
        }

        /* step 0c9: */
        /* REGc = abs (REG7) */
        if (REG7 >= 0)
            REGc = REG7;
        else
            REGc = S16(-REG7);

        REGf = REG6;  /* step counter */

        do {
            REGd = S16(REGd - REGc);
            msb = ((mb_q & 0x8000) != 0);
            mb_q = S16((uint16_t)mb_q << 1);
            if (REGd >= 0)
                mb_q = S16(mb_q + 1);
            else
                REGd = S16(REGd + REGc);
            REGd = S16((uint16_t)REGd << 1);
            REGd = S16(REGd + msb);
        } while ((REGf = S16(REGf - 1)) >= 0);

        if (REGe >= 0)
            mb->result = mb_q;
        else
            mb->result = S16(-mb_q);
        break;

    case 0x11:
        REG5 = S16((REG5 & 0x00ff) | (data << 8));
        REGf = 0x0000;  /* do everything in one step */
        goto step_048;

    case 0x1c:
        /* window test? */
        REG5 = S16((REG5 & 0x00ff) | (data << 8));
        do {
            REGe = S16((REG4 + REG7) >> 1);
            REGf = S16((REG5 + REG8) >> 1);
            if ((REGb < REGe) && (REGf < REGe) && ((REGe + REGf) >= 0)) {
                REG7 = REGe; REG8 = REGf;
            } else {
                REG4 = REGe; REG5 = REGf;
            }
        } while ((REG6 = S16(REG6 - 1)) >= 0);

        mb->result = REG8;
        break;

    case 0x1d:
        REG3 = S16((REG3 & 0x00ff) | (data << 8));

        REG2 = S16(REG2 - REG0);
        if (REG2 < 0)
            REG2 = S16(-REG2);

        REG3 = S16(REG3 - REG1);
        if (REG3 < 0)
            REG3 = S16(-REG3);

        /* fall into command 1e */
        /* fall through */

    case 0x1e:
        /* result = max (REG2, REG3) + 3/8 * min (REG2, REG3) */
        if (REG3 >= REG2) {
            REGc = REG2; REGd = REG3;
        } else {
            REGd = REG2; REGc = REG3;
        }
        REGc = S16(REGc >> 2);
        REGd = S16(REGd + REGc);
        REGc = S16(REGc >> 1);
        mb->result = REGd = S16(REGc + REGd);
        break;

    default:    /* 1f: nothing */
        break;
    }
}

#include "cpu/m6502.h"

#include <string.h>

#define RD(a)    (c->read(c->user, (uint16_t)(a)))
#define WR(a, v) (c->write(c->user, (uint16_t)(a), (uint8_t)(v)))

void m6502_init(m6502 *c, m6502_read_fn rd, m6502_write_fn wr, void *user)
{
    memset(c, 0, sizeof *c);
    c->read = rd;
    c->write = wr;
    c->user = user;
}

void m6502_reset(m6502 *c)
{
    c->a = c->x = c->y = 0;
    c->s = 0xfd;
    c->p = M6502_I | M6502_U;
    c->pc = (uint16_t)(RD(0xfffc) | (RD(0xfffd) << 8));
    c->nmi_pending = 0;
    c->irq = 0;
    c->jammed = 0;
}

void m6502_nmi(m6502 *c) { c->nmi_pending = 1; }

static void push(m6502 *c, uint8_t v) { WR(0x100 | c->s, v); c->s--; }
static uint8_t pull(m6502 *c) { c->s++; return RD(0x100 | c->s); }

static void setnz(m6502 *c, uint8_t v)
{
    c->p = (uint8_t)((c->p & ~(M6502_N | M6502_Z)) | (v & 0x80) | (v ? 0 : M6502_Z));
}

static void interrupt(m6502 *c, uint16_t vector)
{
    push(c, (uint8_t)(c->pc >> 8));
    push(c, (uint8_t)c->pc);
    push(c, (uint8_t)((c->p & ~M6502_B) | M6502_U));
    c->p |= M6502_I;
    c->pc = (uint16_t)(RD(vector) | (RD(vector + 1) << 8));
}

static void adc(m6502 *c, uint8_t v)
{
    unsigned carry = c->p & M6502_C;
    if (c->p & M6502_D) {
        /* NMOS: N, V and Z come from the binary sum (Z) and the sum before
         * the high digit's correction (N, V). */
        unsigned lo = (c->a & 0x0f) + (v & 0x0f) + carry;
        unsigned hi = (c->a & 0xf0) + (v & 0xf0);
        uint8_t bin = (uint8_t)(c->a + v + carry);
        if (lo > 9) { lo += 6; hi += 0x10; }
        c->p &= (uint8_t)~(M6502_N | M6502_V | M6502_Z | M6502_C);
        if (!bin) c->p |= M6502_Z;
        if (hi & 0x80) c->p |= M6502_N;
        if (~(c->a ^ v) & (c->a ^ hi) & 0x80) c->p |= M6502_V;
        if (hi > 0x90) hi += 0x60;
        if (hi > 0xff) c->p |= M6502_C;
        c->a = (uint8_t)((lo & 0x0f) | (hi & 0xf0));
    } else {
        unsigned sum = c->a + v + carry;
        c->p &= (uint8_t)~(M6502_V | M6502_C);
        if (~(c->a ^ v) & (c->a ^ sum) & 0x80) c->p |= M6502_V;
        if (sum > 0xff) c->p |= M6502_C;
        c->a = (uint8_t)sum;
        setnz(c, c->a);
    }
}

static void sbc(m6502 *c, uint8_t v)
{
    unsigned borrow = (c->p & M6502_C) ? 0 : 1;
    unsigned diff = c->a - v - borrow;
    if (c->p & M6502_D) {
        int lo = (c->a & 0x0f) - (v & 0x0f) - (int)borrow;
        int hi = (c->a & 0xf0) - (v & 0xf0);
        if (lo & 0x10) { lo -= 6; hi -= 0x10; }
        c->p &= (uint8_t)~(M6502_V | M6502_C);
        if ((c->a ^ v) & (c->a ^ diff) & 0x80) c->p |= M6502_V;
        if (!(diff & 0x100)) c->p |= M6502_C;
        setnz(c, (uint8_t)diff);
        if (hi & 0x100) hi -= 0x60;
        c->a = (uint8_t)((lo & 0x0f) | (hi & 0xf0));
    } else {
        c->p &= (uint8_t)~(M6502_V | M6502_C);
        if ((c->a ^ v) & (c->a ^ diff) & 0x80) c->p |= M6502_V;
        if (!(diff & 0x100)) c->p |= M6502_C;
        c->a = (uint8_t)diff;
        setnz(c, c->a);
    }
}

static void cmp(m6502 *c, uint8_t r, uint8_t v)
{
    c->p = (uint8_t)((c->p & ~M6502_C) | (r >= v ? M6502_C : 0));
    setnz(c, (uint8_t)(r - v));
}

/* Addressing modes */
enum { IMP, ACC, IMM, ZP, ZPX, ZPY, IZX, IZY, ABS, ABX, ABY, IND, REL };

/* Instructions */
enum { ILL, ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC, CLD, CLI, CLV, CMP, CPX, CPY, DEC,
       DEX, DEY, EOR, INC, INX, INY, JMP, JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS,
       SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA };

typedef struct opdef { uint8_t ins, mode, cycles; } opdef;

#define O(i, m, n) { i, m, n }
#define X_ { ILL, IMP, 2 }
static const opdef OPS[256] = {
/*00*/ O(BRK,IMP,7), O(ORA,IZX,6), X_, X_, X_, O(ORA,ZP,3), O(ASL,ZP,5), X_,
       O(PHP,IMP,3), O(ORA,IMM,2), O(ASL,ACC,2), X_, X_, O(ORA,ABS,4), O(ASL,ABS,6), X_,
/*10*/ O(BPL,REL,2), O(ORA,IZY,5), X_, X_, X_, O(ORA,ZPX,4), O(ASL,ZPX,6), X_,
       O(CLC,IMP,2), O(ORA,ABY,4), X_, X_, X_, O(ORA,ABX,4), O(ASL,ABX,7), X_,
/*20*/ O(JSR,ABS,6), O(AND,IZX,6), X_, X_, O(BIT,ZP,3), O(AND,ZP,3), O(ROL,ZP,5), X_,
       O(PLP,IMP,4), O(AND,IMM,2), O(ROL,ACC,2), X_, O(BIT,ABS,4), O(AND,ABS,4), O(ROL,ABS,6), X_,
/*30*/ O(BMI,REL,2), O(AND,IZY,5), X_, X_, X_, O(AND,ZPX,4), O(ROL,ZPX,6), X_,
       O(SEC,IMP,2), O(AND,ABY,4), X_, X_, X_, O(AND,ABX,4), O(ROL,ABX,7), X_,
/*40*/ O(RTI,IMP,6), O(EOR,IZX,6), X_, X_, X_, O(EOR,ZP,3), O(LSR,ZP,5), X_,
       O(PHA,IMP,3), O(EOR,IMM,2), O(LSR,ACC,2), X_, O(JMP,ABS,3), O(EOR,ABS,4), O(LSR,ABS,6), X_,
/*50*/ O(BVC,REL,2), O(EOR,IZY,5), X_, X_, X_, O(EOR,ZPX,4), O(LSR,ZPX,6), X_,
       O(CLI,IMP,2), O(EOR,ABY,4), X_, X_, X_, O(EOR,ABX,4), O(LSR,ABX,7), X_,
/*60*/ O(RTS,IMP,6), O(ADC,IZX,6), X_, X_, X_, O(ADC,ZP,3), O(ROR,ZP,5), X_,
       O(PLA,IMP,4), O(ADC,IMM,2), O(ROR,ACC,2), X_, O(JMP,IND,5), O(ADC,ABS,4), O(ROR,ABS,6), X_,
/*70*/ O(BVS,REL,2), O(ADC,IZY,5), X_, X_, X_, O(ADC,ZPX,4), O(ROR,ZPX,6), X_,
       O(SEI,IMP,2), O(ADC,ABY,4), X_, X_, X_, O(ADC,ABX,4), O(ROR,ABX,7), X_,
/*80*/ X_, O(STA,IZX,6), X_, X_, O(STY,ZP,3), O(STA,ZP,3), O(STX,ZP,3), X_,
       O(DEY,IMP,2), X_, O(TXA,IMP,2), X_, O(STY,ABS,4), O(STA,ABS,4), O(STX,ABS,4), X_,
/*90*/ O(BCC,REL,2), O(STA,IZY,6), X_, X_, O(STY,ZPX,4), O(STA,ZPX,4), O(STX,ZPY,4), X_,
       O(TYA,IMP,2), O(STA,ABY,5), O(TXS,IMP,2), X_, X_, O(STA,ABX,5), X_, X_,
/*A0*/ O(LDY,IMM,2), O(LDA,IZX,6), O(LDX,IMM,2), X_, O(LDY,ZP,3), O(LDA,ZP,3), O(LDX,ZP,3), X_,
       O(TAY,IMP,2), O(LDA,IMM,2), O(TAX,IMP,2), X_, O(LDY,ABS,4), O(LDA,ABS,4), O(LDX,ABS,4), X_,
/*B0*/ O(BCS,REL,2), O(LDA,IZY,5), X_, X_, O(LDY,ZPX,4), O(LDA,ZPX,4), O(LDX,ZPY,4), X_,
       O(CLV,IMP,2), O(LDA,ABY,4), O(TSX,IMP,2), X_, O(LDY,ABX,4), O(LDA,ABX,4), O(LDX,ABY,4), X_,
/*C0*/ O(CPY,IMM,2), O(CMP,IZX,6), X_, X_, O(CPY,ZP,3), O(CMP,ZP,3), O(DEC,ZP,5), X_,
       O(INY,IMP,2), O(CMP,IMM,2), O(DEX,IMP,2), X_, O(CPY,ABS,4), O(CMP,ABS,4), O(DEC,ABS,6), X_,
/*D0*/ O(BNE,REL,2), O(CMP,IZY,5), X_, X_, X_, O(CMP,ZPX,4), O(DEC,ZPX,6), X_,
       O(CLD,IMP,2), O(CMP,ABY,4), X_, X_, X_, O(CMP,ABX,4), O(DEC,ABX,7), X_,
/*E0*/ O(CPX,IMM,2), O(SBC,IZX,6), X_, X_, O(CPX,ZP,3), O(SBC,ZP,3), O(INC,ZP,5), X_,
       O(INX,IMP,2), O(SBC,IMM,2), O(NOP,IMP,2), X_, O(CPX,ABS,4), O(SBC,ABS,4), O(INC,ABS,6), X_,
/*F0*/ O(BEQ,REL,2), O(SBC,IZY,5), X_, X_, X_, O(SBC,ZPX,4), O(INC,ZPX,6), X_,
       O(SED,IMP,2), O(SBC,ABY,4), X_, X_, X_, O(SBC,ABX,4), O(INC,ABX,7), X_,
};

/* Does the instruction only write its operand (no page-crossing penalty,
 * it always takes the long way)? */
static int is_store(int ins) { return ins == STA || ins == STX || ins == STY; }
static int is_rmw(int ins) { return ins == ASL || ins == LSR || ins == ROL || ins == ROR || ins == INC || ins == DEC; }

int m6502_step(m6502 *c)
{
    if (c->nmi_pending) {
        c->nmi_pending = 0;
        interrupt(c, 0xfffa);
        c->cycles += 7;
        return 7;
    }
    if (c->irq && !(c->p & M6502_I)) {
        c->op_cycles = 0;
        interrupt(c, 0xfffe);
        c->cycles += 7;
        return 7;
    }
    if (c->jammed) {
        c->cycles += 2;
        return 2;
    }

    c->op_cycles = 0;
    uint8_t opcode = RD(c->pc);
    const opdef *d = &OPS[opcode];
    if (d->ins == ILL) {
        c->jammed = 1;
        c->cycles += 2;
        return 2;
    }
    c->pc++;
    int cycles = d->cycles;
    uint16_t ea = 0;
    int extra = 0;          /* a page was crossed by the index */

    switch (d->mode) {
    case IMP: case ACC: break;
    case IMM: ea = c->pc++; break;
    case ZP:  ea = RD(c->pc++); break;
    /* (indexing takes a cycle in which the CPU reads the unindexed address) */
    case ZPX: ea = RD(c->pc++); (void)RD(ea); ea = (uint8_t)(ea + c->x); break;
    case ZPY: ea = RD(c->pc++); (void)RD(ea); ea = (uint8_t)(ea + c->y); break;
    case IZX: {
        uint8_t z = (uint8_t)(RD(c->pc++) + c->x);
        ea = (uint16_t)(RD(z) | (RD((uint8_t)(z + 1)) << 8));
        break;
    }
    case IZY: {
        uint8_t z = RD(c->pc++);
        uint16_t base = (uint16_t)(RD(z) | (RD((uint8_t)(z + 1)) << 8));
        ea = (uint16_t)(base + c->y);
        extra = (base ^ ea) & 0xff00 ? 1 : 0;
        break;
    }
    case ABS: ea = (uint16_t)(RD(c->pc) | (RD(c->pc + 1) << 8)); c->pc += 2; break;
    case ABX: {
        uint16_t base = (uint16_t)(RD(c->pc) | (RD(c->pc + 1) << 8));
        c->pc += 2;
        ea = (uint16_t)(base + c->x);
        extra = (base ^ ea) & 0xff00 ? 1 : 0;
        break;
    }
    case ABY: {
        uint16_t base = (uint16_t)(RD(c->pc) | (RD(c->pc + 1) << 8));
        c->pc += 2;
        ea = (uint16_t)(base + c->y);
        extra = (base ^ ea) & 0xff00 ? 1 : 0;
        break;
    }
    case IND: {
        uint16_t ptr = (uint16_t)(RD(c->pc) | (RD(c->pc + 1) << 8));
        c->pc += 2;
        /* the NMOS part does not carry into the pointer's high byte */
        ea = (uint16_t)(RD(ptr) | (RD((uint16_t)((ptr & 0xff00) | ((ptr + 1) & 0xff))) << 8));
        break;
    }
    case REL: {
        int8_t off = (int8_t)RD(c->pc++);
        ea = (uint16_t)(c->pc + off);
        break;
    }
    }
    if (extra && !is_store(d->ins) && !is_rmw(d->ins)) cycles++;
    c->op_cycles = (uint8_t)cycles;

#define BRANCH(cond) do { if (cond) { cycles += ((c->pc ^ ea) & 0xff00) ? 2 : 1; c->pc = ea; } } while (0)
#define FLAG(f, on)  (c->p = (uint8_t)((c->p & ~(f)) | ((on) ? (f) : 0)))

    uint8_t v;
    switch (d->ins) {
    case ADC: adc(c, RD(ea)); break;
    case SBC: sbc(c, RD(ea)); break;
    case AND: c->a &= RD(ea); setnz(c, c->a); break;
    case ORA: c->a |= RD(ea); setnz(c, c->a); break;
    case EOR: c->a ^= RD(ea); setnz(c, c->a); break;
    case BIT:
        v = RD(ea);
        c->p = (uint8_t)((c->p & ~(M6502_N | M6502_V | M6502_Z)) | (v & 0xc0) | ((v & c->a) ? 0 : M6502_Z));
        break;
    case CMP: cmp(c, c->a, RD(ea)); break;
    case CPX: cmp(c, c->x, RD(ea)); break;
    case CPY: cmp(c, c->y, RD(ea)); break;
    case LDA: c->a = RD(ea); setnz(c, c->a); break;
    case LDX: c->x = RD(ea); setnz(c, c->x); break;
    case LDY: c->y = RD(ea); setnz(c, c->y); break;
    case STA: WR(ea, c->a); break;
    case STX: WR(ea, c->x); break;
    case STY: WR(ea, c->y); break;
    case ASL: case LSR: case ROL: case ROR: {
        unsigned in = d->mode == ACC ? c->a : RD(ea);
        unsigned carry = c->p & M6502_C, out;
        if (d->ins == ASL)      { out = in << 1; FLAG(M6502_C, in & 0x80); }
        else if (d->ins == LSR) { out = in >> 1; FLAG(M6502_C, in & 1); }
        else if (d->ins == ROL) { out = (in << 1) | carry; FLAG(M6502_C, in & 0x80); }
        else                    { out = (in >> 1) | (carry << 7); FLAG(M6502_C, in & 1); }
        v = (uint8_t)out;
        setnz(c, v);
        if (d->mode == ACC) c->a = v; else WR(ea, v);
        break;
    }
    case INC: v = (uint8_t)(RD(ea) + 1); setnz(c, v); WR(ea, v); break;
    case DEC: v = (uint8_t)(RD(ea) - 1); setnz(c, v); WR(ea, v); break;
    case INX: c->x++; setnz(c, c->x); break;
    case INY: c->y++; setnz(c, c->y); break;
    case DEX: c->x--; setnz(c, c->x); break;
    case DEY: c->y--; setnz(c, c->y); break;
    case TAX: c->x = c->a; setnz(c, c->x); break;
    case TAY: c->y = c->a; setnz(c, c->y); break;
    case TXA: c->a = c->x; setnz(c, c->a); break;
    case TYA: c->a = c->y; setnz(c, c->a); break;
    case TSX: c->x = c->s; setnz(c, c->x); break;
    case TXS: c->s = c->x; break;
    case PHA: push(c, c->a); break;
    case PHP: push(c, (uint8_t)(c->p | M6502_B | M6502_U)); break;
    case PLA: c->a = pull(c); setnz(c, c->a); break;
    case PLP: c->p = (uint8_t)((pull(c) & ~M6502_B) | M6502_U); break;
    case CLC: FLAG(M6502_C, 0); break;
    case SEC: FLAG(M6502_C, 1); break;
    case CLD: FLAG(M6502_D, 0); break;
    case SED: FLAG(M6502_D, 1); break;
    case CLI: FLAG(M6502_I, 0); break;
    case SEI: FLAG(M6502_I, 1); break;
    case CLV: FLAG(M6502_V, 0); break;
    case BCC: BRANCH(!(c->p & M6502_C)); break;
    case BCS: BRANCH(c->p & M6502_C); break;
    case BNE: BRANCH(!(c->p & M6502_Z)); break;
    case BEQ: BRANCH(c->p & M6502_Z); break;
    case BPL: BRANCH(!(c->p & M6502_N)); break;
    case BMI: BRANCH(c->p & M6502_N); break;
    case BVC: BRANCH(!(c->p & M6502_V)); break;
    case BVS: BRANCH(c->p & M6502_V); break;
    case JMP: c->pc = ea; break;
    case JSR:
        c->pc--;
        push(c, (uint8_t)(c->pc >> 8));
        push(c, (uint8_t)c->pc);
        c->pc = ea;
        break;
    case RTS:
        c->pc = pull(c);
        c->pc |= (uint16_t)(pull(c) << 8);
        c->pc++;
        break;
    case RTI:
        c->p = (uint8_t)((pull(c) & ~M6502_B) | M6502_U);
        c->pc = pull(c);
        c->pc |= (uint16_t)(pull(c) << 8);
        break;
    case BRK:
        c->pc++;
        push(c, (uint8_t)(c->pc >> 8));
        push(c, (uint8_t)c->pc);
        push(c, (uint8_t)(c->p | M6502_B | M6502_U));
        c->p |= M6502_I;
        c->pc = (uint16_t)(RD(0xfffe) | (RD(0xffff) << 8));
        break;
    case NOP: default: break;
    }
    c->cycles += (uint64_t)cycles;
    return cycles;
}

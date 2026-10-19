#include "avg/avg.h"

#include <stdlib.h>
#include <string.h>

#define DEFAULT_MAX_INSTRUCTIONS 50000
#define UNIT 65536.0f           /* the beam's position is 16.16 fixed point */

void tp_avg_list_free(tp_avg_list *l)
{
    free(l->lines);
    memset(l, 0, sizeof *l);
}

int tp_avg_list_reserve(tp_avg_list *l, int n)
{
    if (n <= l->cap) return 0;
    int cap = l->cap ? l->cap : 1024;
    while (cap < n) cap *= 2;
    tp_line *p = (tp_line *)realloc(l->lines, (size_t)cap * sizeof *p);
    if (!p) return -1;
    l->lines = p;
    l->cap = cap;
    return 0;
}

static int push_line(tp_avg_list *l, const tp_line *ln)
{
    if (l->count == l->cap && tp_avg_list_reserve(l, l->count + 1) != 0) return -1;
    l->lines[l->count++] = *ln;
    return 0;
}

static unsigned word_at(const uint8_t *mem, unsigned addr)
{
    return (unsigned)mem[addr & 0xffff] | ((unsigned)mem[(addr + 1) & 0xffff] << 8);
}

/* The normalization before a vector: both deltas are shifted up until one
 * of them uses its top bit; each shift halves the time the vector timer
 * runs. Returns the shifts. */
static int normalize(unsigned *dvy, unsigned *dvx)
{
    int i = 0;
    while (((*dvy ^ (*dvy << 1)) & 0x1000) == 0 && ((*dvx ^ (*dvx << 1)) & 0x1000) == 0 && i < 16) {
        i++;
        *dvy = (*dvy & 0x1000) | ((*dvy << 1) & 0x1fff);
        *dvx = (*dvx & 0x1000) | ((*dvx << 1) & 0x1fff);
    }
    return i;
}

/* A colour RAM entry as MAME shows it (active low bits). */
static void entry_rgb(uint8_t data, uint8_t *r, uint8_t *g, uint8_t *b)
{
    unsigned d = (unsigned)~data;
    *r = (uint8_t)(((d >> 1) & 1) * 0xf3 + (d & 1) * 0x0c);
    *g = (uint8_t)(((d >> 3) & 1) * 0xf3);
    *b = (uint8_t)(((d >> 2) & 1) * 0xf3);
}

int tp_avg_run(const uint8_t *mem, const uint8_t *colram, uint16_t start, int flip_x, int flip_y,
               tp_avg_list *out, int max_instructions)
{
    unsigned pc = start;
    unsigned stack[4] = { 0, 0, 0, 0 };
    unsigned sp = 0;
    int32_t xpos = 0, ypos = 0;             /* 16.16, the center at 0, y up */
    unsigned scale = 0, bin_scale = 0;
    unsigned intensity = 0, color = 0;
    int depth = 0;
    unsigned src = 0, sub = 0, nline = 0;
    unsigned items = 0, item = 0, ikey = 0, nin = 0;    /* the object in the sub-list (tp_line.item) */

    if (max_instructions <= 0) max_instructions = DEFAULT_MAX_INSTRUCTIONS;
    out->count = 0;
    out->instructions = 0;
    out->status = TP_AVG_OK;

    for (;;) {
        if (out->instructions >= max_instructions) return out->status = TP_AVG_LIMIT;
        out->instructions++;

        if (depth == 0) {
            src = pc;
            sub = 0;
            nline = 0;
        }
        if (depth <= 1) {
            /* each instruction of the list or a sub-list begins an object */
            item = items++;
            ikey = 0;
            nin = 0;
        }
        unsigned w = word_at(mem, pc);
        pc += 2;
        unsigned dvy = 0, dvx = 0, z = 0;
        int k, vector = 0, cycles = 0;

        switch (w >> 13) {
        case 0:  /* VCTR */
            dvy = w & 0x1fff;
            w = word_at(mem, pc);
            pc += 2;
            dvx = w & 0x1fff;
            z = w >> 13;
            k = normalize(&dvy, &dvx) + (int)bin_scale;
            cycles = k < 15 ? 0x8000 >> k : 1;
            vector = 1;
            break;
        case 1:  /* HALT */
            return out->status;
        case 2:  /* SVEC */
            dvy = (w >> 8) & 0x1f;
            dvy = ((dvy & 0x10) << 8) | ((dvy & 0x0f) << 8);
            dvx = w & 0x1f;
            dvx = ((dvx & 0x10) << 8) | ((dvx & 0x0f) << 8);
            z = (w >> 5) & 7;
            k = normalize(&dvy, &dvx) + (int)bin_scale;
            cycles = k < 8 ? 0x100 >> k : 1;
            vector = 1;
            break;
        case 3:
            if (w & 0x1000) {           /* SCAL */
                scale = w & 0xff;
                bin_scale = (w >> 8) & 7;
            } else if (w & 0x0800) {    /* STAT: colour */
                color = w & 0x0f;
            } else {                    /* STAT: intensity */
                intensity = (w >> 4) & 0x0f;
            }
            break;
        case 4:  /* CNTR */
            xpos = ypos = 0;
            break;
        case 5:  /* JSRL */
            stack[sp & 3] = pc;
            sp = (sp + 1) & 0xf;
            pc = 0x2000 + ((w & 0x1fff) << 1);
            if (depth == 0) { sub = pc; items = 0; }
            if (depth == 1) ikey = pc & 0xffff;
            depth++;
            break;
        case 6:  /* RTSL */
            sp = (sp - 1) & 0xf;
            pc = stack[sp & 3];
            if (depth > 0) depth--;
            break;
        case 7:  /* JMPL */
            pc = 0x2000 + ((w & 0x1fff) << 1);
            /* back to the start: the picture is complete (the generator
             * runs on, round the list again) */
            if (pc == start && out->instructions > 1) return out->status;
            break;
        }

        if (vector) {
            /* The DACs take the top ten bits of each delta. */
            int64_t dx = (int64_t)((int32_t)((dvx >> 3) ^ 0x200) - 0x200) * cycles * (int32_t)(scale ^ 0xff);
            int64_t dy = (int64_t)((int32_t)((dvy >> 3) ^ 0x200) - 0x200) * cycles * (int32_t)(scale ^ 0xff);
            int32_t nx = xpos + (int32_t)(dx >> 4), ny = ypos + (int32_t)(dy >> 4);
            unsigned lum = z == 1 ? intensity : z << 1;
            if (lum) {
                tp_line ln;
                memset(&ln, 0, sizeof ln);
                ln.x0 = (float)(xpos / UNIT); ln.y0 = (float)(ypos / UNIT);
                ln.x1 = (float)(nx / UNIT);   ln.y1 = (float)(ny / UNIT);
                if (flip_x) { ln.x0 = -ln.x0; ln.x1 = -ln.x1; }
                if (flip_y) { ln.y0 = -ln.y0; ln.y1 = -ln.y1; }
                ln.intensity = (uint8_t)lum;
                ln.z = (uint8_t)z;
                ln.color = (uint8_t)color;
                entry_rgb(colram[color & 15], &ln.r, &ln.g, &ln.b);
                ln.src = (uint16_t)src;
                ln.sub = (uint16_t)sub;
                ln.n = (uint16_t)nline;
                ln.item = (uint16_t)item;
                ln.ikey = (uint16_t)ikey;
                ln.in = (uint16_t)nin;
                if (push_line(out, &ln) != 0) return out->status = TP_AVG_LIMIT;
            }
            nline++;
            nin++;
            xpos = nx;
            ypos = ny;
        }
    }
}

uint32_t tp_avg_cycles(const uint8_t *mem, uint16_t start, int *halts)
{
    /* States the machine goes through per instruction, 8 clock cycles each
     * (the state PROM 136002-125, the same as Battlezone's). */
    static const uint8_t STATES[8] = { 8, 4, 6, 7, 5, 5, 3, 2 };
    unsigned pc = start, sp = 0, bin_scale = 0;
    unsigned stack[4] = { 0, 0, 0, 0 };
    uint32_t cycles = 0;

    for (int n = 0; n < DEFAULT_MAX_INSTRUCTIONS; n++) {
        unsigned w = word_at(mem, pc), dvy, dvx;
        int k;
        pc += 2;
        cycles += 8u * STATES[w >> 13];
        switch (w >> 13) {
        case 0:
            dvy = w & 0x1fff;
            dvx = word_at(mem, pc) & 0x1fff;
            pc += 2;
            k = normalize(&dvy, &dvx) + (int)bin_scale;
            cycles += k < 15 ? 0x8000u >> k : 1u;
            break;
        case 1:
            if (halts) *halts = 1;
            return cycles;
        case 2:
            dvy = (w & 0x1f00);
            dvx = (w & 0x1f) << 8;
            k = normalize(&dvy, &dvx) + (int)bin_scale;
            cycles += k < 8 ? 0x100u >> k : 1u;
            break;
        case 3:
            if (w & 0x1000) bin_scale = (w >> 8) & 7;
            break;
        case 4:  /* CNTR: the beam's way back takes a timer run too */
            dvy = w & 0x1fff;
            dvx = 0;
            k = normalize(&dvy, &dvx);
            cycles += k < 15 ? 0x8000u >> k : 1u;
            break;
        case 5:
            stack[sp & 3] = pc;
            sp++;
            pc = 0x2000 + ((w & 0x1fff) << 1);
            break;
        case 6:
            sp--;
            pc = stack[sp & 3];
            break;
        case 7:
            pc = 0x2000 + ((w & 0x1fff) << 1);
            if (pc == start && n > 0) {
                if (halts) *halts = 0;
                return cycles;      /* one round of an endless list */
            }
            break;
        }
    }
    if (halts) *halts = 0;
    return cycles;          /* a runaway list */
}

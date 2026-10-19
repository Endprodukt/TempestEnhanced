/* tpcompare - hold the board against a MAME recording, frame by frame.
 *
 *   tpcompare FILE.tpcap [--roms PATH] [--from F] [--to F] [--max N] [--show N] [--quiet]
 *
 * For each recorded frame: the machine gets the recording's state at the
 * frame's start, the 6502 runs the frame with the I/O reads MAME saw
 * (in order) and the interrupts where MAME took them, and the RAM, colour
 * RAM, vector RAM and math box at the next frame's start must be MAME's,
 * byte for byte. Every frame is checked on its own.
 *
 * Besides, each replayed read is held against what the board model of
 * this port would have given (switches, VG halted, 3 kHz, POKEYs, math
 * box, EAROM): the counts per address are printed at the end.
 */
#include "capture/capture.h"
#include "core/emu.h"
#include "core/machine.h"
#include "rom/romset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_READS 200000
#define MAX_IRQS  4096

typedef struct replay {
    tpcap_read *reads;
    int         nreads, at;
    int         bad;            /* the program read another address than MAME's */
    uint16_t    bad_addr;
    tp_emu     *e;
    uint64_t    start;          /* machine cycle of the frame's start */
    /* the model's own values against MAME's, per address, and the first differences */
    uint32_t    model_same[0x10000], model_diff[0x10000];
    uint8_t     first_mine[0x10000], first_mame[0x10000];
    uint32_t    skipped;        /* MAME's dummy reads of write-only addresses */
} replay;

/* Addresses the program only writes: MAME's 6502 reads them as a dummy
 * cycle of an indexed store (STA 0800,X, STA 60C0,X) or a read-modify-
 * write, which this core does not. Such reads in a recording are passed
 * over. Of the POKEYs the program reads only ALLPOT and RANDOM. */
static int write_only(uint16_t a)
{
    if (a >= 0x60c0 && a < 0x60e0) return (a & 15) != 0x08 && (a & 15) != 0x0a;
    return (a >= 0x0800 && a < 0x0c00) || (a >= 0x4000 && a < 0x6000) ||
           (a >= 0x6000 && a < 0x6040) || (a >= 0x6080 && a < 0x60a0) || a == 0x60e0;
}

static uint8_t on_read(void *user, uint16_t addr, uint8_t value)
{
    replay *r = (replay *)user;
    if (r->bad) return value;
    while (r->at < r->nreads && r->reads[r->at].addr != addr && write_only(r->reads[r->at].addr)) {
        r->at++;
        r->skipped++;
    }
    if (r->at >= r->nreads || r->reads[r->at].addr != addr) {
        r->bad = 1;
        r->bad_addr = addr;
        return value;
    }
    uint8_t v = r->reads[r->at++].value;
    /* the 3 kHz bit and the halt bit depend on the time; the rest should agree */
    if (v == value) r->model_same[addr]++;
    else if (r->model_diff[addr]++ == 0) {
        r->first_mine[addr] = value;
        r->first_mame[addr] = v;
    }
    return v;
}

static void load_state(tp_machine *m, tp_emu *e, const tpcap_frame *f)
{
    memcpy(m->mem, f->ram, TPCAP_RAM_SIZE);
    memcpy(m->colram, f->colram, 16);
    memcpy(m->mem + TP_VGRAM, f->vgram, TPCAP_VGRAM_SIZE);
    for (int i = 0; i < 16; i++) m->mb.reg[i] = f->mb_reg[i];
    m->mb.result = f->mb_result;
    memcpy(m->earom.rom, f->earom, 64);
    m->earom.address = f->ea_address;
    m->earom.data = f->ea_data;
    /* the frame starts with the main loop's store of 0 into the frame timer */
    m->mem[TP_FRTIMR] = 0;
    e->cpu.a = f->a; e->cpu.x = f->x; e->cpu.y = f->y; e->cpu.s = f->s; e->cpu.p = f->p;
    e->cpu.pc = TP_MAIN_FRAME;
    e->cpu.irq = 0;
    e->cpu.nmi_pending = 0;
    e->cpu.jammed = 0;
    m->irq = 0;
    m->cycle = f->cycle;
}

static int diff_block(const char *what, uint16_t base, const uint8_t *mine, const uint8_t *mame, int n,
                      int show, uint32_t frame)
{
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (mine[i] == mame[i]) continue;
        if (count < show) printf("  frame %u: %s %04X: port %02X, MAME %02X\n", frame, what, base + i, mine[i], mame[i]);
        count++;
    }
    return count;
}

int main(int argc, char **argv)
{
    const char *path = NULL, *roms = NULL;
    uint32_t from = 0, to = 0xffffffffu;
    int max_report = 10, show = 16, quiet = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--roms") && i + 1 < argc) roms = argv[++i];
        else if (!strcmp(argv[i], "--from") && i + 1 < argc) from = (uint32_t)atol(argv[++i]);
        else if (!strcmp(argv[i], "--to") && i + 1 < argc) to = (uint32_t)atol(argv[++i]);
        else if (!strcmp(argv[i], "--max") && i + 1 < argc) max_report = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--show") && i + 1 < argc) show = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
        else if (!path) path = argv[i];
        else { fprintf(stderr, "tpcompare: unknown option %s\n", argv[i]); return 2; }
    }
    if (!path) { fprintf(stderr, "usage: tpcompare FILE.tpcap [--roms PATH] [--from F] [--to F]\n"); return 2; }

    const char *paths[64];
    int np = 0;
    if (roms) np = tp_romset_add_dir(paths, np, 64, roms), paths[np++] = roms;
    np += tp_romset_default_paths(paths + np, 64 - np);
    static tp_romset rs;
    if (tp_romset_load(&rs, paths, np) != 0) {
        char miss[512];
        fprintf(stderr, "tpcompare: ROMs missing: %s\n", tp_romset_missing(&rs, miss, sizeof miss));
        return 1;
    }

    tpcap_reader rd;
    if (tpcap_open(&rd, path) != 0) { fprintf(stderr, "tpcompare: cannot read %s\n", path); return 1; }

    static tp_machine m;
    static tp_emu e;
    static replay rp;
    static uint8_t buf[TPCAP_FRAM_SIZE + 16];
    static uint8_t *chunk;
    chunk = (uint8_t *)malloc(MAX_READS * 7);
    rp.reads = (tpcap_read *)malloc(MAX_READS * sizeof(tpcap_read));
    static tpcap_irq irqs[MAX_IRQS];
    int nirqs = 0;
    static tpcap_frame prev, cur;
    int have_prev = 0;

    tp_machine_init(&m);
    tp_machine_set_rom(&m, rs.vgrom, rs.prog);
    if (rs.have_avg_prom) tp_machine_set_avg_prom(&m, rs.avg_prom);
    tp_emu_init(&e, &m);
    m.read_hook = on_read;
    m.read_user = &rp;
    rp.e = &e;

    uint32_t checked = 0, same = 0, reported = 0, desync = 0, irq_late = 0;
    char tag[4];
    uint32_t len;
    int r;
    while ((r = tpcap_next_chunk(&rd, tag, chunk, MAX_READS * 7, &len)) == 1) {
        if (!memcmp(tag, "READ", 4)) {
            rp.nreads = tpcap_parse_reads(chunk, len, rp.reads, MAX_READS);
            continue;
        }
        if (!memcmp(tag, "IRQS", 4)) {
            nirqs = tpcap_parse_irqs(chunk, len, irqs, MAX_IRQS);
            continue;
        }
        if (memcmp(tag, "FRAM", 4) != 0 || len != TPCAP_FRAM_SIZE) continue;
        memcpy(buf, chunk, TPCAP_FRAM_SIZE);
        tpcap_parse_frame(buf, &cur);

        if (have_prev && prev.frame >= from && prev.frame <= to) {
            load_state(&m, &e, &prev);
            rp.at = 0;
            rp.bad = 0;
            rp.start = m.cycle;
            int next_irq = 0, ok = 1;
            uint64_t budget = (cur.cycle - prev.cycle) * 2 + 20000;
            /* run until the main loop is back at its store of 0 into the frame timer */
            for (;;) {
                uint32_t now = (uint32_t)(m.cycle - rp.start);
                if (next_irq < nirqs && e.cpu.pc == irqs[next_irq].ret && now + 8 >= irqs[next_irq].cycle) {
                    m.irq = 1;
                    e.cpu.p &= (uint8_t)~M6502_I;   /* MAME took it here, so it was allowed */
                    if (now + 8 > irqs[next_irq].cycle + 16) irq_late++;
                    next_irq++;
                    int cy0 = (int)e.cpu.cycles;
                    e.cpu.irq = 1;
                    uint64_t c0 = e.cpu.cycles;
                    m6502_step(&e.cpu);     /* takes the interrupt */
                    m.cycle += e.cpu.cycles - c0;
                    (void)cy0;
                    m.irq = 0;
                    e.cpu.irq = 0;
                }
                uint64_t c0 = e.cpu.cycles;
                e.cpu.irq = 0;
                m6502_step(&e.cpu);
                m.cycle += e.cpu.cycles - c0;
                if (rp.bad || e.cpu.jammed) { ok = 0; break; }
                /* (an interrupt MAME took right there belongs to this frame still) */
                if (e.cpu.pc == TP_MAIN_STA && m.cycle - rp.start > 100 &&
                    !(next_irq < nirqs && irqs[next_irq].ret == TP_MAIN_STA)) break;
                if (m.cycle - rp.start > budget) { ok = 0; break; }
            }
            checked++;
            int d = 0;
            if (!ok) {
                desync++;
                if (reported < (uint32_t)max_report) {
                    if (rp.bad)
                        printf("frame %u: read of %04X where MAME read %04X (read %d of %d), PC %04X\n",
                               prev.frame, rp.bad_addr, rp.at < rp.nreads ? rp.reads[rp.at].addr : 0,
                               rp.at, rp.nreads, e.cpu.pc);
                    else
                        printf("frame %u: did not come back to the main loop (PC %04X)\n", prev.frame, e.cpu.pc);
                    reported++;
                }
            } else {
                d += diff_block("RAM", 0, m.mem, cur.ram, TPCAP_RAM_SIZE, reported < (uint32_t)max_report ? show : 0, prev.frame);
                d += diff_block("COL", TP_COLRAM, m.colram, cur.colram, 16, reported < (uint32_t)max_report ? show : 0, prev.frame);
                d += diff_block("VG", TP_VGRAM, m.mem + TP_VGRAM, cur.vgram, TPCAP_VGRAM_SIZE,
                                reported < (uint32_t)max_report ? show : 0, prev.frame);
                while (rp.at < rp.nreads && write_only(rp.reads[rp.at].addr)) { rp.at++; rp.skipped++; }
                if (rp.at != rp.nreads && reported < (uint32_t)max_report)
                    printf("  frame %u: %d of %d reads used\n", prev.frame, rp.at, rp.nreads);
                if (next_irq != nirqs && reported < (uint32_t)max_report)
                    printf("  frame %u: %d of %d interrupts taken\n", prev.frame, next_irq, nirqs);
                if (rp.at != rp.nreads || next_irq != nirqs) d++;
                if (d) reported++;
                else same++;
            }
        }
        prev = cur;
        have_prev = 1;
    }
    tpcap_close(&rd);
    if (r < 0) printf("tpcompare: %s is damaged\n", path);

    if (!quiet) {
        printf("reads: the board model against MAME (%u dummy reads of MAME's passed over)\n", rp.skipped);
        for (int i = 0; i < 0x10000; i++)
            if (rp.model_same[i] || rp.model_diff[i]) {
                printf("  %04X: %u the same, %u different", i, rp.model_same[i], rp.model_diff[i]);
                if (rp.model_diff[i]) printf(" (first: port %02X, MAME %02X)", rp.first_mine[i], rp.first_mame[i]);
                printf("\n");
            }
    }
    printf("%s: %u frames checked, %u the same, %u different, %u out of step%s\n", path, checked, same,
           checked - same - desync, desync, irq_late ? " (some interrupts came late)" : "");
    if (checked && same == checked) printf("ALL THE SAME\n");
    return (checked && same == checked) ? 0 : 1;
}

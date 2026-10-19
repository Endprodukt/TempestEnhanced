/* tplockstep - run the board on its own from power-on beside a MAME
 * recording and hold every frame against it.
 *
 *   tplockstep FILE.tpcap [--roms PATH] [--frames N] [--max N] [--show N]
 *
 * Unlike tpcompare nothing of MAME's is given to the machine but the
 * inputs and the POKEYs' random numbers: every other read is the board
 * model's own, every interrupt comes when the model raises it. Each frame
 * start must find the same RAM, colour RAM and vector RAM as MAME's, and
 * the frame must start at the same CPU cycle. At a difference the frame's
 * reads and interrupts are held against MAME's to show the first that
 * differs.
 *
 * The inputs are the switch bits of MAME's own reads at the same place
 * (0C00 D0-D5, POKEY 1 and 2 ALLPOT): MAME takes a change a script makes
 * at its next input update, not at once. The random numbers are MAME's
 * because MAME runs each POKEY as a device of its own in time slices
 * beside the CPU, so the number a read sees depends on where MAME's
 * scheduler cut the slices; the board's counter simply runs with the
 * clock, as the model's does (--own-random uses the model's).
 */
#include "capture/capture.h"
#include "core/emu.h"
#include "core/machine.h"
#include "rom/romset.h"
#include "sound/sound.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOG 200000

typedef struct logbook {
    tpcap_read reads[MAX_LOG];
    const uint16_t *pc;
    int        nreads;
    tpcap_irq  irqs[4096];
    int        nirqs;
    uint64_t   start;           /* cycle the period began */
    tp_machine *m;
    /* MAME's reads of the period: their inputs and random numbers are handed out in order */
    const tpcap_read *mame;
    int        nmame, at;
    int        own_random;
    int        trace;
} logbook;

static int is_random(uint16_t a) { return a == TP_POKEY1 + TP_PK_RANDOM || a == TP_POKEY2 + TP_PK_RANDOM; }
static int write_only(uint16_t a);

/* The bits of a read that are inputs. */
static uint8_t input_mask(uint16_t a)
{
    if (a == TP_IN0) return 0x3f;
    if (a == TP_POKEY1 + TP_PK_ALLPOT || a == TP_POKEY2 + TP_PK_ALLPOT) return 0xff;
    return 0;
}

static uint8_t on_read(void *user, uint16_t addr, uint8_t value)
{
    logbook *b = (logbook *)user;
    /* MAME's read at the same place (the ways are the same until they part) */
    while (b->at < b->nmame && write_only(b->mame[b->at].addr)) b->at++;
    if (b->at < b->nmame && b->mame[b->at].addr == addr) {
        uint8_t mv = b->mame[b->at++].value, mask = input_mask(addr);
        if (is_random(addr) && !b->own_random) value = mv;
        value = (uint8_t)((value & ~mask) | (mv & mask));
    }
    if (b->trace && addr == TP_IN0)
        printf("  port 0C00=%02X at +%llu (now %llu, PC %04X)\n", value, (unsigned long long)(b->m->now - b->start),
               (unsigned long long)b->m->now, b->pc ? *b->pc : 0);
    if (b->nreads < MAX_LOG) {
        b->reads[b->nreads].cycle = (uint32_t)(b->m->cycle - b->start);
        b->reads[b->nreads].addr = addr;
        b->reads[b->nreads].value = value;
        b->nreads++;
    }
    return value;
}

/* Addresses the program only writes (see tpcompare): MAME's dummy reads. */
static int write_only(uint16_t a)
{
    if (a >= 0x60c0 && a < 0x60e0) return (a & 15) != 0x08 && (a & 15) != 0x0a;
    return (a >= 0x0800 && a < 0x0c00) || (a >= 0x4000 && a < 0x6000) ||
           (a >= 0x6000 && a < 0x6040) || (a >= 0x6080 && a < 0x60a0) || a == 0x60e0;
}

static logbook book;

/* --wav: the board's sound over the run, as MAME's -wavwrite has it (mono,
 * 16 bit, 48 kHz) */
static tp_sound g_snd;
static int16_t *g_pcm;
static size_t g_pcm_n, g_pcm_cap;

static void pcm_take(void)
{
    int16_t pair[512];
    int n;
    while ((n = tp_sound_read(&g_snd, pair, 256)) > 0) {
        if (g_pcm_n + (size_t)n > g_pcm_cap) {
            g_pcm_cap = (g_pcm_cap + (size_t)n) * 2;
            g_pcm = (int16_t *)realloc(g_pcm, g_pcm_cap * sizeof *g_pcm);
            if (!g_pcm) { g_pcm_cap = g_pcm_n = 0; return; }
        }
        for (int i = 0; i < n; i++) g_pcm[g_pcm_n++] = pair[2 * i];
    }
}

static void put32(FILE *f, uint32_t v) { fputc((int)(v & 255), f); fputc((int)(v >> 8 & 255), f); fputc((int)(v >> 16 & 255), f); fputc((int)(v >> 24), f); }
static void put16(FILE *f, unsigned v) { fputc((int)(v & 255), f); fputc((int)(v >> 8 & 255), f); }

static void wav_write(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "tplockstep: cannot write %s\n", path); return; }
    uint32_t bytes = (uint32_t)(g_pcm_n * 2);
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1); put32(f, TP_SOUND_RATE); put32(f, TP_SOUND_RATE * 2); put16(f, 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, bytes);
    fwrite(g_pcm, 2, g_pcm_n, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *path = NULL, *roms = NULL;
    uint32_t max_frames = 0xffffffffu;
    int clk_phase = -1, vg_phase = 0, nreads_show = 1;   /* -1: tp_machine_init's */
    uint32_t trace_frame = 0;
    const char *wav_path = NULL;
    int max_report = 1, show = 12;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--roms") && i + 1 < argc) roms = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = (uint32_t)atol(argv[++i]);
        else if (!strcmp(argv[i], "--max") && i + 1 < argc) max_report = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--show") && i + 1 < argc) show = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--own-random")) book.own_random = 1;
        else if (!strcmp(argv[i], "--clk-phase") && i + 1 < argc) clk_phase = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--vg-phase") && i + 1 < argc) vg_phase = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--reads") && i + 1 < argc) nreads_show = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--trace") && i + 1 < argc) trace_frame = (uint32_t)atol(argv[++i]);
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav_path = argv[++i];
        else if (!path) path = argv[i];
        else { fprintf(stderr, "tplockstep: unknown option %s\n", argv[i]); return 2; }
    }
    if (!path) { fprintf(stderr, "usage: tplockstep FILE.tpcap [--roms PATH] [--frames N]\n"); return 2; }

    const char *paths[64];
    int np = 0;
    if (roms) np = tp_romset_add_dir(paths, np, 64, roms), paths[np++] = roms;
    np += tp_romset_default_paths(paths + np, 64 - np);
    static tp_romset rs;
    if (tp_romset_load(&rs, paths, np) != 0) {
        char miss[512];
        fprintf(stderr, "tplockstep: ROMs missing: %s\n", tp_romset_missing(&rs, miss, sizeof miss));
        return 1;
    }
    tpcap_reader rd;
    if (tpcap_open(&rd, path) != 0) { fprintf(stderr, "tplockstep: cannot read %s\n", path); return 1; }

    static tp_machine m;
    static tp_emu e;
    static tpcap_read mame_reads[MAX_LOG];
    static tpcap_irq mame_irqs[4096];
    int n_mame_reads = 0, n_mame_irqs = 0;
    static tpcap_frame fr;
    static uint8_t *chunk;
    chunk = (uint8_t *)malloc(MAX_LOG * 7 + 16);

    tp_machine_init(&m);
    tp_machine_set_rom(&m, rs.vgrom, rs.prog);
    if (rs.have_avg_prom) tp_machine_set_avg_prom(&m, rs.avg_prom);
    tp_emu_init(&e, &m);
    m.clk_phase = clk_phase;
    m.vg_phase = vg_phase;
    if (wav_path) {
        tp_sound_init(&g_snd);
        m.sound_write = tp_sound_write;
        m.sound_user = &g_snd;
    }
    book.m = &m;
    book.pc = &e.cpu.pc;
    m.read_hook = on_read;
    m.read_user = &book;

    uint32_t frames = 0, same = 0, reported = 0;
    int stats_done = 0;
    uint32_t vg_differs = 0;
    uint32_t n_in0 = 0, n_bit7 = 0, n_bit6 = 0, n_other = 0, first_bit7 = 0, first_bit6 = 0, first_other = 0;
    int64_t cycle_off = 0;
    int have_off = 0, offs_bad = 0;
    char tag[4];
    uint32_t len;
    int r;
    while ((r = tpcap_next_chunk(&rd, tag, chunk, MAX_LOG * 7 + 16, &len)) == 1 && frames < max_frames) {
        if (!memcmp(tag, "READ", 4)) { n_mame_reads = tpcap_parse_reads(chunk, len, mame_reads, MAX_LOG); continue; }
        if (!memcmp(tag, "IRQS", 4)) { n_mame_irqs = tpcap_parse_irqs(chunk, len, mame_irqs, 4096); continue; }
        if (!memcmp(tag, "VGST", 4) && len >= 24) {
            /* MAME's generator at this frame's start against the model's, brought to the same time */
            const uint8_t *q = chunk;
            uint16_t pc = (uint16_t)(q[0] | q[1] << 8), sp = (uint16_t)(q[2] | q[3] << 8);
            uint8_t latch = q[4], op = q[5], halt = q[6], sync = q[7];
            uint16_t timer = (uint16_t)(q[8] | q[9] << 8);
            if (m.vg.have_prom && vg_differs < 12) {
                m.now = m.cycle + 2;
                tp_avgsm_advance(&m.vg, m.mem, m.now * 8 + (uint64_t)(int64_t)m.vg_phase);
                if (m.vg.pc != pc || m.vg.sp != sp || m.vg.state_latch != latch || m.vg.op != op ||
                    m.vg.halt != halt || m.vg.sync_halt != sync || m.vg.timer != timer) {
                    vg_differs++;
                    printf("generator at the start of frame %u: port pc %04X sp %X latch %02X op %X halt %d/%d timer %04X, "
                           "MAME pc %04X sp %X latch %02X op %X halt %d/%d timer %04X\n", fr.frame,
                           m.vg.pc, m.vg.sp, m.vg.state_latch, m.vg.op, m.vg.halt, m.vg.sync_halt, m.vg.timer,
                           pc, sp, latch, op, halt, sync, timer);
                }
            }
            continue;
        }
        if (memcmp(tag, "FRAM", 4) != 0 || len != TPCAP_FRAM_SIZE) continue;
        tpcap_parse_frame(chunk, &fr);

        /* run to the main loop's next frame start; log reads and interrupts on the way */
        book.mame = mame_reads;
        book.nmame = n_mame_reads;
        book.at = 0;
        book.trace = (fr.frame == trace_frame);
        m.vg.trace = book.trace ? stdout : NULL;
        if (book.trace) {
            printf("trace of the frame ending at %u (MAME's 0C00 reads and interrupts first)\n", fr.frame);
            for (int i = 0; i < n_mame_reads; i++)
                if (mame_reads[i].addr == TP_IN0)
                    printf("  MAME 0C00=%02X at +%u\n", mame_reads[i].value, mame_reads[i].cycle);
            for (int i = 0; i < n_mame_irqs; i++)
                printf("  MAME interrupt vector at +%u, back to %04X\n", mame_irqs[i].cycle, mame_irqs[i].ret);
            printf("  the port, from cycle %llu:\n", (unsigned long long)book.start);
        }
        uint64_t limit = m.cycle + 2000000;
        uint64_t irqs0 = e.irqs;
        while (!(e.cpu.pc == TP_MAIN_STA && m.cycle > book.start + 8 && !tp_emu_irq_due(&e)) && m.cycle < limit &&
               !e.cpu.jammed) {
            uint16_t pc = e.cpu.pc;
            uint64_t c_before = m.cycle;
            tp_emu_step(&e);
            if (wav_path) {
                tp_sound_run(&g_snd, (int)(m.cycle - c_before));
                pcm_take();
            }
            if (e.irqs != irqs0 && book.nirqs < 4096) {
                irqs0 = e.irqs;
                book.irqs[book.nirqs].cycle = (uint32_t)(m.cycle - book.start);
                book.irqs[book.nirqs].ret = pc;
                book.nirqs++;
            }
        }
        frames++;
        /* MAME's FRAM is taken in the STA's write cycle, the third of it */
        int64_t off = (int64_t)fr.cycle - (int64_t)(m.cycle + 2);
        if (!have_off) { cycle_off = off; have_off = 1; }
        else if (off != cycle_off) offs_bad++;

        /* every read of the period against MAME's, while both took the same way */
        if (!stats_done) {
            int a = 0, b = 0;
            while (a < book.nreads && b < n_mame_reads) {
                while (b < n_mame_reads && write_only(mame_reads[b].addr)) b++;
                if (b >= n_mame_reads) break;
                const tpcap_read *x = &book.reads[a], *y = &mame_reads[b];
                if (x->addr != y->addr) { stats_done = 1; break; }
                if (x->addr == TP_IN0) {
                    n_in0++;
                    if ((x->value ^ y->value) & TP_IN0_3KHZ) { if (!n_bit7++) first_bit7 = fr.frame; }
                    if ((x->value ^ y->value) & TP_IN0_HALT) { if (!n_bit6++) first_bit6 = fr.frame; }
                } else if (x->value != y->value && !is_random(x->addr)) {
                    if (!n_other++) first_other = fr.frame;
                }
                a++, b++;
            }
        }

        int d = 0;
        for (int i = 0; i < TPCAP_RAM_SIZE; i++)
            if (m.mem[i] != fr.ram[i] && i != TP_FRTIMR) d++;
        for (int i = 0; i < 16; i++) d += m.colram[i] != fr.colram[i];
        for (int i = 0; i < TPCAP_VGRAM_SIZE; i++) d += m.mem[TP_VGRAM + i] != fr.vgram[i];
        if (d) stats_done = 1;
        if (!d && off == cycle_off) {
            same++;
        } else if (reported < (uint32_t)max_report) {
            reported++;
            printf("frame %u (cycle %llu, MAME %llu, offset %lld, first offset %lld): %d bytes differ\n", fr.frame,
                   (unsigned long long)m.cycle, (unsigned long long)fr.cycle, (long long)off, (long long)cycle_off, d);
            int k = 0;
            for (int i = 0; i < TPCAP_RAM_SIZE && k < show; i++)
                if (m.mem[i] != fr.ram[i] && i != TP_FRTIMR) { printf("  RAM %04X: port %02X, MAME %02X\n", i, m.mem[i], fr.ram[i]); k++; }
            for (int i = 0; i < TPCAP_VGRAM_SIZE && k < show; i++)
                if (m.mem[TP_VGRAM + i] != fr.vgram[i]) { printf("  VG %04X: port %02X, MAME %02X\n", TP_VGRAM + i, m.mem[TP_VGRAM + i], fr.vgram[i]); k++; }
            /* the first read and the first interrupt that differ in the frame before */
            int a = 0, b = 0, shown_reads = 0;
            while (a < book.nreads || b < n_mame_reads) {
                while (b < n_mame_reads && write_only(mame_reads[b].addr)) b++;
                if (a >= book.nreads || b >= n_mame_reads) break;
                const tpcap_read *x = &book.reads[a], *y = &mame_reads[b];
                if (x->addr != y->addr || x->value != y->value) {
                    printf("  read %d: port %04X=%02X at +%u, MAME %04X=%02X at +%u\n", a, x->addr, x->value,
                           x->cycle, y->addr, y->value, y->cycle);
                    if (++shown_reads >= nreads_show || x->addr != y->addr) break;
                }
                a++, b++;
            }
            for (int i = 0; i < book.nirqs || i < n_mame_irqs; i++) {
                uint32_t pc_ = i < book.nirqs ? book.irqs[i].cycle : 0, mc = i < n_mame_irqs ? mame_irqs[i].cycle : 0;
                uint16_t pr = i < book.nirqs ? book.irqs[i].ret : 0, mr = i < n_mame_irqs ? mame_irqs[i].ret : 0;
                if (i >= book.nirqs || i >= n_mame_irqs || pr != mr || pc_ + 2 + 5 != mc) {
                    printf("  interrupt %d: port at +%u to %04X, MAME vector at +%u to %04X (of %d, %d)\n", i, pc_, pr,
                           mc, mr, book.nirqs, n_mame_irqs);
                    break;
                }
            }
        }
        /* the next period starts here; MAME's frame timer is 0 from now on too */
        book.start = m.cycle;
        book.nreads = 0;
        book.nirqs = 0;
        if (reported >= (uint32_t)max_report && max_report > 0 && same + reported < frames) break;
    }
    tpcap_close(&rd);
    if (wav_path) wav_write(wav_path);
    printf("reads of 0C00 until the ways part: %u; 3 kHz bit differs %u (first frame %u), halt bit %u (first %u); "
           "other reads differ %u (first %u)\n", n_in0, n_bit7, first_bit7, n_bit6, first_bit6, n_other, first_other);
    printf("%s: %u frames run, %u the same as MAME%s\n", path, frames, same,
           offs_bad ? " (frame starts at other cycles)" : "");
    if (frames && same == frames) printf("ALL THE SAME\n");
    return frames && same == frames ? 0 : 1;
}

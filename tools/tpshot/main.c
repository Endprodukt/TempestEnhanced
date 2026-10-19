/* tpshot - run Tempest without a screen and write its picture as SVG.
 *
 *   tpshot [--roms PATH] [--seconds S] [--play "T WHAT [ARG];..."] [--svg FILE] [--at T]
 *          [--layout vertical|horizontal]
 *
 * --play: a script of times (seconds) and actions: coin, start, start2,
 * fire, zap (held 0.1 s), spin N (turn the spinner N steps over the
 * next frames; negative turns the other way).
 * --at T: the picture at time T (default: the end).
 * --layout: the picture as the game program places it on the screen
 * (render/layout.h): native on an upright tube, or laid out for a
 * horizontal one; without it, the generator's own field.
 */
#include "avg/avg.h"
#include "core/emu.h"
#include "core/machine.h"
#include "render/layout.h"
#include "render/svg.h"
#include "rom/romset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct act { double t; char what[16]; int arg; } act;

static int parse_play(const char *s, act *out, int max)
{
    int n = 0;
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", s);
    for (char *e = strtok(buf, ";"); e && n < max; e = strtok(NULL, ";")) {
        act a;
        memset(&a, 0, sizeof a);
        int k = sscanf(e, "%lf %15s %d", &a.t, a.what, &a.arg);
        if (k >= 2) out[n++] = a;
    }
    return n;
}

int main(int argc, char **argv)
{
    const char *roms = NULL, *svg = "tpshot.svg", *play = "", *layout = NULL;
    double seconds = 10.0, at = -1.0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--roms") && i + 1 < argc) roms = argv[++i];
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atof(argv[++i]);
        else if (!strcmp(argv[i], "--play") && i + 1 < argc) play = argv[++i];
        else if (!strcmp(argv[i], "--svg") && i + 1 < argc) svg = argv[++i];
        else if (!strcmp(argv[i], "--at") && i + 1 < argc) at = atof(argv[++i]);
        else if (!strcmp(argv[i], "--layout") && i + 1 < argc) layout = argv[++i];
        else { fprintf(stderr, "tpshot: unknown option %s\n", argv[i]); return 2; }
    }
    if (at < 0 || at > seconds) at = seconds;

    const char *paths[64];
    int np = 0;
    if (roms) np = tp_romset_add_dir(paths, np, 64, roms), paths[np++] = roms;
    np = np < 64 ? np + tp_romset_default_paths(paths + np, 64 - np) : np;
    static tp_romset rs;
    if (tp_romset_load(&rs, paths, np) != 0) {
        char miss[512];
        fprintf(stderr, "tpshot: ROMs missing: %s\n", tp_romset_missing(&rs, miss, sizeof miss));
        return 1;
    }

    static tp_machine m;
    static tp_emu e;
    tp_machine_init(&m);
    tp_machine_set_rom(&m, rs.vgrom, rs.prog);
    if (rs.have_avg_prom) tp_machine_set_avg_prom(&m, rs.avg_prom);
    tp_emu_init(&e, &m);

    act acts[256];
    int na = parse_play(play, acts, 256);
    int spin_left = 0;
    uint64_t release_at[8] = { 0 };     /* coin, start, start2, fire, zap */
    const uint64_t hold = (uint64_t)(0.1 * TP_CPU_HZ);
    const uint64_t step = 2000;         /* cycles per slice */
    uint64_t end = (uint64_t)(seconds * TP_CPU_HZ), shot = (uint64_t)(at * TP_CPU_HZ);
    int done_act[256] = { 0 };
    tp_avg_list pic;
    memset(&pic, 0, sizeof pic);
    int have_pic = 0;
    uint32_t last_go = 0;

    while (m.cycle < end) {
        for (int i = 0; i < na; i++) {
            if (done_act[i] || m.cycle < (uint64_t)(acts[i].t * TP_CPU_HZ)) continue;
            done_act[i] = 1;
            const char *w = acts[i].what;
            if (!strcmp(w, "coin"))   { m.in.in0 &= (uint8_t)~TP_IN0_COIN_L; release_at[0] = m.cycle + hold; }
            if (!strcmp(w, "start"))  { m.in.start1 = 1; release_at[1] = m.cycle + hold; }
            if (!strcmp(w, "start2")) { m.in.start2 = 1; release_at[2] = m.cycle + hold; }
            if (!strcmp(w, "fire"))   { m.in.fire[0] = 1; release_at[3] = m.cycle + hold; }
            if (!strcmp(w, "zap"))    { m.in.zap[0] = 1; release_at[4] = m.cycle + hold; }
            if (!strcmp(w, "spin"))   spin_left = acts[i].arg;
        }
        if (release_at[0] && m.cycle >= release_at[0]) { m.in.in0 |= TP_IN0_COIN_L; release_at[0] = 0; }
        if (release_at[1] && m.cycle >= release_at[1]) { m.in.start1 = 0; release_at[1] = 0; }
        if (release_at[2] && m.cycle >= release_at[2]) { m.in.start2 = 0; release_at[2] = 0; }
        if (release_at[3] && m.cycle >= release_at[3]) { m.in.fire[0] = 0; release_at[3] = 0; }
        if (release_at[4] && m.cycle >= release_at[4]) { m.in.zap[0] = 0; release_at[4] = 0; }
        if (spin_left && (m.cycle % (TP_CYCLES_PER_IRQ * 2)) < step) {
            int d = spin_left > 0 ? 1 : -1;
            m.in.spinner[0] = (uint8_t)((m.in.spinner[0] + d) & 0x0f);
            spin_left -= d;
        }
        tp_emu_run(&e, step);
        if (e.cpu.jammed) { fprintf(stderr, "tpshot: CPU stopped at %04X\n", e.cpu.pc); break; }
        /* the picture: the list as the program has it each time it starts the generator */
        if (m.vg_starts != last_go && m.cycle <= shot + step) {
            last_go = m.vg_starts;
            tp_avg_run(m.mem, m.colram, TP_VGRAM, m.coinout & 0x08, m.coinout & 0x10, &pic, 0);
            have_pic = 1;
        }
    }
    printf("tpshot: %.1f s, %llu interrupts, %u frames, %u generator starts, %d lines in the picture, PC %04X\n",
           (double)m.cycle / TP_CPU_HZ, (unsigned long long)e.irqs, e.frames, m.vg_starts,
           have_pic ? pic.count : 0, e.cpu.pc);
    printf("tpshot: state %02X, credits %02X, frame counter %02X\n", m.mem[0x00], m.mem[0x06], m.mem[0x04]);
    if (have_pic && layout) {
        static tp_config cfg;
        tp_config_defaults(&cfg);
        cfg.orientation = !strcmp(layout, "horizontal") ? TP_ORIENT_HORIZONTAL : TP_ORIENT_VERTICAL;
        tp_line *placed = (tp_line *)malloc((size_t)(pic.count + TP_LAYOUT_EXTRA) * sizeof *placed);
        int n = tp_layout(&cfg, pic.lines, pic.count, placed);
        if (tp_svg_write_board(svg, placed, n, cfg.orientation == TP_ORIENT_HORIZONTAL ? 4.0f / 3.0f : 3.0f / 4.0f) != 0)
            fprintf(stderr, "tpshot: cannot write %s\n", svg);
        free(placed);
    } else
    if (have_pic && tp_svg_write(svg, pic.lines, pic.count) != 0) fprintf(stderr, "tpshot: cannot write %s\n", svg);
    tp_avg_list_free(&pic);
    return 0;
}

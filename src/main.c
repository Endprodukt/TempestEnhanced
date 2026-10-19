/* tempest - Tempest (Atari, 1981) on the board of src/core, in real time.
 *
 * Finds the ROM set (the program and the vector ROM are read from it),
 * then runs the original program on the 6502 core against the board, as
 * fast as the cabinet (1.512 MHz, the interrupt 246 times a second), with
 * the sound of its two POKEYs, and shows what its vector generator draws:
 * natively for an upright monitor, or laid out for a horizontal one
 * (render/layout.h), with pictures between the game's frames
 * (render/smooth.h).
 *
 *   tempest [--roms PATH] [--config INI] [--nosound] [--hz N]
 *           [--dvg [PORT] | --window | --windowed] [--vertical] [--shot FILE.svg]
 *
 *   --shot  at the end, the last picture shown as SVG (and what the game is doing)
 *
 * Settings: tempest.ini next to the program, the owner's in save/; the
 * service menu (F2, platform/menu.h) changes them. Controls:
 * platform/controls.h. The EAROM (high scores, the bookkeeping) is kept
 * in save/earom.bin.
 */
#include "avg/avg.h"
#include "core/emu.h"
#include "core/hiscore.h"
#include "core/machine.h"
#include "platform/app.h"
#include "platform/audio.h"
#include "platform/controls.h"
#include "platform/menu.h"
#include "render/config.h"
#include "render/draw.h"
#include "render/enhance.h"
#include "render/logo.h"
#include "render/fireworks.h"
#include "render/frames.h"
#include "render/layout.h"
#include "render/nextlevel.h"
#include "render/svg.h"
#include "render/title.h"
#include "rom/romset.h"
#include "sound/sound.h"

#include <vectorpie/vp_credits.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATHS 48

/* More than this much real time behind (a stall, a debugger) is dropped
 * rather than caught up with. */
#define MAX_CATCHUP_S 0.25
#define SLICE 1000              /* CPU cycles the game runs between looks at its pictures */

/* RAM of the program (revision 3) the port looks at */
#define RAM_QSTATE   0x0000     /* the state routine running */
#define RAM_QNXTSTA  0x0002     /* the state after a pause */
#define RAM_QTMPAUS  0x0004     /* the pause's time left */
#define RAM_QSTATUS  0x0005     /* D7: a game is played (clear: the attract mode) */
#define ST_GAME      0x80
#define RAM_CURSL1   0x0200     /* the segment the claw is on */
#define ST_PAUSE     0x0a
#define ST_PLAY      0x04
#define ST_INITIALS  0x12       /* initials are entered */
#define ST_RATING    0x16       /* the player chooses the level */

enum { MODE_GAME, MODE_MENU, MODE_SPIN_TEST, MODE_PATTERN };

static int rom_paths(int argc, char **argv, const tp_config *cfg, const char **paths)
{
    int n = 0;
    const char *arg = tp_arg(argc, argv, "--roms");
    if (arg) {
        n = tp_romset_add_dir(paths, n, MAX_PATHS, arg);
        paths[n++] = arg;
    }
    if (cfg->rom_path[0]) {
        n = tp_romset_add_dir(paths, n, MAX_PATHS, cfg->rom_path);
        paths[n++] = cfg->rom_path;
    }
    /* The cabinet's own ROM folders: AdvanceMAME's dir_rom. */
    char rcpath[1024];
    vp_rc *rc = NULL;
    if (vp_rc_default_path(rcpath, sizeof rcpath) == VP_OK && vp_rc_load(rcpath, &rc) == VP_OK) {
        const char *dirs = vp_rc_get(rc, "dir_rom");
        if (dirs) {
            static char copy[2048];
            snprintf(copy, sizeof copy, "%s", dirs);
            for (char *p = strtok(copy, ":;"); p && n < MAX_PATHS; p = strtok(NULL, ":;"))
                n = tp_romset_add_dir(paths, n, MAX_PATHS, p);
        }
        vp_rc_free(rc);
    }
    n += tp_romset_default_paths(paths + n, MAX_PATHS - n);
    return n;
}

static void earom_load(tp_earom *e, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return;
    uint8_t buf[64];
    if (fread(buf, 1, sizeof buf, f) == sizeof buf) memcpy(e->rom, buf, sizeof buf);
    fclose(f);
}

static void earom_save(tp_earom *e, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "tempest: cannot write %s\n", path);
        return;
    }
    fwrite(e->rom, 1, sizeof e->rom, f);
    fclose(f);
    e->dirty = 0;
}

/* The port's own high score table (core/hiscore.h): the menu's file. Once
 * the program has set up its table after power-on, the file's best are
 * put back into it; from then on the program's table is merged into the
 * file's. Only while the game is played or in its attract mode (states 04
 * and 0A), never while initials are entered (the name is not complete). */
static void hiscores_update(tp_machine *m)
{
    static tp_hiscore kept[TP_HISCORE_KEEP];
    static int nkept = -1;                      /* -1: the file not read yet */
    uint8_t state = m->mem[RAM_QSTATE];
    if (state != ST_PLAY && state != ST_PAUSE) return;
    tp_hiscore ram[TP_HISCORE_PLACES], merged[TP_HISCORE_KEEP];
    memset(ram, 0, sizeof ram);
    memset(merged, 0, sizeof merged);
    int n = tp_hiscore_read(m->mem, ram);
    if (n <= 0) return;
    int first = nkept < 0;
    if (first) nkept = tp_hiscore_load(vp_hiscore_path(), kept, TP_HISCORE_KEEP);
    int nm = tp_hiscore_merge(kept, nkept, ram, n, merged, TP_HISCORE_KEEP);
    if (first) tp_hiscore_put(m->mem, merged, nm);
    int changed = nm != nkept;
    for (int i = 0; !changed && i < nm; i++)
        changed = merged[i].score != kept[i].score || strcmp(merged[i].name, kept[i].name) != 0;
    if (!changed) return;
    if (tp_hiscore_write(merged, nm, vp_hiscore_path()) != 0) {
        fprintf(stderr, "tempest: cannot write %s\n", vp_hiscore_path());
        return;
    }
    memcpy(kept, merged, sizeof merged);
    nkept = nm;
}

/* The settings into the machine. */
static void apply_settings(const tp_config *cfg, tp_machine *m, tp_sound *snd)
{
    m->in.dsw1 = cfg->dsw1;
    m->in.dsw2 = cfg->dsw2;
    m->in.diff = (uint8_t)(cfg->difficulty & 3);
    m->in.rating = (uint8_t)(cfg->rating != 0);
    m->in.cocktail = (uint8_t)(cfg->cocktail != 0);
    if (snd) snd->volume = cfg->volume;
}

/* The game's frames on a fixed beat or the cabinet's own (core/emu.h). */
static void apply_pace(const tp_config *cfg, tp_emu *e)
{
    if ((e->steady != 0) != (cfg->steady_frames != 0)) tp_emu_steady(e, cfg->steady_frames != 0);
}

/* --- running the game ----------------------------------------------------------- */

/* Run `cycles`, the spinner's turn of this picture spread over them (the
 * game takes at most 7 counts an interrupt), the coin switches as pulses. */
static void run_game(tp_machine *m, tp_emu *e, tp_sound *snd, int have_sound, tp_frames *fr, int smooth,
                     double *spin_pos, double spin, double *coin_left, double cycles)
{
    double total = cycles > 1.0 ? cycles : 1.0;
    while (cycles > 0.0 && !e->cpu.jammed) {
        double slice = cycles < SLICE ? cycles : SLICE;
        *spin_pos += spin * slice / total;
        m->in.spinner[0] = (uint8_t)((long)floor(*spin_pos) & 0x0f);
        uint8_t in0 = 0x3f;
        if (coin_left[0] > 0.0) { in0 &= (uint8_t)~TP_IN0_COIN_L; coin_left[0] -= slice / TP_CPU_HZ; }
        if (coin_left[1] > 0.0) { in0 &= (uint8_t)~TP_IN0_COIN_C; coin_left[1] -= slice / TP_CPU_HZ; }
        m->in.in0 = in0;
        uint64_t c0 = m->cycle;
        tp_emu_run(e, (uint64_t)slice + 1);
        int done = (int)(m->cycle - c0);
        cycles -= done;
        if (have_sound) tp_sound_run(snd, done);
        if (fr) tp_frames_check(fr, m, e, smooth);
    }
}

/* --- the spinner's test: level 1 without enemies ------------------------------- */

typedef struct spin_test {
    tp_machine m;
    tp_emu     e;
    int        ready;
    double     spin_pos;
    int        segment;     /* the claw's segment as last seen */
    int        moved;       /* segments moved since fire was last pressed */
    tp_avg_list pic;
} spin_test;

/* Power on, a coin, start, level 1, and on to the pause before its first
 * life: the claw is on the tube, no enemy has come yet. The pause is then
 * held; the claw still follows the spinner (the game's pause moves it). */
static int spin_test_prepare(spin_test *t, const tp_romset *roms, const tp_config *cfg)
{
    tp_machine *m = &t->m;
    tp_machine_init(m);
    tp_machine_set_rom(m, roms->vgrom, roms->prog);
    if (roms->have_avg_prom) tp_machine_set_avg_prom(m, roms->avg_prom);
    apply_settings(cfg, m, NULL);
    m->in.dsw1 = 0x00;                  /* (whatever the owner's coinage: one coin, one play) */
    tp_emu_init(&t->e, m);
    tp_emu_run(&t->e, (uint64_t)(3.0 * TP_CPU_HZ));
    m->in.in0 &= (uint8_t)~TP_IN0_COIN_L;
    tp_emu_run(&t->e, (uint64_t)(0.1 * TP_CPU_HZ));
    m->in.in0 |= TP_IN0_COIN_L;
    tp_emu_run(&t->e, (uint64_t)(1.0 * TP_CPU_HZ));
    m->in.start1 = 1;
    tp_emu_run(&t->e, (uint64_t)(0.1 * TP_CPU_HZ));
    m->in.start1 = 0;
    for (int i = 0; i < 200 && !(m->mem[RAM_QSTATE] == ST_PAUSE && m->mem[RAM_QNXTSTA] == ST_PLAY); i++) {
        m->in.fire[0] = i == 20;        /* the choice of level: the first, level 1 */
        tp_emu_run(&t->e, (uint64_t)(0.1 * TP_CPU_HZ));
    }
    m->in.fire[0] = 0;
    t->ready = m->mem[RAM_QSTATE] == ST_PAUSE && m->mem[RAM_QNXTSTA] == ST_PLAY;
    t->segment = m->mem[RAM_CURSL1] & 0x0f;
    t->moved = 0;
    t->spin_pos = 0.0;
    return t->ready ? 0 : -1;
}

static void spin_test_run(spin_test *t, const tp_controls *ctl, double dt)
{
    tp_machine *m = &t->m;
    double cycles = dt * TP_CPU_HZ, total = cycles > 1.0 ? cycles : 1.0;
    double spin = (double)ctl->raw * ctl->sens;     /* the spinner alone: the digital turning is for the menu */
    while (cycles > 0.0) {
        double slice = cycles < SLICE ? cycles : SLICE;
        t->spin_pos += spin * slice / total;
        m->in.spinner[0] = (uint8_t)((long)floor(t->spin_pos) & 0x0f);
        if (m->mem[RAM_QSTATE] == ST_PAUSE) m->mem[RAM_QTMPAUS] = 0x20;     /* the pause goes on */
        uint64_t c0 = m->cycle;
        tp_emu_run(&t->e, (uint64_t)slice + 1);
        cycles -= (double)(m->cycle - c0);
        int seg = m->mem[RAM_CURSL1] & 0x0f;
        if (seg != t->segment) {
            int d = ((seg - t->segment + 8) & 15) - 8;
            t->moved += d;
            t->segment = seg;
        }
    }
    tp_avg_list l;
    memset(&l, 0, sizeof l);
    tp_avg_run(m->mem, m->colram, TP_VGRAM, m->coinout & 0x08, m->coinout & 0x10, &l, 0);
    if (l.count > 0) {                  /* (while the buffers are swapped, the last stays) */
        tp_avg_list_free(&t->pic);
        t->pic = l;
    } else {
        tp_avg_list_free(&l);
    }
}

/* --- the test pattern ------------------------------------------------------------- */

static void pattern_line(vp_dvg *d, int x0, int y0, int x1, int y1)
{
    vp_moveto(d, x0, y0);
    vp_lineto(d, x1, y1);
}

static void draw_pattern(vp_dvg *d, const tp_config *cfg)
{
    int x[4], y[4], a, b, c, e;
    int horizontal = cfg->orientation == TP_ORIENT_HORIZONTAL;
    /* the edge of the picture, its middle, and a circle that is round when
     * the proportions are right */
    static const float U[4] = { -0.64f, 0.64f, 0.64f, -0.64f }, V[4] = { -0.48f, -0.48f, 0.48f, 0.48f };
    static const float X[4] = { -290.0f, 290.0f, 290.0f, -290.0f }, Y[4] = { -290.0f, -290.0f, 290.0f, 290.0f };
    vp_color(d, 255, 255, 255);
    for (int i = 0; i < 4; i++) {
        if (horizontal) tp_layout_screen(cfg, U[i], V[i], &x[i], &y[i]);
        else tp_layout_point(cfg, X[i], Y[i], &x[i], &y[i]);
    }
    for (int i = 0; i < 4; i++) pattern_line(d, x[i], y[i], x[(i + 1) & 3], y[(i + 1) & 3]);
    float k = horizontal ? 0.05f : 20.0f;
    if (horizontal) { tp_layout_screen(cfg, -k, 0.0f, &a, &b); tp_layout_screen(cfg, k, 0.0f, &c, &e); }
    else { tp_layout_point(cfg, -k, 0.0f, &a, &b); tp_layout_point(cfg, k, 0.0f, &c, &e); }
    pattern_line(d, a, b, c, e);
    if (horizontal) { tp_layout_screen(cfg, 0.0f, -k, &a, &b); tp_layout_screen(cfg, 0.0f, k, &c, &e); }
    else { tp_layout_point(cfg, 0.0f, -k, &a, &b); tp_layout_point(cfg, 0.0f, k, &c, &e); }
    pattern_line(d, a, b, c, e);
    vp_color(d, 0, 200, 255);
    for (int i = 0; i < 64; i++) {
        float p0 = (float)i / 64.0f * 6.2831853f, p1 = (float)(i + 1) / 64.0f * 6.2831853f;
        if (horizontal) {
            tp_layout_screen(cfg, 0.4f * cosf(p0), 0.4f * sinf(p0), &a, &b);
            tp_layout_screen(cfg, 0.4f * cosf(p1), 0.4f * sinf(p1), &c, &e);
        } else {
            tp_layout_point(cfg, 200.0f * cosf(p0), 200.0f * sinf(p0), &a, &b);
            tp_layout_point(cfg, 200.0f * cosf(p1), 200.0f * sinf(p1), &c, &e);
        }
        pattern_line(d, a, b, c, e);
    }
    /* every brightness the game uses, in its blue */
    for (int i = 1; i <= 15; i++) {
        float g = powf((float)i / 15.0f, cfg->gamma) * cfg->brightness;
        if (g > 1.0f) g = 1.0f;
        vp_color(d, 0, 0, (uint8_t)(243.0f * g));
        int bx = 900 + (i - 1) * 160;
        pattern_line(d, bx, 1300, bx + 110, 1300);
        pattern_line(d, bx, 1330, bx + 110, 1330);
    }
    tp_menu_text(d, "PICTURE TEST PATTERN", 2900, 90, 1);
    tp_menu_text(d, "SIZE AND POSITION ON THE PICTURE PAGE", 1050, 60, 0);
    tp_menu_text(d, "FIRE OR ZAP OR ESC BACK", 900, 60, 0);
}

/* --- main ---------------------------------------------------------------------------- */

static tp_line *g_placed;
static int g_placed_cap;
static tp_fireworks g_fireworks;

/* The lines laid out and drawn; fx (NULL: none) adds the screen's own
 * enhancements, dt seconds since the last picture. */
static void show(vp_dvg *d, const tp_config *cfg, const tp_avg_list *pic, tp_enhance *fx, double dt)
{
    if (g_placed_cap < 3 * pic->count + TP_LAYOUT_EXTRA + TP_ENHANCE_SCREEN) {   /* (the fireworks add copies) */
        free(g_placed);
        g_placed_cap = 3 * pic->count + 512;
        g_placed = (tp_line *)malloc((size_t)g_placed_cap * sizeof *g_placed);
        if (!g_placed) { g_placed_cap = 0; return; }
    }
    /* a high score's fireworks across the width of a horizontal screen */
    if (cfg->orientation == TP_ORIENT_HORIZONTAL && cfg->fx[TP_FX_FIREWORKS] && tp_fireworks_in(pic)) {
        int n = tp_fireworks_layout(&g_fireworks, cfg, pic, g_placed, g_placed_cap);
        tp_draw_lines(d, cfg, g_placed, n);
        return;
    }
    tp_fireworks_reset(&g_fireworks);
    int n = tp_layout(cfg, pic->lines, pic->count, g_placed);
    if (fx) n += tp_enhance_screen(fx, cfg, g_placed, n, dt);
    tp_draw_lines(d, cfg, g_placed, n);
}

int main(int argc, char **argv)
{
    static tp_config cfg;
    tp_config_defaults(&cfg);
    char cfg_path[1024], buf[1024];
    const char *arg = tp_arg(argc, argv, "--config");
    if (arg) {
        snprintf(cfg_path, sizeof cfg_path, "%s", arg);
    } else {
        /* the package's defaults, then the owner's own in save/ */
        tp_config_load(&cfg, vp_asset_path("tempest.ini", buf, sizeof buf));
        snprintf(cfg_path, sizeof cfg_path, "%s/tempest.ini", vp_save_dir());
    }
    tp_config_load(&cfg, cfg_path);

    static tp_romset roms;
    const char *paths[MAX_PATHS];
    int npaths = rom_paths(argc, argv, &cfg, paths);
    if (tp_romset_load(&roms, paths, npaths) != 0) {
        char missing[512];
        fprintf(stderr, "tempest: ROMs missing: %s (tempest.zip, MAME set \"tempest\"; --roms PATH)\n",
                tp_romset_missing(&roms, missing, sizeof missing));
        return 1;
    }
    printf("tempest: ROM set complete%s\n", roms.have_avg_prom ? "" : " (no 136002-125: the vector generator's timing is approximate)");

    tp_app app;
    if (tp_app_open(&app, "Tempest Enhanced", argc, argv, cfg.board_hz) != 0) return 1;

    static tp_machine m;
    static tp_emu emu;
    static tp_sound snd;
    static tp_controls ctl;
    static tp_menu menu;
    static tp_frames frames;
    static tp_nextlevel nextlevel;
    static tp_enhance fx;
    static tp_logo logo;
    static spin_test test;
    static spin_test still;         /* level 1 standing still, behind the picture page */
    int still_ready = 0;            /* 1 ready, -1 could not be reached */
    static tp_avg_list pic;

    char ea_path[1024];
    snprintf(ea_path, sizeof ea_path, "%s/earom.bin", vp_save_dir());

    tp_machine_init(&m);
    tp_machine_set_rom(&m, roms.vgrom, roms.prog);
    if (roms.have_avg_prom) tp_machine_set_avg_prom(&m, roms.avg_prom);
    earom_load(&m.earom, ea_path);
    tp_emu_init(&emu, &m);
    tp_frames_init(&frames);
    tp_nextlevel_init(&nextlevel);
    tp_enhance_init(&fx);
    tp_logo_init(&logo);
    tp_controls_init(&ctl, &cfg);
    menu.capture = -1;

    int have_sound = 0;
    if (!tp_flag(argc, argv, "--nosound")) {
        tp_sound_init(&snd);
        m.sound_write = tp_sound_write;
        m.sound_user = &snd;
        have_sound = tp_audio_open(&snd) == 0;
    }
    apply_settings(&cfg, &m, &snd);
    apply_pace(&cfg, &emu);

    int mode = MODE_GAME;
    double spin_pos = 0.0, ea_dirty_since = -1.0, clock = 0.0;
    double sens_repeat_at = 0.0;
    double title_since = -1.0;      /* when the attract mode's title was complete, -1: it is not */
    int sens_dir = 0;

    while (!app.quit) {
        double dt;
        if (!tp_app_poll(&app, &dt)) break;
        if (dt > MAX_CATCHUP_S) dt = MAX_CATCHUP_S;
        clock += dt;
        tp_controls_read(&ctl, app.in, dt);

        if (ctl.menu && menu.capture < 0) {
            if (mode == MODE_GAME) {
                mode = MODE_MENU;
                tp_menu_open(&menu);
                app.hold_cancel = 1;                /* Esc goes back in the menu, it does not end */
            } else {
                tp_config_save(&cfg, cfg_path);     /* the menu's key again: save, back to the game */
                mode = MODE_GAME;
                menu.open = 0;
                app.hold_cancel = 0;
            }
        }

        tp_app_begin_frame(&app);
        switch (mode) {
        case MODE_GAME:
            m.in.fire[0] = (uint8_t)ctl.fire;
            m.in.zap[0] = (uint8_t)ctl.zap;
            m.in.start1 = (uint8_t)ctl.start1;
            m.in.start2 = (uint8_t)ctl.start2;
            run_game(&m, &emu, &snd, have_sound, &frames, cfg.smooth, &spin_pos, ctl.spin, ctl.coin_left,
                     dt * TP_CPU_HZ);
            tp_frames_picture(&frames, m.cycle, cfg.smooth, &pic);
            /* the START lamps as the game sets them (60E0: D1 player 1, D0
             * player 2, lit when clear) */
            vp_lamps_set(((m.leds & 0x02) ? 0u : (unsigned)VP_LAMP_START1) |
                         ((m.leds & 0x01) ? 0u : (unsigned)VP_LAMP_START2));
            tp_nextlevel_update(&nextlevel, &m, &emu, &pic);
            tp_layout_set_attract(!(m.mem[RAM_QSTATUS] & ST_GAME));
            tp_layout_set_entry(m.mem[RAM_QSTATE] == ST_INITIALS);
            tp_layout_set_rating(m.mem[RAM_QSTATE] == ST_RATING);
            show(app.dvg, &cfg, tp_enhance_picture(&fx, &cfg, tp_logo_picture(&logo, &cfg, &pic), m.mem, dt), &fx, dt);
            /* ENHANCED under the attract mode's title, once it has come all the way */
            if (cfg.fx[TP_FX_TITLE] && tp_title_complete(&pic)) {
                if (title_since < 0.0) title_since = clock;
                tp_title_draw(app.dvg, &cfg, clock - title_since);
            } else {
                title_since = -1.0;
            }
            break;
        case MODE_MENU:
            switch (tp_menu_update(&menu, &cfg, &ctl, app.in, clock)) {
            case TP_MENU_CHANGED:
                tp_controls_configure(&ctl, &cfg);
                apply_settings(&cfg, &m, &snd);
                apply_pace(&cfg, &emu);
                if (app.on_board) app.frame_hz = cfg.board_hz;     /* (the picture page's BOARD RATE) */
                break;
            case TP_MENU_RESET_SCORES:
                tp_earom_init(&m.earom);
                earom_save(&m.earom, ea_path);
                break;
            case TP_MENU_SPIN_TEST:
                if (spin_test_prepare(&test, &roms, &cfg) == 0) mode = MODE_SPIN_TEST;
                else fprintf(stderr, "tempest: the spinner's test could not reach level 1\n");
                break;
            case TP_MENU_PATTERN:
                mode = MODE_PATTERN;
                break;
            case TP_MENU_CLOSE:
                tp_config_save(&cfg, cfg_path);
                mode = MODE_GAME;
                menu.open = 0;
                app.hold_cancel = 0;
                break;
            }
            if (mode == MODE_MENU && tp_menu_on_picture(&menu)) {
                /* behind the picture page: level 1 standing still, half as
                 * bright, laid out as the settings say now */
                if (!still_ready) {
                    still_ready = spin_test_prepare(&still, &roms, &cfg) == 0 ? 1 : -1;
                    if (still_ready > 0)
                        tp_avg_run(still.m.mem, still.m.colram, TP_VGRAM, still.m.coinout & 0x08,
                                   still.m.coinout & 0x10, &still.pic, 0);
                }
                if (still_ready > 0) {
                    tp_config dim = cfg;
                    dim.brightness *= 0.5f;
                    tp_layout_set_attract(0);
                    tp_layout_set_flight(0);
                    show(app.dvg, &dim, &still.pic, NULL, 0.0);
                }
            }
            if (mode == MODE_MENU) tp_menu_draw(&menu, &cfg, app.dvg);
            break;
        case MODE_SPIN_TEST: {
            /* left and right change the sensitivity, fire counts from 0 again */
            int dx = ctl.nav.x;
            if (dx && (dx != sens_dir || clock >= sens_repeat_at)) {
                sens_repeat_at = clock + (dx != sens_dir ? 0.4 : 0.06);
                cfg.spin_sens += 0.1f * (float)dx;
                if (cfg.spin_sens < 0.5f) cfg.spin_sens = 0.5f;
                if (cfg.spin_sens > 100.0f) cfg.spin_sens = 100.0f;
                tp_controls_configure(&ctl, &cfg);
            }
            sens_dir = dx;
            if (ctl.nav.fire) test.moved = 0;
            if (ctl.nav.back) mode = MODE_MENU;
            spin_test_run(&test, &ctl, dt);
            tp_layout_set_attract(0);
            tp_layout_set_flight(0);
            show(app.dvg, &cfg, &test.pic, NULL, 0.0);
            char line[96];
            tp_menu_text(app.dvg, "SPINNER TEST  ONE TURN SHOULD MOVE THE CLAW 5 SEGMENTS", 3950, 62, 1);
            snprintf(line, sizeof line, "MOVED %d SEGMENTS", test.moved);
            tp_menu_text(app.dvg, line, 3830, 62, 0);
            snprintf(line, sizeof line, "SENSITIVITY %.1f", cfg.spin_sens);
            tp_menu_text(app.dvg, line, 260, 62, 1);
            tp_menu_text(app.dvg, "LEFT RIGHT SENSITIVITY   FIRE FROM 0   ZAP OR ESC BACK", 140, 52, 0);
            break;
        }
        default:
            draw_pattern(app.dvg, &cfg);
            if (ctl.nav.fire || ctl.nav.back) mode = MODE_MENU;
            break;
        }
        if (tp_app_end_frame(&app) != 0) break;
        if (emu.cpu.jammed) {
            fprintf(stderr, "tempest: the CPU stopped at %04X\n", emu.cpu.pc);
            break;
        }
        if (have_sound) {
            snd.volume = cfg.volume;
            tp_audio_balance(&snd);
        }

        /* the EAROM: saved a second after the program last changed it */
        if (m.earom.dirty) {
            if (ea_dirty_since < 0.0) ea_dirty_since = clock;
            else if (clock - ea_dirty_since > 1.0) {
                earom_save(&m.earom, ea_path);
                ea_dirty_since = -1.0;
            }
        }

        /* the high scores: looked at once a second (the program sets its
         * table up at power-on, from the EAROM) */
        if (floor(clock) != floor(clock - dt) && clock > 3.0) hiscores_update(&m);
    }

    if (m.earom.dirty) earom_save(&m.earom, ea_path);
    const char *shot = tp_arg(argc, argv, "--shot");
    if (shot) {
        if (tp_svg_write(shot, pic.lines, pic.count) != 0) fprintf(stderr, "tempest: cannot write %s\n", shot);
        printf("tempest: %.1f s run, %llu interrupts, %u pictures, %d lines in the last, state %02X\n",
               (double)m.cycle / TP_CPU_HZ, (unsigned long long)emu.irqs, frames.pictures, pic.count, m.mem[0x00]);
    }
    if (have_sound) tp_audio_close();
    tp_controls_close(&ctl);
    tp_app_close(&app);
    tp_frames_free(&frames);
    tp_nextlevel_free(&nextlevel);
    tp_enhance_free(&fx);
    tp_logo_free(&logo);
    tp_avg_list_free(&pic);
    tp_avg_list_free(&test.pic);
    tp_avg_list_free(&still.pic);
    free(g_placed);
    return 0;
}

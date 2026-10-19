/* menu.c - see menu.h. */
#include "platform/menu.h"
#include "platform/vpbind.h"

#include <SDL.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum {
    L_GAP = 0,
    L_PAGE_CONTROLS, L_PAGE_SPINNER, L_PAGE_PICTURE, L_PAGE_GAME, L_PAGE_FX, L_VOLUME, L_RESET_SCORES, L_EXIT, L_BACK,
    L_MENU_HOLD, L_DEFAULTS,
    L_SENS, L_SPIN_TEST,
    L_ORIENT, L_SMOOTH, L_STEADY, L_BOARD_HZ, L_SIZE, L_WIDTH, L_HEIGHT, L_POS_X, L_POS_Y, L_FLIP_X, L_FLIP_Y, L_BRIGHT, L_PATTERN,
    L_FX_ALL_ON, L_FX_ALL_OFF,
    L_FX0,                      /* + TP_FX_xxx: an enhancement on or off */
    L_DIP0 = L_FX0 + TP_FX_COUNT,   /* + D_xxx: an option switch */
    L_BIND0 = L_DIP0 + 16,      /* + TP_ACT_xxx */
    L_END = 0xff
};

enum { P_MAIN, P_CONTROLS, P_SPINNER, P_PICTURE, P_GAME, P_FX, N_PAGES };
static const char *const TITLE[N_PAGES] = { "TEMPEST ENHANCED  SERVICE MENU", "CONTROLS", "SPINNER", "PICTURE", "GAME SETTINGS",
                                            "ENHANCEMENTS" };

static const char *const FX_LABEL[TP_FX_COUNT] = {
    "LANE GLOW", "WARP STARFIELD", "ZAPPER FLASH", "SHOT TRAILS", "EXPLOSION SPARKS", "CLAW BREAKS APART", "ENEMIES BREAK APART",
    "NO OLD RIM IN THE FLIGHT",
    "STARS BESIDE THE TUBE", "WIDE FIREWORKS", "ENHANCED UNDER THE TITLE", "RECHARGE IN THE MIDDLE", "MARQUEE TITLE LETTERING",
};

static const char *const BIND_LABEL[TP_ACT_COUNT] = {
    "SPINNER", "TURN LEFT", "TURN RIGHT", "FIRE", "SUPERZAPPER", "START 1", "START 2", "COIN LEFT", "COIN CENTER",
    "SERVICE MENU",
};

/* --- the option switches, by what they mean (MAME's tempest.cpp) --- */
enum { D_LIVES, D_BONUS, D_DIFF, D_RATING, D_LANG, D_MIN, D_COINAGE, D_RIGHT, D_LEFT, D_BONUSCOIN, D_CABINET, D_COUNT };
enum { W_DSW1, W_DSW2, W_DIFF, W_RATING, W_COCKTAIL };
typedef struct dip {
    const char *label;
    int where, mask, n;
    int value[8];
    const char *name[8];
} dip;
static const dip DIPS[D_COUNT] = {
    { "LIVES", W_DSW2, 0xc0, 4, { 0xc0, 0x00, 0x40, 0x80 }, { "2", "3", "4", "5" } },
    { "BONUS LIFE", W_DSW2, 0x38, 8, { 0x08, 0x00, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38 },
      { "10000", "20000", "30000", "40000", "50000", "60000", "70000", "NONE" } },
    { "DIFFICULTY", W_DIFF, 3, 4, { 2, 3, 0, 1 }, { "EASY", "MEDIUM 1", "MEDIUM 2", "HARD" } },
    { "START LEVELS", W_RATING, 1, 2, { 1, 0 }, { "1 3 5 7 9", "TIED TO HIGH SCORE" } },
    { "LANGUAGE", W_DSW2, 0x06, 4, { 0x00, 0x02, 0x04, 0x06 }, { "ENGLISH", "FRENCH", "GERMAN", "SPANISH" } },
    { "MINIMUM", W_DSW2, 0x01, 2, { 0x00, 0x01 }, { "1 CREDIT", "2 CREDITS" } },
    { "COINAGE", W_DSW1, 0x03, 4, { 0x02, 0x01, 0x00, 0x03 }, { "FREE PLAY", "2 COINS 1 CREDIT", "1 COIN 1 CREDIT", "1 COIN 2 CREDITS" } },
    { "RIGHT COIN", W_DSW1, 0x0c, 4, { 0x00, 0x04, 0x08, 0x0c }, { "X1", "X4", "X5", "X6" } },
    { "LEFT COIN", W_DSW1, 0x10, 2, { 0x00, 0x10 }, { "X1", "X2" } },
    { "BONUS COINS", W_DSW1, 0xe0, 7, { 0x00, 0x80, 0x40, 0xa0, 0x60, 0x20, 0xc0 },
      { "NONE", "1 EACH 5", "1 EACH 4 AND DEMO", "1 EACH 3", "2 EACH 4 AND DEMO", "1 EACH 2", "FREEZE MODE" } },
    { "CABINET", W_COCKTAIL, 1, 2, { 0, 1 }, { "UPRIGHT", "COCKTAIL" } },
};

static int dip_get(const tp_config *c, int where)
{
    switch (where) {
    case W_DSW1:     return c->dsw1;
    case W_DSW2:     return c->dsw2;
    case W_DIFF:     return c->difficulty;
    case W_RATING:   return c->rating;
    default:         return c->cocktail;
    }
}

static void dip_set(tp_config *c, int where, int v)
{
    switch (where) {
    case W_DSW1:     c->dsw1 = (uint8_t)v; break;
    case W_DSW2:     c->dsw2 = (uint8_t)v; break;
    case W_DIFF:     c->difficulty = v; break;
    case W_RATING:   c->rating = v; break;
    default:         c->cocktail = v; break;
    }
}

static int dip_index(const tp_config *c, const dip *d)
{
    int v = dip_get(c, d->where) & d->mask;
    if (d->mask == 0xe0 && v == 0xe0) v = 0xc0;     /* (both freeze settings are one) */
    for (int i = 0; i < d->n; i++)
        if (d->value[i] == v) return i;
    return 0;
}

/* --- the pages -------------------------------------------------------------------- */

#define PAGE_MAX 24

static int page_lines(int page, uint8_t *out)
{
    int n = 0;
#define ADD(l) out[n++] = (uint8_t)(l)
    switch (page) {
    case P_CONTROLS:
        ADD(L_BIND0 + TP_ACT_SPINNER); ADD(L_BIND0 + TP_ACT_SPIN_LEFT); ADD(L_BIND0 + TP_ACT_SPIN_RIGHT); ADD(L_GAP);
        for (int a = TP_ACT_FIRE; a <= TP_ACT_MENU; a++) ADD(L_BIND0 + a);
        ADD(L_MENU_HOLD); ADD(L_GAP); ADD(L_DEFAULTS); ADD(L_BACK);
        break;
    case P_SPINNER:
        ADD(L_SENS); ADD(L_SPIN_TEST); ADD(L_GAP); ADD(L_BACK);
        break;
    case P_PICTURE:
        ADD(L_ORIENT); ADD(L_SMOOTH); ADD(L_STEADY); ADD(L_BOARD_HZ); ADD(L_GAP);
        ADD(L_SIZE); ADD(L_WIDTH); ADD(L_HEIGHT); ADD(L_POS_X); ADD(L_POS_Y); ADD(L_FLIP_X); ADD(L_FLIP_Y);
        ADD(L_BRIGHT); ADD(L_GAP); ADD(L_PATTERN); ADD(L_BACK);
        break;
    case P_FX:
        for (int f = 0; f < TP_FX_COUNT; f++) ADD(L_FX0 + f);
        ADD(L_GAP); ADD(L_FX_ALL_ON); ADD(L_FX_ALL_OFF); ADD(L_GAP); ADD(L_BACK);
        break;
    case P_GAME:
        for (int d = D_LIVES; d <= D_MIN; d++) ADD(L_DIP0 + d);
        ADD(L_GAP);
        for (int d = D_COINAGE; d <= D_CABINET; d++) ADD(L_DIP0 + d);
        ADD(L_GAP); ADD(L_BACK);
        break;
    default:
        ADD(L_PAGE_CONTROLS); ADD(L_PAGE_SPINNER); ADD(L_PAGE_PICTURE); ADD(L_PAGE_GAME); ADD(L_PAGE_FX); ADD(L_GAP);
        ADD(L_VOLUME); ADD(L_RESET_SCORES); ADD(L_GAP); ADD(L_EXIT);
        break;
    }
#undef ADD
    out[n] = L_END;
    return n;
}

static int chosen_line(const uint8_t *lines, int item, int *count)
{
    int k = 0, line = L_GAP;
    for (int i = 0; lines[i] != L_END; i++) {
        if (lines[i] == L_GAP) continue;
        if (k == item) line = lines[i];
        k++;
    }
    *count = k;
    return line;
}

static int step(int v, int d, int lo, int hi)
{
    v += d;
    return v > hi ? lo : v < lo ? hi : v;
}

static float stepf(float v, int d, float by, float lo, float hi)
{
    v += (float)d * by;
    return v < lo ? lo : v > hi ? hi : v;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int tp_menu_on_picture(const tp_menu *u)
{
    return u->open && u->page == P_PICTURE;
}

void tp_menu_open(tp_menu *u)
{
    u->open = 1;
    u->page = P_MAIN;
    u->capture = -1;
    u->adding = 0;
    u->confirm = 0;
    u->dir_x = u->dir_y = 0;
}

int tp_menu_update(tp_menu *u, tp_config *cfg, tp_controls *ctl, vp_input *in, double now)
{
    if (u->page < 0 || u->page >= N_PAGES) u->page = P_MAIN;

    if (u->capture >= 0) {
        char spec[TP_BIND_SIZE];
        int r = tp_controls_capture_poll(ctl, in, spec, sizeof spec);
        if (r == 0) return TP_MENU_NOTHING;
        int action = u->capture;
        u->capture = -1;
        /* (what was pressed is still held: it is not to count as a push) */
        u->dir_x = ctl->nav.x;
        u->dir_y = ctl->nav.y;
        u->repeat_at = now + 0.6;
        ctl->fire_was = 1;
        ctl->back_was = 1;
        if (r < 0) return TP_MENU_NOTHING;
        if (r == 2) {                   /* none or auto */
            u->adding = 0;
            if (action == TP_ACT_MENU && !strcmp(spec, "none")) return TP_MENU_NOTHING;   /* (never none: F2 must stay) */
            snprintf(cfg->bind[action], TP_BIND_SIZE, "%s", spec);
            return TP_MENU_CHANGED;
        }
        /* The first since the cursor came to the line replaces what was
         * bound; every further one is added (what is there still counts). */
        if (u->adding != action + 1 || !strcmp(cfg->bind[action], "auto") || !strcmp(cfg->bind[action], "none")) {
            snprintf(cfg->bind[action], TP_BIND_SIZE, "%s", spec);
            u->adding = action + 1;
            return TP_MENU_CHANGED;
        }
        tp_config_bind_add(cfg, action, spec);
        return TP_MENU_CHANGED;
    }

    uint8_t lines[PAGE_MAX + 1];
    int n;
    page_lines(u->page, lines);
    int line = chosen_line(lines, u->item[u->page], &n);
    if (u->item[u->page] >= n) {
        u->item[u->page] = 0;
        line = chosen_line(lines, 0, &n);
    }

    /* Delete on a binding (also the keypad's): back to auto */
    if ((vpb_key_pressed(SDL_SCANCODE_DELETE) || vpb_key_pressed(SDL_SCANCODE_KP_PERIOD)) && line >= L_BIND0) {
        u->adding = 0;
        if (!strcmp(cfg->bind[line - L_BIND0], "auto")) return TP_MENU_NOTHING;
        snprintf(cfg->bind[line - L_BIND0], TP_BIND_SIZE, "auto");
        return TP_MENU_CHANGED;
    }

    if (ctl->nav.back) {
        u->adding = 0;
        if (u->page == P_MAIN) return TP_MENU_CLOSE;
        u->page = P_MAIN;
        return TP_MENU_NOTHING;
    }

    /* a push counts once, and again and again while it is held */
    int x = ctl->nav.x, y = ctl->nav.y, dx = 0, dy = 0;
    if (x != u->dir_x || y != u->dir_y) {
        dx = x != u->dir_x ? x : 0;
        dy = y != u->dir_y ? y : 0;
        u->repeat_at = now + 0.45;
    } else if ((x || y) && now >= u->repeat_at) {
        dx = x;
        dy = y;
        u->repeat_at = now + 0.08;
    }
    u->dir_x = x;
    u->dir_y = y;
    dy += ctl->nav.spin;
    int fire = ctl->nav.fire;

    if (dy) {
        u->item[u->page] = clampi(u->item[u->page] + dy, 0, n - 1);
        u->confirm = 0;
        u->adding = 0;                  /* the next binding replaces again */
        return TP_MENU_NOTHING;
    }
    int d = dx ? dx : (fire ? 1 : 0);
    if (!d) return TP_MENU_NOTHING;

    if (line >= L_BIND0) {
        int action = line - L_BIND0;
        if (fire) {
            u->capture = action;
            tp_controls_capture_begin(ctl, action);
            return TP_MENU_NOTHING;
        }
        /* left, right: auto, then none, then auto again (the service menu
         * never none: the cabinet could not reach the menu any more) */
        const char *to = strcmp(cfg->bind[action], "none") || action == TP_ACT_MENU ? "none" : "auto";
        if (action == TP_ACT_MENU) to = "auto";
        u->adding = 0;
        if (!strcmp(cfg->bind[action], to)) return TP_MENU_NOTHING;
        snprintf(cfg->bind[action], TP_BIND_SIZE, "%s", to);
        return TP_MENU_CHANGED;
    }
    if (line >= L_FX0 && line < L_FX0 + TP_FX_COUNT) {
        cfg->fx[line - L_FX0] = !cfg->fx[line - L_FX0];
        return TP_MENU_CHANGED;
    }
    if (line >= L_DIP0 && line < L_DIP0 + D_COUNT) {
        const dip *p = &DIPS[line - L_DIP0];
        int i = step(dip_index(cfg, p), d, 0, p->n - 1);
        dip_set(cfg, p->where, (dip_get(cfg, p->where) & ~p->mask) | p->value[i]);
        return TP_MENU_CHANGED;
    }
    switch (line) {
    case L_PAGE_CONTROLS: if (fire) { u->page = P_CONTROLS; u->adding = 0; } break;
    case L_PAGE_SPINNER:  if (fire) u->page = P_SPINNER; break;
    case L_PAGE_PICTURE:  if (fire) u->page = P_PICTURE; break;
    case L_PAGE_GAME:     if (fire) u->page = P_GAME; break;
    case L_PAGE_FX:       if (fire) u->page = P_FX; break;
    case L_FX_ALL_ON:
    case L_FX_ALL_OFF:
        if (!fire) break;
        for (int f = 0; f < TP_FX_COUNT; f++) cfg->fx[f] = line == L_FX_ALL_ON;
        return TP_MENU_CHANGED;
    case L_BACK:          if (fire) u->page = P_MAIN; break;
    case L_EXIT:          if (fire) return TP_MENU_CLOSE; break;
    case L_VOLUME:        cfg->volume = stepf(cfg->volume, d, 0.1f, 0.0f, 1.0f); return TP_MENU_CHANGED;
    case L_RESET_SCORES:
        if (!fire) break;
        if (!u->confirm) { u->confirm = 1; break; }
        u->confirm = 0;
        return TP_MENU_RESET_SCORES;
    case L_MENU_HOLD:     cfg->menu_hold = !cfg->menu_hold; return TP_MENU_CHANGED;
    case L_DEFAULTS:
        if (!fire) break;
        for (int i = 0; i < TP_ACT_COUNT; i++) snprintf(cfg->bind[i], TP_BIND_SIZE, "auto");
        return TP_MENU_CHANGED;
    case L_SENS:          cfg->spin_sens = stepf(cfg->spin_sens, d, 0.1f, 0.5f, 100.0f); return TP_MENU_CHANGED;
    case L_SPIN_TEST:     if (fire) return TP_MENU_SPIN_TEST; break;
    case L_ORIENT:        cfg->orientation = !cfg->orientation; return TP_MENU_CHANGED;
    case L_SMOOTH:        cfg->smooth = !cfg->smooth; return TP_MENU_CHANGED;
    case L_STEADY:        cfg->steady_frames = !cfg->steady_frames; return TP_MENU_CHANGED;
    case L_BOARD_HZ:      cfg->board_hz = stepf(cfg->board_hz, d, 5.0f, 40.0f, 60.0f); return TP_MENU_CHANGED;
    case L_SIZE:
        cfg->size_x = stepf(cfg->size_x, d, 0.01f, 0.5f, 1.5f);
        cfg->size_y = stepf(cfg->size_y, d, 0.01f, 0.5f, 1.5f);
        return TP_MENU_CHANGED;
    case L_WIDTH:         cfg->size_x = stepf(cfg->size_x, d, 0.01f, 0.5f, 1.5f); return TP_MENU_CHANGED;
    case L_HEIGHT:        cfg->size_y = stepf(cfg->size_y, d, 0.01f, 0.5f, 1.5f); return TP_MENU_CHANGED;
    case L_POS_X:         cfg->pos_x = clampi(cfg->pos_x + d * 16, -1024, 1024); return TP_MENU_CHANGED;
    case L_POS_Y:         cfg->pos_y = clampi(cfg->pos_y + d * 16, -1024, 1024); return TP_MENU_CHANGED;
    case L_FLIP_X:        cfg->flip_x = !cfg->flip_x; return TP_MENU_CHANGED;
    case L_FLIP_Y:        cfg->flip_y = !cfg->flip_y; return TP_MENU_CHANGED;
    case L_BRIGHT:        cfg->brightness = stepf(cfg->brightness, d, 0.05f, 0.3f, 2.0f); return TP_MENU_CHANGED;
    case L_PATTERN:       if (fire) return TP_MENU_PATTERN; break;
    }
    return TP_MENU_NOTHING;
}

/* --- drawing ------------------------------------------------------------------------ */

static void describe(int line, const tp_menu *u, const tp_config *cfg, const char **label, char *value, size_t size)
{
    value[0] = '\0';
    if (line >= L_BIND0) {
        int action = line - L_BIND0;
        *label = BIND_LABEL[action];
        if (u->capture == action) {
            snprintf(value, size, "%s", action == TP_ACT_SPINNER ? "TURN IT OR MOVE THE MOUSE" : "PRESS THE BUTTON");
        } else {
            /* all of them by what they are ("LEFT CTRL + JOY1 BUTTON 1"),
             * " + " between them, cut short if too long */
            size_t k = 0;
            value[0] = '\0';
            for (const char *p = cfg->bind[action]; *p;) {
                const char *q = strstr(p, " or ");
                size_t len = q ? (size_t)(q - p) : strlen(p);
                char one[64], word[64], rest[16];
                int j, n;
                snprintf(one, sizeof one, "%.*s", (int)(len < 63 ? len : 63), p);
                if (!strncmp(one, "key ", 4)) snprintf(word, sizeof word, "%s", one + 4);
                else if (sscanf(one, "joy%d button %d", &j, &n) == 2) snprintf(word, sizeof word, "joy%d button %d", j, n + 1);
                else if (sscanf(one, "joy%d axis %d %15s", &j, &n, rest) == 3) snprintf(word, sizeof word, "joy%d axis %d %s", j, n + 1, rest);
                else if (sscanf(one, "joy%d hat %d %15s", &j, &n, rest) == 3) snprintf(word, sizeof word, "joy%d hat %s", j, rest);
                else if (!strncmp(one, "mouse button ", 13)) snprintf(word, sizeof word, "mouse %s", one + 13);
                else snprintf(word, sizeof word, "%s", one);
                if (k && k + 3 < size) { memcpy(value + k, " + ", 3); k += 3; }
                for (const char *w = word; *w && k + 1 < size; w++) value[k++] = (char)toupper((unsigned char)*w);
                value[k] = '\0';
                if (!q) break;
                p = q + 4;
            }
            if (k > 44) strcpy(value + 41, "...");
        }
        return;
    }
    if (line >= L_FX0 && line < L_FX0 + TP_FX_COUNT) {
        *label = FX_LABEL[line - L_FX0];
        snprintf(value, size, "%s", cfg->fx[line - L_FX0] ? "ON" : "OFF");
        return;
    }
    if (line >= L_DIP0 && line < L_DIP0 + D_COUNT) {
        const dip *p = &DIPS[line - L_DIP0];
        *label = p->label;
        snprintf(value, size, "%s", p->name[dip_index(cfg, p)]);
        return;
    }
    switch (line) {
    case L_PAGE_CONTROLS: *label = "CONTROLS"; break;
    case L_PAGE_SPINNER:  *label = "SPINNER"; break;
    case L_PAGE_PICTURE:  *label = "PICTURE"; break;
    case L_PAGE_GAME:     *label = "GAME SETTINGS"; break;
    case L_PAGE_FX:       *label = "ENHANCEMENTS"; break;
    case L_FX_ALL_ON:     *label = "ALL ON"; break;
    case L_FX_ALL_OFF:    *label = "ALL OFF  AS THE CABINET"; break;
    case L_VOLUME:        *label = "VOLUME"; snprintf(value, size, "%d", (int)(cfg->volume * 10.0f + 0.5f)); break;
    case L_RESET_SCORES:  *label = "RESET HIGH SCORES"; snprintf(value, size, "%s", u->confirm ? "START AGAIN" : ""); break;
    case L_EXIT:          *label = "SAVE AND EXIT"; break;
    case L_BACK:          *label = "BACK"; break;
    case L_MENU_HOLD:     *label = "START 1 AND FIRE HELD"; snprintf(value, size, "%s", cfg->menu_hold ? "OPENS MENU" : "OFF"); break;
    case L_DEFAULTS:      *label = "ALL BACK TO AUTO"; break;
    case L_SENS:          *label = "SENSITIVITY"; snprintf(value, size, "%.1f", cfg->spin_sens); break;
    case L_SPIN_TEST:     *label = "TEST ON LEVEL 1"; break;
    case L_ORIENT:        *label = "SCREEN"; snprintf(value, size, "%s", cfg->orientation ? "HORIZONTAL" : "VERTICAL"); break;
    case L_SMOOTH:        *label = "SMOOTH MOTION"; snprintf(value, size, "%s", cfg->smooth ? "ON" : "OFF"); break;
    case L_BOARD_HZ:      *label = "BOARD RATE"; snprintf(value, size, "%d PICTURES A SECOND", (int)(cfg->board_hz + 0.5f)); break;
    case L_STEADY:        *label = "GAME FRAMES"; snprintf(value, size, "%s", cfg->steady_frames ? "STEADY" : "AS THE CABINET"); break;
    case L_SIZE:          *label = "SIZE"; snprintf(value, size, "%d", (int)(cfg->size_x * 100.0f + 0.5f)); break;
    case L_WIDTH:         *label = "WIDTH"; snprintf(value, size, "%d", (int)(cfg->size_x * 100.0f + 0.5f)); break;
    case L_HEIGHT:        *label = "HEIGHT"; snprintf(value, size, "%d", (int)(cfg->size_y * 100.0f + 0.5f)); break;
    case L_POS_X:         *label = "POSITION LEFT RIGHT"; snprintf(value, size, "%d", cfg->pos_x); break;
    case L_POS_Y:         *label = "POSITION DOWN UP"; snprintf(value, size, "%d", cfg->pos_y); break;
    case L_FLIP_X:        *label = "MIRROR LEFT RIGHT"; snprintf(value, size, "%s", cfg->flip_x ? "ON" : "OFF"); break;
    case L_FLIP_Y:        *label = "MIRROR UP DOWN"; snprintf(value, size, "%s", cfg->flip_y ? "ON" : "OFF"); break;
    case L_BRIGHT:        *label = "BRIGHTNESS"; snprintf(value, size, "%d", (int)(cfg->brightness * 100.0f + 0.5f)); break;
    case L_PATTERN:       *label = "TEST PATTERN"; break;
    default:              *label = "?"; break;
    }
}

void tp_menu_text(vp_dvg *d, const char *s, float y, float h, int highlight)
{
    vp_text_font(VP_FONT_ASTEROIDS);
    if (highlight) vp_color(d, 255, 220, 60);
    else vp_color(d, 150, 160, 255);
    vp_text(d, s, 2048, y, h, VP_TEXT_CENTER);
}

void tp_menu_draw(const tp_menu *u, const tp_config *cfg, vp_dvg *d)
{
    int page = u->page >= 0 && u->page < N_PAGES ? u->page : P_MAIN;
    const float STEP = 170.0f, GAP = 90.0f, H = 78.0f;
    const float LEFT = 520.0f, RIGHT = 3600.0f;
    uint8_t lines[PAGE_MAX + 1];
    page_lines(page, lines);

    vp_text_font(VP_FONT_ASTEROIDS);
    vp_color(d, 255, 220, 60);
    vp_text(d, TITLE[page], 2048, 3740, 95, VP_TEXT_CENTER);

    float height = 0.0f;
    for (int i = 0; lines[i] != L_END; i++) height += lines[i] ? STEP : GAP;
    float y = 2048.0f + height * 0.5f - STEP * 0.5f + 60.0f;
    int k = 0, chosen_line_id = L_GAP;
    for (int i = 0; lines[i] != L_END; i++) {
        int line = lines[i];
        if (!line) { y -= GAP; continue; }
        const char *label;
        char value[TP_BIND_SIZE + 16];
        describe(line, u, cfg, &label, value, sizeof value);
        int chosen = k == u->item[page];
        if (chosen) {
            chosen_line_id = line;
            vp_color(d, 255, 255, 255);
            vp_moveto(d, (int)LEFT - 160, (int)y + 70);
            vp_lineto(d, (int)LEFT - 70, (int)y + 38);
            vp_lineto(d, (int)LEFT - 160, (int)y + 6);
        } else {
            vp_color(d, 60, 200, 90);
        }
        vp_text(d, label, LEFT, y, H, VP_TEXT_LEFT);
        if (value[0]) {
            float room = RIGHT - LEFT - vp_text_width(label, H) - 150.0f, w = vp_text_width(value, H), h = H;
            if (w > room && w > 0.0f) h = H * room / w;
            if (chosen) vp_color(d, 255, 255, 255);
            else vp_color(d, 80, 220, 255);
            if (u->capture >= 0 && chosen) vp_color(d, 255, 60, 50);
            vp_text(d, value, RIGHT, y, h, VP_TEXT_RIGHT);
        }
        y -= STEP;
        k++;
    }
    vp_color(d, 110, 130, 255);
    const char *one = "SPINNER OR UP DOWN CHOOSE   LEFT RIGHT CHANGE", *two = "START 1 OR ENTER DO   ESC BACK   F2 SAVE";
    if (u->capture >= 0) {
        int cabinet_line = u->capture >= TP_ACT_START1 && u->capture <= TP_ACT_MENU;
        one = u->adding == u->capture + 1 ? "PRESS THE BUTTON TO ADD" : "PRESS THE BUTTON";
        two = cabinet_line ? "ENTER - BACK   DEL - AUTO" : "START 1 - BACK  DEL - AUTO  START 2 - NONE";
    } else if (chosen_line_id >= L_BIND0) {
        one = "LEFT RIGHT - AUTO OR NONE";
        two = "START, ENTER - ADD BUTTON   DEL - AUTO";
    }
    vp_text(d, one, 2048, 300, 50, VP_TEXT_CENTER);
    vp_text(d, two, 2048, 200, 50, VP_TEXT_CENTER);
}

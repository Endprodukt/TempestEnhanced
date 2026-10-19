#include "platform/controls.h"
#include "platform/vpbind.h"

#include <SDL.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COIN_PULSE_S 0.1        /* the coin switch closed this long: well inside the coin routine's 16..800 ms */
#define NAV_COUNTS   40.0       /* spinner counts (the owner's) for a step through a menu */

enum { B_AUTO = 0, B_NONE, B_KEY, B_BUTTON, B_AXIS, B_HAT, B_MOUSE_X, B_MOUSE_Y, B_MOUSE_BUTTON };

static const char *const HAT_NAME[4] = { "up", "right", "down", "left" };
static const char *const MOUSE_BUTTON[3] = { "left", "middle", "right" };
static const uint8_t HAT_BIT[4] = { SDL_HAT_UP, SDL_HAT_RIGHT, SDL_HAT_DOWN, SDL_HAT_LEFT };

/* --- bindings as text ------------------------------------------------------------ */

static void parse_bind(const char *spec, tp_bind *b)
{
    int joy, k;
    char word[16];
    memset(b, 0, sizeof *b);
    b->type = B_AUTO;
    if (!strcmp(spec, "none")) {
        b->type = B_NONE;
    } else if (!strcmp(spec, "mouse x")) {
        b->type = B_MOUSE_X;
    } else if (!strcmp(spec, "mouse y")) {
        b->type = B_MOUSE_Y;
    } else if (!strncmp(spec, "mouse button ", 13)) {
        for (int i = 0; i < 3; i++)
            if (!strcmp(spec + 13, MOUSE_BUTTON[i])) {
                b->type = B_MOUSE_BUTTON;
                b->index = (int16_t)(i + 1);        /* SDL_BUTTON_LEFT ... */
            }
    } else if (!strncmp(spec, "key ", 4)) {
        SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
        if (spec[4] == '#') sc = (SDL_Scancode)atoi(spec + 5);
        else sc = SDL_GetScancodeFromName(spec + 4);
        if (sc > SDL_SCANCODE_UNKNOWN && sc < SDL_NUM_SCANCODES) {
            b->type = B_KEY;
            b->index = (int16_t)sc;
        }
    } else if (sscanf(spec, "joy%d button %d", &joy, &k) == 2 && joy >= 1 && joy <= TP_MAX_JOYSTICKS) {
        b->type = B_BUTTON;
        b->joy = (uint8_t)(joy - 1);
        b->index = (int16_t)k;
    } else if (sscanf(spec, "joy%d axis %d %1s", &joy, &k, word) == 3 && joy >= 1 && joy <= TP_MAX_JOYSTICKS) {
        b->type = B_AXIS;
        b->joy = (uint8_t)(joy - 1);
        b->index = (int16_t)k;
        b->dir = word[0] == '-' ? -1 : 1;
    } else if (sscanf(spec, "joy%d hat %d %15s", &joy, &k, word) == 3 && joy >= 1 && joy <= TP_MAX_JOYSTICKS) {
        for (int i = 0; i < 4; i++)
            if (!strcmp(word, HAT_NAME[i])) {
                b->type = B_HAT;
                b->joy = (uint8_t)(joy - 1);
                b->index = (int16_t)k;
                b->dir = (int8_t)HAT_BIT[i];
            }
    }
}

/* Bindings as written ("auto or joy2 button 3"): each of the " or " parts. */
static int parse_binds(const char *spec, tp_bind *out)
{
    int n = 0;
    for (const char *p = spec; n < TP_BIND_ALTS;) {
        const char *q = strstr(p, " or ");
        char one[TP_BIND_SIZE];
        size_t len = q ? (size_t)(q - p) : strlen(p);
        if (len >= sizeof one) len = sizeof one - 1;
        memcpy(one, p, len);
        one[len] = '\0';
        parse_bind(one, &out[n]);
        if (out[n].type != B_NONE || n == 0) n++;   /* ("none" only alone) */
        if (!q) break;
        p = q + 4;
    }
    if (n > 1 && out[0].type == B_NONE) {           /* (a "none" first and something after it) */
        memmove(out, out + 1, (size_t)(n - 1) * sizeof *out);
        n--;
    }
    return n;
}

/* --- joysticks ------------------------------------------------------------------- */

static void open_joysticks(tp_controls *c)
{
    /* (SDL counts its opens: these are the SDK's joysticks once more) */
    for (int i = 0; i < c->joys; i++)
        if (c->joy[i]) SDL_JoystickClose((SDL_Joystick *)c->joy[i]);
    c->joys = SDL_NumJoysticks();
    if (c->joys > TP_MAX_JOYSTICKS) c->joys = TP_MAX_JOYSTICKS;
    if (c->joys < 0) c->joys = 0;
    for (int i = 0; i < c->joys; i++) c->joy[i] = SDL_JoystickOpen(i);
}

static float axis_of(const tp_controls *c, int joy, int axis)
{
    if (joy >= c->joys || !c->joy[joy]) return 0.0f;
    return (float)SDL_JoystickGetAxis((SDL_Joystick *)c->joy[joy], axis) / 32767.0f;
}

/* --- reading --------------------------------------------------------------------- */

/* A binding of the program's own: 0..1 (an axis: -1..1, the way that
 * counts positive). */
static float bound(const tp_controls *c, const tp_bind *b, const Uint8 *keys)
{
    SDL_Joystick *j = b->joy < c->joys ? (SDL_Joystick *)c->joy[b->joy] : NULL;
    switch (b->type) {
    case B_KEY:    return keys[b->index] ? 1.0f : 0.0f;
    case B_BUTTON: return j && SDL_JoystickGetButton(j, b->index) ? 1.0f : 0.0f;
    case B_HAT:    return j && (SDL_JoystickGetHat(j, b->index) & (uint8_t)b->dir) ? 1.0f : 0.0f;
    case B_AXIS:   return axis_of(c, b->joy, b->index) * (float)b->dir;
    case B_MOUSE_BUTTON: return SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(b->index) ? 1.0f : 0.0f;
    default:       return 0.0f;
    }
}

/* What "auto" is, read directly as every other binding (not by advmame.rc's
 * names): the keys, the first stick and the mouse's buttons AdvanceMAME
 * gives these by default. The spinner's auto is the mouse, which the SDK
 * reads (p1_dialx). */
static const char *const AUTO_SPEC[TP_ACT_COUNT] = {
    "",
    "key Left or joy1 axis 0 - or joy1 hat 0 left",
    "key Right or joy1 axis 0 + or joy1 hat 0 right",
    "key Left Ctrl or joy1 button 0 or mouse button left",
    "key Left Alt or joy1 button 1 or mouse button right",
    "key 1", "key 2", "key 5", "key 6",
    "",
};
static tp_bind g_auto[TP_ACT_COUNT][TP_BIND_ALTS];
static int g_nauto[TP_ACT_COUNT];

/* A switch: any of its bindings, "auto" the ones above. */
static int held(const tp_controls *c, vp_input *in, const Uint8 *keys, int act)
{
    (void)in;
    for (int k = 0; k < c->nbind[act]; k++) {
        const tp_bind *b = &c->bind[act][k];
        if (b->type == B_AUTO) {
            for (int a = 0; a < g_nauto[act]; a++)
                if (bound(c, &g_auto[act][a], keys) > 0.5f) return 1;
        } else if (bound(c, b, keys) > 0.5f) {
            return 1;
        }
    }
    return 0;
}

/* The spinner's counts since the last read: of all its bindings. */
static int spinner_one(const tp_controls *c, const tp_bind *b, vp_input *in, const Uint8 *keys, double dt)
{
    switch (b->type) {
    case B_AUTO:    return vp_input_delta(in, "p1_dialx");
    case B_MOUSE_X: return vp_input_delta(in, "p1_trackballx");
    case B_MOUSE_Y: return vp_input_delta(in, "p1_trackbally");
    case B_AXIS: {
        /* a stick as a spinner: turned at most two turns' worth a second */
        float v = bound(c, b, keys);
        if (fabsf(v) < 0.15f) return 0;
        return (int)lround((double)v * 2.0 * 1200.0 * dt);
    }
    default:        return 0;
    }
}

static int spinner_counts(const tp_controls *c, vp_input *in, const Uint8 *keys, double dt)
{
    int n = 0;
    for (int k = 0; k < c->nbind[TP_ACT_SPINNER]; k++) n += spinner_one(c, &c->bind[TP_ACT_SPINNER][k], in, keys, dt);
    return n;
}

void tp_controls_init(tp_controls *c, const tp_config *cfg)
{
    memset(c, 0, sizeof *c);
    c->cap_action = -1;
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    open_joysticks(c);
    tp_controls_configure(c, cfg);
}

void tp_controls_close(tp_controls *c)
{
    for (int i = 0; i < c->joys; i++)
        if (c->joy[i]) SDL_JoystickClose((SDL_Joystick *)c->joy[i]);
    c->joys = 0;
}

void tp_controls_configure(tp_controls *c, const tp_config *cfg)
{
    for (int i = 0; i < TP_ACT_COUNT; i++) {
        c->nbind[i] = parse_binds(cfg->bind[i], c->bind[i]);
        g_nauto[i] = AUTO_SPEC[i][0] ? parse_binds(AUTO_SPEC[i], g_auto[i]) : 0;
    }
    c->sens = cfg->spin_sens / 100.0f * (cfg->spin_reverse ? -1.0f : 1.0f);
    c->key_rate = cfg->key_speed * 16.0f;
    c->menu_hold = cfg->menu_hold;
}

void tp_controls_read(tp_controls *c, vp_input *in, double dt)
{
    if (SDL_NumJoysticks() != c->joys && SDL_NumJoysticks() <= TP_MAX_JOYSTICKS) open_joysticks(c);
    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    int left = held(c, in, keys, TP_ACT_SPIN_LEFT), right = held(c, in, keys, TP_ACT_SPIN_RIGHT);
    if (left && right) left = right = 0;
    c->raw = spinner_counts(c, in, keys, dt);
    c->spin = (double)c->raw * c->sens + (double)(right - left) * c->key_rate * dt;

    c->fire = held(c, in, keys, TP_ACT_FIRE);
    c->zap = held(c, in, keys, TP_ACT_ZAP);
    c->start1 = held(c, in, keys, TP_ACT_START1);
    c->start2 = held(c, in, keys, TP_ACT_START2);
    for (int k = 0; k < 2; k++) {
        int coin = held(c, in, keys, TP_ACT_COIN1 + k);
        if (coin && !c->coin_was[k] && c->coin_left[k] <= 0.0) c->coin_left[k] = COIN_PULSE_S;
        c->coin_was[k] = coin;
    }

    /* For a menu: its own controls only, whatever is bound (vpbind.h) - the
     * arrow keys, any stick or hat, the spinner (the mouse), START 1, Enter
     * and the keypad's Enter to do, Esc to go back. No binding (none among
     * them) can take the menu from the cabinet, and none acts in it. */
    tp_nav nav = { 0, 0, 0, 0, 0 };
    vpb_update();
    nav.x = vpb_menu_way_x();
    nav.y = vpb_menu_way_y();
    c->nav_spin += (double)vp_input_delta(in, "p1_dialx") * (c->sens < 0.0f ? -1.0 : 1.0);
    while (c->nav_spin >= NAV_COUNTS) { nav.spin++; c->nav_spin -= NAV_COUNTS; }
    while (c->nav_spin <= -NAV_COUNTS) { nav.spin--; c->nav_spin += NAV_COUNTS; }
    int fire_now = vpb_menu_confirm_held(in);
    nav.fire = fire_now && !c->fire_was;
    c->fire_was = fire_now;
    int back_now = keys[SDL_SCANCODE_ESCAPE];
    nav.back = back_now && !c->back_was;
    c->back_was = back_now;
    c->nav = nav;

    int menu_now = keys[SDL_SCANCODE_F2] || held(c, in, keys, TP_ACT_MENU);
    /* A cabinet without a key for it: start 1 and fire held together. */
    if (c->menu_hold && c->fire && c->start1) {
        if (!c->hold_since) c->hold_since = SDL_GetTicks() | 1u;
        else if (SDL_GetTicks() - c->hold_since >= 2000u) menu_now = 1;
    } else {
        c->hold_since = 0;
    }
    c->menu = menu_now && !c->menu_was;
    c->menu_was = menu_now;
}

/* --- a new binding --------------------------------------------------------------- */

/* Never bound: the menu's keys (F2, Esc, Enter, the keypad's Enter, Delete);
 * for what is not itself a start, a coin or the menu, also the cabinet's
 * starts and coins. */
static int kept_key(int action, int sc)
{
    if (sc == SDL_SCANCODE_F2 || sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER
        || sc == SDL_SCANCODE_DELETE || sc == SDL_SCANCODE_KP_PERIOD) return 1;
    if (action >= TP_ACT_START1 && action <= TP_ACT_MENU) return 0;
    for (int k = TP_ACT_START1; k <= TP_ACT_COIN2; k++)
        for (int i = 0; i < g_nauto[k]; i++)
            if (g_auto[k][i].type == B_KEY && g_auto[k][i].index == sc) return 1;
    return 0;
}

void tp_controls_capture_begin(tp_controls *c, int action)
{
    c->cap_action = action;
    c->cap_armed = 0;
    c->cap_still = 0;
    for (int j = 0; j < TP_MAX_JOYSTICKS; j++)
        for (int a = 0; a < TP_MAX_AXES; a++)
            c->cap_rest[j][a] = j < c->joys && c->joy[j] ? SDL_JoystickGetAxis((SDL_Joystick *)c->joy[j], a) : 0;
}

int tp_controls_capture_poll(tp_controls *c, vp_input *in, char *spec, size_t size)
{
    if (c->cap_action < 0) return -1;
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    int relative = c->cap_action == TP_ACT_SPINNER;
    int found = 0, moving = 0;
    spec[0] = '\0';

    /* Leaving it as it was: Esc, Enter (START 1 too, where it is not START
     * 1's own line); START 2 makes it none, Delete auto. */
    if (c->cap_armed) {
        int cabinet_line = c->cap_action >= TP_ACT_START1 && c->cap_action <= TP_ACT_MENU;
        if (keys[SDL_SCANCODE_ESCAPE] || vpb_key_pressed(SDL_SCANCODE_RETURN) || vpb_key_pressed(SDL_SCANCODE_KP_ENTER)
            || (!cabinet_line && vp_input_pressed(in, "start1"))) {
            c->cap_action = -1;
            return -1;
        }
        if (!cabinet_line && vp_input_pressed(in, "start2")) { c->cap_action = -1; snprintf(spec, size, "none"); return 2; }
        if (vpb_key_pressed(SDL_SCANCODE_DELETE) || vpb_key_pressed(SDL_SCANCODE_KP_PERIOD)) {
            c->cap_action = -1;
            snprintf(spec, size, "auto");
            return 2;
        }
        if (!cabinet_line)              /* a start or a coin is the cabinet's */
            for (int k = TP_ACT_START1; k <= TP_ACT_COIN2; k++)
                for (int i = 0; i < g_nauto[k]; i++)
                    if (g_auto[k][i].type == B_KEY && keys[g_auto[k][i].index]) return 0;
    }
    if (relative) {
        /* (the SDK reads the mouse; its moves since the last picture) */
        int mx = vp_input_delta(in, "p1_trackballx"), my = vp_input_delta(in, "p1_trackbally");
        if (abs(mx) > 2 || abs(my) > 2) moving = 1;
        if (abs(mx) > 30 || abs(my) > 30) {
            found = 1;
            snprintf(spec, size, "%s", abs(mx) >= abs(my) ? "mouse x" : "mouse y");
        }
    }
    for (int sc = 1; sc < SDL_NUM_SCANCODES && !found; sc++) {
        if (!keys[sc] || kept_key(c->cap_action, sc)) continue;
        found = 1;
        if (relative) continue;             /* (a key turns no spinner: only keeps the wait from arming) */
        const char *name = SDL_GetScancodeName((SDL_Scancode)sc);
        if (name && *name) snprintf(spec, size, "key %s", name);
        else snprintf(spec, size, "key #%d", sc);
    }
    for (int j = 0; j < c->joys && !found; j++) {
        SDL_Joystick *joy = (SDL_Joystick *)c->joy[j];
        if (!joy) continue;
        int axes = SDL_JoystickNumAxes(joy);
        if (axes > TP_MAX_AXES) axes = TP_MAX_AXES;
        for (int a = 0; a < axes && !found; a++) {
            int v = SDL_JoystickGetAxis(joy, a) - c->cap_rest[j][a];
            if (v > 3000 || v < -3000) moving = 1;
            if (v > 19000 || v < -19000) {
                found = 1;
                snprintf(spec, size, "joy%d axis %d %c", j + 1, a, v > 0 ? '+' : '-');
            }
        }
        if (relative) continue;
        for (int b = 0; b < SDL_JoystickNumButtons(joy) && !found; b++)
            if (SDL_JoystickGetButton(joy, b)) {
                found = 1;
                snprintf(spec, size, "joy%d button %d", j + 1, b);
            }
        for (int h = 0; h < SDL_JoystickNumHats(joy) && !found; h++) {
            uint8_t v = SDL_JoystickGetHat(joy, h);
            for (int i = 0; i < 4 && !found; i++)
                if (v & HAT_BIT[i]) {
                    found = 1;
                    snprintf(spec, size, "joy%d hat %d %s", j + 1, h, HAT_NAME[i]);
                }
        }
    }
    if (!relative && !found) {          /* the mouse's buttons */
        Uint32 m = SDL_GetMouseState(NULL, NULL);
        for (int i = 0; i < 3 && !found; i++)
            if (m & SDL_BUTTON(i + 1)) { found = 1; snprintf(spec, size, "mouse button %s", MOUSE_BUTTON[i]); }
    }
    if (!c->cap_armed) {
        /* What opened the line has to be let go first. */
        if (found || moving) {
            int action = c->cap_action;
            tp_controls_capture_begin(c, action);
        } else if (++c->cap_still >= 12) {
            c->cap_armed = 1;
        }
        return 0;
    }
    if (!found || !spec[0]) return 0;
    c->cap_action = -1;
    return 1;
}

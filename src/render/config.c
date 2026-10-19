#include "render/config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tp_config_defaults(tp_config *c)
{
    memset(c, 0, sizeof *c);
    c->orientation = TP_ORIENT_VERTICAL;
    c->smooth = 1;
    c->steady_frames = 1;
    c->size_x = c->size_y = 1.0f;
    c->brightness = 1.0f;
    c->gamma = 0.8f;
    c->board_hz = 50.0f;
    /* MAME's defaults: 1 coin 1 play, 1 credit minimum, English, bonus life
     * at 20000, 3 lives, Medium1, levels 1, 3, 5, 7, 9, upright */
    c->dsw1 = 0x00;
    c->dsw2 = 0x00;
    c->difficulty = 3;
    c->rating = 1;
    c->spin_sens = 6.0f;
    c->key_speed = 8.0f;
    for (int i = 0; i < TP_ACT_COUNT; i++) snprintf(c->bind[i], TP_BIND_SIZE, "auto");
    c->menu_hold = 1;
    for (int i = 0; i < TP_FX_COUNT; i++) c->fx[i] = 1;
    c->volume = 1.0f;
}

static const char *const FX_NAME[TP_FX_COUNT] = {
    "lane", "warp", "zap", "trails", "sparks", "claw", "shatter", "rim", "stars", "fireworks", "title", "recharge", "logo",
};

const char *tp_config_fx_name(int fx)
{
    return fx >= 0 && fx < TP_FX_COUNT ? FX_NAME[fx] : "";
}

static const char *const ACTION_NAME[TP_ACT_COUNT] = {
    "spinner", "spin_left", "spin_right", "fire", "zap", "start1", "start2", "coin1", "coin2", "menu",
};
static const char *const ORIENT_NAME[2] = { "vertical", "horizontal" };

const char *tp_config_action_name(int action)
{
    return action >= 0 && action < TP_ACT_COUNT ? ACTION_NAME[action] : "";
}

int tp_config_bind_add(tp_config *c, int action, const char *spec)
{
    if (action < 0 || action >= TP_ACT_COUNT || !spec[0]) return 0;
    char *list = c->bind[action];
    if (!strcmp(list, "none") || !list[0]) {
        snprintf(list, TP_BIND_SIZE, "%s", spec);
        return 1;
    }
    /* the ones there, and where the last begins */
    int n = 1;
    const char *last = list;
    for (const char *p = list;; p += 4) {
        const char *q = strstr(p, " or ");
        size_t len = q ? (size_t)(q - p) : strlen(p);
        if (len == strlen(spec) && !strncmp(p, spec, len)) return 0;
        if (!q) break;
        p = q;
        last = q + 4;
        n++;
    }
    char out[2 * TP_BIND_SIZE];         /* (room to see that it is too long) */
    if (n >= TP_BIND_ALTS) snprintf(out, sizeof out, "%.*s%s", (int)(last - list), list, spec);
    else snprintf(out, sizeof out, "%s or %s", list, spec);
    if (strlen(out) >= TP_BIND_SIZE - 1) return 0;
    memcpy(list, out, strlen(out) + 1);      /* (it fits: checked above) */
    return 1;
}

int tp_config_bind_drop(tp_config *c, int action)
{
    if (action < 0 || action >= TP_ACT_COUNT) return 0;
    char *list = c->bind[action];
    char *last = NULL;
    for (char *q = strstr(list, " or "); q; q = strstr(q + 4, " or ")) last = q;
    if (last) {
        *last = '\0';
        return 1;
    }
    if (!strcmp(list, "auto")) return 0;
    snprintf(list, TP_BIND_SIZE, "auto");
    return 1;
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

static int set(tp_config *c, const char *key, const char *val)
{
    struct { const char *k; float *f; } floats[] = {
        { "size_x", &c->size_x }, { "size_y", &c->size_y }, { "brightness", &c->brightness },
        { "gamma", &c->gamma }, { "board_hz", &c->board_hz }, { "spin_sens", &c->spin_sens },
        { "key_speed", &c->key_speed }, { "volume", &c->volume },
    };
    struct { const char *k; int *i; } ints[] = {
        { "smooth", &c->smooth }, { "steady_frames", &c->steady_frames },
        { "pos_x", &c->pos_x }, { "pos_y", &c->pos_y },
        { "flip_x", &c->flip_x }, { "flip_y", &c->flip_y }, { "difficulty", &c->difficulty },
        { "rating", &c->rating }, { "cocktail", &c->cocktail },
        { "spin_reverse", &c->spin_reverse }, { "menu_hold", &c->menu_hold },
    };
    for (unsigned i = 0; i < sizeof floats / sizeof floats[0]; i++)
        if (!strcmp(key, floats[i].k)) { *floats[i].f = (float)atof(val); return 0; }
    for (unsigned i = 0; i < sizeof ints / sizeof ints[0]; i++)
        if (!strcmp(key, ints[i].k)) { *ints[i].i = atoi(val); return 0; }
    if (!strcmp(key, "dsw1")) { c->dsw1 = (uint8_t)strtol(val, NULL, 16); return 0; }
    if (!strcmp(key, "dsw2")) { c->dsw2 = (uint8_t)strtol(val, NULL, 16); return 0; }
    if (!strcmp(key, "rom_path")) { snprintf(c->rom_path, sizeof c->rom_path, "%s", val); return 0; }
    if (!strcmp(key, "orientation")) {
        for (int i = 0; i < 2; i++)
            if (!strcmp(val, ORIENT_NAME[i])) { c->orientation = i; return 0; }
        return -1;
    }
    if (!strncmp(key, "fx_", 3)) {
        for (int i = 0; i < TP_FX_COUNT; i++)
            if (!strcmp(key + 3, FX_NAME[i])) { c->fx[i] = atoi(val) != 0; return 0; }
        return -1;
    }
    if (!strncmp(key, "bind_", 5)) {
        for (int i = 0; i < TP_ACT_COUNT; i++)
            if (!strcmp(key + 5, ACTION_NAME[i])) { snprintf(c->bind[i], TP_BIND_SIZE, "%s", *val ? val : "auto"); return 0; }
        return -1;
    }
    return -1;
}

int tp_config_load(tp_config *c, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        char *p = trim(line);
        if (!*p || *p == ';' || *p == '#') continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(p), *val = trim(eq + 1);
        if (set(c, key, val) != 0) fprintf(stderr, "%s: setting not understood: %s = %s\n", path, key, val);
    }
    fclose(f);
    return 0;
}

int tp_config_save(const tp_config *c, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "; Tempest Enhanced: the owner's settings (see the README)\n");
    fprintf(f, "rom_path = %s\n", c->rom_path);
    fprintf(f, "orientation = %s\nsmooth = %d\nsteady_frames = %d\n", ORIENT_NAME[c->orientation & 1], c->smooth,
            c->steady_frames);
    fprintf(f, "size_x = %.3f\nsize_y = %.3f\npos_x = %d\npos_y = %d\nflip_x = %d\nflip_y = %d\n",
            c->size_x, c->size_y, c->pos_x, c->pos_y, c->flip_x, c->flip_y);
    fprintf(f, "brightness = %.2f\ngamma = %.2f\nboard_hz = %.0f\n", c->brightness, c->gamma, c->board_hz);
    fprintf(f, "dsw1 = %02X\ndsw2 = %02X\ndifficulty = %d\nrating = %d\ncocktail = %d\n",
            c->dsw1, c->dsw2, c->difficulty, c->rating, c->cocktail);
    fprintf(f, "spin_sens = %.2f\nspin_reverse = %d\nkey_speed = %.1f\n",
            c->spin_sens, c->spin_reverse, c->key_speed);
    for (int i = 0; i < TP_ACT_COUNT; i++) fprintf(f, "bind_%s = %s\n", ACTION_NAME[i], c->bind[i]);
    fprintf(f, "menu_hold = %d\nvolume = %.2f\n", c->menu_hold, c->volume);
    for (int i = 0; i < TP_FX_COUNT; i++) fprintf(f, "fx_%s = %d\n", FX_NAME[i], c->fx[i]);
    fclose(f);
    return 0;
}

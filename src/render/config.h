/* config.h - the settings: the picture (orientation, size, place,
 * brightness, smooth motion), the cabinet's option switches, the spinner
 * and the controls.
 *
 * Read from tempest.ini (key = value, ';' or '#' comments): the package's
 * next to the program, then the owner's in save/. The service menu (F2,
 * platform/menu.h) changes them and writes the owner's file.
 */
#ifndef TP_CONFIG_H
#define TP_CONFIG_H

#include <stdint.h>

/* What can be bound to a key, a button, a stick (platform/controls.h). */
enum {
    TP_ACT_SPINNER = 0,                 /* relative: the spinner (auto: p1_dialx) */
    TP_ACT_SPIN_LEFT, TP_ACT_SPIN_RIGHT,/* digital turning */
    TP_ACT_FIRE, TP_ACT_ZAP,
    TP_ACT_START1, TP_ACT_START2,
    TP_ACT_COIN1, TP_ACT_COIN2,
    TP_ACT_MENU,                        /* the service menu (F2 always) */
    TP_ACT_COUNT
};
#define TP_BIND_SIZE 320       /* (up to TP_BIND_ALTS bindings, joined by " or ") */
#define TP_BIND_ALTS 8

enum { TP_ORIENT_VERTICAL = 0, TP_ORIENT_HORIZONTAL };

/* The enhancements: first those on the game's picture (render/enhance.c),
 * then the others. */
enum {
    TP_FX_LANE, TP_FX_WARP, TP_FX_ZAP, TP_FX_TRAILS, TP_FX_SPARKS, TP_FX_CLAW, TP_FX_SHATTER, TP_FX_RIM,
    TP_FX_GAME_COUNT,
    TP_FX_STARS = TP_FX_GAME_COUNT,     /* stars beside the playfield */
    TP_FX_FIREWORKS,                    /* the high score's fireworks over the width, more of them */
    TP_FX_TITLE,                        /* ENHANCED under the title */
    TP_FX_RECHARGE,                     /* SUPERZAPPER RECHARGE large in the middle */
    TP_FX_LOGO,                         /* the title in the marquee's lettering */
    TP_FX_COUNT
};

typedef struct tp_config {
    /* --- picture --- */
    int   orientation;          /* TP_ORIENT_xxx: as on the cabinet (vertical), or laid out for a horizontal screen */
    int   smooth;               /* 1: pictures between the game's frames (render/smooth.h) */
    int   steady_frames;        /* 1: the game's frames on a fixed beat, the cabinet's average (core/emu.h) */
    float size_x, size_y;       /* 1.0: the picture as large as the screen allows */
    int   pos_x, pos_y;         /* board units the picture is moved by */
    int   flip_x, flip_y;
    float brightness, gamma;
    float board_hz;             /* pictures a second sent to the vector board (package/tempest.ini) */

    /* --- the cabinet's option switches, as MAME's ports have them --- */
    uint8_t dsw1;               /* N13: coinage, right and left coin, bonus coins */
    uint8_t dsw2;               /* L12: minimum, language, bonus life, lives */
    int     difficulty;         /* POKEY 2 D0-D1 (MAME: 2 easy, 3 medium1, 0 medium2, 1 hard) */
    int     rating;             /* 1: levels 1, 3, 5, 7, 9; 0: tied to the high score */
    int     cocktail;

    /* --- the spinner --- */
    float spin_sens;            /* percent: game counts per spinner count * 100 (MAME's sensitivity) */
    int   spin_reverse;
    float key_speed;            /* digital turning: segments a second */

    /* --- controls --- */
    char  bind[TP_ACT_COUNT][TP_BIND_SIZE];  /* "auto", "none", "key NAME", "joyN button K", "joyN axis K +/-",
                                              * "joyN hat K up/down/left/right", "mouse x", "mouse y"; up to
                                              * TP_BIND_ALTS of them joined by " or ", as in MAME */
    int   menu_hold;            /* start 1 and fire held two seconds open the service menu */

    float volume;               /* 0..1 */

    /* --- the enhancements (render/enhance.h), each 1 on, 0 off --- */
    int   fx[TP_FX_COUNT];
    char  rom_path[512];
} tp_config;

void tp_config_defaults(tp_config *c);
const char *tp_config_fx_name(int fx);     /* its setting's name without "fx_" */
int  tp_config_load(tp_config *c, const char *path);    /* overlays what the file has; -1: no file */
int  tp_config_save(const tp_config *c, const char *path);
const char *tp_config_action_name(int action);

/* One more binding for an action (the ones there still do it too); with
 * TP_BIND_ALTS there already the last one is replaced. Returns 1 if the
 * bindings changed (0: it was there already, or it does not fit). */
int tp_config_bind_add(tp_config *c, int action, const char *spec);

/* The action's last binding taken away; the last one left: back to auto.
 * Returns 1 if the bindings changed. */
int tp_config_bind_drop(tp_config *c, int action);

#endif

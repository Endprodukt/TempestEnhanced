/* menu.h - Tempest Enhanced's service menu.
 *
 * F2 opens it (or a key bound to it, or start 1 and fire held two
 * seconds); the game stands still meanwhile. Pages:
 *
 *   CONTROLS       what every action is bound to: START 1 or Enter on a
 *                  line, then press the key or button, or move the mouse or
 *                  the spinner, that is to do it. The first since the
 *                  cursor came to the line replaces what was bound, every
 *                  further one is added. Left / right: auto or none.
 *                  Delete: auto. While it waits: START 2 none, Esc or
 *                  Enter leave it as it was. One control may do several.
 *   SPINNER        sensitivity (MAME's percent), the spinner's counts per
 *                  turn, reverse, how many segments a turn moves, set to
 *                  five segments a turn, the speed of digital turning, and
 *                  a test on level 1 without enemies
 *   PICTURE        vertical or horizontal, smooth motion, size, place,
 *                  mirroring, brightness, a test pattern; a still picture
 *                  of level 1 behind it shows every change
 *   GAME SETTINGS  the cabinet's option switches by what they mean
 *   volume, the high scores back to none
 *
 *   up / down or the spinner   choose a line
 *   left / right               change its value (a binding: auto or none)
 *   START 1 or Enter           change it, open a page, or do what it says
 *   Esc                        back
 *   F2                         save the settings and go back to the game
 *
 * The menu's own controls (any stick or hat counts as the arrows), never
 * what is bound: tp_nav.
 */
#ifndef TP_MENU_H
#define TP_MENU_H

#include "platform/controls.h"
#include "render/config.h"

#include <vectorpie/vp.h>

enum {
    TP_MENU_NOTHING = 0,
    TP_MENU_CHANGED,        /* a setting changed: take it over */
    TP_MENU_RESET_SCORES,   /* the EAROM back to empty */
    TP_MENU_SPIN_TEST,      /* the spinner's test on level 1 */
    TP_MENU_PATTERN,        /* the picture's test pattern */
    TP_MENU_CLOSE,          /* save and leave */
};

typedef struct tp_menu {
    int    open;
    int    page;
    int    item[8];         /* the line chosen on each page */
    int    capture;         /* the action a binding is being taken for, or -1 */
    int    adding;          /* the line (+1) that had something bound since the cursor came: the next is added */
    int    del_was;         /* the Del key was down at the last look */
    int    confirm;         /* fire once more resets the high scores */
    int    dir_x, dir_y;
    double repeat_at;
} tp_menu;

void tp_menu_open(tp_menu *u);
/* Once a picture while open, after tp_controls_read. */
int  tp_menu_update(tp_menu *u, tp_config *cfg, tp_controls *ctl, vp_input *in, double now);
void tp_menu_draw(const tp_menu *u, const tp_config *cfg, vp_dvg *d);

/* 1 while the PICTURE page is open: a still picture of the game goes
 * behind it, so that every change shows at once. */
int  tp_menu_on_picture(const tp_menu *u);

/* A line of text in the menu's look, for the screens it opens. */
void tp_menu_text(vp_dvg *d, const char *s, float y, float h, int highlight);

#endif

/* controls.h - keys, buttons, sticks, the mouse and the spinner as the
 * Tempest board's inputs.
 *
 * The spinner: the owner's spinner counts (TP_ACT_SPINNER; "auto" is
 * advmame.rc's p1_dialx, on a PC the mouse) times the sensitivity
 * (tp_config.spin_sens, MAME's percent), reversed if asked; digital
 * turning (TP_ACT_SPIN_LEFT / _RIGHT, the arrow keys or the stick) at
 * tp_config.key_speed segments a second. The game sees 16 counts to a
 * segment. Fire, superzapper, the starts, two coin switches (as a 100 ms
 * pulse) and the service menu.
 *
 * Every action can be bound to any key, any button, hat or axis of any
 * joystick, a mouse button, the spinner also to the mouse's x or y, up to
 * TP_BIND_ALTS of them at once (tp_config.bind: joined by " or "), all read
 * directly. An action left at "auto" has what AdvanceMAME gives it by
 * default, read directly as well (controls.c's AUTO_SPEC):
 *
 *   spinner         the mouse (the SDK's p1_dialx)
 *   turn            the arrow keys, the first stick or hat
 *   fire, zap       Left Ctrl, Left Alt, the first stick's buttons 1 and 2,
 *                   the mouse's left and right button
 *   start 1, 2      1, 2
 *   coins           5, 6
 *   service menu    the F2 key, always; a binding adds a second way, and
 *                   start 1 with fire held two seconds (tp_config.menu_hold)
 *
 * The menus never go by the bindings: tp_nav is their own controls.
 */
#ifndef TP_CONTROLS_H
#define TP_CONTROLS_H

#include "render/config.h"

#include <stddef.h>
#include <vectorpie/vp_input.h>

#define TP_MAX_JOYSTICKS 8
#define TP_MAX_AXES      16

typedef struct tp_bind {
    uint8_t type;           /* what it is bound to (controls.c) */
    uint8_t joy;            /* which joystick, from 0 */
    int16_t index;          /* the scancode, button, axis or hat */
    int8_t  dir;            /* an axis: 1 or -1; a hat: its direction bit */
} tp_bind;

/* Where the menu's own controls point: x and y -1, 0, 1 (y: up is -1) by
 * the arrow keys, any stick or hat; fire (START 1, Enter, the keypad's
 * Enter) and back (Esc) as they go down; the spinner (the mouse) moves y. */
typedef struct tp_nav {
    int x, y, fire, back;
    int spin;               /* whole steps the spinner moved the selection (+ down) */
} tp_nav;

typedef struct tp_controls {
    tp_bind bind[TP_ACT_COUNT][TP_BIND_ALTS];
    int     nbind[TP_ACT_COUNT];
    float   sens;           /* game counts per spinner count */
    float   key_rate;       /* game counts a second, turning digitally */
    int     menu_hold;

    /* what the last read found */
    double  spin;           /* game counts this picture */
    int     raw;            /* the spinner's own counts this picture (signed as turned) */
    int     fire, zap, start1, start2;
    double  coin_left[2];   /* seconds left of the coin switch's pulse */
    tp_nav  nav;
    int     menu;           /* the service menu's key went down */
    double  nav_spin;       /* spinner counts not yet a menu step */

    int     fire_was, coin_was[2], menu_was, back_was;
    uint32_t hold_since;

    void   *joy[TP_MAX_JOYSTICKS];
    int     joys;

    /* taking a new binding from the next thing pressed */
    int     cap_action;     /* -1: none */
    int     cap_armed;
    int     cap_still;
    int16_t cap_rest[TP_MAX_JOYSTICKS][TP_MAX_AXES];
} tp_controls;

void tp_controls_init(tp_controls *c, const tp_config *cfg);
void tp_controls_close(tp_controls *c);
/* Take the bindings and the spinner's settings (again after they changed). */
void tp_controls_configure(tp_controls *c, const tp_config *cfg);
/* Once per picture: sample everything; dt the seconds since the last. */
void tp_controls_read(tp_controls *c, vp_input *in, double dt);

/* A new binding: begin, then once per picture after tp_controls_read.
 * 0 while waiting, 1 with the binding in spec (tp_config.bind's form), 2
 * with what the action is to be instead ("none": START 2, "auto": Delete),
 * -1 if left as it was (Esc, Enter, START 1). For the spinner only movement
 * counts: the mouse, or a stick's axis. */
void tp_controls_capture_begin(tp_controls *c, int action);
int  tp_controls_capture_poll(tp_controls *c, vp_input *in, char *spec, size_t size);

#endif

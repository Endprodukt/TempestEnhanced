/* enhance.h - graphic enhancements laid over the game's own picture.
 *
 * The game's picture stays as it is; these add to it or change its
 * colours, each switched on or off in the service menu (tp_config.fx):
 *
 *   LANE      the claw's lane (the game draws its two spokes in their own
 *             colour) lit: its rim segments in that colour too
 *   WARP      in the flight from level to level, a starfield of the port's
 *             own instead of the game's stars (clouds of dots that change
 *             every game frame), as Tempest 2000's: stars standing in
 *             space, the viewer flying through them towards the tube's
 *             far end (the vanishing point, fixed), each a streak along
 *             the way as long as the speed; the speed gathering down the
 *             tube and running out as the next tube comes, then the
 *             streaks shrink to dots and fade
 *   ZAP       the tube flashing in changing colours while the superzapper
 *             works (RAM 0125 counts up then)
 *   TRAILS    shots leave a short trail fading out (their place three
 *             pictures before, fainter)
 *   SPARKS    an explosion that shows up throws a few short sparks
 *   CLAW      the claw, destroyed, breaks into its lines, which fly apart
 *             turning and fade
 *   SHATTER   so does an enemy that is hit: its lines where an explosion
 *             shows up and it is gone
 *   RIM       in the flight to the next level the old tube's near rim is
 *             left out once the claw is past it (wider than RIM_GONE): the
 *             game drops it then itself in some levels, in others (4, ...)
 *             it draws it on all round the screen
 *   STARS     faint twinkling stars drifting outwards beside the
 *             playfield, each softly tinted (in play, on a horizontal
 *             screen)
 *
 * All in generator units on the game's picture (so the layout places them
 * with the playfield), except the stars, which are laid on the screen.
 */
#ifndef TP_ENHANCE_H
#define TP_ENHANCE_H

#include "avg/avg.h"
#include "render/config.h"

#define TP_FX_SUB          0x7f00  /* the sub-list of lines of the port's own (not the game's) */
#define TP_ENHANCE_SCREEN  64      /* lines tp_enhance_screen adds at most */

#define TP_FX_TRAIL_AGES   6
#define TP_FX_TRAIL_MAX    512
#define TP_FX_PARTICLES    64
#define TP_FX_FRAGMENTS    96
#define TP_FX_BOOMS        32
#define TP_FX_ENEMY_LINES  384
#define TP_FX_NWARP        64
#define TP_FX_NSTARS       32

typedef struct tp_fx_particle { float x, y, vx, vy, age, life; } tp_fx_particle;
typedef struct tp_fx_fragment {
    float x, y, dx, dy, vx, vy, spin, age, life;
    uint8_t r, g, b, intensity;
} tp_fx_fragment;
typedef struct tp_fx_warp { float x, y, z; uint8_t colour; } tp_fx_warp;

typedef struct tp_enhance {
    tp_avg_list out;                                    /* the picture with the enhancements */
    tp_line  trail[TP_FX_TRAIL_AGES][TP_FX_TRAIL_MAX];  /* the shots of the pictures before */
    int      trail_n[TP_FX_TRAIL_AGES], trail_head;
    tp_fx_particle p[TP_FX_PARTICLES];
    int      np;
    float    boom[TP_FX_BOOMS][2];                       /* explosions of the picture before */
    int      nboom;
    tp_fx_fragment f[TP_FX_FRAGMENTS];                   /* the claw's and enemies' lines flying apart */
    int      nf;
    tp_line  claw[16];                                   /* the claw of the picture before */
    int      nclaw;
    tp_line  enemy[TP_FX_ENEMY_LINES];                   /* the enemies of the picture before */
    int      nenemy;
    tp_fx_warp warp[TP_FX_NWARP];                         /* the flight's starfield */
    float    warp_cx, warp_cy, warp_on;                  /* the vanishing point; how far it has come in (0..1) */
    float    warp_v;                                     /* the viewer's speed (depths a second) */
    float    vp_x, vp_y;                                 /* the tube's far end's middle, as last seen */
    int      have_vp;
    float    star[TP_FX_NSTARS][6];                      /* direction (2), distance, twinkle phase, its speed, colour */
    int      stars_ready;
    double   time;                                       /* seconds of pictures shown */
    uint32_t rng;
    uint32_t pictures;
    uint8_t  state;                                      /* the game's state of the last picture */
} tp_enhance;

void tp_enhance_init(tp_enhance *fx);
void tp_enhance_free(tp_enhance *fx);

/* The game's picture (generator units) with the enhancements switched on;
 * mem the game's RAM, dt seconds since the last picture shown. Returns
 * fx->out (or pic itself when nothing is switched on). */
const tp_avg_list *tp_enhance_picture(tp_enhance *fx, const tp_config *cfg, const tp_avg_list *pic,
                                      const uint8_t *mem, double dt);

/* The screen's own (board units), after the layout: placed holds n lines;
 * returns how many lines were added after them (at most
 * TP_ENHANCE_SCREEN). */
int tp_enhance_screen(tp_enhance *fx, const tp_config *cfg, tp_line *placed, int n, double dt);

#endif

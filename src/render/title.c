#include "render/title.h"

#include "render/layout.h"
#include "render/logo.h"

#include <math.h>
#include <vectorpie/vp_text.h>

/* The title come all the way in (measured over the attract mode: x
 * -195..203, the top -146), alone: its stack of copies, which reaches down
 * to -44, taken apart to its last layer, whose letters end at -98. */
#define FULL_LEFT   (-190.0f)
#define FULL_RIGHT  198.0f
#define FULL_TOP    (-143.0f)
#define BELOW_MAX   110.0f          /* nothing of a title picture lies lower */
#define COPYRIGHT_Y 75.0f           /* the copyright line lies below this */
#define ALONE_Y     (-95.0f)        /* the title alone ends above this */

/* ENHANCED: its letters WORD_H tall (generator units,
 * y down), WORD_GAP under the title (render/logo.h: the game's logo ends at
 * -98, the marquee's lettering lower). Its copies come from the depth
 * towards a point under it, as the title's do. */
#define WORD_GAP    10.0f
#define WORD_H      24.0f           /* ENHANCED then about 0.85 of the title's width on a horizontal screen */
#define DEPTH_Y     70.0f           /* how far below the word the copies vanish */
#define COPIES      7
#define COPY_DZ     0.45f           /* depth from one copy to the next behind it */
#define FAR_Z       7.0f            /* where the front copy starts */
#define FLY_S       0.35            /* the stack flies in */
#define TAKE_S      0.30            /* then its copies are taken away from the back */
#define FADE_S      0.08

int tp_title_complete(const tp_avg_list *pic)
{
    if (pic->count < 60) return 0;
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, title_y1 = -1e9f;
    for (int i = 0; i < pic->count; i++) {
        const tp_line *q = &pic->lines[i];
        if (q->sub != TP_TEXT_SUB) return 0;            /* a playfield: no title */
        float qy1 = fmaxf(q->y0, q->y1);
        x0 = fminf(x0, fminf(q->x0, q->x1)); x1 = fmaxf(x1, fmaxf(q->x0, q->x1));
        y0 = fminf(y0, fminf(q->y0, q->y1)); y1 = fmaxf(y1, qy1);
        if (qy1 < COPYRIGHT_Y) title_y1 = fmaxf(title_y1, qy1);
    }
    return x0 <= FULL_LEFT && x1 >= FULL_RIGHT && y0 <= FULL_TOP && y1 <= BELOW_MAX && title_y1 <= ALONE_Y;
}

/* The copies' colours, as the title's change through them. */
static const uint8_t PALETTE[][3] = {
    { 255, 255, 255 }, { 255, 230, 0 }, { 255, 40, 40 }, { 255, 0, 255 },
    { 60, 60, 255 }, { 0, 230, 255 }, { 0, 255, 60 },
};
#define NPAL ((int)(sizeof PALETTE / sizeof PALETTE[0]))

/* One copy at depth z (1: the word itself): smaller and nearer the point
 * it comes from the deeper it is. */
static void draw_copy(vp_dvg *d, const tp_config *cfg, float z, const uint8_t rgb[3], double k)
{
    float word_top = tp_logo_bottom(cfg) + WORD_GAP;
    float vanish = word_top + WORD_H + DEPTH_Y;
    float base = vanish + (word_top + WORD_H - vanish) / z;         /* the baseline (y points down) */
    float top = base - WORD_H / z;
    int lx, ly, hx, hy;
    tp_layout_map(cfg, 0.0f, base, &lx, &ly);
    tp_layout_map(cfg, 0.0f, top, &hx, &hy);
    float h = fabsf((float)(hy - ly));
    if (h < 6.0f) return;
    k *= cfg->brightness;
    if (k > 1.0) k = 1.0;
    vp_color(d, (uint8_t)(rgb[0] * k), (uint8_t)(rgb[1] * k), (uint8_t)(rgb[2] * k));
    /* (centred without the spacing the lettering keeps after the last letter) */
    vp_text(d, "ENHANCED", (float)lx + h / 6.0f, (float)ly, h, VP_TEXT_CENTER);
}

void tp_title_draw(vp_dvg *d, const tp_config *cfg, double shown)
{
    static const uint8_t WORD[3] = { 255, 210, 60 };
    vp_text_font(VP_FONT_ASTEROIDS);

    /* the front copy's depth: from far away to the word, slowing down */
    double p = shown >= FLY_S ? 1.0 : shown / FLY_S;
    float z = 1.0f + (FAR_Z - 1.0f) * (float)((1.0 - p) * (1.0 - p));
    /* the copies behind it, taken away from the back once it has come */
    int copies = COPIES;
    if (shown > FLY_S) {
        double q = (shown - FLY_S) / TAKE_S;
        copies = q >= 1.0 ? 1 : COPIES - (int)(q * (COPIES - 1));
    }
    double fade = shown >= FADE_S ? 1.0 : shown / FADE_S;
    int phase = (int)(shown * 24.0);            /* the colours move through the stack */

    for (int c = copies - 1; c >= 1; c--)       /* back to front */
        draw_copy(d, cfg, z + COPY_DZ * (float)c, PALETTE[(c + phase) % NPAL], 0.8 * fade);
    double glow = 0.85 + 0.15 * sin(shown * 3.0);
    draw_copy(d, cfg, z, WORD, glow * fade);
}

#include "render/layout.h"

#include <math.h>
#include <string.h>

/* --- the native picture ------------------------------------------------------------ */

#define NATIVE_HALF 300.0f      /* generator units from the middle to the edge of the board (MAME: 580 by 570) */

void tp_layout_point(const tp_config *cfg, float x, float y, int *bx, int *by)
{
    float k = 2048.0f / NATIVE_HALF;
    float u = x * k * cfg->size_x, v = y * k * cfg->size_y;
    if (cfg->flip_x) u = -u;
    if (cfg->flip_y) v = -v;
    *bx = (int)lroundf(2048.0f + u + (float)cfg->pos_x);
    *by = (int)lroundf(2048.0f - v + (float)cfg->pos_y);
}

/* --- horizontal ------------------------------------------------------------------- */

/* The playfield in generator units, as the cabinet shows it, when there is
 * no tube to take it from: the tube and all that moves in it, the claw at
 * the rim included (measured over many levels). */
#define PF_TOP     (-196.0f)
#define PF_BOTTOM  238.0f
#define PF_HALF_W  245.0f
/* With a tube: its box, and room for the claw, which stands out of it by
 * up to 37 on every side (measured over a played game). */
#define TUBE_SUB   0x240a
#define CLAW_BELOW 40.0f
#define CLAW_ABOVE 40.0f
#define CLAW_SIDE  38.0f
/* A tube box larger than this, or smaller than that, is the flight into
 * it or out of it (it grows from a point): the scale then stays as it was. */
#define TUBE_MAX_W 540.0f
#define TUBE_MAX_H 470.0f
#define TUBE_MIN_W 200.0f
#define TUBE_MIN_H 150.0f
/* Text rows: the game's text above this, or below that, goes to the side
 * strips (the score, lives, high score and level at the top; credits and
 * the like at the bottom). */
#define TEXT_TOP    (-160.0f)
#define TEXT_BOTTOM 195.0f
#define STRIP_MIN   0.17f           /* screen units each side strip keeps at least */
#define TEXT_FIELD  262.0f          /* a picture of text only reaches this far from the middle (measured) */
/* A picture of text only is drawn at most this much larger than that: the
 * logo, flown in, with the copyright below (x -195..203, y -146..96) then
 * just as tall as the screen. So the scale stays the same through the
 * whole flight: the logo grows, the copyright stands still. */
#define TEXT_GROW   2.15f
#define TEXT_CX     4.0f
#define TEXT_CY     (-25.0f)
#define TEXT_SCALE_MAX ((1.0f - 2.0f * MARGIN) / (PF_BOTTOM - PF_TOP))
#define EASE        0.15f           /* of the way to a new scale per picture */
/* The upright tube's 3:4 over the generator's field of 570 by 580: a unit
 * of x is this much of a unit of y on the screen. */
#define ASPECT_X   (0.75f * 580.0f / 570.0f)
/* The playfield, though, a unit of x as long as one of y: the screen is
 * wide enough, and the circle of level 1 (357 by 357) is then round. */
#define PF_ASPECT_X 1.0f
#define SCREEN_W   (4.0f / 3.0f)        /* screen units: the height is 1 */
#define MARGIN     0.03f
/* The playfield as large as the screen takes it with the claw (the room
 * for it beside the tube, CLAW_*) and this little margin; the text in the
 * corners does not make it smaller (it keeps its strips, STRIP_MIN, and
 * lies over the playfield's corners where a level is that wide). */
#define PF_MARGIN  0.01f
#define ROW_GAP    0.025f
#define SIDE_SPLIT (-60.0f)             /* a text row's middle left of this goes left */
/* The stars of the flight from one level to the next (their sub-list): the
 * game spreads them to x +-222 at most (measured), a little more than the
 * upright tube's width. Across a horizontal screen they are spread out to
 * its edges, so that the cloud of them is as wide as the screen as it was
 * as tall as the upright one: in space all of them alike; while the tube
 * is still flown through (it is larger than the screen), those near the
 * middle (in its far end) as they are, further out more. */
#define STAR_SUB   0x2e22
/* The attract mode's text (score, INSERT COINS, credits, the copyright):
 * it stays where the cabinet shows it, over the playfield, where the high
 * score table (a picture of text only, reaching just as far) puts it: the
 * text's field, from the score's top to the credits line (measured), as
 * tall as the screen. */
/* In the flight to the next level the game writes SUPERZAPPER RECHARGE
 * small under the tube; it goes to the middle of the screen, this much
 * larger than the text's scale, light. */
#define RECHARGE_GROW 2.0f
static const uint8_t RECHARGE_RGB[3] = { 215, 235, 255 };

#define HUD_TOP    (-209.0f)
#define HUD_BOTTOM 255.0f
/* The text above and below the playfield (score, high score, credits) this
 * much larger than the cabinet has it: in a game in the strips, in the
 * attract mode and the pictures of text only above (from the field's top)
 * and below (from its bottom). */
#define HUD_GROW   1.6f
/* The copyright line (the text's colour 7, as the tube's blue, at y
 * 207..219) in a colour of its own, so that it stands out over the tube. */
/* In a game, of the text above the playfield: the high score (colour 5,
 * the small letters at y -209..-197 in the middle) is left out; the word
 * LEVEL the game writes beside the level's number during the flight to
 * the next level (colour 7, left of x 10) too, so that nothing new shows
 * up between the levels; the level's number (colour 7, right of it) is
 * this much larger than the other text, as tall as the score. */
#define HISCORE_COLOR 5
/* The player's score (colour 5, left of SIDE_SPLIT, above the playfield):
 * in the attract mode and the pictures of text only too where a game has
 * it, in the top left corner, as large. */
#define SCORE_COLOR   5
#define HISCORE_Y0    (-211.0f)
#define HISCORE_Y1    (-195.0f)
#define HISCORE_X0    (-56.0f)
#define HISCORE_X1    52.0f
#define LEVEL_COLOR   7
#define LEVEL_X       10.0f
#define LEVEL_GROW    2.0f
/* The level's number goes to the bottom right, after the word LEVEL in
 * the game's own lettering, drawn by the port always (the game writes it
 * only during the flight, and then to the left of the number, just here:
 * its strokes, generator units). */
static const float LEVEL_WORD[][4] = {
    { -63.75f, -191.25f, -63.75f, -179.30f }, { -63.75f, -179.30f, -55.78f, -179.30f },     /* L */
    { -51.80f, -179.30f, -51.80f, -191.25f }, { -51.80f, -191.25f, -43.83f, -191.25f },     /* E */
    { -45.82f, -185.27f, -51.80f, -185.27f }, { -51.80f, -179.30f, -43.83f, -179.30f },
    { -39.84f, -191.25f, -35.86f, -179.30f }, { -35.86f, -179.30f, -31.88f, -191.25f },     /* V */
    { -27.89f, -179.30f, -27.89f, -191.25f }, { -27.89f, -191.25f, -19.92f, -191.25f },     /* E */
    { -21.91f, -185.27f, -27.89f, -185.27f }, { -27.89f, -179.30f, -19.92f, -179.30f },
    { -15.94f, -191.25f, -15.94f, -179.30f }, { -15.94f, -179.30f,  -7.97f, -179.30f },     /* L */
};
#define LEVEL_WORD_N ((int)(sizeof LEVEL_WORD / sizeof LEVEL_WORD[0]))
/* The number itself larger than the word, on its baseline (the game's
 * number starts at x 15.94, its letters end at y -179.30). */
#define NUMBER_X0     15.94f
#define NUMBER_BASE   (-179.30f)
#define NUMBER_GROW   1.6f
#define COPYRIGHT_COLOR 7
#define COPYRIGHT_Y0    200.0f
#define COPYRIGHT_Y1    225.0f
static const uint8_t COPYRIGHT_RGB[3] = { 255, 140, 0 };
/* The high score table (a picture of text only under the red HIGH SCORES
 * at y -135..-111): its first place (colour 7 at y -92..-80) bright yellow
 * and this much larger, the others larger too. */
#define TABLE_HEAD_Y0 (-140.0f)
#define TABLE_HEAD_Y1 (-105.0f)
#define FIRST_Y0      (-95.0f)
#define FIRST_Y1      (-76.0f)
#define FIRST_GROW    1.8f
/* The other places (colour 7 at y -72..60, 20 apart) larger too, their
 * spacing with them, from the second's top, and a little lower, clear of
 * the first; the last then still well above the copyright. */
#define OTHERS_Y0     (-75.0f)
#define OTHERS_Y1     65.0f
#define OTHERS_TOP    (-71.7f)
#define OTHERS_GROW   1.35f
#define OTHERS_DOWN   8.0f
#define TABLE_DOWN    22.0f         /* the table moved down, clear of the larger text above it */
static const uint8_t FIRST_RGB[3] = { 255, 255, 0 };
/* The boxes that close in on the middle before the logo flies in (a shape
 * of the vector ROM, only there): over the whole screen, the outermost
 * (x +-310.5, y +-335.4, measured) to its edges. */
#define BOX_SHAPE  0x34aa
/* The choice of level (state 16): what it shows below its copyright, from
 * PLAYER 1 down to the TIME left (measured). */
/* Entering initials (state 12): the screen's parts top to bottom, each its
 * stretch of y (measured), how much larger, and the room above it; laid
 * one under the other and the whole as large as the screen takes it. The
 * copyright (below the instructions) is left out. */
typedef struct entry_part { float y0, y1, grow, gap; } entry_part;
static const entry_part ENTRY[] = {
    { -212.0f, -176.0f, 1.0f,  0.0f },  /* score, high score, level */
    { -140.0f, -105.0f, 1.0f, 22.0f },  /* HIGH SCORES */
    {  -95.0f,  -76.0f, 1.8f, 10.0f },  /* the first place */
    {  -75.0f,   65.0f, 1.35f, 8.0f },  /* the others */
    {   95.0f,  135.0f, 1.4f, 26.0f },  /* PLAYER 1 */
    {  140.0f,  202.0f, 1.4f, 14.0f },  /* ENTER YOUR INITIALS, SPIN KNOB TO CHANGE, PRESS FIRE TO SELECT */
};
#define ENTRY_N    ((int)(sizeof ENTRY / sizeof ENTRY[0]))
#define ENTRY_HALF_W 240.0f             /* half its width, at the widest */

#define RATE_X0    (-233.0f)
#define RATE_X1    215.0f
#define RATE_Y0    (-76.0f)
#define RATE_Y1    207.0f
#define BOX_HALF_W 310.5f
#define BOX_HALF_H 335.4f
#define STAR_REACH 222.0f

typedef struct box { float x0, y0, x1, y1; } box;

static box line_box(const tp_line *l)
{
    box b = { fminf(l->x0, l->x1), fminf(l->y0, l->y1), fmaxf(l->x0, l->x1), fmaxf(l->y0, l->y1) };
    return b;
}

static int touch(const box *a, const box *b, float gx, float gy)
{
    return a->x0 <= b->x1 + gx && a->x1 >= b->x0 - gx && a->y0 <= b->y1 + gy && a->y1 >= b->y0 - gy;
}

static void merge(box *a, const box *b)
{
    a->x0 = fminf(a->x0, b->x0); a->y0 = fminf(a->y0, b->y0);
    a->x1 = fmaxf(a->x1, b->x1); a->y1 = fmaxf(a->y1, b->y1);
}

/* Which band of the text above (0) or below (1) the playfield a line is
 * in, -1 none. */
static int hud_band(const tp_line *l)
{
    if (l->sub != TP_TEXT_SUB) return -1;
    box b = line_box(l);
    if (b.x1 - b.x0 > 40.0f || b.y1 - b.y0 > 40.0f) return -1;
    return b.y1 < TEXT_TOP ? 0 : b.y0 > TEXT_BOTTOM ? 1 : -1;
}

/* A point of a band's text made larger, from the field's top or bottom. */
static void hud_grow(int band, float *x, float *y)
{
    *x = TEXT_CX + (*x - TEXT_CX) * HUD_GROW;
    *y = band == 0 ? HUD_TOP + (*y - HUD_TOP) * HUD_GROW : HUD_BOTTOM - (HUD_BOTTOM - *y) * HUD_GROW;
}

/* In a game: a line of the level's number. */
static int is_level_number(const tp_line *l)
{
    return hud_band(l) == 0 && l->color == LEVEL_COLOR && fminf(l->x0, l->x1) >= LEVEL_X;
}

static void number_grow(float *x, float *y)
{
    *x = NUMBER_X0 + (*x - NUMBER_X0) * NUMBER_GROW;
    *y = NUMBER_BASE + (*y - NUMBER_BASE) * NUMBER_GROW;
}

/* In a game: a line of the text above the playfield that is left out. */
static int left_out(const tp_line *l)
{
    if (hud_band(l) != 0) return 0;
    if (l->color == HISCORE_COLOR && fminf(l->y0, l->y1) > HISCORE_Y0 && fmaxf(l->y0, l->y1) < HISCORE_Y1 &&
        fminf(l->x0, l->x1) > HISCORE_X0 && fmaxf(l->x0, l->x1) < HISCORE_X1)
        return 1;
    return l->color == LEVEL_COLOR && fmaxf(l->x0, l->x1) < LEVEL_X;
}

static int is_score(const tp_line *l)
{
    return hud_band(l) == 0 && l->color == SCORE_COLOR && (l->x0 + l->x1) * 0.5f < SIDE_SPLIT;
}

static int is_copyright(const tp_line *l, int band)
{
    return band == 1 && l->color == COPYRIGHT_COLOR && fminf(l->y0, l->y1) > COPYRIGHT_Y0 &&
           fmaxf(l->y0, l->y1) < COPYRIGHT_Y1;
}

static void set_rgb(tp_line *l, const uint8_t rgb[3])
{
    l->r = rgb[0]; l->g = rgb[1]; l->b = rgb[2];
}

#define MAX_TEXT 1024
#define MAX_ROWS 32

typedef struct row {
    box   b;
    int   band;         /* 0: above the playfield, 1: below */
    int   side;         /* 0: left, 1: right */
    int   level;        /* the level's row (bottom right, with LEVEL) */
    int   color;        /* a row is of one colour: the game's texts are (so
                         * that LEVEL, written beside the score during the
                         * flight to the next level, does not join it) */
    float scale;        /* screen units per generator unit (y) */
    float u0, v0;       /* where the row's top left corner goes */
} row;

static void to_board(const tp_config *cfg, float u, float v, float *bx, float *by)
{
    u *= cfg->size_x;
    v *= cfg->size_y;
    if (cfg->flip_x) u = -u;
    if (cfg->flip_y) v = -v;
    *bx = 2048.0f + u * (4096.0f / SCREEN_W) + (float)cfg->pos_x;
    *by = 2048.0f - v * 4096.0f + (float)cfg->pos_y;
}

/* The playfield of a picture: the tube's box, the height and half width
 * to fit (with room for the claw), and the scale that fits it. 0 if the
 * tube is not at rest (flown into or out of: larger or smaller than a
 * level's, or none). */
static int playfield_of(const tp_line *in, int count, box *tube, float *y0, float *y1, float *hw, float *s_fit)
{
    box t = { 1e9f, 1e9f, -1e9f, -1e9f };
    for (int i = 0; i < count; i++)
        if (in[i].sub == TUBE_SUB) {
            box b = line_box(&in[i]);
            merge(&t, &b);
        }
    *tube = t;
    *y0 = PF_TOP; *y1 = PF_BOTTOM; *hw = PF_HALF_W;
    int steady = 1;
    if (t.x1 >= t.x0) {
        if (t.x1 - t.x0 > TUBE_MAX_W || t.y1 - t.y0 > TUBE_MAX_H ||
            t.x1 - t.x0 < TUBE_MIN_W || t.y1 - t.y0 < TUBE_MIN_H) {
            steady = 0;
        } else {
            *y0 = t.y0 - CLAW_ABOVE;
            *y1 = t.y1 + CLAW_BELOW;
            *hw = fmaxf(-t.x0, t.x1) + CLAW_SIDE;
        }
    }
    *s_fit = fminf((1.0f - 2.0f * PF_MARGIN) / (*y1 - *y0),
                   (SCREEN_W - 2.0f * PF_MARGIN) / (2.0f * *hw * PF_ASPECT_X));
    return steady;
}

static int was_text;                /* the picture before was text only */
static int attract;                 /* the game is in its attract mode */
static int entry;                   /* initials are entered (the copyright left out) */
static int rating;                  /* the choice of level (its own layout) */
static int flight;                  /* a flight from one level to the next: the playfield's scale holds */
/* the playfield's scale, the middle of its height, its half width (eased) */
static float s_cur = 0.0f, cy_cur = 0.0f, hw_cur = PF_HALF_W;
/* the text's scale and the strip's width of the last picture with a
 * playfield, for the score in a picture of text only */
static float hud_s = TEXT_SCALE_MAX, hud_strip = STRIP_MIN;
/* how the last picture was laid out (tp_layout_map): screen units are
 * (x - cx) * ASPECT_X * s, (y - cy) * s */
static int   last_horizontal;
static float last_s = 1.0f, last_cx, last_cy;

/* The attract mode's text below the playfield (credits and coinage, the
 * minimum, the copyright): how far each of its lines moves so that the
 * line is centred on the screen (dx per picture line, 0 for the rest).
 * The game starts CREDITS at the same place whatever follows, so with FREE
 * PLAY that line stood well left of the middle. */
#define MAX_BELOW 8
static void centre_below(const tp_line *in, int count, float *dx)
{
    static int8_t which[8192 + TP_LAYOUT_EXTRA];
    box ln[MAX_BELOW];
    int nl = 0;
    for (int i = 0; i < count; i++) {
        dx[i] = 0.0f;
        which[i] = -1;
        if (in[i].ikey == BOX_SHAPE || hud_band(&in[i]) != 1) continue;
        box b = line_box(&in[i]);
        int k;
        for (k = 0; k < nl; k++)
            if (b.y0 <= ln[k].y1 + 1.0f && b.y1 >= ln[k].y0 - 1.0f) break;
        if (k == nl) {
            if (nl == MAX_BELOW) continue;
            ln[nl++] = b;
        } else {
            merge(&ln[k], &b);
        }
        which[i] = (int8_t)k;
    }
    for (int i = 0; i < count; i++)
        if (which[i] >= 0) dx[i] = TEXT_CX - (ln[which[i]].x0 + ln[which[i]].x1) * 0.5f;
}

static int horizontal(const tp_config *cfg, const tp_line *in, int count, tp_line *out)
{
    /* what is text above or below the playfield, and is there a playfield */
    static int row_of[8192 + TP_LAYOUT_EXTRA];
    int playfield = 0;
    for (int i = 0; i < count; i++) {
        box b = line_box(&in[i]);
        row_of[i] = -1;
        if (in[i].sub != TP_TEXT_SUB && b.x1 - b.x0 < 600.0f && b.y1 - b.y0 < 600.0f) playfield = 1;
    }
    static float below_dx[8192 + TP_LAYOUT_EXTRA];
    if (attract) centre_below(in, count, below_dx);

    /* the choice of level, by its state or by what it shows: its five
     * holes (colour 6, 16 lines each, between y 85 and 120) */
    int select = rating, holes = 0;
    for (int i = 0; i < count; i++)
        if (in[i].color == 6 && in[i].y0 > 85.0f && in[i].y0 < 120.0f && in[i].y1 > 85.0f && in[i].y1 < 120.0f) holes++;
    if (!playfield && !select) select = holes >= 40;
    if (rating && holes < 40) {
        /* the state is the choice's, the picture not yet (for some frames
         * after start the game still shows the screen before, in part): it
         * stays dark, not that screen in the choice's place */
        for (int i = 0; i < count; i++) {
            out[i] = in[i];
            out[i].intensity = 0;
            to_board(cfg, 0.0f, 0.0f, &out[i].x0, &out[i].y0);
            to_board(cfg, 0.0f, 0.0f, &out[i].x1, &out[i].y1);
        }
        return count;
    }
    if (!playfield && entry) {
        /* entering initials: the parts stacked, each grown, the whole fitted */
        float top[ENTRY_N], h = 0.0f, w = 2.0f * ENTRY_HALF_W * ASPECT_X;
        for (int k = 0; k < ENTRY_N; k++) {
            h += ENTRY[k].gap;
            top[k] = h;
            h += (ENTRY[k].y1 - ENTRY[k].y0) * ENTRY[k].grow;
        }
        float s_e = fminf((1.0f - 2.0f * MARGIN) / h, (SCREEN_W - 2.0f * MARGIN) / w);
        was_text = 0;
        last_s = s_e; last_cx = 0.0f; last_cy = 0.0f;
        for (int i = 0; i < count; i++) {
            out[i] = in[i];
            float my = (in[i].y0 + in[i].y1) * 0.5f;
            int k = -1;
            for (int j = 0; j < ENTRY_N && k < 0; j++)
                if (my >= ENTRY[j].y0 && my <= ENTRY[j].y1) k = j;
            if (k < 0) {
                out[i].intensity = 0;           /* (the copyright, and what lies between) */
                k = 0;
            }
            const entry_part *e = &ENTRY[k];
            if (k == 2 && in[i].color == COPYRIGHT_COLOR) set_rgb(&out[i], FIRST_RGB);
            /* about its own middle across, from its top down */
            float cx = 0.0f;
            float u0 = (cx + (in[i].x0 - cx) * e->grow) * ASPECT_X * s_e;
            float u1 = (cx + (in[i].x1 - cx) * e->grow) * ASPECT_X * s_e;
            float v0 = (top[k] + (in[i].y0 - e->y0) * e->grow - h * 0.5f) * s_e;
            float v1 = (top[k] + (in[i].y1 - e->y0) * e->grow - h * 0.5f) * s_e;
            to_board(cfg, u0, v0, &out[i].x0, &out[i].y0);
            to_board(cfg, u1, v1, &out[i].x1, &out[i].y1);
        }
        return count;
    }
    if (!playfield && select) {
        /* the choice of level: wide as the playfield is (a unit of x as long
         * as one of y), as large as the screen takes it, fixed (so it does
         * not move); without the copyright at its top */
        float s_r = fminf((1.0f - 2.0f * MARGIN) / (RATE_Y1 - RATE_Y0),
                          (SCREEN_W - 2.0f * MARGIN) / ((RATE_X1 - RATE_X0) * PF_ASPECT_X));
        float cx = (RATE_X0 + RATE_X1) * 0.5f, cy = (RATE_Y0 + RATE_Y1) * 0.5f;
        was_text = 0;
        last_s = s_r; last_cx = cx; last_cy = cy;
        for (int i = 0; i < count; i++) {
            out[i] = in[i];
            if (hud_band(&in[i]) == 0 && in[i].color == COPYRIGHT_COLOR) out[i].intensity = 0;
            to_board(cfg, (in[i].x0 - cx) * PF_ASPECT_X * s_r, (in[i].y0 - cy) * s_r, &out[i].x0, &out[i].y0);
            to_board(cfg, (in[i].x1 - cx) * PF_ASPECT_X * s_r, (in[i].y1 - cy) * s_r, &out[i].x1, &out[i].y1);
        }
        return count;
    }
    if (!playfield) {
        /* text only (the logo flying in, the high score table, the choice
         * of level): as large as the screen takes what is there, but at
         * most TEXT_GROW times the size the whole field (+-262) has when it
         * is as tall as the screen, so that the logo, small at first, still
         * flies in; smaller at once (nothing leaves the screen), larger
         * gently (the tables do not jump) */
        static float s_txt = 0.0f, cx_txt = 0.0f, cy_txt = 0.0f;
        static tp_line moved[8192];
        static uint8_t sc[8192];            /* the score's lines (before the text grows) */
        /* the high score table's first place: its box, if this is the table */
        int head = 0, below = 0;
        box first = { 1e9f, 1e9f, -1e9f, -1e9f };
        for (int i = 0; i < count; i++) {
            const tp_line *l = &in[i];
            /* (text under the table, above the band: after a game, RANKING
             * FROM 1 TO 99 and the player; then the other places keep
             * their size, for room) */
            if (l->sub == TP_TEXT_SUB && hud_band(l) < 0 && fminf(l->y0, l->y1) > OTHERS_Y1) below++;
            if (l->r > 200 && l->g < 60 && l->b < 60 && fminf(l->y0, l->y1) > TABLE_HEAD_Y0 &&
                fmaxf(l->y0, l->y1) < TABLE_HEAD_Y1) head++;
            if (l->color == COPYRIGHT_COLOR && fminf(l->y0, l->y1) > FIRST_Y0 && fmaxf(l->y0, l->y1) < FIRST_Y1) {
                box b = line_box(l);
                merge(&first, &b);
            }
        }
        int table = head >= 20 && first.x1 >= first.x0;
        float fcx = (first.x0 + first.x1) * 0.5f, fcy = (first.y0 + first.y1) * 0.5f;
        /* the band's text larger, the copyright and the first place in their
         * colours (in generator units, before the fit) */
        for (int i = 0; i < count; i++) {
            tp_line *o = &moved[i];
            *o = in[i];
            int band = hud_band(&in[i]);
            sc[i] = (uint8_t)is_score(&in[i]);
            if (sc[i]) {
                /* (placed below, in the corner) */
            } else if (band >= 0) {
                if (is_copyright(&in[i], band)) set_rgb(o, COPYRIGHT_RGB);
                if (attract && band == 1) { o->x0 += below_dx[i]; o->x1 += below_dx[i]; }
                hud_grow(band, &o->x0, &o->y0);
                hud_grow(band, &o->x1, &o->y1);
            } else if (table) {
                if (in[i].color == COPYRIGHT_COLOR && fminf(in[i].y0, in[i].y1) > FIRST_Y0 &&
                    fmaxf(in[i].y0, in[i].y1) < FIRST_Y1) {
                    set_rgb(o, FIRST_RGB);
                    o->x0 = fcx + (o->x0 - fcx) * FIRST_GROW; o->y0 = fcy + (o->y0 - fcy) * FIRST_GROW;
                    o->x1 = fcx + (o->x1 - fcx) * FIRST_GROW; o->y1 = fcy + (o->y1 - fcy) * FIRST_GROW;
                } else if (!below && in[i].color == COPYRIGHT_COLOR && fminf(in[i].y0, in[i].y1) > OTHERS_Y0 &&
                           fmaxf(in[i].y0, in[i].y1) < OTHERS_Y1) {
                    o->x0 = fcx + (o->x0 - fcx) * OTHERS_GROW;
                    o->x1 = fcx + (o->x1 - fcx) * OTHERS_GROW;
                    o->y0 = OTHERS_TOP + OTHERS_DOWN + (o->y0 - OTHERS_TOP) * OTHERS_GROW;
                    o->y1 = OTHERS_TOP + OTHERS_DOWN + (o->y1 - OTHERS_TOP) * OTHERS_GROW;
                }
                o->y0 += TABLE_DOWN;
                o->y1 += TABLE_DOWN;
            }
        }
        in = moved;
        box all = { 1e9f, 1e9f, -1e9f, -1e9f }, score = { 1e9f, 1e9f, -1e9f, -1e9f };
        for (int i = 0; i < count; i++) {
            if (sc[i]) {
                box b = line_box(&in[i]);
                merge(&score, &b);
                continue;                                   /* (in the corner, below) */
            }
            if (in[i].ikey == BOX_SHAPE) continue;          /* (over the whole screen, below) */
            box b = line_box(&in[i]);
            merge(&all, &b);
        }
        float base = (1.0f - 2.0f * MARGIN) / (2.0f * TEXT_FIELD);
        if (count && all.x1 >= all.x0) {
            float w = (all.x1 - all.x0) * ASPECT_X, h = all.y1 - all.y0;
            float fit = fminf((1.0f - 2.0f * MARGIN) / (h > 1.0f ? h : 1.0f),
                              (SCREEN_W - 2.0f * MARGIN) / (w > 1.0f ? w : 1.0f));
            fit = fminf(fit, base * TEXT_GROW);
            if (s_txt <= 0.0f || !was_text) {
                s_txt = fit;
            } else {
                if (fit < s_txt) s_txt = fit;
                else s_txt += (fit - s_txt) * EASE;
            }
            /* the middle: the flown-in logo's, so the copyright below it
             * stands still; moved only as far as all of it needs to be on
             * the screen (the high score table is taller) */
            cx_txt = TEXT_CX;
            cy_txt = TEXT_CY;
            float ry = (0.5f - MARGIN) / s_txt, rx = (SCREEN_W * 0.5f - MARGIN) / (s_txt * ASPECT_X);
            if (cy_txt > all.y0 + ry) cy_txt = all.y0 + ry;
            if (cy_txt < all.y1 - ry) cy_txt = all.y1 - ry;
            if (cx_txt > all.x0 + rx) cx_txt = all.x0 + rx;
            if (cx_txt < all.x1 - rx) cx_txt = all.x1 - rx;
        }
        was_text = 1;
        last_s = s_txt; last_cx = cx_txt; last_cy = cy_txt;
        float bu = (SCREEN_W * 0.5f - MARGIN) / BOX_HALF_W, bv = (0.5f - MARGIN) / BOX_HALF_H;
        float sw = (score.x1 - score.x0) * ASPECT_X;
        float ss = fminf(fminf(hud_s, TEXT_SCALE_MAX) * HUD_GROW, hud_strip / (sw > 1.0f ? sw : 1.0f));
        for (int i = 0; i < count; i++) {
            out[i] = in[i];
            if (sc[i]) {
                float u = -SCREEN_W * 0.5f + MARGIN, v = -0.5f + MARGIN;
                to_board(cfg, u + (in[i].x0 - score.x0) * ASPECT_X * ss, v + (in[i].y0 - score.y0) * ss,
                         &out[i].x0, &out[i].y0);
                to_board(cfg, u + (in[i].x1 - score.x0) * ASPECT_X * ss, v + (in[i].y1 - score.y0) * ss,
                         &out[i].x1, &out[i].y1);
                continue;
            }
            if (in[i].ikey == BOX_SHAPE) {
                to_board(cfg, in[i].x0 * bu, in[i].y0 * bv, &out[i].x0, &out[i].y0);
                to_board(cfg, in[i].x1 * bu, in[i].y1 * bv, &out[i].x1, &out[i].y1);
                continue;
            }
            to_board(cfg, (in[i].x0 - cx_txt) * ASPECT_X * s_txt, (in[i].y0 - cy_txt) * s_txt, &out[i].x0, &out[i].y0);
            to_board(cfg, (in[i].x1 - cx_txt) * ASPECT_X * s_txt, (in[i].y1 - cy_txt) * s_txt, &out[i].x1, &out[i].y1);
        }
        return count;
    }
    was_text = 0;

    /* the playfield: the tube's box with room for the claw, as large as the
     * height and the side strips allow; eased from level to level, held
     * while the tube is flown through */
    box tube;
    float y0, y1, hw, s_fit;
    int steady = playfield_of(in, count, &tube, &y0, &y1, &hw, &s_fit) && !flight;
    if (s_cur <= 0.0f) {
        s_cur = s_fit; cy_cur = (y0 + y1) * 0.5f; hw_cur = hw;
    } else if (steady) {
        /* smaller at once (nothing may leave the screen), larger gently */
        if (s_fit < s_cur) s_cur = s_fit;
        else s_cur += (s_fit - s_cur) * EASE;
        cy_cur += ((y0 + y1) * 0.5f - cy_cur) * EASE;
        if (hw > hw_cur) hw_cur = hw;
        else hw_cur += (hw - hw_cur) * EASE;
        /* and the middle where the whole playfield is on the screen */
        float reach = (0.5f - PF_MARGIN) / s_cur;
        if (cy_cur > y0 + reach) cy_cur = y0 + reach;
        if (cy_cur < y1 - reach) cy_cur = y1 - reach;
    }
    float s = s_cur, cy = cy_cur;
    last_s = s; last_cx = 0.0f; last_cy = cy;

    /* the game's text above and below the playfield, in rows: lines that
     * touch make a block, blocks side by side on a line make a row */
    row rows[MAX_ROWS];
    int nrows = 0;
    static uint8_t gone[8192];
    for (int i = 0; i < count; i++) gone[i] = !attract && left_out(&in[i]);
    for (int i = 0; i < count && nrows < MAX_ROWS; i++) {
        if (in[i].sub != TP_TEXT_SUB || gone[i]) continue;
        box b = line_box(&in[i]);
        if (b.x1 - b.x0 > 40.0f || b.y1 - b.y0 > 40.0f) continue;
        int band = b.y1 < TEXT_TOP ? 0 : b.y0 > TEXT_BOTTOM ? 1 : -1;
        if (band < 0) continue;
        int level = !attract && is_level_number(&in[i]);
        if (level) band = 1;
        int side = (b.x0 + b.x1) * 0.5f < SIDE_SPLIT ? 0 : 1;
        int k;
        for (k = 0; k < nrows; k++)
            if (rows[k].band == band && rows[k].level == level && rows[k].color == in[i].color &&
                touch(&rows[k].b, &b, 30.0f, 2.0f)) break;
        if (k == nrows) {
            rows[k].b = b;
            rows[k].band = band;
            rows[k].side = side;
            rows[k].level = level;
            rows[k].color = in[i].color;
            nrows++;
        } else {
            merge(&rows[k].b, &b);
        }
        row_of[i] = k;
    }
    /* rows that have grown into each other are one */
    for (int a = 0; a < nrows; a++)
        for (int b = a + 1; b < nrows; b++)
            if (rows[a].band == rows[b].band && rows[a].level == rows[b].level && rows[a].color == rows[b].color &&
                touch(&rows[a].b, &rows[b].b, 30.0f, 2.0f)) {
                merge(&rows[a].b, &rows[b].b);
                for (int i = 0; i < count; i++)
                    if (row_of[i] == b) row_of[i] = a;
                rows[b].b.x0 = 1e9f;        /* (empty from now on) */
                rows[b].b.x1 = -1e9f;
            }
    /* the level's row: the word LEVEL before the number, at the right */
    int lv = -1, lv_line = -1;
    for (int i = 0; i < count && lv < 0; i++)
        if (row_of[i] >= 0 && rows[row_of[i]].level) { lv = row_of[i]; lv_line = i; }
    if (lv >= 0) {
        /* its box: the number as it is drawn, larger, and the word */
        box nb = { 1e9f, 1e9f, -1e9f, -1e9f };
        for (int i = 0; i < count; i++) {
            if (row_of[i] != lv) continue;
            float ax = in[i].x0, ay = in[i].y0, bx = in[i].x1, by = in[i].y1;
            number_grow(&ax, &ay);
            number_grow(&bx, &by);
            box b = { fminf(ax, bx), fminf(ay, by), fmaxf(ax, bx), fmaxf(ay, by) };
            merge(&nb, &b);
        }
        rows[lv].b = nb;
    }
    if (lv >= 0)
        for (int j = 0; j < LEVEL_WORD_N; j++) {
            box b = { fminf(LEVEL_WORD[j][0], LEVEL_WORD[j][2]), fminf(LEVEL_WORD[j][1], LEVEL_WORD[j][3]),
                      fmaxf(LEVEL_WORD[j][0], LEVEL_WORD[j][2]), fmaxf(LEVEL_WORD[j][1], LEVEL_WORD[j][3]) };
            merge(&rows[lv].b, &b);
        }
    for (int k = 0; k < nrows; k++)
        rows[k].side = rows[k].level ? 1 : (rows[k].b.x0 + rows[k].b.x1) * 0.5f < SIDE_SPLIT ? 0 : 1;

    /* each strip's rows, top to bottom (above) or bottom to top (below) */
    float strip = fmaxf(SCREEN_W * 0.5f - hw_cur * PF_ASPECT_X * s - 2.0f * MARGIN, STRIP_MIN);
    hud_s = s;
    hud_strip = strip;
    for (int band = 0; band < 2; band++)
        for (int side = 0; side < 2; side++) {
            int order[MAX_ROWS], n = 0;
            for (int k = 0; k < nrows; k++)
                if (rows[k].band == band && rows[k].side == side && rows[k].b.x1 >= rows[k].b.x0) order[n++] = k;
            for (int a = 1; a < n; a++)             /* by their place on the cabinet's screen */
                for (int b = a; b > 0 && rows[order[b]].b.y0 < rows[order[b - 1]].b.y0; b--) {
                    int t = order[b]; order[b] = order[b - 1]; order[b - 1] = t;
                }
            float v = band == 0 ? -0.5f + MARGIN : 0.5f - MARGIN;
            for (int j = 0; j < n; j++) {
                row *r = &rows[order[band == 0 ? j : n - 1 - j]];
                float w = (r->b.x1 - r->b.x0) * ASPECT_X, h = r->b.y1 - r->b.y0;
                /* (no larger than the text of the whole picture fitted to the height) */
                float grow = HUD_GROW * (r->level ? LEVEL_GROW : 1.0f);
                r->scale = fminf(fminf(s, TEXT_SCALE_MAX) * grow, strip / (w > 1.0f ? w : 1.0f));
                float wu = w * r->scale, hv = h * r->scale;
                r->u0 = side == 0 ? -SCREEN_W * 0.5f + MARGIN : SCREEN_W * 0.5f - MARGIN - wu;
                if (band == 0) { r->v0 = v; v += hv + ROW_GAP; }
                else           { v -= hv; r->v0 = v; v -= ROW_GAP; }
            }
        }

    /* the stars: x * k in space; in the tube x * (1 + (k - 1) * (|x| / STAR_REACH)^2)
     * up to the reach, x * k beyond */
    float star_k = (SCREEN_W * 0.5f - MARGIN) / (STAR_REACH * PF_ASPECT_X * s);
    if (star_k < 1.0f) star_k = 1.0f;
    int in_tube = tube.x1 - tube.x0 > TUBE_MAX_W || tube.y1 - tube.y0 > TUBE_MAX_H;
    float s_hud = (1.0f - 2.0f * MARGIN) / (HUD_BOTTOM - HUD_TOP), cy_hud = (HUD_TOP + HUD_BOTTOM) * 0.5f;

    /* the flight's text (SUPERZAPPER RECHARGE, under the tube, above the
     * bands): its box, to the middle (the bands' text, the copyright of
     * the attract mode's flights among it, stays in its place) */
    int recharge = flight && cfg->fx[TP_FX_RECHARGE];
    box rb = { 1e9f, 1e9f, -1e9f, -1e9f };
    for (int i = 0; recharge && i < count; i++)
        if (in[i].sub == TP_TEXT_SUB && row_of[i] < 0 && !gone[i] && hud_band(&in[i]) < 0) {
            box b = line_box(&in[i]);
            merge(&rb, &b);
        }
    if (rb.x1 < rb.x0) recharge = 0;
    float rs = TEXT_SCALE_MAX * RECHARGE_GROW, rcx = (rb.x0 + rb.x1) * 0.5f, rcy = (rb.y0 + rb.y1) * 0.5f;
    if (recharge && (rb.x1 - rb.x0) * ASPECT_X * rs > SCREEN_W - 2.0f * MARGIN)
        rs = (SCREEN_W - 2.0f * MARGIN) / ((rb.x1 - rb.x0) * ASPECT_X);

    for (int i = 0; i < count; i++) {
        out[i] = in[i];
        float x0 = in[i].x0, y0 = in[i].y0, x1 = in[i].x1, y1 = in[i].y1;
        if (in[i].sub == STAR_SUB && star_k > 1.0f) {
            float a0 = in_tube ? fminf(fabsf(x0) / STAR_REACH, 1.0f) : 1.0f;
            float a1 = in_tube ? fminf(fabsf(x1) / STAR_REACH, 1.0f) : 1.0f;
            x0 *= 1.0f + (star_k - 1.0f) * a0 * a0;
            x1 *= 1.0f + (star_k - 1.0f) * a1 * a1;
        }
        float u0, v0, u1, v1;
        int k = row_of[i];
        if (k >= 0 && attract && !(rows[k].band == 0 && rows[k].side == 0)) {
            /* (the score at the top left as in a game, the rest in place) */
            int band = rows[k].band;
            if (is_copyright(&in[i], band)) set_rgb(&out[i], COPYRIGHT_RGB);
            if (band == 1) { x0 += below_dx[i]; x1 += below_dx[i]; }
            hud_grow(band, &x0, &y0);
            hud_grow(band, &x1, &y1);
            u0 = (x0 - TEXT_CX) * ASPECT_X * s_hud; v0 = (y0 - cy_hud) * s_hud;
            u1 = (x1 - TEXT_CX) * ASPECT_X * s_hud; v1 = (y1 - cy_hud) * s_hud;
        } else if (k >= 0) {
            const row *r = &rows[k];
            if (r->level) {
                number_grow(&x0, &y0);
                number_grow(&x1, &y1);
            }
            u0 = r->u0 + (x0 - r->b.x0) * ASPECT_X * r->scale; v0 = r->v0 + (y0 - r->b.y0) * r->scale;
            u1 = r->u0 + (x1 - r->b.x0) * ASPECT_X * r->scale; v1 = r->v0 + (y1 - r->b.y0) * r->scale;
        } else if (recharge && in[i].sub == TP_TEXT_SUB && !gone[i] && hud_band(&in[i]) < 0) {
            u0 = (x0 - rcx) * ASPECT_X * rs; v0 = (y0 - rcy) * rs;
            u1 = (x1 - rcx) * ASPECT_X * rs; v1 = (y1 - rcy) * rs;
            set_rgb(&out[i], RECHARGE_RGB);
            out[i].intensity = 15;
        } else {
            u0 = x0 * PF_ASPECT_X * s; v0 = (y0 - cy) * s;
            u1 = x1 * PF_ASPECT_X * s; v1 = (y1 - cy) * s;
        }
        to_board(cfg, u0, v0, &out[i].x0, &out[i].y0);
        to_board(cfg, u1, v1, &out[i].x1, &out[i].y1);
        if (gone[i]) out[i].intensity = 0;          /* (not drawn) */
    }
    /* the word LEVEL, in the number's colour and brightness */
    int total = count;
    if (lv >= 0) {
        const row *r = &rows[lv];
        tp_line word = in[lv_line];
        for (int j = 0; j < LEVEL_WORD_N; j++) {
            tp_line *o = &out[total];
            *o = word;
            to_board(cfg, r->u0 + (LEVEL_WORD[j][0] - r->b.x0) * ASPECT_X * r->scale,
                     r->v0 + (LEVEL_WORD[j][1] - r->b.y0) * r->scale, &o->x0, &o->y0);
            to_board(cfg, r->u0 + (LEVEL_WORD[j][2] - r->b.x0) * ASPECT_X * r->scale,
                     r->v0 + (LEVEL_WORD[j][3] - r->b.y0) * r->scale, &o->x1, &o->y1);
            row_of[total++] = lv;
        }
    }

    /* In a game the text sits in the corners: drawn after the playfield,
     * corner by corner (in the game's order within one), so that the beam
     * jumps out to each corner once a picture instead of to and fro
     * between them (after a long jump the deflection settles, and the text
     * drawn then trembles). */
    if (!attract) {
        static tp_line sorted[8192 + TP_LAYOUT_EXTRA];
        int n = 0;
        for (int i = 0; i < total; i++)
            if (row_of[i] < 0) sorted[n++] = out[i];
        static const int corner[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };   /* band, side */
        for (int c = 0; c < 4; c++)
            for (int i = 0; i < total; i++) {
                int k = row_of[i];
                if (k >= 0 && rows[k].band == corner[c][0] && rows[k].side == corner[c][1]) sorted[n++] = out[i];
            }
        if (n == total) memcpy(out, sorted, (size_t)total * sizeof *out);
    }
    return total;
}

void tp_layout_set_attract(int on)
{
    attract = on != 0;
}

void tp_layout_set_rating(int on)
{
    rating = on != 0;
}

void tp_layout_set_entry(int on)
{
    entry = on != 0;
}

void tp_layout_set_flight(int on)
{
    flight = on != 0;
}

int tp_layout_aim(const tp_line *lines, int count)
{
    box tube;
    float y0, y1, hw, s_fit;
    if (count > 8192 || !playfield_of(lines, count, &tube, &y0, &y1, &hw, &s_fit)) return 0;
    s_cur = s_fit;
    hw_cur = hw;
    cy_cur = (y0 + y1) * 0.5f;
    float reach = (0.5f - PF_MARGIN) / s_cur;
    if (cy_cur > y0 + reach) cy_cur = y0 + reach;
    if (cy_cur < y1 - reach) cy_cur = y1 - reach;
    return 1;
}

void tp_layout_screen(const tp_config *cfg, float u, float v, int *bx, int *by)
{
    float x, y;
    to_board(cfg, u, v, &x, &y);
    *bx = (int)lroundf(x);
    *by = (int)lroundf(y);
}

void tp_layout_map(const tp_config *cfg, float x, float y, int *bx, int *by)
{
    if (!last_horizontal || cfg->orientation != TP_ORIENT_HORIZONTAL) {
        tp_layout_point(cfg, x, y, bx, by);
        return;
    }
    tp_layout_screen(cfg, (x - last_cx) * ASPECT_X * last_s, (y - last_cy) * last_s, bx, by);
}

int tp_layout(const tp_config *cfg, const tp_line *lines, int count, tp_line *out)
{
    last_horizontal = cfg->orientation == TP_ORIENT_HORIZONTAL && count <= 8192;
    if (cfg->orientation == TP_ORIENT_HORIZONTAL && count <= 8192)
        return horizontal(cfg, lines, count, out);
    for (int i = 0; i < count; i++) {
        int x0, y0, x1, y1;
        tp_layout_point(cfg, lines[i].x0, lines[i].y0, &x0, &y0);
        tp_layout_point(cfg, lines[i].x1, lines[i].y1, &x1, &y1);
        out[i] = lines[i];
        out[i].x0 = (float)x0; out[i].y0 = (float)y0;
        out[i].x1 = (float)x1; out[i].y1 = (float)y1;
    }
    return count;
}

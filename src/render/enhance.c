#include "render/enhance.h"

#include "render/layout.h"

#include <math.h>
#include <string.h>

/* RAM of the program (revision 3) */
#define RAM_QSTATE   0x0000
#define RAM_SUZTIM   0x0125     /* in play: counts up while the superzapper works, else 0 (found by trying; it counts in the flight down the tube too) */
#define ST_PLAY      0x04
#define ST_ENDLIFE   0x06
#define ST_ENDWAVE   0x0e
#define ST_INTO      0x18
#define ST_DOWN      0x20

/* The game's sub-lists */
#define TUBE_SUB     0x240a
#define RIM_GONE     800.0f     /* a near rim wider than this (generator units) is passed: a tube at rest is ~490 at most */
#define CLAW_SUB     0x2dbc
#define SHOT_SUB     0x28fe
#define BOOM_SUB     0x2cd6
#define STAR_SUB     0x2e22
#define ENEMY_SUB    0x2690     /* the enemies the game draws line by line (flippers ...) */
#define ENEMY2_SUB   0x2a54

#define TRAIL_AGE    3          /* the trail: the shots of this many pictures before, fainter */
#define TRAIL_BRIGHT 0.45f
#define SPARKS       6          /* sparks an explosion throws */
#define SPARK_V0     120.0f     /* their speed, generator units a second */
#define SPARK_V1     260.0f
#define SPARK_LIFE   0.45f
#define SPARK_TAIL   0.018f     /* a spark's line: its way in this many seconds */
#define BOOM_SAME    30.0f      /* an explosion this near one of the picture before is that one */
#define CLAW_LIFE    1.1f
#define CLAW_PUSH    3.5f       /* a fragment's speed: its distance from the middle times this, a second */
#define SHATTER_R    45.0f      /* an enemy's line this near a new explosion, and gone, flies apart */
#define SHATTER_LIFE 0.8f
#define SHATTER_V    90.0f      /* and so fast at least */

/* The warp starfield (generator units): stars in space, at a depth z
 * (1 the farthest, WARP_NEAR the nearest), seen at the vanishing point
 * plus (x, y) / z; the viewer flies into the depth, each star a streak from
 * where it is to where it was WARP_TAIL seconds before. */
#define WARP_SPREAD  75.0f      /* how far from the line of flight the stars are (x, y) */
#define WARP_NEAR    0.12f
#define WARP_V0      0.15f      /* the speed at the start of the flight, depths a second */
#define WARP_VMAX    1.10f
#define WARP_ACCEL   0.75f      /* down the tube: the speed gathers by this a second */
#define WARP_BRAKE   0.90f      /* as the next tube comes: it runs out by this part a second */
#define WARP_TAIL    0.14f      /* a streak: the way of this many seconds */
#define WARP_HEAD    0.25f      /* its lighter head: this part of it */
#define WARP_IN      2.0f       /* it comes in over 1/this seconds */
#define WARP_OUT     4.0f       /* and goes over 1/this, the stars still flying */
static const uint8_t WARP_RGB[6][3] = {
    { 90, 140, 255 }, { 60, 220, 255 }, { 240, 100, 240 }, { 160, 110, 255 }, { 255, 120, 150 }, { 255, 190, 90 },
};

static const uint8_t ZAP_RGB[6][3] = {
    { 255, 255, 255 }, { 255, 255, 0 }, { 255, 60, 60 }, { 255, 0, 255 }, { 60, 120, 255 }, { 0, 255, 255 },
};

static float rnd(tp_enhance *fx)
{
    fx->rng ^= fx->rng << 13;
    fx->rng ^= fx->rng >> 17;
    fx->rng ^= fx->rng << 5;
    return (float)(fx->rng & 0xffffff) / (float)0x1000000;
}

void tp_enhance_init(tp_enhance *fx)
{
    memset(fx, 0, sizeof *fx);
    fx->rng = 0x9e3779b9u;
}

void tp_enhance_free(tp_enhance *fx)
{
    tp_avg_list_free(&fx->out);
    memset(fx, 0, sizeof *fx);
}

static uint8_t dim(uint8_t intensity, float k)
{
    int i = (int)lroundf((float)intensity * k);
    return (uint8_t)(i < 1 ? 1 : i > 15 ? 15 : i);
}

static int near_pt(float ax, float ay, float bx, float by)
{
    return fabsf(ax - bx) < 0.6f && fabsf(ay - by) < 0.6f;
}

static tp_line *add(tp_enhance *fx)
{
    if (fx->out.count >= fx->out.cap && tp_avg_list_reserve(&fx->out, fx->out.count * 2 + 64) != 0) return NULL;
    return &fx->out.lines[fx->out.count++];
}

/* A line of the port's own, cleared, in a colour. */
static tp_line *add_own(tp_enhance *fx, uint8_t r, uint8_t g, uint8_t b, uint8_t intensity)
{
    tp_line *o = add(fx);
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->sub = TP_FX_SUB;
    o->r = r; o->g = g; o->b = b;
    o->intensity = intensity;
    o->z = 1;
    return o;
}

/* --- the tube: its spokes, its two rims, the claw's lane -------------------------------- */

#define MAX_TUBE 256

typedef struct tube {
    int   n, idx[MAX_TUBE];         /* its lines in the picture */
    int   kind[MAX_TUBE];           /* 0 a spoke, 1 the near rim, 2 the far rim */
    int   far_end[MAX_TUBE];        /* a spoke: which end is the far one (0: x0,y0; 1: x1,y1), -1 unknown */
    int   lane[2];                  /* the claw's lane: its two spokes (tube line numbers), -1 none */
} tube;

static void tube_of(const tp_avg_list *pic, tube *t)
{
    t->n = 0;
    t->lane[0] = t->lane[1] = -1;
    for (int i = 0; i < pic->count && t->n < MAX_TUBE; i++)
        if (pic->lines[i].sub == TUBE_SUB) t->idx[t->n++] = i;
    /* chains of lines joined end to start: the rims; single lines: the spokes */
    int chain[MAX_TUBE], nchain = 0;
    float len[MAX_TUBE];
    for (int k = 0; k < t->n; k++) {
        const tp_line *q = &pic->lines[t->idx[k]];
        if (k && near_pt(pic->lines[t->idx[k - 1]].x1, pic->lines[t->idx[k - 1]].y1, q->x0, q->y0)) {
            chain[k] = chain[k - 1];
        } else {
            chain[k] = nchain;
            len[nchain++] = 0.0f;
        }
        len[chain[k]] += hypotf(q->x1 - q->x0, q->y1 - q->y0);
    }
    int size[MAX_TUBE] = { 0 };
    for (int k = 0; k < t->n; k++) size[chain[k]]++;
    int far = -1, nrims = 0;
    for (int c = 0; c < nchain; c++)
        if (size[c] >= 2) {
            nrims++;
            if (far < 0 || len[c] < len[far]) far = c;
        }
    for (int k = 0; k < t->n; k++) {
        t->kind[k] = size[chain[k]] >= 2 ? (nrims >= 2 && chain[k] == far ? 2 : 1) : 0;
        t->far_end[k] = -1;
    }
    if (nrims < 2) return;
    /* a spoke's far end: on the far rim */
    for (int k = 0; k < t->n; k++) {
        if (t->kind[k] != 0) continue;
        const tp_line *q = &pic->lines[t->idx[k]];
        for (int j = 0; j < t->n && t->far_end[k] < 0; j++) {
            if (t->kind[j] != 2) continue;
            const tp_line *r = &pic->lines[t->idx[j]];
            if (near_pt(q->x0, q->y0, r->x0, r->y0) || near_pt(q->x0, q->y0, r->x1, r->y1)) t->far_end[k] = 0;
            else if (near_pt(q->x1, q->y1, r->x0, r->y0) || near_pt(q->x1, q->y1, r->x1, r->y1)) t->far_end[k] = 1;
        }
    }
    /* the claw's lane: the two spokes the game draws in another colour than the others */
    int count[16] = { 0 }, most = -1;
    for (int k = 0; k < t->n; k++)
        if (t->kind[k] == 0) count[pic->lines[t->idx[k]].color & 15]++;
    for (int c = 0; c < 16; c++)
        if (count[c] && (most < 0 || count[c] > count[most])) most = c;
    int nl = 0;
    for (int k = 0; k < t->n; k++)
        if (t->kind[k] == 0 && (pic->lines[t->idx[k]].color & 15) != most && t->far_end[k] >= 0) {
            if (nl < 2) t->lane[nl] = k;
            nl++;
        }
    if (nl != 2) t->lane[0] = t->lane[1] = -1;
}

static void ends(const tp_line *q, int far_end, float *nx, float *ny, float *fx_, float *fy)
{
    if (far_end == 0) { *fx_ = q->x0; *fy = q->y0; *nx = q->x1; *ny = q->y1; }
    else              { *fx_ = q->x1; *fy = q->y1; *nx = q->x0; *ny = q->y0; }
}

/* --- lines flying apart (the claw, enemies) ------------------------------------------------- */

static void fly_apart(tp_enhance *fx, const tp_line *q, float cx, float cy, float push, float v_min, float life)
{
    if (fx->nf >= TP_FX_FRAGMENTS) return;
    tp_fx_fragment *f = &fx->f[fx->nf++];
    f->x = (q->x0 + q->x1) * 0.5f; f->y = (q->y0 + q->y1) * 0.5f;
    f->dx = (q->x1 - q->x0) * 0.5f; f->dy = (q->y1 - q->y0) * 0.5f;
    float ox = f->x - cx, oy = f->y - cy, d = hypotf(ox, oy);
    float v = fmaxf(d * push, v_min);
    if (d < 0.5f) { float a = rnd(fx) * 6.2831853f; ox = cosf(a); oy = sinf(a); d = 1.0f; }
    f->vx = ox / d * v + (rnd(fx) - 0.5f) * 60.0f;
    f->vy = oy / d * v + (rnd(fx) - 0.5f) * 60.0f;
    f->spin = (rnd(fx) - 0.5f) * 16.0f;
    f->age = 0.0f;
    f->life = life;
    f->r = q->r; f->g = q->g; f->b = q->b; f->intensity = q->intensity;
}

/* --- the warp starfield ----------------------------------------------------------------------- */

static void warp_star(tp_enhance *fx, tp_fx_warp *w, float z)
{
    float a = rnd(fx) * 6.2831853f, r = WARP_SPREAD * sqrtf(rnd(fx));
    w->x = cosf(a) * r;
    w->y = sinf(a) * r;
    w->z = z;
    w->colour = (uint8_t)(rnd(fx) * 6.0f) % 6;
}

/* flying: 1 down the tube (gathering speed), 2 towards the next tube
 * (running out), 0 the flight over (the stars fly on and fade) */
static void warp(tp_enhance *fx, int flying, float t)
{
    if (fx->warp_on <= 0.0f) {
        if (!flying) return;
        for (int i = 0; i < TP_FX_NWARP; i++) warp_star(fx, &fx->warp[i], WARP_NEAR + (1.0f - WARP_NEAR) * rnd(fx));
        fx->warp_v = WARP_V0;
    }
    fx->warp_cx = fx->vp_x;
    fx->warp_cy = fx->vp_y;
    if (flying == 1) fx->warp_v = fminf(WARP_VMAX, fx->warp_v + WARP_ACCEL * t);
    else fx->warp_v -= fx->warp_v * WARP_BRAKE * t;
    fx->warp_on = flying ? fminf(1.0f, fx->warp_on + t * WARP_IN) : fmaxf(0.0f, fx->warp_on - t * WARP_OUT);
    if (fx->warp_on <= 0.0f) return;
    for (int i = 0; i < TP_FX_NWARP; i++) {
        tp_fx_warp *w = &fx->warp[i];
        w->z -= fx->warp_v * t;
        if (w->z < WARP_NEAR) warp_star(fx, w, w->z + (1.0f - WARP_NEAR));
    }
    /* by their angle round the middle, so the beam goes round once */
    int order[TP_FX_NWARP];
    float ang[TP_FX_NWARP];
    for (int i = 0; i < TP_FX_NWARP; i++) {
        order[i] = i;
        ang[i] = atan2f(fx->warp[i].y, fx->warp[i].x);
    }
    for (int a = 1; a < TP_FX_NWARP; a++)
        for (int b = a; b > 0 && ang[order[b]] < ang[order[b - 1]]; b--) {
            int s = order[b]; order[b] = order[b - 1]; order[b - 1] = s;
        }
    float tail = fx->warp_v * WARP_TAIL;
    for (int j = 0; j < TP_FX_NWARP; j++) {
        const tp_fx_warp *w = &fx->warp[order[j]];
        float z0 = w->z + tail, z1 = w->z + tail * WARP_HEAD, z2 = w->z;
        float k = fx->warp_on * (0.35f + 0.65f * (1.0f - w->z));
        const uint8_t *rgb = WARP_RGB[w->colour];
        tp_line *o = add_own(fx, rgb[0], rgb[1], rgb[2], dim(15, k));
        if (!o) return;
        o->sub = STAR_SUB;          /* (laid out as the game's stars are: over the width) */
        o->x0 = fx->warp_cx + w->x / z0; o->y0 = fx->warp_cy + w->y / z0;
        o->x1 = fx->warp_cx + w->x / z1; o->y1 = fx->warp_cy + w->y / z1;
        /* the head: the colour, lighter */
        o = add_own(fx, (uint8_t)((rgb[0] + 255) / 2), (uint8_t)((rgb[1] + 255) / 2), (uint8_t)((rgb[2] + 255) / 2),
                    dim(15, k));
        if (!o) return;
        o->sub = STAR_SUB;
        o->x0 = fx->warp_cx + w->x / z1; o->y0 = fx->warp_cy + w->y / z1;
        o->x1 = fx->warp_cx + w->x / z2; o->y1 = fx->warp_cy + w->y / z2;
        if (z0 - z2 < 0.002f) o->x1 += 0.5f;        /* (a dot still shows) */
    }
}

/* --- the picture -------------------------------------------------------------------------- */

const tp_avg_list *tp_enhance_picture(tp_enhance *fx, const tp_config *cfg, const tp_avg_list *pic,
                                      const uint8_t *mem, double dt)
{
    int any = 0;
    for (int i = 0; i < TP_FX_GAME_COUNT; i++) any |= cfg->fx[i];
    fx->pictures++;
    fx->time += dt;
    uint8_t st = mem[RAM_QSTATE];
    fx->state = st;
    if (!any) {
        fx->np = fx->nf = fx->nboom = fx->nclaw = fx->nenemy = 0;
        fx->warp_on = 0.0f;
        return pic;
    }
    int flight = st == ST_DOWN || st == ST_ENDWAVE || st == ST_INTO;
    int zap = cfg->fx[TP_FX_ZAP] && st == ST_PLAY && mem[RAM_SUZTIM] != 0;
    float t = (float)dt;

    fx->out.count = 0;
    if (tp_avg_list_reserve(&fx->out, pic->count * 2 + 512) != 0) return pic;

    static tube tb;
    tube_of(pic, &tb);
    static int tube_k[8192];
    for (int i = 0; i < pic->count && i < 8192; i++) tube_k[i] = -1;
    for (int k = 0; k < tb.n; k++) if (tb.idx[k] < 8192) tube_k[tb.idx[k]] = k;

    /* the lane's corners */
    int lane = cfg->fx[TP_FX_LANE] && tb.lane[0] >= 0 && st == ST_PLAY;
    float na[2] = { 0 }, nb[2] = { 0 }, fa[2] = { 0 }, fb[2] = { 0 };
    tp_line lane_line;
    memset(&lane_line, 0, sizeof lane_line);
    if (lane) {
        const tp_line *a = &pic->lines[tb.idx[tb.lane[0]]], *b = &pic->lines[tb.idx[tb.lane[1]]];
        ends(a, tb.far_end[tb.lane[0]], &na[0], &na[1], &fa[0], &fa[1]);
        ends(b, tb.far_end[tb.lane[1]], &nb[0], &nb[1], &fb[0], &fb[1]);
        lane_line = *a;
    }

    /* the vanishing point: the middle of the tube's far end, as long as
     * there is one to see (in play and as the flight begins) */
    if (tb.n && !(st == ST_ENDWAVE || st == ST_INTO)) {
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (int k = 0; k < tb.n; k++) {
            if (tb.kind[k] != 2) continue;
            const tp_line *q = &pic->lines[tb.idx[k]];
            x0 = fminf(x0, fminf(q->x0, q->x1)); x1 = fmaxf(x1, fmaxf(q->x0, q->x1));
            y0 = fminf(y0, fminf(q->y0, q->y1)); y1 = fmaxf(y1, fmaxf(q->y0, q->y1));
        }
        if (x1 >= x0) { fx->vp_x = (x0 + x1) * 0.5f; fx->vp_y = (y0 + y1) * 0.5f; fx->have_vp = 1; }
    }
    /* the old tube's near rim, passed in the flight (only with both rims
     * there: one alone may be the far rim) */
    int hide_rim = 0;
    if (cfg->fx[TP_FX_RIM] && flight) {
        float x0 = 1e9f, x1 = -1e9f;
        int far_rim = 0;
        for (int k = 0; k < tb.n; k++) {
            if (tb.kind[k] == 2) far_rim = 1;
            if (tb.kind[k] != 1) continue;
            const tp_line *q = &pic->lines[tb.idx[k]];
            x0 = fminf(x0, fminf(q->x0, q->x1)); x1 = fmaxf(x1, fmaxf(q->x0, q->x1));
        }
        hide_rim = far_rim && x1 - x0 > RIM_GONE;
    }
    int warping = cfg->fx[TP_FX_WARP] && fx->have_vp;
    int nstar = warping && flight;          /* (the game's stars left out then) */

    for (int i = 0; i < pic->count; i++) {
        const tp_line *q = &pic->lines[i];
        int k = i < 8192 ? tube_k[i] : -1;
        tp_line l = *q;
        if (q->sub == STAR_SUB && nstar) continue;      /* (the warp's own instead, below) */
        if (k >= 0 && hide_rim && tb.kind[k] == 1) continue;
        if (k >= 0 && zap) {
            const uint8_t *c = ZAP_RGB[(fx->pictures / 3 + (uint32_t)k) % 6];
            l.r = c[0]; l.g = c[1]; l.b = c[2];
            l.intensity = 15;
        }
        if (k >= 0 && lane && tb.kind[k] != 0 && !zap) {
            int on = (near_pt(q->x0, q->y0, na[0], na[1]) && near_pt(q->x1, q->y1, nb[0], nb[1])) ||
                     (near_pt(q->x0, q->y0, nb[0], nb[1]) && near_pt(q->x1, q->y1, na[0], na[1])) ||
                     (near_pt(q->x0, q->y0, fa[0], fa[1]) && near_pt(q->x1, q->y1, fb[0], fb[1])) ||
                     (near_pt(q->x0, q->y0, fb[0], fb[1]) && near_pt(q->x1, q->y1, fa[0], fa[1]));
            if (on) { l.r = lane_line.r; l.g = lane_line.g; l.b = lane_line.b; l.color = lane_line.color; }
        }
        tp_line *o = add(fx);
        if (o) *o = l;
    }

    /* the warp: down the tube, towards the next, and fading out after */
    if (warping) warp(fx, st == ST_DOWN ? 1 : flight ? 2 : 0, t);
    else fx->warp_on = 0.0f;

    /* trails: the shots of the pictures before, fainter */
    if (cfg->fx[TP_FX_TRAILS] && st == ST_PLAY) {
        fx->trail_head = (fx->trail_head + 1) % TP_FX_TRAIL_AGES;
        int n = 0;
        for (int i = 0; i < pic->count && n < TP_FX_TRAIL_MAX; i++)
            if (pic->lines[i].sub == SHOT_SUB) fx->trail[fx->trail_head][n++] = pic->lines[i];
        fx->trail_n[fx->trail_head] = n;
        int h = (fx->trail_head - TRAIL_AGE + TP_FX_TRAIL_AGES) % TP_FX_TRAIL_AGES;
        for (int j = 0; j < fx->trail_n[h]; j++) {
            tp_line *o = add(fx);
            if (!o) break;
            *o = fx->trail[h][j];
            o->sub = TP_FX_SUB;
            o->intensity = dim(o->intensity, TRAIL_BRIGHT);
        }
    } else {
        memset(fx->trail_n, 0, sizeof fx->trail_n);
    }

    /* the explosions, and which of them are new */
    float boom[TP_FX_BOOMS][2];
    int nboom = 0;
    for (int i = 0; i < pic->count && nboom < TP_FX_BOOMS;) {
        if (pic->lines[i].sub != BOOM_SUB) { i++; continue; }
        int j = i;
        float sx = 0.0f, sy = 0.0f;
        while (j < pic->count && pic->lines[j].sub == BOOM_SUB && pic->lines[j].item == pic->lines[i].item) {
            sx += pic->lines[j].x0 + pic->lines[j].x1;
            sy += pic->lines[j].y0 + pic->lines[j].y1;
            j++;
        }
        boom[nboom][0] = sx / (float)(2 * (j - i));
        boom[nboom][1] = sy / (float)(2 * (j - i));
        nboom++;
        i = j;
    }
    for (int b = 0; b < nboom; b++) {
        int seen = 0;
        for (int c = 0; c < fx->nboom && !seen; c++)
            seen = hypotf(boom[b][0] - fx->boom[c][0], boom[b][1] - fx->boom[c][1]) < BOOM_SAME;
        if (seen) continue;
        /* sparks */
        for (int s = 0; cfg->fx[TP_FX_SPARKS] && s < SPARKS && fx->np < TP_FX_PARTICLES; s++) {
            tp_fx_particle *p = &fx->p[fx->np++];
            float ang = rnd(fx) * 6.2831853f, v = SPARK_V0 + (SPARK_V1 - SPARK_V0) * rnd(fx);
            p->x = boom[b][0]; p->y = boom[b][1];
            p->vx = cosf(ang) * v; p->vy = sinf(ang) * v;
            p->age = 0.0f;
            p->life = SPARK_LIFE * (0.7f + 0.6f * rnd(fx));
        }
        /* the enemy hit there: its lines of the picture before that are gone now */
        for (int e = 0; cfg->fx[TP_FX_SHATTER] && e < fx->nenemy; e++) {
            const tp_line *q = &fx->enemy[e];
            float mx = (q->x0 + q->x1) * 0.5f, my = (q->y0 + q->y1) * 0.5f;
            if (hypotf(mx - boom[b][0], my - boom[b][1]) > SHATTER_R) continue;
            int still = 0;
            for (int i = 0; i < pic->count && !still; i++) {
                const tp_line *r = &pic->lines[i];
                if (r->sub != q->sub) continue;
                still = fabsf((r->x0 + r->x1) * 0.5f - mx) < 4.0f && fabsf((r->y0 + r->y1) * 0.5f - my) < 4.0f;
            }
            if (!still) fly_apart(fx, q, boom[b][0], boom[b][1], 2.0f, SHATTER_V, SHATTER_LIFE);
        }
    }
    memcpy(fx->boom, boom, sizeof boom);
    fx->nboom = nboom;
    fx->nenemy = 0;
    for (int i = 0; i < pic->count && fx->nenemy < TP_FX_ENEMY_LINES; i++)
        if (pic->lines[i].sub == ENEMY_SUB || pic->lines[i].sub == ENEMY2_SUB) fx->enemy[fx->nenemy++] = pic->lines[i];

    /* the sparks: short, white to yellow to orange */
    for (int i = 0; i < fx->np;) {
        tp_fx_particle *p = &fx->p[i];
        p->age += t;
        if (p->age >= p->life || !cfg->fx[TP_FX_SPARKS]) { fx->p[i] = fx->p[--fx->np]; continue; }
        p->x += p->vx * t;
        p->y += p->vy * t;
        float a = p->age / p->life;
        tp_line *o = add_own(fx, 255, (uint8_t)(a < 0.35f ? 255 : a < 0.7f ? 220 : 140),
                             (uint8_t)(a < 0.35f ? 255 : a < 0.7f ? 90 : 20), dim(15, 1.0f - 0.7f * a));
        if (!o) break;
        o->x0 = p->x - p->vx * SPARK_TAIL; o->y0 = p->y - p->vy * SPARK_TAIL;
        o->x1 = p->x; o->y1 = p->y;
        i++;
    }

    /* the claw breaking: it was there, and now (in play or at a life's end) is not */
    int nclaw = 0;
    tp_line claw[16];
    for (int i = 0; i < pic->count && nclaw < 16; i++)
        if (pic->lines[i].sub == CLAW_SUB) claw[nclaw++] = pic->lines[i];
    if (cfg->fx[TP_FX_CLAW] && !nclaw && fx->nclaw && (st == ST_PLAY || st == ST_ENDLIFE)) {
        float cx = 0.0f, cy = 0.0f;
        for (int k = 0; k < fx->nclaw; k++) { cx += fx->claw[k].x0 + fx->claw[k].x1; cy += fx->claw[k].y0 + fx->claw[k].y1; }
        cx /= (float)(2 * fx->nclaw); cy /= (float)(2 * fx->nclaw);
        for (int k = 0; k < fx->nclaw; k++) fly_apart(fx, &fx->claw[k], cx, cy, CLAW_PUSH, 30.0f, CLAW_LIFE);
    }
    memcpy(fx->claw, claw, (size_t)nclaw * sizeof *claw);
    fx->nclaw = nclaw;

    /* the lines flying apart, turning and fading */
    for (int i = 0; i < fx->nf;) {
        tp_fx_fragment *f = &fx->f[i];
        f->age += t;
        if (f->age >= f->life) { fx->f[i] = fx->f[--fx->nf]; continue; }
        f->x += f->vx * t;
        f->y += f->vy * t;
        float ang = f->spin * f->age, c = cosf(ang), s = sinf(ang);
        float dx = f->dx * c - f->dy * s, dy = f->dx * s + f->dy * c;
        tp_line *o = add_own(fx, f->r, f->g, f->b, dim(f->intensity, 1.0f - f->age / f->life));
        if (!o) break;
        o->x0 = f->x - dx; o->y0 = f->y - dy;
        o->x1 = f->x + dx; o->y1 = f->y + dy;
        i++;
    }
    return &fx->out;
}

/* --- the screen's own: stars beside the playfield ---------------------------------------- */

#define STAR_SPEED   0.22f      /* outwards, of their distance a second (and a little more) */
/* their colours: white and tinted */
static const uint8_t STAR_RGB[6][3] = {
    { 255, 255, 255 }, { 130, 160, 255 }, { 255, 130, 180 }, { 255, 215, 110 }, { 120, 235, 235 }, { 195, 140, 255 },
};

int tp_enhance_screen(tp_enhance *fx, const tp_config *cfg, tp_line *placed, int n, double dt)
{
    if (!cfg->fx[TP_FX_STARS] || cfg->orientation != TP_ORIENT_HORIZONTAL || fx->state != ST_PLAY) return 0;
    /* the playfield on the screen: the tube's box (no tube: no stars) */
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int i = 0; i < n; i++)
        if (placed[i].sub == TUBE_SUB) {
            x0 = fminf(x0, fminf(placed[i].x0, placed[i].x1)); x1 = fmaxf(x1, fmaxf(placed[i].x0, placed[i].x1));
            y0 = fminf(y0, fminf(placed[i].y0, placed[i].y1)); y1 = fmaxf(y1, fmaxf(placed[i].y0, placed[i].y1));
        }
    if (x1 < x0) return 0;
    if (!fx->stars_ready) {
        for (int s = 0; s < TP_FX_NSTARS; s++) {
            float ang = rnd(fx) * 6.2831853f;
            fx->star[s][0] = cosf(ang); fx->star[s][1] = sinf(ang); fx->star[s][2] = 0.05f + 0.8f * rnd(fx);
            fx->star[s][3] = rnd(fx) * 6.2831853f; fx->star[s][4] = 3.0f + 6.0f * rnd(fx);
            fx->star[s][5] = (float)((int)(rnd(fx) * 6.0f) % 6);
        }
        fx->stars_ready = 1;
    }
    /* the stars: out from the middle, each a dot, beside the playfield,
     * twinkling; by their place from left to right, so the beam goes once
     * across */
    int idx[TP_FX_NSTARS], ni = 0;
    float bx[TP_FX_NSTARS], by[TP_FX_NSTARS];
    for (int s = 0; s < TP_FX_NSTARS; s++) {
        float *st = fx->star[s];
        st[2] += (st[2] * STAR_SPEED + 0.01f) * (float)dt;
        float u = st[0] * st[2], v = st[1] * st[2];
        if (fabsf(u) > 0.68f || fabsf(v) > 0.52f) {
            float ang = rnd(fx) * 6.2831853f;
            st[0] = cosf(ang); st[1] = sinf(ang); st[2] = 0.05f + 0.1f * rnd(fx);
            st[5] = (float)((int)(rnd(fx) * 6.0f) % 6);
            continue;
        }
        int px, py;
        tp_layout_screen(cfg, u, v, &px, &py);
        if (px > x0 - 60 && px < x1 + 60 && py > y0 - 60 && py < y1 + 60) continue;
        bx[ni] = (float)px; by[ni] = (float)py;
        idx[ni++] = s;
    }
    for (int a = 1; a < ni; a++)
        for (int b = a; b > 0 && bx[b] < bx[b - 1]; b--) {
            float tx = bx[b]; bx[b] = bx[b - 1]; bx[b - 1] = tx;
            float ty = by[b]; by[b] = by[b - 1]; by[b - 1] = ty;
            int ti = idx[b]; idx[b] = idx[b - 1]; idx[b - 1] = ti;
        }
    int added = 0;
    for (int j = 0; j < ni && added < TP_ENHANCE_SCREEN; j++) {
        const float *st = fx->star[idx[j]];
        float tw = 0.5f + 0.5f * sinf(st[3] + (float)fx->time * st[4]);       /* twinkling */
        float k = (0.15f + 0.85f * tw * tw) * (0.6f + 0.4f * fminf(1.0f, st[2] * 1.6f));
        tp_line *o = &placed[n + added++];
        memset(o, 0, sizeof *o);
        o->sub = TP_FX_SUB;
        o->x0 = o->x1 = bx[j];
        o->y0 = o->y1 = by[j];
        const uint8_t *rgb = STAR_RGB[(int)st[5] % 6];
        o->r = rgb[0]; o->g = rgb[1]; o->b = rgb[2];
        o->intensity = dim(15, k);
        o->z = 1;
    }
    return added;
}

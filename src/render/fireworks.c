#include "render/fireworks.h"

#include "render/layout.h"

#include <math.h>
#include <string.h>

/* The screen as render/layout.c has it: units as tall as the screen, x
 * -2/3..2/3; a unit of x of the generator this much of one of y. */
#define ASPECT_X    (0.75f * 580.0f / 570.0f)
#define SCREEN_W    (4.0f / 3.0f)
#define MARGIN      0.03f

/* The fireworks' field (generator units): its height fills the screen's. */
#define FW_TOP      (-100.0f)
#define FW_BOTTOM   260.0f
#define FW_RADIUS   60.0f           /* a cloud's dots reach about this far from its middle */
#define FW_SPREAD   55.0f           /* a middle this far out goes 3/4 of the way to the edge */
/* The copies: how many pictures before, and how far up or down (the first
 * also mirrored, so that it fills the other side). */
#define COPY1_AGO   18
#define COPY2_AGO   36
#define COPY1_DV    (-0.10f)
#define COPY2_DV    0.14f

int tp_fireworks_in(const tp_avg_list *pic)
{
    for (int i = 0; i < pic->count; i++)
        if (TP_FW_IS_SHAPE(pic->lines[i].ikey)) return 1;
    return 0;
}

void tp_fireworks_reset(tp_fireworks *fw)
{
    fw->head = 0;
    fw->count = 0;
}

/* The clouds of a picture: its lines of the shape, by the call (item) that drew them. */
static void clouds_of(const tp_avg_list *pic, tp_fw_picture *p)
{
    int item[TP_FW_CLOUDS];
    p->n = 0;
    for (int i = 0; i < pic->count; i++) {
        const tp_line *l = &pic->lines[i];
        if (!TP_FW_IS_SHAPE(l->ikey)) continue;
        int k;
        for (k = 0; k < p->n; k++)
            if (item[k] == l->item) break;
        if (k == p->n) {
            if (p->n == TP_FW_CLOUDS) continue;
            item[k] = l->item;
            memset(&p->c[k], 0, sizeof p->c[k]);
            p->c[k].r = l->r; p->c[k].g = l->g; p->c[k].b = l->b;
            p->c[k].intensity = l->intensity;
            p->n++;
        }
        tp_fw_cloud *c = &p->c[k];
        if (c->n == TP_FW_DOTS) continue;
        c->dx[c->n] = l->x0;            /* (a dot: x0,y0 is x1,y1) */
        c->dy[c->n] = l->y0;
        c->n++;
    }
    for (int k = 0; k < p->n; k++) {
        tp_fw_cloud *c = &p->c[k];
        float sx = 0.0f, sy = 0.0f;
        for (int j = 0; j < c->n; j++) { sx += c->dx[j]; sy += c->dy[j]; }
        c->mx = sx / (float)c->n;
        c->my = sy / (float)c->n;
        for (int j = 0; j < c->n; j++) { c->dx[j] -= c->mx; c->dy[j] -= c->my; }
    }
}

typedef struct place {
    float s, cy;                /* screen units per generator unit, the field's middle */
    float umax;                 /* how far out a cloud's middle may go */
} place;

typedef struct shown {
    const tp_fw_cloud *c;
    float u, dv;                /* where its middle goes across the screen, how far up or down */
    int   mirror, rot;          /* mirrored; its colours turned by this */
} shown;

/* Where a cloud's middle goes across the screen. */
static float middle_u(const place *pl, float mx)
{
    return pl->umax * tanhf(mx / FW_SPREAD * 0.973f);       /* (tanh 0.973 = 3/4) */
}

static int put_dot(const tp_config *cfg, const tp_fw_cloud *c, int j, float u0, float dv, int mirror, int rot,
                   const place *pl, tp_line *out)
{
    float u = u0 + (mirror ? -c->dx[j] : c->dx[j]) * ASPECT_X * pl->s;
    float v = (c->my + c->dy[j] - pl->cy) * pl->s + dv;
    int bx, by;
    tp_layout_screen(cfg, u, v, &bx, &by);
    memset(out, 0, sizeof *out);
    out->x0 = out->x1 = (float)bx;
    out->y0 = out->y1 = (float)by;
    uint8_t ch[3] = { c->r, c->g, c->b };
    out->r = ch[rot % 3]; out->g = ch[(rot + 1) % 3]; out->b = ch[(rot + 2) % 3];
    out->intensity = c->intensity;
    out->z = 1;
    out->sub = TP_TEXT_SUB;
    out->ikey = TP_FW_SHAPE;
    return 1;
}

int tp_fireworks_layout(tp_fireworks *fw, const tp_config *cfg, const tp_avg_list *pic, tp_line *out, int max)
{
    place pl;
    pl.s = (1.0f - 2.0f * MARGIN) / (FW_BOTTOM - FW_TOP);
    pl.cy = (FW_TOP + FW_BOTTOM) * 0.5f;
    pl.umax = SCREEN_W * 0.5f - MARGIN - FW_RADIUS * ASPECT_X * pl.s;

    /* this picture's clouds into the history */
    fw->head = (fw->head + 1) % TP_FW_HISTORY;
    if (fw->count < TP_FW_HISTORY) fw->count++;
    tp_fw_picture *now = &fw->hist[fw->head];
    clouds_of(pic, now);

    int n = 0;
    /* what else the picture holds, in the same field */
    for (int i = 0; i < pic->count && n < max; i++) {
        const tp_line *l = &pic->lines[i];
        if (TP_FW_IS_SHAPE(l->ikey)) continue;
        int x0, y0, x1, y1;
        tp_layout_screen(cfg, l->x0 * ASPECT_X * pl.s, (l->y0 - pl.cy) * pl.s, &x0, &y0);
        tp_layout_screen(cfg, l->x1 * ASPECT_X * pl.s, (l->y1 - pl.cy) * pl.s, &x1, &y1);
        out[n] = *l;
        out[n].x0 = (float)x0; out[n].y0 = (float)y0;
        out[n].x1 = (float)x1; out[n].y1 = (float)y1;
        n++;
    }
    /* the game's own clouds and the copies' */
    shown sh[3 * TP_FW_CLOUDS];
    int ns = 0;
    static const int ago[3] = { 0, COPY1_AGO, COPY2_AGO };
    for (int k = 0; k < 3; k++) {
        if (fw->count <= ago[k]) continue;
        const tp_fw_picture *p = &fw->hist[(fw->head - ago[k] + TP_FW_HISTORY) % TP_FW_HISTORY];
        for (int c = 0; c < p->n; c++) {
            shown *s = &sh[ns++];
            s->c = &p->c[c];
            s->mirror = k == 1;
            s->rot = k;
            s->u = middle_u(&pl, s->c->mx) * (s->mirror ? -1.0f : 1.0f);
            s->dv = k == 1 ? COPY1_DV : k == 2 ? COPY2_DV : 0.0f;
        }
    }
    /* the beam's way short: the clouds from left to right, in each the
     * next dot the nearest (the same order in every picture: a cloud is
     * its shape, only larger) */
    for (int a = 1; a < ns; a++)
        for (int b = a; b > 0 && sh[b].u < sh[b - 1].u; b--) {
            shown t = sh[b]; sh[b] = sh[b - 1]; sh[b - 1] = t;
        }
    for (int k = 0; k < ns; k++) {
        const tp_fw_cloud *cl = sh[k].c;
        float sign = sh[k].mirror ? -1.0f : 1.0f;
        int used[TP_FW_DOTS] = { 0 }, j = 0;
        for (int i = 1; i < cl->n; i++)
            if (sign * cl->dx[i] < sign * cl->dx[j]) j = i;
        for (int left = cl->n; left > 0 && n < max; left--) {
            n += put_dot(cfg, cl, j, sh[k].u, sh[k].dv, sh[k].mirror, sh[k].rot, &pl, &out[n]);
            used[j] = 1;
            int next = -1;
            float best = 1e30f;
            for (int i = 0; i < cl->n; i++) {
                if (used[i]) continue;
                float dx = (cl->dx[i] - cl->dx[j]) * ASPECT_X, dy = cl->dy[i] - cl->dy[j];
                if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; next = i; }
            }
            if (next < 0) break;
            j = next;
        }
    }
    return n;
}

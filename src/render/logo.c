#include "render/logo.h"

#include "render/logo_shape.h"

#include <math.h>
#include <string.h>

#define LOGO_SHAPE  0x3f4e          /* the game's logo (its shape's address) */
#define LOGO_X0     (-195.0f)       /* the game's logo, come in (measured) */
#define LOGO_X1     203.0f
#define LOGO_TOP    (-146.0f)
#define LOGO_BOTTOM (-98.0f)

void tp_logo_init(tp_logo *lg)
{
    memset(lg, 0, sizeof *lg);
}

void tp_logo_free(tp_logo *lg)
{
    tp_avg_list_free(&lg->out);
}

float tp_logo_bottom(const tp_config *cfg)
{
    return cfg->fx[TP_FX_LOGO] ? TP_LOGO_BOTTOM : LOGO_BOTTOM;
}

/* A copy: its lines from first, n of them; how large (1: come in) and
 * where (the game's logo's origin moved by ox, oy). */
typedef struct copy { int first, n; float k, ox, oy; } copy;

static void emit(tp_logo *lg, const tp_line *proto, const copy *c, int front)
{
    int paths = front ? TP_LOGO_PATHS : TP_LOGO_FACES;
    for (int p = 0; p < paths; p++) {
        const tp_logo_path *lp = &TP_LOGO_PATH[p];
        int segs = lp->closed ? lp->n : lp->n - 1;
        for (int s = 0; s < segs; s++) {
            if (lg->out.count >= lg->out.cap) return;
            const float *a = TP_LOGO_PT[lp->first + s], *b = TP_LOGO_PT[lp->first + (s + 1) % lp->n];
            tp_line *o = &lg->out.lines[lg->out.count++];
            *o = *proto;
            o->x0 = c->ox + a[0] * c->k; o->y0 = c->oy + a[1] * c->k;
            o->x1 = c->ox + b[0] * c->k; o->y1 = c->oy + b[1] * c->k;
            if (front) { o->r = lp->r; o->g = lp->g; o->b = lp->b; }
        }
    }
}

const tp_avg_list *tp_logo_picture(tp_logo *lg, const tp_config *cfg, const tp_avg_list *pic)
{
    if (!cfg->fx[TP_FX_LOGO]) return pic;
    int any = 0;
    for (int i = 0; i < pic->count && !any; i++) any = pic->lines[i].ikey == LOGO_SHAPE;
    if (!any) return pic;

    /* the copies and the largest (the front one) */
    static copy cp[256];
    int ncp = 0, front = -1;
    for (int i = 0; i < pic->count;) {
        const tp_line *q = &pic->lines[i];
        if (q->ikey != LOGO_SHAPE) { i++; continue; }
        int j = i;
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f;
        while (j < pic->count && pic->lines[j].ikey == LOGO_SHAPE && pic->lines[j].item == q->item) {
            const tp_line *l = &pic->lines[j];
            x0 = fminf(x0, fminf(l->x0, l->x1)); x1 = fmaxf(x1, fmaxf(l->x0, l->x1));
            y0 = fminf(y0, fminf(l->y0, l->y1));
            j++;
        }
        if (ncp < 256) {
            copy *c = &cp[ncp];
            c->first = i; c->n = j - i;
            c->k = (x1 - x0) / (LOGO_X1 - LOGO_X0);
            c->ox = x0 - LOGO_X0 * c->k;
            c->oy = y0 - LOGO_TOP * c->k;
            if (front < 0 || c->k > cp[front].k) front = ncp;
            ncp++;
        }
        i = j;
    }

    int need = pic->count + ncp * TP_LOGO_SEGS + 16;
    if (tp_avg_list_reserve(&lg->out, need) != 0) return pic;
    lg->out.count = 0;
    lg->out.status = pic->status;
    lg->out.instructions = pic->instructions;
    int c = 0;
    for (int i = 0; i < pic->count;) {
        if (c < ncp && i == cp[c].first) {
            /* (of the copies behind every other one: the lettering is about
             * twice as long a way for the beam as the game's logo) */
            if (c == front || !(c & 1)) emit(lg, &pic->lines[i], &cp[c], c == front);
            i += cp[c].n;
            c++;
            continue;
        }
        if (pic->lines[i].ikey != LOGO_SHAPE) lg->out.lines[lg->out.count++] = pic->lines[i];
        i++;
    }
    return &lg->out;
}

#include "render/smooth.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 1024
#define FAR_JUMP  120.0f        /* generator units: farther apart than this is a jump, not a move */

void tp_smooth_init(tp_smooth *s)
{
    memset(s, 0, sizeof *s);
}

void tp_smooth_free(tp_smooth *s)
{
    tp_avg_list_free(&s->cur);
    tp_avg_list_free(&s->next);
    free(s->pair);
    free(s->taken);
    memset(s, 0, sizeof *s);
}

static void copy_list(tp_avg_list *dst, const tp_avg_list *src)
{
    if (tp_avg_list_reserve(dst, src->count > 0 ? src->count : 1) != 0) {
        dst->count = 0;
        return;
    }
    if (src->count) memcpy(dst->lines, src->lines, (size_t)src->count * sizeof *src->lines);
    dst->count = src->count;
    dst->status = src->status;
    dst->instructions = src->instructions;
}

/* The objects of one sub-list of a picture, in order: their first line,
 * how many lines, their shape. An object is one instruction of the
 * sub-list (a call of a shape, or a vector of its own); vectors of its
 * own that join end to start are one figure (an enemy the program draws
 * in perspective, line by line). */
typedef struct obj { int first, n; uint16_t key, item; } obj;

static int joins(const tp_line *p, const tp_line *q)
{
    return fabsf(p->x1 - q->x0) < 0.01f && fabsf(p->y1 - q->y0) < 0.01f;
}

static int objects(const tp_avg_list *l, uint16_t sub, int from, obj *out, int max, int *end)
{
    int n = 0, i = from;
    for (; i < l->count && l->lines[i].sub == sub; i++) {
        const tp_line *q = &l->lines[i];
        if (n && out[n - 1].item == q->item) { out[n - 1].n++; continue; }
        if (n && !q->ikey && !out[n - 1].key && joins(&l->lines[i - 1], q)) {
            out[n - 1].n++;
            out[n - 1].item = q->item;
            continue;
        }
        if (n == max) continue;
        out[n].first = i;
        out[n].n = 1;
        out[n].key = q->ikey;
        out[n].item = q->item;
        n++;
    }
    *end = i;
    return n;
}

static void pair_two(tp_smooth *s, const obj *a, const obj *b)
{
    for (int k = 0; k < a->n; k++) {
        s->pair[a->first + k] = b->first + k;
        s->taken[b->first + k] = 1;
    }
}

/* The middle of an object's lines. */
static void middle(const tp_avg_list *l, const obj *o, float *x, float *y)
{
    float sx = 0.0f, sy = 0.0f;
    for (int k = 0; k < o->n; k++) {
        const tp_line *q = &l->lines[o->first + k];
        sx += q->x0 + q->x1;
        sy += q->y0 + q->y1;
    }
    *x = sx / (float)(2 * o->n);
    *y = sy / (float)(2 * o->n);
}

typedef struct cand { float d; int i, j; } cand;

static int by_distance(const void *p, const void *q)
{
    float a = ((const cand *)p)->d, b = ((const cand *)q)->d;
    return a < b ? -1 : a > b;
}

#define MAX_CANDS 65536
/* A shot turns from picture to picture (its shape changes, the number of
 * its lines does not): an object of another shape may be the partner, if
 * none of its own is as near. */
#define OTHER_SHAPE 6.0f

/* Pair the objects of a and b, then their lines by number. If the
 * sub-list holds the same objects in both (shape and size, in order),
 * each with its own in order. Otherwise something came or went: each
 * object with the nearest of the same size in the other picture, of its
 * own shape before another (by order, an enemy or a shot gone at the
 * front would pair each of the others with its neighbour, and they would
 * jump). */
#define TEXT_SUB  0x2004          /* the game's letters and digits */
#define TEXT_SAME 3.0f            /* text that came or went pairs only where it stands still */

static void pair_objects(tp_smooth *s, uint16_t sub, const obj *a, int na, const obj *b, int nb)
{
    int same = na == nb;
    for (int i = 0; same && i < na; i++)
        same = a[i].key == b[i].key && a[i].n == b[i].n;
    if (same) {
        for (int i = 0; i < na; i++) pair_two(s, &a[i], &b[i]);
        return;
    }

    static cand c[MAX_CANDS];
    static float bx[MAX_ITEMS], by[MAX_ITEMS];
    static uint8_t used_a[MAX_ITEMS], used_b[MAX_ITEMS];
    int nc = 0;
    for (int j = 0; j < nb; j++) {
        used_b[j] = 0;
        middle(&s->next, &b[j], &bx[j], &by[j]);
    }
    for (int i = 0; i < na; i++) {
        used_a[i] = 0;
        float ax, ay;
        middle(&s->cur, &a[i], &ax, &ay);
        for (int j = 0; j < nb && nc < MAX_CANDS; j++) {
            if (b[j].n != a[i].n) continue;
            float d = hypotf(bx[j] - ax, by[j] - ay);
            if (d > FAR_JUMP) continue;
            /* (text that changed is another text: a score's digit, or a
             * scene coming after another, where letters alike would glide
             * from the one to the other) */
            if (sub == TEXT_SUB && d > TEXT_SAME) continue;
            if (b[j].key != a[i].key) d += OTHER_SHAPE;
            c[nc].d = d; c[nc].i = i; c[nc].j = j;
            nc++;
        }
    }
    qsort(c, (size_t)nc, sizeof *c, by_distance);
    for (int k = 0; k < nc; k++) {
        if (used_a[c[k].i] || used_b[c[k].j]) continue;
        used_a[c[k].i] = used_b[c[k].j] = 1;
        pair_two(s, &a[c[k].i], &b[c[k].j]);
    }
}

void tp_smooth_set(tp_smooth *s, const tp_avg_list *cur, uint64_t t, const tp_avg_list *next, uint32_t span)
{
    copy_list(&s->cur, cur);
    s->t_cur = t;
    s->span = 0;
    if (!next || !span || !next->count) return;
    copy_list(&s->next, next);
    s->span = span;

    if (s->cap_pair < s->cur.count) {
        free(s->pair);
        s->cap_pair = s->cur.count + 256;
        s->pair = (int *)malloc((size_t)s->cap_pair * sizeof *s->pair);
    }
    if (s->cap_taken < s->next.count) {
        free(s->taken);
        s->cap_taken = s->next.count + 256;
        s->taken = (uint8_t *)malloc((size_t)s->cap_taken);
    }
    if (!s->pair || !s->taken) { s->span = 0; s->cap_pair = s->cap_taken = 0; return; }
    for (int i = 0; i < s->cur.count; i++) s->pair[i] = -1;
    memset(s->taken, 0, (size_t)s->next.count);

    /* sub-list by sub-list: each runs as one stretch of lines in both */
    static obj oa[MAX_ITEMS], ob[MAX_ITEMS];
    int i = 0;
    while (i < s->cur.count) {
        uint16_t sub = s->cur.lines[i].sub;
        int end_a, end_b;
        int na = objects(&s->cur, sub, i, oa, MAX_ITEMS, &end_a);
        int j = 0;
        while (j < s->next.count && s->next.lines[j].sub != sub) j++;
        if (j < s->next.count) {
            int nb = objects(&s->next, sub, j, ob, MAX_ITEMS, &end_b);
            pair_objects(s, sub, oa, na, ob, nb);
        }
        i = end_a;
    }
    /* partners too far apart: a jump */
    for (int k = 0; k < s->cur.count; k++) {
        int p = s->pair[k];
        if (p < 0) continue;
        const tp_line *a = &s->cur.lines[k], *b = &s->next.lines[p];
        if (fabsf(a->x0 - b->x0) > FAR_JUMP || fabsf(a->y0 - b->y0) > FAR_JUMP ||
            fabsf(a->x1 - b->x1) > FAR_JUMP || fabsf(a->y1 - b->y1) > FAR_JUMP) {
            s->pair[k] = -1;
            s->taken[p] = 0;
        }
    }
}

void tp_smooth_picture(tp_smooth *s, uint64_t now, tp_avg_list *out)
{
    out->count = 0;
    if (!s->span) {
        copy_list(out, &s->cur);
        return;
    }
    float a = (float)((double)(now - s->t_cur) / (double)s->span);
    if (now < s->t_cur) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    if (tp_avg_list_reserve(out, s->cur.count + s->next.count + 1) != 0) return;
    for (int k = 0; k < s->cur.count; k++) {
        int p = s->pair[k];
        if (p < 0) {
            if (a < 0.5f) out->lines[out->count++] = s->cur.lines[k];
            continue;
        }
        const tp_line *x = &s->cur.lines[k], *y = &s->next.lines[p];
        tp_line l = a < 0.5f ? *x : *y;
        l.x0 = x->x0 + (y->x0 - x->x0) * a; l.y0 = x->y0 + (y->y0 - x->y0) * a;
        l.x1 = x->x1 + (y->x1 - x->x1) * a; l.y1 = x->y1 + (y->y1 - x->y1) * a;
        out->lines[out->count++] = l;
    }
    if (a >= 0.5f)
        for (int k = 0; k < s->next.count; k++)
            if (!s->taken[k]) out->lines[out->count++] = s->next.lines[k];
}

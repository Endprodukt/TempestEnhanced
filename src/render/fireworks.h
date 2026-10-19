/* fireworks.h - the fireworks of a new high score across a horizontal screen.
 *
 * When a game ends with a score for the table, the program fires
 * fireworks (state 24, a picture of the text sub-list only): clouds of
 * dots, each one call of a shape the program writes into each of its two
 * picture buffers (2D4A, 2692), flying out from the middle of
 * its field and lit again elsewhere (their middles measured at x
 * -142..176, y -57..215, a cloud's dots up to 83 from its middle). On a
 * horizontal screen the port lays them out over the whole width, larger:
 * each cloud as it is, its middle pushed outwards (the further from the
 * middle, the more, up to the screen's edges), and it adds more of them:
 * the same fireworks of a moment before, mirrored and higher up, and of
 * a moment before that, lower down, in other colours.
 */
#ifndef TP_FIREWORKS_H
#define TP_FIREWORKS_H

#include "avg/avg.h"
#include "render/config.h"

#define TP_FW_SHAPE   0x2d4a    /* the shape of a cloud of dots, in one picture buffer */
#define TP_FW_SHAPE2  0x2692    /* and in the other */
#define TP_FW_IS_SHAPE(k) ((k) == TP_FW_SHAPE || (k) == TP_FW_SHAPE2)
#define TP_FW_CLOUDS  24        /* clouds a picture holds at most (the game: 16) */
#define TP_FW_DOTS    24        /* dots a cloud holds at most (the game: 16) */
#define TP_FW_HISTORY 64        /* pictures kept for the copies */

typedef struct tp_fw_cloud {
    float   mx, my;                         /* its middle (generator units) */
    int     n;
    float   dx[TP_FW_DOTS], dy[TP_FW_DOTS]; /* its dots from the middle */
    uint8_t r, g, b, intensity;
} tp_fw_cloud;

typedef struct tp_fw_picture {
    int         n;
    tp_fw_cloud c[TP_FW_CLOUDS];
} tp_fw_picture;

typedef struct tp_fireworks {
    tp_fw_picture hist[TP_FW_HISTORY];      /* the pictures shown, the last at head */
    int           head, count;
} tp_fireworks;

/* 1 if the picture is the fireworks. */
int  tp_fireworks_in(const tp_avg_list *pic);

/* The fireworks no longer shown: the copies start afresh next time. */
void tp_fireworks_reset(tp_fireworks *fw);

/* The picture (generator units) laid out over a horizontal screen, with
 * the copies, into out (board units, at most max lines; three times the
 * picture's lines are enough). Once for each picture shown. Returns the
 * number of lines. */
int  tp_fireworks_layout(tp_fireworks *fw, const tp_config *cfg, const tp_avg_list *pic, tp_line *out, int max);

#endif

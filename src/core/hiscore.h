/* hiscore.h - the game's high score table, kept longer than the cabinet
 * keeps it, and shown by the VectorPie menu.
 *
 * The program (revision 3) keeps its table of 8 in RAM: the initials at
 * 0606 (3 a place, the last letter first, 0..25 A..Z, 26 a space), the
 * scores at 0706 (3 BCD bytes a place, low first), the best place last.
 * Only the best three are kept in the EAROM; the others are the factory's
 * again after power-on.
 *
 * The port keeps a table of its own, the best 10, in save/highscores.txt,
 * the format of the SDK's vp_hiscore that the menu shows in its hint bar
 * (on the marquee): up to 10 lines, best first, "NAME SCORE", a name of 1
 * to 3 of A-Z 0-9 . -. After power-on its best 8 are put back into the
 * program's table; each new score of the program's is merged into it.
 */
#ifndef TP_HISCORE_H
#define TP_HISCORE_H

#include <stdint.h>

#define TP_HISCORE_PLACES 8         /* the program's table */
#define TP_HISCORE_KEEP   10        /* the port's (the menu's file holds 10) */
#define TP_RAM_INITIALS   0x0606
#define TP_RAM_HISCORES   0x0706

typedef struct tp_hiscore {
    char name[4];
    long score;
} tp_hiscore;

/* The table from the program's RAM, best first, into out (8 places).
 * Returns how many places hold a score (0: the program has not set up
 * its table yet). */
int tp_hiscore_read(const uint8_t *mem, tp_hiscore *out);

/* The best of table (at most 8) into the program's RAM, its places in
 * order. */
void tp_hiscore_put(uint8_t *mem, const tp_hiscore *table, int count);

/* Both tables in one, best first, into out (at most max): an entry that
 * is in both (name and score) once, as often as it is in the one that
 * has it more often. Of equal scores, a's first. Returns the count. */
int tp_hiscore_merge(const tp_hiscore *a, int na, const tp_hiscore *b, int nb, tp_hiscore *out, int max);

/* The file at path into out (at most max), best first. Returns the count
 * (0: no file, or none in it). */
int tp_hiscore_load(const char *path, tp_hiscore *out, int max);

/* The table into the file at path (written beside it, then renamed over
 * it). 0, or -1 if it cannot be written. */
int tp_hiscore_write(const tp_hiscore *table, int count, const char *path);

#endif

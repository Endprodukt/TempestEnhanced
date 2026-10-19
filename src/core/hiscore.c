#include "core/hiscore.h"

#include <stdio.h>
#include <string.h>

static int bcd(uint8_t b)
{
    return (b >> 4) * 10 + (b & 0x0f);
}

int tp_hiscore_read(const uint8_t *mem, tp_hiscore *out)
{
    int n = 0;
    for (int place = 0; place < TP_HISCORE_PLACES; place++) {
        int slot = TP_HISCORE_PLACES - 1 - place;          /* the best is the last */
        const uint8_t *in = &mem[TP_RAM_INITIALS + 3 * slot];
        const uint8_t *sc = &mem[TP_RAM_HISCORES + 3 * slot];
        if ((sc[0] & 0x0f) > 9 || (sc[0] >> 4) > 9 || (sc[1] & 0x0f) > 9 || (sc[1] >> 4) > 9 ||
            (sc[2] & 0x0f) > 9 || (sc[2] >> 4) > 9)
            return 0;                                       /* not a table (yet) */
        long score = (long)bcd(sc[2]) * 10000 + bcd(sc[1]) * 100 + bcd(sc[0]);
        if (score <= 0) continue;
        tp_hiscore *h = &out[n++];
        memset(h->name, 0, sizeof h->name);
        for (int k = 0; k < 3; k++) {
            uint8_t c = in[2 - k];                          /* the last letter first */
            h->name[k] = c < 26 ? (char)('A' + c) : '.';    /* a space: '.', as the menu's format has it */
        }
        h->name[3] = 0;
        for (int k = 2; k > 0 && h->name[k] == '.'; k--) h->name[k] = 0;   /* (no trailing spaces) */
        if (h->name[0] == '.' && !h->name[1]) strcpy(h->name, "---");
        h->score = score;
    }
    return n;
}

void tp_hiscore_put(uint8_t *mem, const tp_hiscore *table, int count)
{
    if (count > TP_HISCORE_PLACES) count = TP_HISCORE_PLACES;
    for (int place = 0; place < count; place++) {
        int slot = TP_HISCORE_PLACES - 1 - place;          /* the best is the last */
        uint8_t *in = &mem[TP_RAM_INITIALS + 3 * slot];
        uint8_t *sc = &mem[TP_RAM_HISCORES + 3 * slot];
        const char *name = table[place].name;
        int len = (int)strlen(name);
        for (int k = 0; k < 3; k++) {
            char ch = k < len ? name[k] : ' ';
            in[2 - k] = ch >= 'A' && ch <= 'Z' ? (uint8_t)(ch - 'A') : 26;     /* else a space */
        }
        long s = table[place].score;
        if (s > 999999) s = 999999;
        for (int k = 0; k < 3; k++, s /= 100) {
            int two = (int)(s % 100);
            sc[k] = (uint8_t)(((two / 10) << 4) | (two % 10));
        }
    }
}

static int same(const tp_hiscore *a, const tp_hiscore *b)
{
    return a->score == b->score && !strcmp(a->name, b->name);
}

static int times(const tp_hiscore *t, int n, const tp_hiscore *h)
{
    int c = 0;
    for (int i = 0; i < n; i++) c += same(&t[i], h);
    return c;
}

int tp_hiscore_merge(const tp_hiscore *a, int na, const tp_hiscore *b, int nb, tp_hiscore *out, int max)
{
    tp_hiscore all[2 * (TP_HISCORE_KEEP + TP_HISCORE_PLACES)];
    int n = 0;
    /* a's entries, then those of b's it has fewer of */
    for (int i = 0; i < na && n < (int)(sizeof all / sizeof all[0]); i++) all[n++] = a[i];
    for (int i = 0; i < nb && n < (int)(sizeof all / sizeof all[0]); i++)
        if (times(b, i + 1, &b[i]) > times(a, na, &b[i])) all[n++] = b[i];
    /* best first, of equal scores the earlier first (a's before b's) */
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && all[j].score > all[j - 1].score; j--) {
            tp_hiscore t = all[j]; all[j] = all[j - 1]; all[j - 1] = t;
        }
    if (n > max) n = max;
    memcpy(out, all, (size_t)n * sizeof *out);
    return n;
}

int tp_hiscore_load(const char *path, tp_hiscore *out, int max)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[128];
    while (n < max && fgets(line, sizeof line, f)) {
        char name[16];
        long score;
        if (sscanf(line, "%15s %ld", name, &score) != 2 || score <= 0) continue;
        memset(out[n].name, 0, sizeof out[n].name);
        memcpy(out[n].name, name, strlen(name) < 3 ? strlen(name) : 3);
        out[n].score = score;
        n++;
    }
    fclose(f);
    /* best first, as the file should have them */
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && out[j].score > out[j - 1].score; j--) {
            tp_hiscore t = out[j]; out[j] = out[j - 1]; out[j - 1] = t;
        }
    return n;
}

int tp_hiscore_write(const tp_hiscore *table, int count, const char *path)
{
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.new", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    for (int i = 0; i < count; i++) fprintf(f, "%s %ld\n", table[i].name, table[i].score);
    int bad = fflush(f) != 0;
    bad |= fclose(f) != 0;
    if (bad) {
        remove(tmp);
        return -1;
    }
    if (rename(tmp, path) == 0) return 0;
    remove(path);           /* (Windows does not rename over a file) */
    return rename(tmp, path) == 0 ? 0 : -1;
}

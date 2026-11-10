#include "rom/romset.h"

#include "miniz.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum dest { D_VGROM, D_PROG, D_AVG };

typedef struct rom_entry {
    const char *name;     /* MAME's name, for messages only */
    uint32_t    crc;
    uint32_t    size;
    enum dest   dest;
    uint32_t    offset;   /* within its destination */
    int         optional; /* not needed by the game */
    int         half_of;  /* a 2K chip: the 4K ROM whose half it is (-1: none) */
} rom_entry;

/* MAME "tempest" (src/mame/atari/tempest.cpp): revision 3 in 4K chips;
 * then the same program in 2K chips, two to each 4K ROM, byte for byte
 * (MAME "tempest3", whose files the "tempest" set keeps in tempest1/,
 * tempest2/, tempest3/; older sets, AdvanceMAME's of 1996 among them,
 * hold only these). Those have no 136002-125: the vector generator's
 * timing is then approximate. */
static const rom_entry ROMS[] = {
    { "136002-133.d1",  0x1d0cc503, 0x1000, D_PROG, 0x0000, 0, -1 },
    { "136002-134.f1",  0xc88e3524, 0x1000, D_PROG, 0x1000, 0, -1 },
    { "136002-235.j1",  0xa4b2ce3f, 0x1000, D_PROG, 0x2000, 0, -1 },
    { "136002-136.lm1", 0x65a9a9f9, 0x1000, D_PROG, 0x3000, 0, -1 },
    { "136002-237.p1",  0xde4e9e34, 0x1000, D_PROG, 0x4000, 0, -1 },
    { "136002-138.np3", 0x9995256d, 0x1000, D_VGROM, 0x0000, 0, -1 },
    { "136002-125.d7",  0x5903af03, 0x0100, D_AVG, 0, 1, -1 },
    { "136002-113.d1",  0x65d61fe7, 0x0800, D_PROG, 0x0000, 1, 0 },
    { "136002-114.e1",  0x11077375, 0x0800, D_PROG, 0x0800, 1, 0 },
    { "136002-115.f1",  0xf3e2827a, 0x0800, D_PROG, 0x1000, 1, 1 },
    { "136002-316.h1",  0xaeb0f7e9, 0x0800, D_PROG, 0x1800, 1, 1 },
    { "136002-217.j1",  0xef2eb645, 0x0800, D_PROG, 0x2000, 1, 2 },
    { "136002-118.k1",  0xbeb352ab, 0x0800, D_PROG, 0x2800, 1, 2 },
    { "136002-119.lm1", 0xa4de050f, 0x0800, D_PROG, 0x3000, 1, 3 },
    { "136002-120.mn1", 0x35619648, 0x0800, D_PROG, 0x3800, 1, 3 },
    { "136002-121.p1",  0x73d38e47, 0x0800, D_PROG, 0x4000, 1, 4 },
    { "136002-222.r1",  0x707bd5c3, 0x0800, D_PROG, 0x4800, 1, 4 },
    { "136002-123.np3", 0x29f7e937, 0x0800, D_VGROM, 0x0000, 1, 5 },
    { "136002-124.r3",  0xc16ec351, 0x0800, D_VGROM, 0x0800, 1, 5 },
};
#define NROMS ((int)(sizeof ROMS / sizeof ROMS[0]))
/* the search ends with the 4K set and its PROM (the 2K chips are only
 * used as long as no better is found) */
#define ALL_FOUND 0x7fu

/* A 4K ROM: found itself, or both its halves. */
static int have(const tp_romset *rs, int i)
{
    if (rs->found & (1u << i)) return 1;
    int halves = 0;
    for (int k = 0; k < NROMS; k++)
        if (ROMS[k].half_of == i && (rs->found & (1u << k))) halves++;
    return halves == 2;
}

static int complete(const tp_romset *rs)
{
    for (int i = 0; i < NROMS; i++)
        if (!ROMS[i].optional && !have(rs, i)) return 0;
    return 1;
}

static void place(tp_romset *rs, int i, const uint8_t *data)
{
    const rom_entry *e = &ROMS[i];
    switch (e->dest) {
    case D_VGROM: memcpy(rs->vgrom + e->offset, data, e->size); break;
    case D_PROG:  memcpy(rs->prog + e->offset, data, e->size); break;
    case D_AVG:   memcpy(rs->avg_prom, data, e->size); rs->have_avg_prom = 1; break;
    }
    rs->found |= 1u << i;
}

/* Offer one file's contents: placed if it is a wanted ROM not yet found. */
static void offer(tp_romset *rs, const uint8_t *data, size_t size)
{
    uint32_t crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, data, size);
    for (int i = 0; i < NROMS; i++)
        if (ROMS[i].crc == crc && ROMS[i].size == size && !(rs->found & (1u << i))) place(rs, i, data);
}

static int wanted_size(size_t size)
{
    for (int i = 0; i < NROMS; i++)
        if (ROMS[i].size == size) return 1;
    return 0;
}

static void scan_zip(tp_romset *rs, const char *path)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_file(&zip, path, 0)) return;
    mz_uint n = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < n && (rs->found & ALL_FOUND) != ALL_FOUND; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory) continue;
        if (!wanted_size((size_t)st.m_uncomp_size)) continue;
        /* The zip's own CRC tells whether the file is worth extracting. */
        int match = 0;
        for (int k = 0; k < NROMS; k++)
            if (ROMS[k].crc == st.m_crc32 && !(rs->found & (1u << k))) match = 1;
        if (!match) continue;
        size_t size = 0;
        void *data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
        if (data) {
            offer(rs, (const uint8_t *)data, size);
            mz_free(data);
        }
    }
    mz_zip_reader_end(&zip);
}

static void scan_file(tp_romset *rs, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return;
    uint8_t buf[0x1000 + 1];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (n < sizeof buf && wanted_size(n)) offer(rs, buf, n);
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int ends_with_zip(const char *path)
{
    size_t n = strlen(path);
    return n > 4 && (strcmp(path + n - 4, ".zip") == 0 || strcmp(path + n - 4, ".ZIP") == 0);
}

static void scan_dir(tp_romset *rs, const char *path)
{
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *de;
    char full[2048];
    while ((de = readdir(d)) != NULL && (rs->found & ALL_FOUND) != ALL_FOUND) {
        if (de->d_name[0] == '.') continue;
        snprintf(full, sizeof full, "%s/%s", path, de->d_name);
        if (!is_dir(full) && !ends_with_zip(full)) scan_file(rs, full);
    }
    closedir(d);
}

int tp_romset_load(tp_romset *rs, const char *const *paths, int npaths)
{
    memset(rs, 0, sizeof *rs);
    for (int i = 0; i < npaths && (rs->found & ALL_FOUND) != ALL_FOUND; i++) {
        if (!paths[i] || !*paths[i]) continue;
        if (is_dir(paths[i])) scan_dir(rs, paths[i]);
        else if (ends_with_zip(paths[i])) scan_zip(rs, paths[i]);
        else scan_file(rs, paths[i]);
    }
    return complete(rs) ? 0 : -1;
}

const char *tp_romset_missing(const tp_romset *rs, char *buf, int size)
{
    int len = 0;
    buf[0] = '\0';
    for (int i = 0; i < NROMS; i++) {
        if (ROMS[i].optional || have(rs, i)) continue;
        len += snprintf(buf + len, len < size ? (size_t)(size - len) : 0, "%s%s", len ? ", " : "", ROMS[i].name);
        if (len >= size) break;
    }
    return buf;
}

#define MAX_STORE 48
static char g_store[MAX_STORE][1024];
static int  g_nstore;

int tp_romset_add_dir(const char **out, int n, int max, const char *dir)
{
    static const char *const forms[] = { "%s/tempest.zip", "%s" };
    for (int f = 0; f < 2 && n < max && g_nstore < MAX_STORE; f++) {
        snprintf(g_store[g_nstore], sizeof g_store[g_nstore], forms[f], dir);
        out[n++] = g_store[g_nstore++];
    }
    return n;
}

int tp_romset_default_paths(const char **out, int max)
{
    int n = 0;
    static char envcopy[2048];

    const char *env = getenv("TP_ROMS");
    if (env && *env) {
        snprintf(envcopy, sizeof envcopy, "%s", env);
        /* ';' separates on Windows, where a path holds a ':' */
        const char *sep = strchr(envcopy, ';') || (envcopy[1] == ':') ? ";" : ":";
        for (char *p = strtok(envcopy, sep); p && n < max; p = strtok(NULL, sep))
            n = tp_romset_add_dir(out, n, max, p);
    }

    n = tp_romset_add_dir(out, n, max, "roms");
    const char *home = getenv("HOME");
    static char h1[1024], h2[1024];
    if (home) {
        snprintf(h1, sizeof h1, "%s/.advance/rom", home);
        snprintf(h2, sizeof h2, "%s/mame/roms", home);
        n = tp_romset_add_dir(out, n, max, h1);
        n = tp_romset_add_dir(out, n, max, h2);
    }
    n = tp_romset_add_dir(out, n, max, "/home/pi/.advance/rom");
    n = tp_romset_add_dir(out, n, max, "/usr/local/share/advance/rom");     /* (AdvanceMAME's own, the cabinet's set) */
    return n;
}

/* romset.h - the original ROMs, read from the owner's MAME ROM set.
 *
 * The program and the vector ROM stay in the ROMs and are read from the
 * owner's set at start-up; none of it is part of this project.
 *
 * The program is revision 3, MAME's set "tempest" (tempest.zip). Its
 * five 4K program ROMs and the vector ROM are what the source in
 * upstream/ (ALEXEC.LDA) assembles to, byte for byte.
 *
 * Files are matched by CRC32, so names and zip layout do not matter.
 */
#ifndef TP_ROMSET_H
#define TP_ROMSET_H

#include <stdint.h>

typedef struct tp_romset {
    uint8_t  vgrom[0x1000];     /* 3000-3FFF */
    uint8_t  prog[0x5000];      /* 9000-DFFF */
    uint8_t  avg_prom[0x100];   /* the vector generator's state PROM: its exact timing (optional) */
    int      have_avg_prom;
    uint32_t found;             /* bit per entry of the ROM table */
} tp_romset;

/* Look in each path (a .zip or a directory) for the set's ROMs, in order,
 * until all are found. Returns 0 when the game can run; otherwise -1 and
 * tp_romset_missing() names what is missing. */
int tp_romset_load(tp_romset *rs, const char *const *paths, int npaths);

/* The ROMs not found, as a comma-separated list (empty when complete). */
const char *tp_romset_missing(const tp_romset *rs, char *buf, int size);

/* The default search list: $TP_ROMS (a ';'- or ':'-separated list), then
 * ./roms and the usual AdvanceMAME/MAME rom folders, each tried as
 * tempest.zip and as a directory. Returns the count. */
int tp_romset_default_paths(const char **out, int max);

/* The same forms for one more folder; appended at out[n], returns the new count. */
int tp_romset_add_dir(const char **out, int n, int max, const char *dir);

#endif

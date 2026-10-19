/* machine.h - the Tempest board as the program sees it.
 *
 * The program runs from the owner's ROMs on the 6502 core (core/emu.h);
 * everything it talks to is here: RAM, colour RAM, vector RAM, the
 * switches, the math box, the EAROM, the two POKEYs, the coin and lamp
 * outputs. Memory map: core/tp_map.h.
 */
#ifndef TP_MACHINE_H
#define TP_MACHINE_H

#include <stdint.h>

#include "avg/avgsm.h"
#include "core/earom.h"
#include "core/pokeyio.h"
#include "core/tp_map.h"
#include "mbox/mathbox.h"

typedef struct tp_inputs {
    uint8_t in0;        /* D0-D5 of 0C00, active low */
    uint8_t dsw1;       /* 0D00 */
    uint8_t dsw2;       /* 0E00 */
    uint8_t cocktail;   /* 1: the cocktail cabinet (pot 4 of POKEY 1 closed) */
    uint8_t diff;       /* difficulty switches, D0-D1 of POKEY 2 (MAME: 3 = Medium1) */
    uint8_t rating;     /* 1: levels 1, 3, 5, 7, 9 (MAME's default) */
    uint8_t spinner[2]; /* each player's encoder count, low 4 bits used */
    uint8_t fire[2], zap[2], start1, start2;   /* 1: pressed */
} tp_inputs;

typedef struct tp_machine tp_machine;

/* A read of an I/O address: gets the value the board would give and
 * returns the value to use (replaying a recording). */
typedef uint8_t (*tp_read_fn)(void *user, uint16_t addr, uint8_t value);
/* A write to a POKEY (60C0-60DF). */
typedef void (*tp_sound_fn)(void *user, uint16_t addr, uint8_t value);

struct tp_machine {
    uint8_t     mem[0x10000];   /* the whole address space; I/O addresses are not used */
    uint8_t     colram[16];
    int         have_rom;
    tp_inputs   in;
    tp_mathbox  mb;
    tp_earom    earom;
    tp_pokeyio  pk[2];

    uint8_t     coinout;        /* 4000: coin counters, flip X and Y */
    uint8_t     leds;           /* 60E0 */
    uint8_t     irq;            /* the interrupt line: set every 6144 cycles, cleared by a write to 5000 */
    uint64_t    cycle;          /* CPU cycles since power on, at the start of the instruction (the emulator keeps it) */
    uint64_t    now;            /* the cycle of the access being made */
    tp_avgsm    vg;             /* the vector generator's state machine (its timing), if its PROM is there */
    int         vg_phase;       /* master clock cycles the generator's time is ahead of 8 * now */
    int         clk_phase;      /* CPU cycles the 3 kHz count is ahead of `now` (-1: MAME's) */
    uint64_t    vg_halt_at;     /* the cycle the vector generator halts at (~0: it goes round) */
    uint64_t    vg_go_at;       /* the cycle it was last started */
    uint32_t    vg_round;       /* CPU cycles of one round of its list, when it goes round */
    uint32_t    vg_starts;      /* writes to VGGO: a new picture */
    uint32_t    watchdog;

    tp_read_fn  read_hook;      /* NULL: none. Called for every read of an I/O address */
    void       *read_user;
    tp_sound_fn sound_write;
    void       *sound_user;
};

void tp_machine_init(tp_machine *m);
/* The ROMs as their places in the address space: 3000-3FFF and
 * 9000-DFFF (the latter mirrored at F000). */
void tp_machine_set_rom(tp_machine *m, const uint8_t *vgrom3000, const uint8_t *prog9000);
/* The vector generator's state PROM (136002-125): with it the halt bit
 * comes clock-exact (avg/avgsm.h); without, from a round's length. */
void tp_machine_set_avg_prom(tp_machine *m, const uint8_t *prom);

uint8_t tp_rd8(tp_machine *m, uint16_t addr);
void    tp_wr8(tp_machine *m, uint16_t addr, uint8_t v);

/* The switches on POKEY 1 and 2 as the inputs say (called before reads). */
void tp_machine_update_pots(tp_machine *m);

#endif

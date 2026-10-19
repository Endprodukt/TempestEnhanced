/* tp_map.h - the Tempest board's addresses (MAME set "tempest", revision 3).
 *
 * The hardware is from MAME's driver (src/mame/atari/tempest.cpp). The few
 * RAM cells and program addresses the port hooks into were found in the
 * program itself (tools/dis6502.py); each says what it is used for here.
 */
#ifndef TP_MAP_H
#define TP_MAP_H

/* --- hardware --- */
#define TP_RAM_END      0x07ff  /* program RAM 0000-07FF */
#define TP_COLRAM       0x0800  /* colour RAM, 16 nibbles, write only */
#define TP_IN0          0x0c00  /* coins, slam, self test, diagnostic step, VG halted, 3 kHz */
#define TP_DSW1         0x0d00  /* option switches N13: coinage */
#define TP_DSW2         0x0e00  /* option switches L12: minimum, language, bonus, lives */
#define TP_VGRAM        0x2000  /* vector RAM 2000-2FFF */
#define TP_VGROM        0x3000  /* vector ROM 3000-3FFF */
#define TP_COINOUT      0x4000  /* coin counters (D0-D2), flip X (D3), flip Y (D4) */
#define TP_VGGO         0x4800
#define TP_WDCLR        0x5000  /* watchdog clear, and the interrupt's acknowledge */
#define TP_VGRST        0x5800
#define TP_EAWRITE      0x6000  /* 6000-603F: address and data to the EAROM */
#define TP_EACTL        0x6040  /* write: EAROM control; read: math box status */
#define TP_EAREAD       0x6050
#define TP_MBLO         0x6060
#define TP_MBHI         0x6070
#define TP_MBGO         0x6080  /* 6080-609F */
#define TP_POKEY1       0x60c0  /* spinner (pots 0-3), cabinet type (pot 4) */
#define TP_POKEY2       0x60d0  /* difficulty, rating, fire, zap, starts */
#define TP_LEDS         0x60e0  /* start lamps (D0, D1, active low), player select / flip (D2) */
#define TP_ROM_LO       0x9000  /* program ROM 9000-DFFF */
#define TP_ROM_HI       0xdfff
#define TP_ROM_MIRROR   0xf000  /* F000-FFFF: D000-DFFF again, for the vectors */

/* POKEY register offsets used here */
#define TP_PK_ALLPOT    0x08    /* read */
#define TP_PK_RANDOM    0x0a    /* read */
#define TP_PK_POTGO     0x0b    /* write */
#define TP_PK_SKCTL     0x0f    /* write */

/* IN0 (0C00), active low except HALT and 3 kHz */
#define TP_IN0_COIN_R   0x01    /* MAME: Coin 3 */
#define TP_IN0_COIN_C   0x02    /* MAME: Coin 2 */
#define TP_IN0_COIN_L   0x04    /* MAME: Coin 1 */
#define TP_IN0_SLAM     0x08
#define TP_IN0_TEST     0x10
#define TP_IN0_DIAG     0x20
#define TP_IN0_HALT     0x40    /* the vector generator has halted (active high) */
#define TP_IN0_3KHZ     0x80

/* POKEY 2's pots as MAME's port IN2 has them (a set bit reads 0 in ALLPOT) */
#define TP_IN2_DIFF     0x03
#define TP_IN2_RATING   0x04
#define TP_IN2_ZAP      0x08    /* clear while pressed (MAME: button 2), of the player the flip selects */
#define TP_IN2_FIRE     0x10    /* clear while pressed (MAME: button 1) */
#define TP_IN2_START1   0x20    /* clear while pressed */
#define TP_IN2_START2   0x40    /* clear while pressed */

/* --- clocks (MAME: MASTER_CLOCK 12.096 MHz) --- */
#define TP_MASTER_HZ        12096000.0
#define TP_CPU_HZ           1512000.0       /* master / 8; the POKEYs run at the same clock */
#define TP_CYCLES_PER_IRQ   6144            /* the 3 kHz clock / 12: 246.09 interrupts a second */

/* --- the program (revision 3), as the port hooks into it --- */
#define TP_RESET_ENTRY  0xd93f  /* the reset vector */
#define TP_IRQ_ENTRY    0xd704  /* the interrupt vector */
#define TP_MAIN_STA     0xc7af  /* the main loop's store of 0 into the frame timer: a frame starts */
#define TP_MAIN_FRAME   0xc7b1  /* the instruction after it */

#define TP_FRTIMR       0x0053  /* frame timer: the interrupt counts it up, the main loop waits for 9 */

#endif

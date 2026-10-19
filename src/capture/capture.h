/* capture.h - Tempest frames recorded from MAME (tools/mame/tpcapture.lua).
 *
 * File: "TPCAP1\0\0", then chunks: 4-byte tag, uint32 little-endian length,
 * payload. Tags:
 *   FRAM  the state when the main loop starts a frame (its store of 0 into
 *         the frame timer, TP_MAIN_STA, before the write):
 *           uint32 frame number, uint64 CPU cycle (MAME's time * 1.512 MHz),
 *           A, X, Y, S, P,
 *           RAM 0000-07FF, colour RAM (16), vector RAM 2000-2FFF,
 *           math box: 16 registers and the result (int16 each),
 *           EAROM: 64 cells, address, data
 *   READ  every read of an I/O address (0800-1FFF, 4000-8FFF) since the
 *         FRAM before, in order: uint32 cycle (since that FRAM), uint16
 *         address, uint8 value
 *   IRQS  every interrupt taken since the FRAM before: uint32 cycle (since
 *         that FRAM) of the vector fetch, uint16 the address it returns to
 *   VGST  MAME's vector generator at the FRAM before: uint16 pc, sp; uint8
 *         state latch, op, halt, sync halt; uint16 timer, dvy, dvx; uint8
 *         binary scale, scale; uint16 stack[4]
 *   POKE  uint16 address, uint8 value: the script wrote this into RAM just
 *         before the FRAM that follows
 * Unknown tags are skipped, so the format can grow.
 */
#ifndef TP_CAPTURE_H
#define TP_CAPTURE_H

#include <stdint.h>
#include <stdio.h>

#define TPCAP_RAM_SIZE   0x0800
#define TPCAP_VGRAM_SIZE 0x1000
#define TPCAP_FRAM_SIZE  (4 + 8 + 5 + TPCAP_RAM_SIZE + 16 + TPCAP_VGRAM_SIZE + 34 + 66)

typedef struct tpcap_frame {
    uint32_t frame;
    uint64_t cycle;
    uint8_t  a, x, y, s, p;
    uint8_t  ram[TPCAP_RAM_SIZE];
    uint8_t  colram[16];
    uint8_t  vgram[TPCAP_VGRAM_SIZE];
    int16_t  mb_reg[16];
    int16_t  mb_result;
    uint8_t  earom[64];
    uint8_t  ea_address, ea_data;
} tpcap_frame;

typedef struct tpcap_read { uint32_t cycle; uint16_t addr; uint8_t value; } tpcap_read;
typedef struct tpcap_irq  { uint32_t cycle; uint16_t ret; } tpcap_irq;

typedef struct tpcap_reader { FILE *f; } tpcap_reader;

int  tpcap_open(tpcap_reader *r, const char *path);   /* 0 or -1 */
void tpcap_close(tpcap_reader *r);

/* The next chunk: its tag and length in *len, its first min(len, cap)
 * bytes in buf. 1, 0 at the end, -1 on a damaged file. */
int  tpcap_next_chunk(tpcap_reader *r, char tag[4], uint8_t *buf, uint32_t cap, uint32_t *len);

void tpcap_parse_frame(const uint8_t *payload, tpcap_frame *t);
/* READ and IRQS payloads: the number of entries written to out (at most max). */
int  tpcap_parse_reads(const uint8_t *payload, uint32_t len, tpcap_read *out, int max);
int  tpcap_parse_irqs(const uint8_t *payload, uint32_t len, tpcap_irq *out, int max);

#endif

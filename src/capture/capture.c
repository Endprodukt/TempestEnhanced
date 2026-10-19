#include "capture/capture.h"

#include <string.h>

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int tpcap_open(tpcap_reader *r, const char *path)
{
    memset(r, 0, sizeof *r);
    r->f = fopen(path, "rb");
    if (!r->f) return -1;
    char magic[8];
    if (fread(magic, 1, 8, r->f) != 8 || memcmp(magic, "TPCAP1\0\0", 8) != 0) {
        fclose(r->f);
        r->f = NULL;
        return -1;
    }
    return 0;
}

void tpcap_close(tpcap_reader *r)
{
    if (r->f) fclose(r->f);
    r->f = NULL;
}

int tpcap_next_chunk(tpcap_reader *r, char tag[4], uint8_t *buf, uint32_t cap, uint32_t *len)
{
    uint8_t head[8];
    size_t n = fread(head, 1, 8, r->f);
    if (n == 0) return 0;
    if (n != 8) return -1;
    memcpy(tag, head, 4);
    *len = u32(head + 4);
    uint32_t take = *len < cap ? *len : cap;
    if (take && fread(buf, 1, take, r->f) != take) return -1;
    if (*len > take && fseek(r->f, (long)(*len - take), SEEK_CUR) != 0) return -1;
    return 1;
}

void tpcap_parse_frame(const uint8_t *p, tpcap_frame *t)
{
    t->frame = u32(p);
    t->cycle = (uint64_t)u32(p + 4) | ((uint64_t)u32(p + 8) << 32);
    p += 12;
    t->a = p[0]; t->x = p[1]; t->y = p[2]; t->s = p[3]; t->p = p[4];
    p += 5;
    memcpy(t->ram, p, TPCAP_RAM_SIZE);
    p += TPCAP_RAM_SIZE;
    memcpy(t->colram, p, 16);
    p += 16;
    memcpy(t->vgram, p, TPCAP_VGRAM_SIZE);
    p += TPCAP_VGRAM_SIZE;
    for (int i = 0; i < 16; i++) t->mb_reg[i] = (int16_t)(uint16_t)(p[2 * i] | (p[2 * i + 1] << 8));
    t->mb_result = (int16_t)(uint16_t)(p[32] | (p[33] << 8));
    p += 34;
    memcpy(t->earom, p, 64);
    t->ea_address = p[64];
    t->ea_data = p[65];
}

int tpcap_parse_reads(const uint8_t *p, uint32_t len, tpcap_read *out, int max)
{
    int n = 0;
    for (uint32_t i = 0; i + 7 <= len && n < max; i += 7, n++) {
        out[n].cycle = u32(p + i);
        out[n].addr = (uint16_t)(p[i + 4] | (p[i + 5] << 8));
        out[n].value = p[i + 6];
    }
    return n;
}

int tpcap_parse_irqs(const uint8_t *p, uint32_t len, tpcap_irq *out, int max)
{
    int n = 0;
    for (uint32_t i = 0; i + 6 <= len && n < max; i += 6, n++) {
        out[n].cycle = u32(p + i);
        out[n].ret = (uint16_t)(p[i + 4] | (p[i + 5] << 8));
    }
    return n;
}

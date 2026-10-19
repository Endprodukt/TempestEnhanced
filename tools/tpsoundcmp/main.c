/* tpsoundcmp - the board's sound against MAME's, over the same run.
 *
 *   tpsoundcmp MAME.wav PORT.wav
 *
 * MAME.wav: tools/capture.sh with TPCAP_WAV; PORT.wav: tplockstep --wav
 * over that recording (the same game, the same POKEY writes). Both mono,
 * 16 bit. The DC is taken out of both (MAME's speaker keeps it), the port's
 * is aligned to MAME's by the cross-correlation of their loudness (10 ms
 * steps) and scaled by the factor that fits best (the sign included: the
 * port gives how far the POKEYs pull the pin down, MAME the pin itself).
 * Then: how alike the waves are, the loudness over time, and the energy
 * in each octave from 62 Hz to 16 kHz.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct pcm { float *x; long n; int rate; } pcm;

static int read_wav(const char *path, pcm *p)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint8_t h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return -1; }
    int ch = 1, bits = 16;
    p->rate = 48000;
    for (;;) {
        uint8_t c[8];
        if (fread(c, 1, 8, f) != 8) { fclose(f); return -1; }
        uint32_t len = (uint32_t)c[4] | (uint32_t)c[5] << 8 | (uint32_t)c[6] << 16 | (uint32_t)c[7] << 24;
        if (!memcmp(c, "fmt ", 4)) {
            uint8_t fm[16];
            if (fread(fm, 1, 16, f) != 16) { fclose(f); return -1; }
            ch = fm[2] | fm[3] << 8;
            p->rate = (int)(fm[4] | fm[5] << 8 | fm[6] << 16 | (uint32_t)fm[7] << 24);
            bits = fm[14] | fm[15] << 8;
            if (len > 16) fseek(f, (long)(len - 16), SEEK_CUR);
        } else if (!memcmp(c, "data", 4)) {
            if (bits != 16) { fclose(f); return -1; }
            long frames = (long)(len / (2u * (unsigned)ch));
            int16_t *raw = (int16_t *)malloc((size_t)len);
            if (!raw || fread(raw, 1, len, f) != len) { free(raw); fclose(f); return -1; }
            p->x = (float *)malloc((size_t)frames * sizeof *p->x);
            for (long i = 0; i < frames; i++) p->x[i] = raw[i * ch] / 32768.0f;
            p->n = frames;
            free(raw);
            fclose(f);
            return 0;
        } else {
            fseek(f, (long)len, SEEK_CUR);
        }
    }
}

/* the DC out: a first order high-pass at 10 Hz */
static void no_dc(pcm *p)
{
    float a = expf(-2.0f * 3.14159265f * 10.0f / (float)p->rate), y = 0.0f, prev = p->n ? p->x[0] : 0.0f;
    for (long i = 0; i < p->n; i++) {
        float x = p->x[i];
        y = a * (y + x - prev);
        prev = x;
        p->x[i] = y;
    }
}

static float *loudness(const pcm *p, int step, long *count)
{
    long n = p->n / step;
    float *e = (float *)calloc((size_t)(n ? n : 1), sizeof *e);
    for (long k = 0; k < n; k++) {
        double s = 0.0;
        for (int i = 0; i < step; i++) s += (double)p->x[k * step + i] * p->x[k * step + i];
        e[k] = (float)sqrt(s / step);
    }
    *count = n;
    return e;
}

static double correlation(const float *a, const float *b, long n)
{
    double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
    for (long i = 0; i < n; i++) { sa += a[i]; sb += b[i]; saa += (double)a[i] * a[i]; sbb += (double)b[i] * b[i]; sab += (double)a[i] * b[i]; }
    double ca = saa - sa * sa / n, cb = sbb - sb * sb / n, cab = sab - sa * sb / n;
    return ca > 0 && cb > 0 ? cab / sqrt(ca * cb) : 0.0;
}

/* in place radix-2 FFT of re/im, n a power of 2 */
static void fft(float *re, float *im, int n)
{
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * 3.14159265358979 / len;
        for (int i = 0; i < n; i += len)
            for (int k = 0; k < len / 2; k++) {
                float wr = (float)cos(ang * k), wi = (float)sin(ang * k);
                float ur = re[i + k], ui = im[i + k];
                float vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi;
                float vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
                re[i + k] = ur + vr; im[i + k] = ui + vi;
                re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
            }
    }
}

#define NBANDS 9

static void bands(const float *x, long n, int rate, double out[NBANDS])
{
    enum { N = 4096 };
    static float re[N], im[N];
    memset(out, 0, NBANDS * sizeof *out);
    for (long at = 0; at + N <= n; at += N) {
        for (int i = 0; i < N; i++) {
            float w = 0.5f - 0.5f * cosf(2.0f * 3.14159265f * i / (N - 1));
            re[i] = x[at + i] * w;
            im[i] = 0.0f;
        }
        fft(re, im, N);
        for (int k = 1; k < N / 2; k++) {
            double hz = (double)k * rate / N;
            int b = (int)floor(log2(hz / 62.5));
            if (b >= 0 && b < NBANDS) out[b] += (double)re[k] * re[k] + (double)im[k] * im[k];
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: tpsoundcmp MAME.wav PORT.wav\n"); return 2; }
    pcm a, b;
    if (read_wav(argv[1], &a) || read_wav(argv[2], &b)) { fprintf(stderr, "tpsoundcmp: cannot read the WAV files\n"); return 1; }
    if (a.rate != b.rate) { fprintf(stderr, "tpsoundcmp: rates differ (%d, %d)\n", a.rate, b.rate); return 1; }
    no_dc(&a);
    no_dc(&b);

    /* align by the loudness, 10 ms steps, within +-2 s */
    int step = a.rate / 100;
    long na, nb;
    float *ea = loudness(&a, step, &na), *eb = loudness(&b, step, &nb);
    long best_lag = 0;
    double best = -2.0;
    for (long lag = -200; lag <= 200; lag++) {
        long n = 0;
        double s = 0;
        for (long i = 0; i < na; i++) {
            long j = i + lag;
            if (j < 0 || j >= nb) continue;
            s += (double)ea[i] * eb[j];
            n++;
        }
        if (n && s / n > best) { best = s / n; best_lag = lag; }
    }
    /* then to the sample, within +-10 ms of that */
    long base = best_lag * step, lag_s = base;
    double bc = -1e30;
    long len = a.n < b.n ? a.n : b.n;
    for (long d = -step; d <= step; d++) {
        long sh = base + d;
        double s = 0;
        for (long i = 0; i < len; i += 7) {
            long j = i + sh;
            if (j < 0 || j >= b.n) continue;
            s += fabs((double)a.x[i] * b.x[j]) * 0 + (double)a.x[i] * b.x[j];
        }
        if (fabs(s) > bc) { bc = fabs(s); lag_s = sh; }
    }
    /* the scale that fits best, sign included */
    double sab = 0, sbb = 0, saa = 0;
    long m = 0;
    for (long i = 0; i < len; i++) {
        long j = i + lag_s;
        if (j < 0 || j >= b.n) continue;
        sab += (double)a.x[i] * b.x[j]; sbb += (double)b.x[j] * b.x[j]; saa += (double)a.x[i] * a.x[i];
        m++;
    }
    double k = sbb > 0 ? sab / sbb : 0.0;
    float *bs = (float *)calloc((size_t)(len > 0 ? len : 1), sizeof *bs), *as = (float *)calloc((size_t)(len > 0 ? len : 1), sizeof *as);
    if (!as || !bs) { fprintf(stderr, "tpsoundcmp: out of memory\n"); return 1; }
    long n = 0;
    for (long i = 0; i < len; i++) {
        long j = i + lag_s;
        if (j < 0 || j >= b.n) continue;
        as[n] = a.x[i];
        bs[n] = (float)(b.x[j] * k);
        n++;
    }
    double wave_r = correlation(as, bs, n);
    long ne;
    pcm A = { as, n, a.rate }, B = { bs, n, a.rate };
    float *la = loudness(&A, step, &ne), *lb = loudness(&B, step, &ne);
    double loud_r = correlation(la, lb, ne);
    double ra = 0, rb = 0;
    for (long i = 0; i < n; i++) { ra += (double)as[i] * as[i]; rb += (double)bs[i] * bs[i]; }

    printf("aligned: the port %+ld samples (%.1f ms) against MAME\n", lag_s, 1000.0 * lag_s / a.rate);
    printf("scale that fits: %.4f (the port's samples times this are MAME's)\n", k);
    printf("loudness: MAME %.4f rms, the port unscaled %.4f rms, %.2f dB louder than MAME\n", sqrt(saa / m),
           sqrt(sbb / m), 20.0 * log10(sqrt(sbb / m) / sqrt(saa / m)));
    printf("waves alike (correlation): %.4f\n", wave_r);
    printf("loudness over time alike (10 ms steps): %.4f\n", loud_r);
    printf("scaled: rms MAME %.4f, port %.4f\n", sqrt(ra / n), sqrt(rb / n));
    double ba[NBANDS], bb[NBANDS];
    bands(as, n, a.rate, ba);
    bands(bs, n, a.rate, bb);
    printf("energy by octave (port scaled against MAME, dB):\n");
    for (int i = 0; i < NBANDS; i++)
        printf("  %5.0f..%5.0f Hz: %+6.2f dB   (MAME %.1f dB of its loudest)\n", 62.5 * pow(2, i), 62.5 * pow(2, i + 1),
               10.0 * log10((bb[i] + 1e-12) / (ba[i] + 1e-12)), 10.0 * log10((ba[i] + 1e-12) / (ba[0] + ba[1] + ba[2] + ba[3] + ba[4] + ba[5] + ba[6] + ba[7] + ba[8] + 1e-12)));
    return 0;
}

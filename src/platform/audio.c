#include "platform/audio.h"

#include <vectorpie/vp_audio.h>

#include <stdio.h>
#include <string.h>

/* The sound goes out as the SDK's stream of a game's own samples
 * (vp_stream_play): the SDK mixes it, resampled to the device, at the
 * cabinet menu's volume (its MUSIC VOLUME: the SDK counts a game's own
 * stream as music), on top of the game's own VOLUME. */
#define AHEAD_MAX   (TP_SOUND_RATE / 4)     /* more waiting than this: the device is behind */
#define AHEAD_KEEP  (TP_SOUND_RATE / 10)    /* then down to this */
#define CHUNK       1024                    /* pairs moved at a time */

static vp_audio *g_audio;
static vp_voice  g_voice;
static uint32_t  g_dry, g_dropped;

int tp_audio_open(tp_sound *snd)
{
    (void)snd;
    if (vp_audio_open(&g_audio) != VP_OK) {
        fprintf(stderr, "tempest: no sound: %s\n", vp_last_error());
        g_audio = NULL;
        return -1;
    }
    g_voice = vp_stream_play(g_audio, TP_SOUND_RATE, 1.0f);
    if (!g_voice) {
        fprintf(stderr, "tempest: no sound stream: %s\n", vp_last_error());
        vp_audio_close(g_audio);
        g_audio = NULL;
        return -1;
    }
    return 0;
}

void tp_audio_close(void)
{
    if (g_audio) {
        if (g_voice) vp_voice_stop(g_audio, g_voice, 0.0f);
        vp_audio_close(g_audio);
    }
    g_audio = NULL;
    g_voice = 0;
}

void tp_audio_balance(tp_sound *snd)
{
    if (!g_audio || !g_voice) return;
    int queued = vp_stream_queued(g_audio, g_voice);
    if (queued <= 0) g_dry++;           /* (it ran dry since the last picture: a click, perhaps) */
    /* More than a quarter of a second waiting: the device is behind. What
     * the game made is dropped, oldest first, down to a tenth of a second. */
    uint32_t waiting = snd->ring_w - snd->ring_r;
    if ((uint32_t)queued + waiting > AHEAD_MAX) {
        uint32_t keep = (uint32_t)queued < AHEAD_KEEP ? AHEAD_KEEP - (uint32_t)queued : 0;
        if (waiting > keep) {
            g_dropped += waiting - keep;
            snd->ring_r = snd->ring_w - keep;
        }
    }
    /* everything new to the stream, as float */
    static int16_t pairs[CHUNK * 2];
    static float frames[CHUNK * 2];
    for (;;) {
        int n = tp_sound_read(snd, pairs, CHUNK);
        if (n <= 0) break;
        for (int i = 0; i < 2 * n; i++) frames[i] = (float)pairs[i] * (1.0f / 32768.0f);
        int took = vp_stream_write(g_audio, g_voice, frames, n);
        if (took < n) g_dropped += (uint32_t)(n - took);
        if (n < CHUNK) break;
    }
}

void tp_audio_trouble(uint32_t *dry, uint32_t *dropped)
{
    *dry = g_dry;
    *dropped = g_dropped;
    g_dry = g_dropped = 0;
}

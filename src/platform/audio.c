#include "platform/audio.h"

#include <SDL.h>

#include <stdio.h>
#include <string.h>

static SDL_AudioDeviceID g_dev;
static int16_t g_last[2];
static volatile uint32_t g_dry, g_dropped;

static void SDLCALL fill(void *user, Uint8 *stream, int len)
{
    tp_sound *snd = (tp_sound *)user;
    int16_t *out = (int16_t *)stream;
    int want = len / 4;                 /* pairs of left and right */
    int got = tp_sound_read(snd, out, want);
    if (got < want) g_dry += (uint32_t)(want - got);
    if (got > 0) { g_last[0] = out[2 * got - 2]; g_last[1] = out[2 * got - 1]; }
    /* Run dry (the game stalled or is just starting): let the last level
     * die away instead of clicking. */
    for (int i = got; i < want; i++)
        for (int k = 0; k < 2; k++) {
            g_last[k] = (int16_t)(g_last[k] - g_last[k] / 64);
            out[2 * i + k] = g_last[k];
        }
}

int tp_audio_open(tp_sound *snd)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "tempest: no sound: %s\n", SDL_GetError());
        return -1;
    }
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof want);
    want.freq = TP_SOUND_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = fill;
    want.userdata = snd;
    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_dev) {
        fprintf(stderr, "tempest: no sound device: %s\n", SDL_GetError());
        return -1;
    }
    SDL_PauseAudioDevice(g_dev, 0);
    return 0;
}

void tp_audio_close(void)
{
    if (g_dev) SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
}

void tp_audio_balance(tp_sound *snd)
{
    /* More than a quarter of a second waiting: the device is behind. Drop
     * what is oldest, down to a tenth of a second. */
    uint32_t waiting = snd->ring_w - snd->ring_r;
    if (waiting > TP_SOUND_RATE / 4) {
        if (g_dev) SDL_LockAudioDevice(g_dev);
        g_dropped += waiting - TP_SOUND_RATE / 10;
        snd->ring_r = snd->ring_w - TP_SOUND_RATE / 10;
        if (g_dev) SDL_UnlockAudioDevice(g_dev);
    }
}

void tp_audio_trouble(uint32_t *dry, uint32_t *dropped)
{
    if (g_dev) SDL_LockAudioDevice(g_dev);
    *dry = g_dry;
    *dropped = g_dropped;
    g_dry = g_dropped = 0;
    if (g_dev) SDL_UnlockAudioDevice(g_dev);
}

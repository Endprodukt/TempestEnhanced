/* audio.h - the sound's samples to the sound device.
 *
 * The sound is made as the game runs (sound/sound.h, its ring); once a
 * picture the new samples go to the SDK's stream of a game's own samples
 * (vp_audio.h, vp_stream_play), which the SDK mixes at the cabinet menu's
 * volume. With no sound device the game runs silent.
 */
#ifndef TP_AUDIO_H
#define TP_AUDIO_H

#include "sound/sound.h"

/* Returns 0, or -1 if no device could be opened (a message is printed). */
int  tp_audio_open(tp_sound *snd);
void tp_audio_close(void);

/* Once per picture: the new samples to the stream, about a tenth of a
 * second kept ahead; what is too far ahead (the device takes samples a
 * little slower than the game makes them) is dropped. */
void tp_audio_balance(tp_sound *snd);

/* Pictures at which the stream had run dry (a click, perhaps), and samples
 * dropped because it ran behind, since the last call. */
void tp_audio_trouble(uint32_t *dry, uint32_t *dropped);

#endif

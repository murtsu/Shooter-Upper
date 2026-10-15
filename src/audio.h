/*
 * audio.h - an 8-bit style sound chip, emulated in software.
 *
 * There are no sound files. Every sound effect is a short recipe of notes
 * (channel, waveform, start pitch, end pitch, volume, length). The mixer
 * builds the samples on the fly, the way a 1980s arcade board did:
 *   channel 0: pulse wave   channel 1: pulse wave
 *   channel 2: triangle     channel 3: noise
 */
#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SFX_LASER = 0, SFX_ENEMY_SHOT, SFX_EXPLODE_SMALL, SFX_EXPLODE_BIG,
    SFX_SMARTBOMB, SFX_ABDUCT, SFX_SCREAM, SFX_CATCH, SFX_LANDING,
    SFX_HUMANOID_DIE, SFX_MUTANT, SFX_HYPER, SFX_HYPER_BACK, SFX_EXTRALIFE,
    SFX_WAVE_START, SFX_WAVE_CLEAR, SFX_PLANET_BLOW, SFX_SHIP_DEATH,
    SFX_BAITER, SFX_GAMEOVER, SFX_RESPAWN, SFX_MINE_DROP,
    SFX_MENU_MOVE, SFX_MENU_SELECT, SFX_MENU_BACK, SFX_TITLE,
    SFX_COUNT
} Sfx;

/* open_device=false is for tests and the WAV dump: no sound card needed. */
bool audio_init(bool open_device);
void audio_shutdown(void);
void audio_play(Sfx s);
void audio_stop_all(void);
void audio_set_volume(float v);          /* 0.0 .. 1.0 */
const char *audio_sfx_name(Sfx s);

/* Mix 'frames' stereo frames into out (interleaved L,R). Used by the SDL
 * callback and by the WAV dump. Returns nothing; clipping is prevented by
 * keeping the summed channel gains below 1.0. */
void audio_render(int16_t *out, int frames);

/* lock the audio thread while changing shared state (no-op without a device) */
void audio_lock(void);
void audio_unlock(void);

#endif

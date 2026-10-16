/*
 * music.h - the title song.
 *
 * The song is an MP3 file that is decoded once at start-up into memory and
 * then looped. The game looks for it in this order, and uses the first one
 * that works:
 *
 *   1. ~/.config/shooter-upper/theme.mp3     the PLAYER's own song
 *   2. <install folder>/music/theme.mp3      the song that ships with the game
 *   3. a built-in chip tune, written in code (35 seconds, same loop length)
 *
 * Recommended file: 35 seconds, 128 kbit/s, stereo, 44.1 kHz. Other lengths
 * (up to 8 minutes) and sample rates are accepted and converted.
 */
#ifndef MUSIC_H
#define MUSIC_H

#include <stdbool.h>
#include <stdint.h>

#define MUSIC_FILE_NAME "theme.mp3"

typedef enum {
    MR_OK = 0, MR_NOT_FOUND, MR_UNREADABLE, MR_NOT_MP3, MR_TOO_SHORT, MR_TOO_LONG, MR_NOMEM
} MusicResult;

typedef enum { MS_NONE = 0, MS_CUSTOM, MS_BUNDLED, MS_BUILTIN } MusicSource;

MusicResult music_load_file(const char *path, MusicSource as);   /* decode an mp3 */
void        music_load_builtin(void);                            /* synthesize the fallback */
MusicSource music_source(void);
float       music_seconds(void);                                 /* length of the loop */
const char *music_result_text(MusicResult r);

/* playback control; all safe to call from the main thread */
void music_set_volume(float v);                  /* 0..1, the player's setting */
void music_play(void);                           /* start from the beginning, fade in */
void music_stop(void);                           /* fade out */
bool music_playing(void);

/* called from the audio thread: adds music into an interleaved stereo buffer */
void music_mix(int16_t *out, int frames);

bool music_export_wav(const char *path);   /* write the loaded loop as a WAV file */

void music_free(void);

#endif

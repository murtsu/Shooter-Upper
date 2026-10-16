/*
 * audio.c - the sound chip. See audio.h for the big picture.
 *
 * How it works, in short:
 *  - audio_play() puts the notes of a sound effect into a small schedule,
 *    each with a start delay.
 *  - audio_render() runs once per output sample: it starts notes whose time
 *    has come, advances the four channel oscillators, adds them together
 *    and applies the master volume.
 *  - The SDL audio callback just calls audio_render().
 */
#include "audio.h"
#include "music.h"
#include <SDL.h>
#include <math.h>
#include <string.h>

#define RATE 44100

enum { CH_P1 = 0, CH_P2 = 1, CH_TRI = 2, CH_NOI = 3, CH_COUNT = 4 };
enum { W_P125 = 0, W_P25, W_P50, W_P75, W_TRI, W_NOISE };

typedef struct {
    int   delay_ms;
    int   ch, wave;
    float f0, f1;     /* start / end pitch in Hz (noise: clock rate)   */
    float vol;
    int   ms;
    float hold;       /* fraction of the note at full volume, then fades */
} Note;

typedef struct { const Note *notes; int count; } SfxDef;

/* MIDI note number to Hz */
static float mf(int n) { return 440.0f * powf(2.0f, (float)(n - 69) / 12.0f); }

/* Note constructor to keep the tables readable */
#define N(d, c, w, a, b, v, m, h) { d, c, w, (float)(a), (float)(b), v, m, h }

/* ---- sound effect recipes ----------------------------------------------- */
static const Note s_laser[]   = { N(0, CH_P1, W_P25, 1500, 260, 0.9f, 95, 0.0f) };
static const Note s_eshot[]   = { N(0, CH_P2, W_P50, 420, 180, 0.5f, 70, 0.2f) };
static const Note s_exps[]    = { N(0, CH_NOI, W_NOISE, 9000, 2500, 0.9f, 170, 0.1f),
                                  N(0, CH_TRI, W_TRI, 180, 60, 0.9f, 170, 0.0f) };
static const Note s_expb[]    = { N(0, CH_NOI, W_NOISE, 6000, 900, 1.0f, 450, 0.2f),
                                  N(0, CH_TRI, W_TRI, 120, 40, 0.9f, 450, 0.1f),
                                  N(0, CH_P2, W_P125, 300, 60, 0.5f, 300, 0.0f) };
static const Note s_smart[]   = { N(0, CH_NOI, W_NOISE, 12000, 1500, 0.9f, 700, 0.5f),
                                  N(0, CH_P1, W_P50, 200, 2400, 0.6f, 250, 0.6f),
                                  N(250, CH_P1, W_P50, 2400, 100, 0.6f, 450, 0.0f),
                                  N(0, CH_TRI, W_TRI, 300, 40, 0.9f, 700, 0.3f) };
static const Note s_abduct[]  = { N(0, CH_P2, W_P50, 880, 880, 0.5f, 60, 0.8f),
                                  N(60, CH_P2, W_P50, 1175, 1175, 0.5f, 60, 0.8f),
                                  N(120, CH_P2, W_P50, 880, 880, 0.5f, 60, 0.8f),
                                  N(180, CH_P2, W_P50, 1175, 1175, 0.5f, 60, 0.8f),
                                  N(240, CH_P2, W_P50, 880, 880, 0.5f, 60, 0.8f),
                                  N(300, CH_P2, W_P50, 1175, 1175, 0.5f, 80, 0.5f) };
static const Note s_scream[]  = { N(0, CH_P1, W_P25, 1800, 900, 0.7f, 100, 0.5f),
                                  N(90, CH_P1, W_P25, 1900, 700, 0.7f, 100, 0.5f),
                                  N(180, CH_P1, W_P25, 2000, 500, 0.7f, 140, 0.3f) };
#define M(n) mf(n)
static const Note s_catch[]   = { N(0, CH_P1, W_P25, 523, 523, 0.7f, 70, 0.7f),
                                  N(70, CH_P1, W_P25, 659, 659, 0.7f, 70, 0.7f),
                                  N(140, CH_P1, W_P25, 784, 784, 0.7f, 70, 0.7f),
                                  N(210, CH_P1, W_P25, 1047, 1047, 0.7f, 160, 0.4f),
                                  N(0, CH_TRI, W_TRI, 131, 131, 0.8f, 370, 0.6f) };
static const Note s_landing[] = { N(0, CH_P1, W_P25, 784, 784, 0.7f, 60, 0.8f),
                                  N(70, CH_P1, W_P25, 1047, 1047, 0.7f, 150, 0.4f),
                                  N(0, CH_TRI, W_TRI, 196, 131, 0.8f, 220, 0.3f) };
static const Note s_hdie[]    = { N(0, CH_P2, W_P125, 400, 60, 0.6f, 400, 0.0f),
                                  N(0, CH_NOI, W_NOISE, 3000, 500, 0.6f, 300, 0.1f) };
static const Note s_mutant[]  = { N(0, CH_P1, W_P25, 900, 200, 0.7f, 80, 0.2f),
                                  N(80, CH_P1, W_P25, 900, 200, 0.7f, 80, 0.2f),
                                  N(160, CH_P1, W_P25, 900, 150, 0.7f, 120, 0.2f) };
static const Note s_hyper[]   = { N(0, CH_P1, W_P50, 200, 3000, 0.6f, 280, 0.7f),
                                  N(0, CH_NOI, W_NOISE, 1000, 8000, 0.4f, 150, 0.2f) };
static const Note s_hback[]   = { N(0, CH_P1, W_P50, 3000, 400, 0.6f, 120, 0.2f) };
static const Note s_extra[]   = { N(0, CH_P1, W_P25, 523, 523, 0.7f, 75, 0.8f),
                                  N(75, CH_P1, W_P25, 659, 659, 0.7f, 75, 0.8f),
                                  N(150, CH_P1, W_P25, 784, 784, 0.7f, 75, 0.8f),
                                  N(225, CH_P1, W_P25, 1047, 1047, 0.7f, 75, 0.8f),
                                  N(300, CH_P1, W_P25, 1319, 1319, 0.7f, 75, 0.8f),
                                  N(375, CH_P1, W_P25, 1568, 1568, 0.7f, 300, 0.5f),
                                  N(150, CH_P2, W_P50, 659, 659, 0.4f, 75, 0.8f),
                                  N(300, CH_P2, W_P50, 988, 988, 0.4f, 380, 0.5f),
                                  N(0, CH_TRI, W_TRI, 131, 131, 0.8f, 700, 0.8f) };
static const Note s_wstart[]  = { N(0, CH_P1, W_P25, 392, 392, 0.7f, 90, 0.8f),
                                  N(90, CH_P1, W_P25, 523, 523, 0.7f, 90, 0.8f),
                                  N(180, CH_P1, W_P25, 659, 659, 0.7f, 90, 0.8f),
                                  N(270, CH_P1, W_P25, 784, 784, 0.7f, 260, 0.5f),
                                  N(0, CH_TRI, W_TRI, 131, 131, 0.8f, 500, 0.7f) };
static const Note s_wclear[]  = { N(0, CH_P1, W_P25, 523, 523, 0.7f, 100, 0.8f),
                                  N(100, CH_P1, W_P25, 659, 659, 0.7f, 100, 0.8f),
                                  N(200, CH_P1, W_P25, 784, 784, 0.7f, 100, 0.8f),
                                  N(300, CH_P1, W_P25, 1047, 1047, 0.7f, 100, 0.8f),
                                  N(400, CH_P1, W_P25, 784, 784, 0.7f, 100, 0.8f),
                                  N(500, CH_P1, W_P25, 1047, 1047, 0.7f, 400, 0.5f),
                                  N(200, CH_P2, W_P50, 523, 523, 0.4f, 100, 0.8f),
                                  N(400, CH_P2, W_P50, 659, 659, 0.4f, 100, 0.8f),
                                  N(500, CH_P2, W_P50, 784, 784, 0.4f, 400, 0.5f),
                                  N(0, CH_TRI, W_TRI, 131, 131, 0.8f, 450, 0.7f),
                                  N(500, CH_TRI, W_TRI, 196, 196, 0.8f, 400, 0.6f) };
static const Note s_planet[]  = { N(0, CH_NOI, W_NOISE, 8000, 300, 1.0f, 1800, 0.6f),
                                  N(0, CH_TRI, W_TRI, 90, 25, 1.0f, 1800, 0.5f),
                                  N(0, CH_P2, W_P125, 500, 40, 0.6f, 1200, 0.1f) };
static const Note s_sdeath[]  = { N(0, CH_NOI, W_NOISE, 7000, 600, 0.9f, 800, 0.3f),
                                  N(0, CH_P1, W_P50, 1200, 100, 0.6f, 700, 0.1f),
                                  N(0, CH_TRI, W_TRI, 160, 35, 0.9f, 800, 0.3f) };
static const Note s_baiter[]  = { N(0, CH_P2, W_P50, 600, 900, 0.5f, 150, 0.9f),
                                  N(150, CH_P2, W_P50, 900, 600, 0.5f, 150, 0.9f),
                                  N(300, CH_P2, W_P50, 600, 900, 0.5f, 150, 0.9f),
                                  N(450, CH_P2, W_P50, 900, 600, 0.5f, 150, 0.9f) };
static const Note s_gameover[]= { N(0, CH_P1, W_P25, 392, 392, 0.7f, 220, 0.8f),
                                  N(240, CH_P1, W_P25, 330, 330, 0.7f, 220, 0.8f),
                                  N(480, CH_P1, W_P25, 262, 262, 0.7f, 220, 0.8f),
                                  N(720, CH_P1, W_P25, 196, 196, 0.7f, 700, 0.3f),
                                  N(0, CH_TRI, W_TRI, 98, 98, 0.8f, 900, 0.8f),
                                  N(720, CH_TRI, W_TRI, 65, 65, 0.8f, 900, 0.5f) };
static const Note s_respawn[] = { N(0, CH_P1, W_P50, 300, 1200, 0.5f, 120, 0.6f) };
static const Note s_mine[]    = { N(0, CH_P2, W_P125, 200, 120, 0.3f, 40, 0.3f) };
static const Note s_mmove[]   = { N(0, CH_P1, W_P50, 1000, 1000, 0.4f, 30, 0.8f) };
static const Note s_msel[]    = { N(0, CH_P1, W_P50, 1200, 1800, 0.5f, 80, 0.6f) };
static const Note s_mback[]   = { N(0, CH_P1, W_P50, 900, 500, 0.5f, 70, 0.6f) };

/* Title jingle: an original little tune. Built at start-up from MIDI notes
 * because the pitches come from a formula, not from a literal number. */
static Note s_title[40];
static int  s_title_n = 0;
/*
 * Fills the s_title array with the notes of the title jingle. Each note's
 * pitch is worked out from a MIDI note number with mf(), so this has to run
 * once at start-up instead of being written as a fixed table. Three parts play
 * together: the melody (square wave), a bass line (triangle wave) and a quiet
 * echo of the melody an octave lower (second square channel).
 */
static void build_title(void)
{
    static const int mel[16] = { 69,72,76,81, 79,76,72,76, 74,77,81,86, 84,81,77,81 };
    static const int bass[4] = { 45, 43, 50, 48 };
    int n = 0;
    for (int i = 0; i < 16; i++)
        s_title[n++] = (Note)N(i * 120, CH_P1, W_P25, mf(mel[i]), mf(mel[i]), 0.65f, 110, 0.8f);
    for (int i = 0; i < 4; i++)
        s_title[n++] = (Note)N(i * 480, CH_TRI, W_TRI, mf(bass[i]), mf(bass[i]), 0.8f, 470, 0.7f);
    for (int i = 0; i < 4; i++)
        s_title[n++] = (Note)N(i * 480 + 240, CH_P2, W_P50, mf(mel[i * 4] - 12), mf(mel[i * 4] - 12), 0.3f, 200, 0.6f);
    s_title_n = n;
}

#define DEF(arr) { arr, (int)(sizeof(arr) / sizeof(arr[0])) }
static SfxDef s_defs[SFX_COUNT];
static const char *s_names[SFX_COUNT] = {
    "laser", "enemy_shot", "explode_small", "explode_big", "smartbomb",
    "abduct", "scream", "catch", "landing", "humanoid_die", "mutant",
    "hyper", "hyper_back", "extralife", "wave_start", "wave_clear",
    "planet_blow", "ship_death", "baiter", "gameover", "respawn",
    "mine_drop", "menu_move", "menu_select", "menu_back", "title"
};

/*
 * Collects every sound effect recipe into the s_defs table, so that
 * audio_play() can find a recipe by its Sfx number. Called once from
 * audio_init().
 */
static void build_defs(void)
{
    build_title();
    SfxDef d[SFX_COUNT] = {
        DEF(s_laser), DEF(s_eshot), DEF(s_exps), DEF(s_expb), DEF(s_smart),
        DEF(s_abduct), DEF(s_scream), DEF(s_catch), DEF(s_landing), DEF(s_hdie),
        DEF(s_mutant), DEF(s_hyper), DEF(s_hback), DEF(s_extra), DEF(s_wstart),
        DEF(s_wclear), DEF(s_planet), DEF(s_sdeath), DEF(s_baiter),
        DEF(s_gameover), DEF(s_respawn), DEF(s_mine), DEF(s_mmove), DEF(s_msel),
        DEF(s_mback), { s_title, 0 }
    };
    d[SFX_TITLE].count = s_title_n;
    memcpy(s_defs, d, sizeof d);
}

/* ======================================================================== */
/* Mixer state                                                              */
/* ======================================================================== */

typedef struct {
    bool  on;
    int   wave;
    float f0, f1, vol, hold;
    int   total, pos;         /* samples */
    float phase;
    uint16_t lfsr;
} Voice;

typedef struct {
    bool  used;
    long  start;              /* absolute sample index to begin at */
    Note  n;
} Pending;

#define MAX_PENDING 96

static Voice   g_voice[CH_COUNT];
static Pending g_pend[MAX_PENDING];
static long    g_now = 0;
static float   g_master = 0.8f;
static SDL_AudioDeviceID g_dev = 0;

static const float k_gain[CH_COUNT] = { 0.22f, 0.20f, 0.28f, 0.20f };

/*
 * Starts one note right now. Each channel (CH_P1, CH_P2, CH_TRI, CH_NOI) can
 * only play one note at a time, so a new note simply replaces whatever that
 * channel was doing. This copies the note's settings into the channel's Voice
 * and rewinds it to the start.
 */
static void trigger(const Note *n)
{
    Voice *v = &g_voice[n->ch];
    v->on = true;
    v->wave = n->wave;
    v->f0 = n->f0; v->f1 = n->f1;
    v->vol = n->vol; v->hold = n->hold;
    v->total = n->ms * RATE / 1000;
    if (v->total < 1) v->total = 1;
    v->pos = 0;
    v->phase = 0.f;
    if (v->lfsr == 0) v->lfsr = 0x4000;
}

/*
 * Makes the sound. The sound card asks for a batch of samples (one sample is
 * one tiny measurement of the sound wave, 44100 per second) and this function
 * fills the batch. For every sample it does four things: 1) starts notes that
 * are due, 2) works out the current value of each of the four channels (pitch
 * slides from f0 to f1, volume holds and then fades), 3) adds the channels
 * together and applies the master volume, 4) writes the result to both the
 * left and right speaker. At the end the music layer is mixed on top. Samples
 * are 16-bit numbers, so values are kept between -1 and 1 and then scaled to
 * fit.
 */
void audio_render(int16_t *out, int frames)
{
    for (int i = 0; i < frames; i++, g_now++) {
        /* start notes whose time has come */
        for (int p = 0; p < MAX_PENDING; p++) {
            if (g_pend[p].used && g_pend[p].start <= g_now) {
                trigger(&g_pend[p].n);
                g_pend[p].used = false;
            }
        }
        float mix = 0.f;
        for (int c = 0; c < CH_COUNT; c++) {
            Voice *v = &g_voice[c];
            if (!v->on) continue;
            float t = (float)v->pos / (float)v->total;           /* 0..1 */
            float f = v->f0 + (v->f1 - v->f0) * t;
            float env;
            if (t < v->hold) env = 1.f;
            else env = 1.f - (t - v->hold) / (1.f - v->hold + 1e-6f);
            if (v->pos < 40) env *= (float)v->pos / 40.f;         /* click-free attack */
            if (env < 0.f) env = 0.f;

            float s;
            if (v->wave == W_NOISE) {
                v->phase += f / (float)RATE;
                while (v->phase >= 1.f) {
                    v->phase -= 1.f;
                    uint16_t bit = (uint16_t)((v->lfsr ^ (v->lfsr >> 1)) & 1u);
                    v->lfsr = (uint16_t)((v->lfsr >> 1) | (bit << 14));
                }
                s = (v->lfsr & 1u) ? 1.f : -1.f;
            } else {
                v->phase += f / (float)RATE;
                if (v->phase >= 1.f) v->phase -= floorf(v->phase);
                if (v->wave == W_TRI) {
                    s = 4.f * fabsf(v->phase - 0.5f) - 1.f;
                } else {
                    static const float duty[4] = { 0.125f, 0.25f, 0.5f, 0.75f };
                    s = (v->phase < duty[v->wave]) ? 1.f : -1.f;
                }
            }
            mix += s * env * v->vol * k_gain[c];
            if (++v->pos >= v->total) v->on = false;
        }
        mix *= g_master;
        if (mix > 1.f) mix = 1.f;
        if (mix < -1.f) mix = -1.f;
        int16_t o = (int16_t)(mix * 32000.f);
        out[i * 2] = o;
        out[i * 2 + 1] = o;
    }
    music_mix(out, frames);          /* the song goes underneath the sound effects */
}

/*
 * The sound card calls audio_render() on its own thread. Anything else that
 * touches the sound data must hold this lock so the two threads never change
 * the same data at the same moment. audio_unlock() lets go again.
 */
void audio_lock(void)   { if (g_dev) SDL_LockAudioDevice(g_dev); }
void audio_unlock(void) { if (g_dev) SDL_UnlockAudioDevice(g_dev); }

/*
 * The function SDL calls whenever the sound card wants more sound. len is in
 * bytes: each frame is 4 bytes (2 speakers times 2 bytes), so len / 4 is the
 * number of frames to make.
 */
static void SDLCALL sdl_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    audio_render((int16_t *)stream, len / 4);
}

/*
 * Gets the sound system ready. With open_device false (used by the tests and
 * by --no-audio) only the sound tables are built and nothing is played.
 * Otherwise it opens the default sound card with 44100 Hz, 16-bit, stereo, and
 * starts it. Returns false if there is no sound card, so main.c can show a
 * dialog instead of crashing.
 */
bool audio_init(bool open_device)
{
    build_defs();
    memset(g_voice, 0, sizeof g_voice);
    memset(g_pend, 0, sizeof g_pend);
    g_now = 0;
    if (!open_device) return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = sdl_cb;
    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_dev) return false;
    SDL_PauseAudioDevice(g_dev, 0);
    return true;
}

/* Closes the sound card. Safe to call even if it was never opened. */
void audio_shutdown(void)
{
    if (g_dev) { SDL_CloseAudioDevice(g_dev); g_dev = 0; }
}

/*
 * Plays a sound effect by queueing its notes. Each note goes into a free slot
 * in the pending list together with the time it should start (delay_ms
 * converted to samples). audio_render() later starts them when their time
 * comes. If all 96 slots are full the extra notes are dropped, which is better
 * than stalling the game.
 */
void audio_play(Sfx s)
{
    if (s < 0 || s >= SFX_COUNT) return;
    if (g_dev) SDL_LockAudioDevice(g_dev);
    const SfxDef *d = &s_defs[s];
    for (int i = 0; i < d->count; i++) {
        for (int p = 0; p < MAX_PENDING; p++) {
            if (g_pend[p].used) continue;
            g_pend[p].used = true;
            g_pend[p].n = d->notes[i];
            g_pend[p].start = g_now + (long)d->notes[i].delay_ms * RATE / 1000;
            break;
        }
    }
    if (g_dev) SDL_UnlockAudioDevice(g_dev);
}

/* Silences everything at once: all four channels and all queued notes. */
void audio_stop_all(void)
{
    if (g_dev) SDL_LockAudioDevice(g_dev);
    memset(g_voice, 0, sizeof g_voice);
    memset(g_pend, 0, sizeof g_pend);
    if (g_dev) SDL_UnlockAudioDevice(g_dev);
}

/*
 * Sets the master volume. 0 is silent and 1 is full. Values outside that range
 * are clamped so a bad setting can never blast the speakers.
 */
void audio_set_volume(float v)
{
    if (v < 0.f) v = 0.f;
    if (v > 1.f) v = 1.f;
    g_master = v;
}

/*
 * Returns the text name of a sound effect, such as "laser". Used by tests and
 * debugging. Gives "?" for a number that does not exist.
 */
const char *audio_sfx_name(Sfx s) { return (s >= 0 && s < SFX_COUNT) ? s_names[s] : "?"; }

/*
 * music.c - decode, loop and (if needed) compose the title song.
 */
#include "music.h"
#include "audio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MINIMP3_IMPLEMENTATION
#include "../third_party/minimp3.h"

#define RATE 44100
#define MIN_SECONDS 5
#define MAX_SECONDS (8 * 60)
#define XFADE 1764            /* 40 ms blended at the loop point so it never clicks */
#define FADE_MS 600

static int16_t *g_buf;        /* interleaved stereo */
static long     g_frames;
static long     g_pos;
static MusicSource g_src = MS_NONE;
static float    g_vol = 0.7f;
static float    g_gain = 0.f, g_target = 0.f, g_step = 0.f;

/* ---------------------------------------------------------------- loading */

static void discard(void)
{
    audio_lock();
    g_target = g_gain = 0.f;
    free(g_buf);
    g_buf = NULL; g_frames = 0; g_pos = 0; g_src = MS_NONE;
    audio_unlock();
}

/* make the end flow into the start: blend the tail with the head, then cut it */
static void make_loop(int16_t *buf, long *frames)
{
    long n = *frames;
    if (n < XFADE * 4) return;
    long keep = n - XFADE;
    for (long i = 0; i < XFADE; i++) {
        float t = (float)i / (float)XFADE;              /* 0..1 */
        for (int c = 0; c < 2; c++) {
            float head = buf[i * 2 + c], tail = buf[(keep + i) * 2 + c];
            buf[i * 2 + c] = (int16_t)(tail * (1.f - t) + head * t);
        }
    }
    *frames = keep;
}

static void install(int16_t *buf, long frames, MusicSource src)
{
    make_loop(buf, &frames);
    audio_lock();
    int16_t *old = g_buf;
    g_buf = buf; g_frames = frames; g_pos = 0; g_src = src;
    g_target = g_gain = 0.f;
    audio_unlock();
    free(old);
}

MusicResult music_load_file(const char *path, MusicSource as)
{
    FILE *f = fopen(path, "rb");
    if (!f) return MR_NOT_FOUND;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 40 * 1024 * 1024) { fclose(f); return size <= 0 ? MR_UNREADABLE : MR_TOO_LONG; }
    uint8_t *data = malloc((size_t)size);
    if (!data) { fclose(f); return MR_NOMEM; }
    if (fread(data, 1, (size_t)size, f) != (size_t)size) { free(data); fclose(f); return MR_UNREADABLE; }
    fclose(f);

    /* skip an ID3v2 tag if there is one */
    long off = 0;
    if (size > 10 && !memcmp(data, "ID3", 3)) {
        off = 10 + ((data[6] & 0x7f) << 21 | (data[7] & 0x7f) << 14 | (data[8] & 0x7f) << 7 | (data[9] & 0x7f));
        if (off > size) off = 0;
    }

    mp3dec_t dec;
    mp3dec_init(&dec);
    long cap = RATE * 40, n = 0;                 /* grows as needed */
    int16_t *pcm = malloc((size_t)cap * 2 * sizeof(int16_t));
    mp3d_sample_t frame[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int hz = 0, frames_ok = 0;
    if (!pcm) { free(data); return MR_NOMEM; }

    while (off < size) {
        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&dec, data + off, (int)(size - off), frame, &info);
        if (info.frame_bytes == 0) break;
        off += info.frame_bytes;
        if (samples <= 0) continue;
        if (!hz) hz = info.hz;
        if (info.hz != hz) continue;             /* ignore stray frames */
        frames_ok++;
        if (n + samples > cap) {
            cap *= 2;
            int16_t *bigger = realloc(pcm, (size_t)cap * 2 * sizeof(int16_t));
            if (!bigger) { free(pcm); free(data); return MR_NOMEM; }
            pcm = bigger;
        }
        for (int i = 0; i < samples; i++) {
            if (info.channels == 1) { pcm[(n + i) * 2] = frame[i]; pcm[(n + i) * 2 + 1] = frame[i]; }
            else                    { pcm[(n + i) * 2] = frame[i * 2]; pcm[(n + i) * 2 + 1] = frame[i * 2 + 1]; }
        }
        n += samples;
        if (n > (long)hz * MAX_SECONDS) { free(pcm); free(data); return MR_TOO_LONG; }
    }
    free(data);
    if (frames_ok < 4 || hz <= 0) { free(pcm); return MR_NOT_MP3; }
    if (n < (long)hz * MIN_SECONDS) { free(pcm); return MR_TOO_SHORT; }

    /* convert to the mixer's 44100 Hz with straight-line interpolation */
    if (hz != RATE) {
        long out_n = (long)((double)n * RATE / hz);
        int16_t *rs = malloc((size_t)out_n * 2 * sizeof(int16_t));
        if (!rs) { free(pcm); return MR_NOMEM; }
        for (long i = 0; i < out_n; i++) {
            double pos = (double)i * hz / RATE;
            long i0 = (long)pos; double fr = pos - (double)i0;
            long i1 = i0 + 1 < n ? i0 + 1 : i0;
            for (int c = 0; c < 2; c++)
                rs[i * 2 + c] = (int16_t)(pcm[i0 * 2 + c] * (1.0 - fr) + pcm[i1 * 2 + c] * fr);
        }
        free(pcm); pcm = rs; n = out_n;
    }
    install(pcm, n, as);
    return MR_OK;
}

const char *music_result_text(MusicResult r)
{
    switch (r) {
    case MR_OK:         return "OK";
    case MR_NOT_FOUND:  return "FILE NOT FOUND";
    case MR_UNREADABLE: return "FILE COULD NOT BE READ";
    case MR_NOT_MP3:    return "NOT A VALID MP3 FILE";
    case MR_TOO_SHORT:  return "SONG IS SHORTER THAN 5 SECONDS";
    case MR_TOO_LONG:   return "SONG IS LONGER THAN 8 MINUTES";
    case MR_NOMEM:      return "OUT OF MEMORY";
    }
    return "?";
}

/* ------------------------------------------------- built-in chip tune
 * 137 BPM, A minor, 20 bars = 35.0 seconds. Chords per 4 bars: Am F C G.
 * Layers arrive over time like a tracker module: bass and drums, then the
 * arpeggio, then the lead, then a break that builds back into the loop.
 */

#define STEP 4825                       /* samples per sixteenth note at 137 BPM */
#define BARS 20
#define STEPS_TOTAL (BARS * 16)

static float hz_of(int midi) { return 440.f * powf(2.f, (float)(midi - 69) / 12.f); }

typedef struct { float *l, *r; long n; } Mix;

static uint32_t g_noise = 1;
static float noise(void) { g_noise = g_noise * 1664525u + 1013904223u; return (float)(g_noise >> 8) / 8388608.f - 1.f; }

/* wave: 0 pulse(duty), 1 triangle, 2 noise, 3 kick (triangle sweeping down) */
static void put(Mix *m, long start, int len, int wave, float f, float duty, float vol, float pan, float decay)
{
    /* pan 0 = left, 0.5 = center, 1 = right (constant-ish power) */
    float gl = pan <= 0.5f ? 1.f : 2.f * (1.f - pan);
    float gr = pan >= 0.5f ? 1.f : 2.f * pan;
    float ph = 0.f;
    for (int i = 0; i < len && start + i < m->n; i++) {
        float t = (float)i / (float)len;
        float env = expf(-t * decay) * (i < 60 ? (float)i / 60.f : 1.f);
        float fr = f, s;
        if (wave == 3) fr = f * (1.f + 3.f * expf(-t * 14.f));
        ph += fr / RATE; ph -= floorf(ph);
        switch (wave) {
        case 0:  s = ph < duty ? 1.f : -1.f; break;
        case 1:
        case 3:  s = 4.f * fabsf(ph - 0.5f) - 1.f; break;
        default: s = noise(); break;
        }
        float v = s * env * vol;
        m->l[start + i] += v * gl;
        m->r[start + i] += v * gr;
    }
}

void music_load_builtin(void)
{
    static const int root[4]    = { 33, 29, 36, 31 };                 /* A1 F1 C2 G1 */
    static const int chord[4][4] = { {57,60,64,69}, {53,57,60,65}, {60,64,67,72}, {55,59,62,67} };
    static const int mel[64] = {
        76,0,0,72, 0,0,69,0, 72,0,0,76, 0,0,74,0,
        77,0,0,72, 0,0,69,0, 72,0,0,77, 0,0,76,0,
        76,0,0,72, 0,0,67,0, 72,0,0,76, 0,0,79,0,
        74,0,0,71, 0,0,67,0, 71,0,74,0, 79,0,74,-1
    };
    static const int bass_oct[8] = { 0, 12, 0, 12, 0, 12, 7, 12 };

    long n = (long)STEPS_TOTAL * STEP;
    Mix m = { calloc((size_t)n, sizeof(float)), calloc((size_t)n, sizeof(float)), n };
    if (!m.l || !m.r) { free(m.l); free(m.r); return; }
    g_noise = 12345;

    for (int s = 0; s < STEPS_TOTAL; s++) {
        int bar = s / 16, st = s % 16, cyc = (bar % 4);
        long at = (long)s * STEP;
        /* drums from the start: kick on the beat, snare on 2 and 4, hats from bar 4 */
        if (st % 4 == 0) put(&m, at, STEP * 3, 3, 52.f, 0.5f, 0.9f, 0.5f, 5.f);
        if (st == 4 || st == 12) put(&m, at, STEP * 3, 2, 9000.f, 0.5f, 0.45f, 0.5f, 6.f);
        if (bar >= 4 && st % 2 == 0) put(&m, at, STEP, 2, 40000.f, 0.5f, 0.16f, 0.72f, 5.f);
        if (bar == 19 && st >= 8) put(&m, at, STEP, 2, 12000.f, 0.5f, 0.35f, 0.5f, 4.f);   /* snare roll */
        /* bass: eighth notes */
        if (st % 2 == 0) put(&m, at, STEP * 2, 1, hz_of(root[cyc] + bass_oct[st / 2]), 0.5f, 0.75f, 0.5f, 3.f);
        /* arpeggio from bar 4 */
        if (bar >= 4) {
            int k = st % 4, oct = (st / 4 % 2) ? 12 : 0;
            put(&m, at, STEP + STEP / 2, 0, hz_of(chord[cyc][k] + oct), 0.25f, 0.20f, 0.25f, 4.f);
        }
        /* lead from bar 8 to 15, twice through the melody, second time an octave up for its last 2 bars */
        if (bar >= 8 && bar < 16) {
            int v = mel[(bar % 4) * 16 + st];
            if (v > 0) {
                if (bar >= 14) v += 12;
                int len = STEP * 3;
                for (int k = st + 1; k < 16 && mel[(bar % 4) * 16 + k] == 0; k++) len += STEP;
                put(&m, at, len, 0, hz_of(v), 0.5f, 0.30f, 0.5f, 1.5f);
                put(&m, at, len, 0, hz_of(v) * 1.006f, 0.125f, 0.12f, 0.6f, 1.5f);   /* slight detune for width */
            }
        }
        /* break bars 16 to 19: a rising line that leads back to bar 0 */
        if (bar >= 16 && st % 4 == 0) {
            int v = 57 + (bar - 16) * 3 + st / 4;
            put(&m, at, STEP * 4, 0, hz_of(v + 12), 0.25f, 0.24f, 0.5f, 2.f);
        }
    }

    float peak = 0.001f;
    for (long i = 0; i < n; i++) {
        if (fabsf(m.l[i]) > peak) peak = fabsf(m.l[i]);
        if (fabsf(m.r[i]) > peak) peak = fabsf(m.r[i]);
    }
    float g = 0.85f * 32767.f / peak;
    int16_t *buf = malloc((size_t)n * 2 * sizeof(int16_t));
    if (buf) {
        for (long i = 0; i < n; i++) {
            buf[i * 2]     = (int16_t)(m.l[i] * g);
            buf[i * 2 + 1] = (int16_t)(m.r[i] * g);
        }
        install(buf, n, MS_BUILTIN);
    }
    free(m.l); free(m.r);
}

/* --------------------------------------------------------------- playback */

MusicSource music_source(void) { return g_src; }
float music_seconds(void) { return g_frames ? (float)g_frames / RATE : 0.f; }
bool  music_playing(void) { return g_target > 0.f; }

void music_set_volume(float v)
{
    if (v < 0.f) v = 0.f;
    if (v > 1.f) v = 1.f;
    audio_lock();
    g_vol = v;
    if (g_target > 0.f) g_target = v;
    audio_unlock();
}

static void fade_to(float target)
{
    audio_lock();
    g_target = target;
    g_step = (target - g_gain) / (RATE * FADE_MS / 1000.f);
    audio_unlock();
}

void music_play(void)
{
    if (!g_buf) return;
    audio_lock();
    if (g_target <= 0.f) { g_pos = 0; g_gain = 0.f; }
    audio_unlock();
    fade_to(g_vol);
}

void music_stop(void) { fade_to(0.f); }

void music_mix(int16_t *out, int frames)
{
    if (!g_buf || (g_target <= 0.f && g_gain <= 0.f)) return;
    for (int i = 0; i < frames; i++) {
        if (g_gain != g_target) {
            g_gain += g_step;
            if ((g_step > 0.f && g_gain >= g_target) || (g_step < 0.f && g_gain <= g_target)) g_gain = g_target;
        }
        for (int c = 0; c < 2; c++) {
            int v = out[i * 2 + c] + (int)(g_buf[g_pos * 2 + c] * g_gain);
            out[i * 2 + c] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
        }
        if (++g_pos >= g_frames) g_pos = 0;
    }
}

bool music_export_wav(const char *path)
{
    if (!g_buf) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    uint32_t data = (uint32_t)(g_frames * 4), riff = 36 + data, rate = RATE, byte_rate = RATE * 4;
    uint16_t fmt = 1, ch = 2, align = 4, bits = 16;
    uint32_t sixteen = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&sixteen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&rate, 4, 1, f); fwrite(&byte_rate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(g_buf, 4, (size_t)g_frames, f);
    return fclose(f) == 0;
}

void music_free(void) { discard(); }

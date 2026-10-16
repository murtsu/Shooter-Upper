#define _POSIX_C_SOURCE 200809L   /* for mkdtemp */
/* test_music: the mp3 loader, the loop and the built-in song. Needs ffmpeg to
 * make its sample files; if ffmpeg is missing those parts are skipped. */
#include "../src/music.h"
#include "../src/audio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, skipped;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

static int make(const char *cmd) { return system(cmd); }

/* every sample file lives in a private temporary folder made at start-up,
   so the test works on any machine and leaves nothing behind */
static char g_dir[64];
static const char *tmp(const char *name)
{
    static char path[4][160];
    static int n;
    char *p = path[n++ % 4];
    snprintf(p, 160, "%s/%s", g_dir, name);
    return p;
}
static int ffmpeg(const char *args, const char *out)
{
    char cmd[512];
    snprintf(cmd, sizeof cmd, "ffmpeg -loglevel error -y -f lavfi -i '%s' %s '%s' </dev/null", args, "-b:a 128k", out);
    return make(cmd);
}

static float render_peak(int frames, double *rms_out)
{
    int16_t *buf = calloc((size_t)frames * 2, sizeof(int16_t));
    float peak = 0; double sum = 0;
    audio_render(buf, frames);
    for (int i = 0; i < frames * 2; i++) { float a = fabsf((float)buf[i]); if (a > peak) peak = a; sum += (double)buf[i] * buf[i]; }
    if (rms_out) *rms_out = sqrt(sum / (frames * 2));
    free(buf);
    return peak;
}

int main(void)
{
    snprintf(g_dir, sizeof g_dir, "/tmp/shooter-upper-test-XXXXXX");
    if (!mkdtemp(g_dir)) { printf("FAIL: could not make a temporary folder\n"); return 1; }
    audio_init(false);

    /* built-in song */
    music_load_builtin();
    CHECK(music_source() == MS_BUILTIN, "builtin loaded");
    CHECK(music_seconds() > 34.5f && music_seconds() < 35.2f, "builtin is about 35 s");
    music_play();
    double rms;
    float pk = 0;
    for (int i = 0; i < 12; i++) { float p = render_peak(44100, &rms); if (p > pk) pk = p; }
    CHECK(pk > 8000 && pk < 32767, "builtin audible and not clipping");
    CHECK(music_playing(), "playing flag");

    /* looping: render past the end and make sure sound continues */
    for (int i = 0; i < 25; i++) render_peak(44100, &rms);
    CHECK(render_peak(44100, &rms) > 1000, "keeps playing after the loop point");

    /* stop fades to silence */
    music_stop();
    render_peak(44100, &rms);
    CHECK(render_peak(4410, &rms) == 0, "silent after fade out");

    /* mp3 files via ffmpeg */
    if (ffmpeg("sine=frequency=440:duration=35", tmp("t44.mp3")) == 0) {
        CHECK(music_load_file(tmp("t44.mp3"), MS_CUSTOM) == MR_OK, "44.1k stereo 128k loads");
        CHECK(music_source() == MS_CUSTOM, "source is custom");
        CHECK(music_seconds() > 34.5f && music_seconds() < 35.3f, "mp3 length near 35 s");
        music_play();
        for (int i = 0; i < 2; i++) render_peak(44100, &rms);
        CHECK(render_peak(44100, &rms) > 1500, "mp3 audible");
    } else skipped++;
    {
        char cmd[512];
        snprintf(cmd, sizeof cmd, "ffmpeg -loglevel error -y -f lavfi -i 'sine=frequency=330:duration=12' -ac 1 -ar 48000 -b:a 96k '%s' </dev/null", tmp("t48.mp3"));
        if (make(cmd) == 0) {
        CHECK(music_load_file(tmp("t48.mp3"), MS_CUSTOM) == MR_OK, "48k mono loads");
        CHECK(music_seconds() > 11.5f && music_seconds() < 12.1f, "resampled length correct");
        } else skipped++;
    }
    if (ffmpeg("sine=frequency=330:duration=2", tmp("short.mp3")) == 0)
        CHECK(music_load_file(tmp("short.mp3"), MS_CUSTOM) == MR_TOO_SHORT, "2 s song rejected as too short");
    else skipped++;

    /* a bad file must not disturb the song that is already loaded */
    float before = music_seconds();
    {
        char cmd[300];
        snprintf(cmd, sizeof cmd, "head -c 5000 /dev/urandom > '%s'", tmp("junk.mp3"));
        (void)make(cmd);
    }
    CHECK(music_load_file(tmp("junk.mp3"), MS_CUSTOM) == MR_NOT_MP3, "random bytes are not an mp3");
    CHECK(music_load_file(tmp("nothere.mp3"), MS_CUSTOM) == MR_NOT_FOUND, "missing file reported");
    CHECK(music_seconds() == before, "failed load keeps the old song");

    music_free();
    {
        char cmd[100];
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", g_dir);
        (void)make(cmd);
    }
    printf(fails ? "%d FAILED\n" : "all music checks passed (%d skipped)\n", fails ? fails : skipped);
    return fails ? 1 : 0;
}

/*
 * test_audio.c - renders every sound effect without a sound card and checks
 * that it makes sound, does not clip, and ends. Optionally writes WAV files
 * so you can listen:  ./test_audio /path/to/outdir
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../src/audio.h"

static void write_wav(const char *path, const int16_t *d, int frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint32_t data = (uint32_t)frames * 4, rate = 44100, br = rate * 4, riff = 36 + data, fmt = 16;
    uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(d, 4, (size_t)frames, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    const int N = 44100 * 4;
    int16_t *buf = malloc((size_t)N * 4);
    int bad = 0;
    audio_init(false);
    audio_set_volume(1.0f);

    for (int s = 0; s < SFX_COUNT; s++) {
        audio_stop_all();
        audio_play((Sfx)s);
        audio_render(buf, N);
        int peak = 0, last = 0;
        for (int i = 0; i < N; i++) {
            int a = abs(buf[i * 2]);
            if (a > peak) peak = a;
            if (a > 200) last = i;
        }
        printf("%-14s peak %5d  length %.2fs\n", audio_sfx_name((Sfx)s), peak, last / 44100.0);
        if (peak < 1000) { printf("  -> SILENT\n"); bad++; }
        if (peak >= 32000) { printf("  -> CLIPS\n"); bad++; }
        if (last >= N - 100) { printf("  -> NEVER ENDS\n"); bad++; }
        if (argc > 1) {
            char p[512];
            snprintf(p, sizeof p, "%s/%s.wav", argv[1], audio_sfx_name((Sfx)s));
            write_wav(p, buf, last + 4410 < N ? last + 4410 : N);
        }
    }

    /* worst case: everything at once */
    audio_stop_all();
    for (int s = 0; s < SFX_COUNT; s++) audio_play((Sfx)s);
    audio_render(buf, N);
    int peak = 0;
    for (int i = 0; i < N; i++) { int a = abs(buf[i * 2]); if (a > peak) peak = a; }
    printf("all at once    peak %5d (limit 32767)\n", peak);
    if (peak > 32000) { printf("  -> over-driven\n"); bad++; }

    free(buf);
    printf(bad ? "\nAUDIO TEST FAILED\n" : "\nall sounds ok\n");
    return bad ? 1 : 0;
}

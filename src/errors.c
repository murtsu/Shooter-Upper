/*
 * errors.c - the text of every dialog, the queue, and the log file.
 * Keep each tip something a normal player can actually do.
 */
#include "errors.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const ErrInfo INFO[ERR_COUNT] = {
    [ERR_NONE] = { ERR_NONE, false, "", "", {NULL}, {NULL}, {0} },

    [ERR_SDL_INIT] = { ERR_SDL_INIT, true, "CANNOT START",
        "The game could not start its video and sound system (SDL).",
        { "Run the game from a desktop session, not from a plain terminal login.",
          "Update your graphics driver and your system, then try again.",
          "On Linux, make sure the sdl2 package is installed.",
          "Send log.txt from the settings folder if it keeps happening." },
        { "QUIT" }, { ERR_ACT_QUIT } },

    [ERR_WINDOW] = { ERR_WINDOW, true, "NO WINDOW",
        "The game could not open its window.",
        { "Try a smaller window: start with  --scale 2 --windowed",
          "Check that your desktop session has a working display.",
          "Update your graphics driver, then try again." },
        { "QUIT" }, { ERR_ACT_QUIT } },

    [ERR_RENDERER_SOFT] = { ERR_RENDERER_SOFT, false, "NO GRAPHICS CARD SUPPORT",
        "Hardware drawing is not available, so the game uses slower software drawing.",
        { "Install or update your graphics driver.",
          "If the game runs slowly, turn off the CRT effect and use a smaller window.",
          "In a virtual machine, enable 3D acceleration for it." },
        { "CONTINUE" }, { ERR_ACT_CONTINUE } },

    [ERR_AUDIO_DEVICE] = { ERR_AUDIO_DEVICE, false, "NO SOUND",
        "The game could not open a sound device, so it runs without sound and music.",
        { "Check that speakers or headphones are connected and not muted.",
          "On Linux, make sure PipeWire or PulseAudio is running.",
          "Close other programs that may be holding the sound device.",
          "Choose RETRY after you have fixed it." },
        { "RETRY", "CONTINUE" }, { ERR_ACT_RETRY, ERR_ACT_CONTINUE } },

    [ERR_MUSIC_FILE] = { ERR_MUSIC_FILE, false, "YOUR SONG DID NOT LOAD",
        "The file theme.mp3 in the settings folder could not be used. The game's own song plays instead.",
        { "Use an MP3 file, 128 kbit/s, stereo, 44.1 kHz.",
          "Make the song between 5 seconds and 8 minutes. 35 seconds is best.",
          "Check the file name is exactly theme.mp3 with small letters.",
          "Convert it with:  ffmpeg -i song.wav -b:a 128k theme.mp3" },
        { "CONTINUE" }, { ERR_ACT_CONTINUE } },

    [ERR_SETTINGS_SAVE] = { ERR_SETTINGS_SAVE, false, "SETTINGS NOT SAVED",
        "Your settings could not be written to disk. They work now but will be forgotten.",
        { "Check there is free space on the disk.",
          "Check you may write to the folder shown below.",
          "Start the game with  --config-dir  and a folder you own." },
        { "RETRY", "CONTINUE" }, { ERR_ACT_RETRY, ERR_ACT_CONTINUE } },

    [ERR_SCORES_SAVE] = { ERR_SCORES_SAVE, false, "HIGH SCORES NOT SAVED",
        "The high score table could not be written to disk.",
        { "Check there is free space on the disk.",
          "Check you may write to the folder shown below.",
          "Choose RETRY after you have fixed it." },
        { "RETRY", "CONTINUE" }, { ERR_ACT_RETRY, ERR_ACT_CONTINUE } },

    [ERR_PAD_LOST] = { ERR_PAD_LOST, false, "CONTROLLER LOST",
        "The controller was unplugged or ran out of battery. The game is paused.",
        { "Plug it in again or switch it on. It is picked up automatically.",
          "Charge the battery if it is a wireless controller.",
          "You can keep playing on the keyboard right now." },
        { "CONTINUE" }, { ERR_ACT_CONTINUE } },

    [ERR_SLOW] = { ERR_SLOW, false, "GAME RUNS TOO SLOWLY",
        "The computer cannot keep up the steady 60 steps a second the game needs.",
        { "FIX IT turns off the CRT effect and makes the window smaller.",
          "Close other programs, especially browsers and video players.",
          "Plug in a laptop and use its high performance mode.",
          "Update your graphics driver." },
        { "FIX IT", "CONTINUE" }, { ERR_ACT_FIX, ERR_ACT_CONTINUE } },

    [ERR_SCREENSHOT] = { ERR_SCREENSHOT, false, "SCREENSHOT FAILED",
        "The screenshot could not be saved in the current folder.",
        { "Check you may write files in the folder you started the game from.",
          "Start the game from your home folder or Pictures folder." },
        { "CONTINUE" }, { ERR_ACT_CONTINUE } },
};

const ErrInfo *err_info(ErrCode c)
{
    return (c > ERR_NONE && c < ERR_COUNT) ? &INFO[c] : &INFO[ERR_NONE];
}

/* ---------------------------------------------------------------- log */

static char g_log_path[600];

void err_log_open(const char *dir)
{
    snprintf(g_log_path, sizeof g_log_path, "%s/log.txt", dir);
    FILE *f = fopen(g_log_path, "rb");               /* keep the log from growing for ever */
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);
        if (size > 64 * 1024) { f = fopen(g_log_path, "wb"); if (f) fclose(f); }
    }
}

static void log_line(const char *text)
{
    fprintf(stderr, "%s\n", text);
    if (!g_log_path[0]) return;
    FILE *f = fopen(g_log_path, "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm *tmv = localtime(&t);
    char stamp[32] = "";
    if (tmv) strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", tmv);
    fprintf(f, "[%s] %s %s\n", stamp, GAME_ID, text);
    fclose(f);
}

/* ---------------------------------------------------------------- queue */

#define QUEUE 6
#define REPEAT_FRAMES (60 * 30)

static struct { ErrCode code; char detail[200]; } g_q[QUEUE];
static int g_qn;
static unsigned long g_frame;
static unsigned long g_last[ERR_COUNT];
static bool g_seen[ERR_COUNT];

void err_tick(void) { g_frame++; }

void err_report(ErrCode c, const char *detail)
{
    if (c <= ERR_NONE || c >= ERR_COUNT) return;
    char line[400];
    snprintf(line, sizeof line, "%s%s%s", INFO[c].title, detail ? ": " : "", detail ? detail : "");
    log_line(line);
    if (g_seen[c] && g_frame - g_last[c] < REPEAT_FRAMES) return;
    g_seen[c] = true; g_last[c] = g_frame;
    for (int i = 0; i < g_qn; i++) if (g_q[i].code == c) return;     /* already waiting */
    if (g_qn >= QUEUE) return;
    g_q[g_qn].code = c;
    snprintf(g_q[g_qn].detail, sizeof g_q[g_qn].detail, "%s", detail ? detail : "");
    g_qn++;
}

bool        err_pending(void) { return g_qn > 0; }
ErrCode     err_current(void) { return g_qn ? g_q[0].code : ERR_NONE; }
const char *err_current_detail(void) { return g_qn ? g_q[0].detail : ""; }

void err_dismiss(void)
{
    if (!g_qn) return;
    for (int i = 1; i < g_qn; i++) g_q[i - 1] = g_q[i];
    g_qn--;
}

void err_format_plain(ErrCode c, const char *detail, char *out, size_t n)
{
    const ErrInfo *e = err_info(c);
    size_t used = (size_t)snprintf(out, n, "%s\n", e->what);
    if (detail && *detail && used < n) used += (size_t)snprintf(out + used, n - used, "\nDetail: %s\n", detail);
    if (e->tips[0] && used < n) used += (size_t)snprintf(out + used, n - used, "\nWhat you can try:\n");
    for (int i = 0; i < ERR_MAX_TIPS && e->tips[i] && used < n; i++)
        used += (size_t)snprintf(out + used, n - used, "  - %s\n", e->tips[i]);
}

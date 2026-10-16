/*
 * storage.c - plain text files, easy to read and to repair by hand.
 */
#include "storage.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

Settings   g_set;
ScoreEntry g_scores[SCORE_SLOTS];

static char g_dir[512] = "";

/*
 * Chooses the folder for settings and scores. Used by --config-dir and by the
 * tests, so they never touch a real player's files.
 */
void storage_set_dir(const char *dir)
{
    snprintf(g_dir, sizeof g_dir, "%s", dir);
}

/*
 * Makes sure the settings folder exists and remembers where it is. Without
 * --config-dir it uses XDG_CONFIG_HOME, or the .config folder in the home
 * directory, or the current folder as a last resort. The folder is called
 * shooter-upper.
 */
static void ensure_dir(void)
{
    if (g_dir[0]) { mkdir(g_dir, 0755); return; }
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    char base[400];
    if (xdg && *xdg) snprintf(base, sizeof base, "%s", xdg);
    else if (home && *home) snprintf(base, sizeof base, "%s/.config", home);
    else snprintf(base, sizeof base, ".");
    mkdir(base, 0755);
    snprintf(g_dir, sizeof g_dir, "%s/%s", base, GAME_ID);
    mkdir(g_dir, 0755);
}

/* Returns the path of the settings folder, creating it if needed. */
const char *storage_dir(void) { ensure_dir(); return g_dir; }

/*
 * Builds the full path of a file inside the settings folder, for example
 * settings.cfg.
 */
static void path_of(char *out, size_t n, const char *file)
{
    ensure_dir();
    snprintf(out, n, "%s/%s", g_dir, file);
}

/* ---- settings ----------------------------------------------------------- */

void settings_defaults(Settings *s)
{
    s->fullscreen = 0;
    s->scale = 3;
    s->volume = 8;
    s->crt = 0;
    s->scheme = SCHEME_MODERN;
    s->difficulty = 1;
    s->adaptive = 1;
    s->music = 7;
    input_default_bindings(s->bind);
}

static const char *k_act_key[ACT_COUNT] = {
    "up", "down", "left", "right", "thrust", "reverse", "fire", "bomb", "hyper", "pause"
};

/*
 * Writes settings.cfg as plain text, one name=value per line, so it is easy to
 * read and fix by hand. Returns false if the file could not be written, so the
 * caller can show a dialog.
 */
bool settings_save(void)
{
    char p[600];
    path_of(p, sizeof p, "settings.cfg");
    FILE *f = fopen(p, "w");
    if (!f) return false;
    fprintf(f, "# %s settings\n", GAME_TITLE);
    fprintf(f, "fullscreen=%d\nscale=%d\nvolume=%d\ncrt=%d\nscheme=%d\n",
            g_set.fullscreen, g_set.scale, g_set.volume, g_set.crt, g_set.scheme);
    fprintf(f, "difficulty=%d\nadaptive=%d\nmusic=%d\n", g_set.difficulty, g_set.adaptive, g_set.music);
    for (int a = 0; a < ACT_COUNT; a++)
        fprintf(f, "bind.%s=%d,%d,%d\n", k_act_key[a],
                (int)g_set.bind[a].key[0], (int)g_set.bind[a].key[1], g_set.bind[a].pad);
    return fclose(f) == 0;
}

/* Keeps a whole number between lo and hi. */
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/*
 * Reads settings.cfg. It starts from the defaults and then replaces only the
 * values it understands, so a missing or damaged file just gives the defaults.
 * Every number is clamped to a safe range.
 */
void settings_load(void)
{
    settings_defaults(&g_set);
    char p[600];
    path_of(p, sizeof p, "settings.cfg");
    FILE *f = fopen(p, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (line[0] == '#' || !eq) continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        if      (!strcmp(k, "fullscreen")) g_set.fullscreen = atoi(v) ? 1 : 0;
        else if (!strcmp(k, "scale"))      g_set.scale = clampi(atoi(v), 1, 8);
        else if (!strcmp(k, "volume"))     g_set.volume = clampi(atoi(v), 0, 10);
        else if (!strcmp(k, "crt"))        g_set.crt = atoi(v) ? 1 : 0;
        else if (!strcmp(k, "scheme"))     g_set.scheme = clampi(atoi(v), 0, 1);
        else if (!strcmp(k, "difficulty")) g_set.difficulty = clampi(atoi(v), 0, 3);
        else if (!strcmp(k, "adaptive"))   g_set.adaptive = atoi(v) ? 1 : 0;
        else if (!strcmp(k, "music"))      g_set.music = clampi(atoi(v), 0, 10);
        else if (!strncmp(k, "bind.", 5)) {
            for (int a = 0; a < ACT_COUNT; a++) {
                if (strcmp(k + 5, k_act_key[a])) continue;
                int k0, k1, pd;
                if (sscanf(v, "%d,%d,%d", &k0, &k1, &pd) == 3) {
                    /* ignore nonsense so a damaged file cannot break input */
                    if (k0 >= 0 && k0 < SDL_NUM_SCANCODES) g_set.bind[a].key[0] = (SDL_Scancode)k0;
                    if (k1 >= 0 && k1 < SDL_NUM_SCANCODES) g_set.bind[a].key[1] = (SDL_Scancode)k1;
                    if (pd >= -1 && pd < SDL_CONTROLLER_BUTTON_MAX) g_set.bind[a].pad = pd;
                }
            }
        }
    }
    fclose(f);
}

/* ---- high scores -------------------------------------------------------- */

void scores_defaults(void)
{
    static const char *names[SCORE_SLOTS] = { "SHO","OTE","RUP","PER","ACE","ZAP","BIT","POW" };
    for (int i = 0; i < SCORE_SLOTS; i++) {
        snprintf(g_scores[i].name, 4, "%s", names[i]);
        g_scores[i].score = (uint32_t)(40000 - i * 4500);
        g_scores[i].wave = 6 - i / 2;
    }
}

/*
 * Writes scores.txt: one line per place with the name, the score and the wave.
 * Returns false on failure.
 */
bool scores_save(void)
{
    char p[600];
    path_of(p, sizeof p, "scores.txt");
    FILE *f = fopen(p, "w");
    if (!f) return false;
    for (int i = 0; i < SCORE_SLOTS; i++)
        fprintf(f, "%s %u %d\n", g_scores[i].name, g_scores[i].score, g_scores[i].wave);
    return fclose(f) == 0;
}

/*
 * Reads scores.txt over the defaults. If the file is short or damaged it stops
 * at the first bad line and the rest stay as defaults. Names are cut to 3
 * letters.
 */
void scores_load(void)
{
    scores_defaults();
    char p[600];
    path_of(p, sizeof p, "scores.txt");
    FILE *f = fopen(p, "r");
    if (!f) return;
    for (int i = 0; i < SCORE_SLOTS; i++) {
        char name[16]; unsigned sc; int wv;
        if (fscanf(f, "%15s %u %d", name, &sc, &wv) != 3) break;
        snprintf(g_scores[i].name, sizeof g_scores[i].name, "%.3s", name);
        g_scores[i].score = sc;
        g_scores[i].wave = wv;
    }
    fclose(f);
}

/*
 * Tells which place a score would get in the table (0 is first), or -1 if it
 * does not make the list. A score of 0 never does.
 */
int scores_rank(uint32_t score)
{
    if (score == 0) return -1;
    for (int i = 0; i < SCORE_SLOTS; i++)
        if (score > g_scores[i].score) return i;
    return -1;
}

/*
 * Puts a new score at the given place and moves everyone below it down one
 * step. The last entry falls off the list.
 */
void scores_insert(int rank, const char *name, uint32_t score, int wave)
{
    if (rank < 0 || rank >= SCORE_SLOTS) return;
    for (int i = SCORE_SLOTS - 1; i > rank; i--) g_scores[i] = g_scores[i - 1];
    snprintf(g_scores[rank].name, 4, "%s", name);
    g_scores[rank].score = score;
    g_scores[rank].wave = wave;
}

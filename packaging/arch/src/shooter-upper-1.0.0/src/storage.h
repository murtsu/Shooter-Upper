/*
 * storage.h - settings and high scores, saved in the user's config folder
 * (~/.config/shooter-upper on Linux, honoring XDG_CONFIG_HOME).
 */
#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "input.h"

typedef struct {
    int fullscreen;       /* 0 / 1                         */
    int scale;            /* window size multiplier 1..8   */
    int volume;           /* 0..10                         */
    int crt;              /* 0 / 1 scanline effect         */
    int scheme;           /* Scheme                        */
    int difficulty;       /* Difficulty, 0..3              */
    int adaptive;         /* Senseability adapts the game  */
    int music;            /* music volume 0..10            */
    Binding bind[ACT_COUNT];
} Settings;

#define SCORE_SLOTS 8
typedef struct { char name[4]; uint32_t score; int wave; } ScoreEntry;

extern Settings   g_set;
extern ScoreEntry g_scores[SCORE_SLOTS];

void settings_defaults(Settings *s);
void settings_load(void);
bool settings_save(void);              /* false when the file could not be written */
const char *storage_dir(void);        /* the folder in use */

void scores_defaults(void);
void scores_load(void);
bool scores_save(void);
int  scores_rank(uint32_t score);                       /* 0..7 or -1       */
void scores_insert(int rank, const char *name, uint32_t score, int wave);

/* for tests: override the folder */
void storage_set_dir(const char *dir);

#endif

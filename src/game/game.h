/*
 * game.h - the pure game simulation.
 *
 * IMPORTANT RULE: nothing in src/game/ may include SDL or touch the screen,
 * the sound card or the keyboard. The simulation takes a small Controls
 * struct in and gives a list of Events out. That is why it can be tested
 * without a window, and why it behaves identically on every machine.
 */
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "../config.h"

#define MAX_ENEMIES  96
#define MAX_BULLETS  48
#define MAX_MINES    32
#define MAX_EVENTS   96

typedef enum {
    E_NONE = 0, E_LANDER, E_MUTANT, E_BAITER, E_BOMBER, E_POD, E_SWARMER
} EnemyType;

/* Lander sub-states */
enum { L_SEEK = 0, L_DESCEND = 1, L_ABDUCT = 2 };

typedef enum {
    H_STAND = 0,    /* standing on the ground                      */
    H_GRABBED,      /* being carried up by a Lander                */
    H_FALLING,      /* dropped, waiting to be caught or to land    */
    H_CARRIED,      /* riding on the player's ship                 */
    H_DEAD
} HumState;

typedef struct {
    bool  alive;
    EnemyType type;
    float x, y, vx, vy;
    int   state;     /* lander sub-state                       */
    int   timer;
    int   shoot;     /* frames until next shot                 */
    int   target;    /* humanoid index a Lander is hunting     */
    float phase;     /* for wobbly movement                    */
} Enemy;

typedef struct {
    HumState state;
    float x, y, vy;
    float fall_from; /* height where the fall began            */
    int   carrier;   /* enemy index when GRABBED               */
} Humanoid;

typedef struct { bool active; float x, y, vx; int life; } Shot;
typedef struct { bool active; float x, y, vx, vy; int life; } Bullet;
typedef struct { bool active; float x, y; int life; } Mine;

typedef struct {
    float x, y, vx, vy;
    int   facing;      /* +1 right, -1 left                    */
    bool  alive;
    int   respawn;     /* frames until it comes back           */
    int   invuln;      /* frames of protection after respawn   */
    int   fire_cd;
    int   hyper;       /* >0 while hidden in hyperspace        */
    bool  hyper_dies;  /* this jump will go wrong              */
    bool  thrusting;   /* for the flame graphics               */
} Ship;

/* Things that happened this step, for sound and visual effects. */
typedef enum {
    EV_NONE = 0,
    EV_LASER, EV_ENEMY_SHOT,
    EV_EXPLODE_SMALL, EV_EXPLODE_BIG, EV_SMARTBOMB,
    EV_ABDUCT, EV_SCREAM, EV_CATCH, EV_LANDING, EV_HUMANOID_DIE,
    EV_MUTANT, EV_HYPER, EV_HYPER_BACK, EV_EXTRALIFE,
    EV_WAVE_START, EV_WAVE_CLEAR, EV_PLANET_BLOW,
    EV_SHIP_DEATH, EV_BAITER, EV_GAMEOVER, EV_RESPAWN, EV_MINE_DROP
} EventType;

typedef struct { int type; float x, y; int arg; } Event;

typedef enum { PH_PLAY = 0, PH_CLEAR, PH_OVER } Phase;

/* How hard the game is. The four grades are presets of the Tuning numbers. */
typedef enum { DIFF_EASY = 0, DIFF_NORMAL, DIFF_HARD, DIFF_INSANE, DIFF_COUNT } Difficulty;

typedef struct {
    float enemy_speed;   /* 1.0 = normal                                    */
    float fire_rate;     /* enemy shots per time, 1.0 = normal               */
    float bullet_speed;  /* speed of enemy bullets                           */
    float aim_error;     /* 1.0 = normal, higher = they miss more            */
    int   spare_ships;   /* extra ships at the start                         */
    int   bombs;         /* smart bombs at the start                         */
    float adapt;         /* Senseability: scales enemy speed, 1.0 = off */
} Tuning;

/* What the player (or the demo AI) is asking for this step. */
typedef struct {
    int up, down;        /* held                                        */
    int thrust;          /* held                                        */
    int face;            /* -1 / +1 sets facing directly, 0 = no change */
    int reverse;         /* one-step pulse: flip facing                 */
    int fire;            /* held                                        */
    int bomb;            /* pulse                                       */
    int hyper;           /* pulse                                       */
} Controls;

typedef struct {
    uint32_t rng;
    uint32_t frame;
    Phase    phase;
    int      phase_timer;
    int      wave;               /* 1-based                              */
    int      banner_timer;       /* "WAVE N" text                        */
    uint32_t score;
    uint32_t next_bonus;
    int      lives;              /* spare ships                          */
    int      bombs;
    bool     planet_alive;
    bool     finished;           /* game over screen has run its course  */
    int      clear_bonus;        /* shown on the wave clear screen       */
    int      clear_humanoids;

    int      wave_timer;         /* frames spent on this wave            */
    int      baiters_spawned;
    int      q_lander, q_bomber, q_pod;  /* still to appear              */
    int      spawn_cd;

    float    camx;               /* world x of the left screen edge      */
    bool     cam_snap;

    Tuning   tune;
    Difficulty diff;

    Ship     ship;
    Enemy    en[MAX_ENEMIES];
    Humanoid hum[HUMANOID_COUNT];
    Shot     shot[MAX_PLAYER_SHOTS];
    Bullet   eb[MAX_BULLETS];
    Mine     mine[MAX_MINES];

    Event    ev[MAX_EVENTS];
    int      nev;
} Game;

/* ---- API --------------------------------------------------------------- */
void  game_init(Game *g, uint32_t seed);
void  game_step(Game *g, const Controls *c);
void  game_set_difficulty(Game *g, Difficulty d);   /* call right after game_init */
const char *game_difficulty_name(Difficulty d);
void  game_ai(const Game *g, Controls *c);       /* autopilot for the demo */

/* world helpers (also used by the renderer) */
float wrapx(float x);                  /* fold into 0..WORLD_W            */
float wrapdx(float from, float to);    /* shortest signed distance        */
float terrain_y(float x);              /* ground height at a world x      */

int   game_enemy_count(const Game *g, bool include_baiters);
int   game_humanoids_alive(const Game *g);
int   game_carried(const Game *g);

#endif

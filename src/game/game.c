/*
 * game.c - the simulation. One call to game_step() advances the world by
 * exactly 1/60 of a second. No SDL in here, see game.h.
 */
#include "game.h"
#include <math.h>
#include <string.h>

#define TWO_PI 6.2831853f

/* ======================================================================== */
/* World helpers                                                            */
/* ======================================================================== */

float wrapx(float x)
{
    x = fmodf(x, (float)WORLD_W);
    if (x < 0.f) x += (float)WORLD_W;
    return x;
}

/* Shortest signed distance from a to b on the looping world.
 * Positive means b is to the right of a. */
float wrapdx(float from, float to)
{
    float d = fmodf(to - from, (float)WORLD_W);
    if (d >  WORLD_W * 0.5f) d -= WORLD_W;
    if (d < -WORLD_W * 0.5f) d += WORLD_W;
    return d;
}

/* The terrain is a sum of sine waves whose frequencies are whole numbers
 * per world width. That guarantees the left and right edges match, so the
 * planet loops without a seam. A lookup table keeps it cheap. */
static float g_terrain[WORLD_W];
static bool  g_terrain_ready = false;

static void terrain_build(void)
{
    for (int i = 0; i < WORLD_W; i++) {
        float t = (float)i / (float)WORLD_W * TWO_PI;
        g_terrain[i] = 198.f
            + 8.f * sinf(t * 3.f)
            + 6.f * sinf(t * 7.f + 1.f)
            + 4.f * sinf(t * 17.f + 2.f)
            + 2.f * sinf(t * 41.f + 0.5f);
    }
    g_terrain_ready = true;
}

float terrain_y(float x)
{
    if (!g_terrain_ready) terrain_build();
    int i = (int)wrapx(x);
    if (i >= WORLD_W) i = WORLD_W - 1;
    return g_terrain[i];
}

/* ======================================================================== */
/* Random numbers: a tiny xorshift so every run with the same seed matches  */
/* ======================================================================== */

static uint32_t rnd(Game *g)
{
    uint32_t x = g->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng = x;
    return x;
}
static float rndf(Game *g) { return (float)(rnd(g) >> 8) * (1.0f / 16777216.0f); }
static int   rndi(Game *g, int lo, int hi) { return lo + (int)(rnd(g) % (uint32_t)(hi - lo + 1)); }

/* ======================================================================== */
/* Small helpers                                                            */
/* ======================================================================== */

static void ev(Game *g, int type, float x, float y, int arg)
{
    if (g->nev < MAX_EVENTS) {
        g->ev[g->nev].type = type;
        g->ev[g->nev].x = x;
        g->ev[g->nev].y = y;
        g->ev[g->nev].arg = arg;
        g->nev++;
    }
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float signf(float v) { return v < 0.f ? -1.f : 1.f; }

static void add_score(Game *g, int pts)
{
    g->score += (uint32_t)pts;
    while (g->score >= g->next_bonus) {
        g->next_bonus += BONUS_EVERY;
        if (g->lives < MAX_SPARE_SHIPS) g->lives++;
        if (g->bombs < MAX_BOMBS) g->bombs++;
        ev(g, EV_EXTRALIFE, g->ship.x, g->ship.y, 0);
    }
}

int game_humanoids_alive(const Game *g)
{
    int n = 0;
    for (int i = 0; i < HUMANOID_COUNT; i++) if (g->hum[i].state != H_DEAD) n++;
    return n;
}

int game_carried(const Game *g)
{
    int n = 0;
    for (int i = 0; i < HUMANOID_COUNT; i++) if (g->hum[i].state == H_CARRIED) n++;
    return n;
}

int game_enemy_count(const Game *g, bool include_baiters)
{
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g->en[i].alive) continue;
        if (!include_baiters && g->en[i].type == E_BAITER) continue;
        n++;
    }
    return n;
}

/* Lowest y a flying thing may have: above the ground, or the screen
 * bottom once the planet is gone. */
static float floor_y(const Game *g, float x)
{
    return g->planet_alive ? terrain_y(x) - 6.f : (float)SCREEN_H - 6.f;
}

static float enemy_radius(EnemyType t)
{
    switch (t) {
    case E_LANDER:  return 7.f;
    case E_MUTANT:  return 6.f;
    case E_BAITER:  return 8.f;
    case E_BOMBER:  return 6.f;
    case E_POD:     return 7.f;
    case E_SWARMER: return 4.f;
    default:        return 5.f;
    }
}

static int enemy_points(EnemyType t)
{
    switch (t) {
    case E_LANDER:  return SCORE_LANDER;
    case E_MUTANT:  return SCORE_MUTANT;
    case E_BAITER:  return SCORE_BAITER;
    case E_BOMBER:  return SCORE_BOMBER;
    case E_POD:     return SCORE_POD;
    case E_SWARMER: return SCORE_SWARMER;
    default:        return 0;
    }
}

/* ======================================================================== */
/* Spawning                                                                 */
/* ======================================================================== */

static int spawn_enemy(Game *g, EnemyType t, float x, float y)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->en[i].alive) continue;
        Enemy *e = &g->en[i];
        memset(e, 0, sizeof *e);
        e->alive  = true;
        e->type   = t;
        e->x      = wrapx(x);
        e->y      = y;
        e->target = -1;
        e->shoot  = rndi(g, 60, 220);
        e->phase  = rndf(g) * TWO_PI;
        return i;
    }
    return -1;
}

/* Counts an enemy's shot timer down. The Tuning fire rate makes it tick
 * faster (hard) or slower (easy); the extra or skipped ticks are chosen with
 * the game's own random numbers, so replays stay identical. */
static bool shoot_tick(Game *g, Enemy *e)
{
    float r = g->tune.fire_rate;
    int dec = 1;
    if (r > 1.f) { if (rndf(g) < r - 1.f) dec = 2; }
    else if (rndf(g) > r) dec = 0;
    e->shoot -= dec;
    return e->shoot <= 0;
}

static void fire_bullet(Game *g, float x, float y, float speed, float aim_error)
{
    speed *= g->tune.bullet_speed;
    aim_error *= g->tune.aim_error;
    const Ship *s = &g->ship;
    if (!s->alive || s->hyper > 0) return;
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &g->eb[i];
        if (b->active) continue;
        float dx = wrapdx(x, s->x) + (rndf(g) - 0.5f) * aim_error;
        float dy = (s->y - y) + (rndf(g) - 0.5f) * aim_error;
        float d  = sqrtf(dx * dx + dy * dy);
        if (d < 1.f) d = 1.f;
        b->active = true;
        b->x = x; b->y = y;
        b->vx = dx / d * speed;
        b->vy = dy / d * speed;
        b->life = 200;
        ev(g, EV_ENEMY_SHOT, x, y, 0);
        return;
    }
}

/* ======================================================================== */
/* Humanoids and the planet                                                 */
/* ======================================================================== */

static void check_planet(Game *g)
{
    if (!g->planet_alive) return;
    if (game_humanoids_alive(g) > 0) return;
    g->planet_alive = false;
    ev(g, EV_PLANET_BLOW, g->camx + SCREEN_W * 0.5f, 200.f, 0);
    /* every Lander turns into a Mutant */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g->en[i];
        if (e->alive && e->type == E_LANDER) {
            e->type = E_MUTANT;
            e->state = 0;
            e->target = -1;
            ev(g, EV_MUTANT, e->x, e->y, 0);
        }
    }
}

static void humanoid_die(Game *g, int i)
{
    Humanoid *h = &g->hum[i];
    if (h->state == H_DEAD) return;
    h->state = H_DEAD;
    ev(g, EV_HUMANOID_DIE, h->x, h->y, 0);
    check_planet(g);
}

static void place_humanoids(Game *g)
{
    for (int i = 0; i < HUMANOID_COUNT; i++) {
        Humanoid *h = &g->hum[i];
        h->state   = H_STAND;
        h->x       = wrapx((float)i * (WORLD_W / (float)HUMANOID_COUNT) + rndf(g) * 140.f + 40.f);
        h->y       = terrain_y(h->x) - 5.f;
        h->vy      = 0.f;
        h->carrier = -1;
    }
}

/* ======================================================================== */
/* Killing things                                                           */
/* ======================================================================== */

static void release_humanoid(Game *g, const Enemy *e)
{
    if (e->type == E_LANDER && e->state == L_ABDUCT && e->target >= 0) {
        Humanoid *h = &g->hum[e->target];
        if (h->state == H_GRABBED) {
            h->state     = H_FALLING;
            h->vy        = 0.f;
            h->fall_from = h->y;
            h->carrier   = -1;
            ev(g, EV_SCREAM, h->x, h->y, 0);
        }
    }
}

static void kill_enemy(Game *g, int i)
{
    Enemy *e = &g->en[i];
    if (!e->alive) return;
    EnemyType t = e->type;
    float x = e->x, y = e->y;
    release_humanoid(g, e);
    e->alive = false;
    add_score(g, enemy_points(t));
    ev(g, (t == E_POD || t == E_BOMBER) ? EV_EXPLODE_BIG : EV_EXPLODE_SMALL, x, y, (int)t);

    if (t == E_POD) {                       /* a Pod bursts into Swarmers */
        int n = rndi(g, 4, 6);
        for (int k = 0; k < n; k++) {
            int s = spawn_enemy(g, E_SWARMER, x + (rndf(g) - 0.5f) * 16.f, y + (rndf(g) - 0.5f) * 16.f);
            if (s >= 0) {
                g->en[s].vx = (rndf(g) - 0.5f) * 3.f;
                g->en[s].vy = (rndf(g) - 0.5f) * 3.f;
                g->en[s].shoot = rndi(g, 50, 120);
            }
        }
    }
}

static void ship_die(Game *g)
{
    Ship *s = &g->ship;
    if (!s->alive) return;
    s->alive = false;
    s->thrusting = false;
    ev(g, EV_SHIP_DEATH, s->x, s->y, 0);
    for (int i = 0; i < HUMANOID_COUNT; i++) {          /* passengers fall  */
        Humanoid *h = &g->hum[i];
        if (h->state == H_CARRIED) {
            h->state = H_FALLING;
            h->vy = 0.f;
            h->fall_from = h->y;
        }
    }
    if (g->lives > 0) {
        g->lives--;
        s->respawn = RESPAWN_FRAMES;
    } else {
        s->respawn = -1;
        g->phase = PH_OVER;
        g->phase_timer = 240;
        ev(g, EV_GAMEOVER, s->x, s->y, 0);
    }
}

static bool on_screen(const Game *g, float x)
{
    return fabsf(wrapdx(g->camx + SCREEN_W * 0.5f, x)) < SCREEN_W * 0.5f + 8.f;
}

static void smart_bomb(Game *g)
{
    ev(g, EV_SMARTBOMB, g->ship.x, g->ship.y, 0);
    /* two passes: Pods release Swarmers in pass one, pass two catches them */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (g->en[i].alive && on_screen(g, g->en[i].x)) kill_enemy(g, i);
        }
    }
    for (int i = 0; i < MAX_BULLETS; i++)
        if (g->eb[i].active && on_screen(g, g->eb[i].x)) g->eb[i].active = false;
    for (int i = 0; i < MAX_MINES; i++)
        if (g->mine[i].active && on_screen(g, g->mine[i].x)) g->mine[i].active = false;
}

/* ======================================================================== */
/* Waves                                                                    */
/* ======================================================================== */

static void start_wave(Game *g)
{
    int w = g->wave;
    g->q_lander = (w == 1) ? 15 : 20;
    g->q_bomber = (w < 2) ? 0 : (w == 2 ? 3 : (w == 3 ? 4 : 5));
    g->q_pod    = (w < 2) ? 0 : (w == 2 ? 1 : (w == 3 ? 3 : 4));
    g->wave_timer = 0;
    g->baiters_spawned = 0;
    g->spawn_cd = 0;
    g->banner_timer = 150;
    g->phase = PH_PLAY;

    if (w % RESTORE_EVERY == 0) {            /* the planet comes back */
        g->planet_alive = true;
        place_humanoids(g);
    }
    for (int i = 0; i < MAX_BULLETS; i++) g->eb[i].active = false;
    for (int i = 0; i < MAX_MINES; i++)   g->mine[i].active = false;
    ev(g, EV_WAVE_START, 0, 0, w);
}

static void spawn_batch(Game *g)
{
    int landers = g->q_lander < 5 ? g->q_lander : 5;
    for (int k = 0; k < landers; k++) {
        float x = g->ship.x + (rndf(g) < 0.5f ? -1.f : 1.f) * (200.f + rndf(g) * 900.f);
        int i = spawn_enemy(g, E_LANDER, x, PLAY_TOP + 4.f);
        if (i >= 0) g->q_lander--;
    }
    if (g->q_bomber > 0) {
        float x = g->ship.x + (rndf(g) < 0.5f ? -1.f : 1.f) * (260.f + rndf(g) * 600.f);
        int i = spawn_enemy(g, E_BOMBER, x, 70.f + rndf(g) * 70.f);
        if (i >= 0) { g->en[i].vx = (rndf(g) < 0.5f ? -0.7f : 0.7f); g->q_bomber--; }
    }
    if (g->q_pod > 0) {
        float x = g->ship.x + (rndf(g) < 0.5f ? -1.f : 1.f) * (260.f + rndf(g) * 600.f);
        int i = spawn_enemy(g, E_POD, x, 60.f + rndf(g) * 90.f);
        if (i >= 0) { g->en[i].vx = (rndf(g) - 0.5f) * 0.8f; g->q_pod--; }
    }
}

/* ======================================================================== */
/* Init                                                                     */
/* ======================================================================== */

static const Tuning PRESETS[DIFF_COUNT] = {
    /* speed fire  bullet aim  ships bombs adapt */
    { 0.85f, 0.80f, 0.90f, 1.40f, 3, 4, 1.f },   /* easy   */
    { 1.00f, 1.00f, 1.00f, 1.00f, 2, 3, 1.f },   /* normal */
    { 1.15f, 1.10f, 1.10f, 0.80f, 2, 2, 1.f },   /* hard   */
    { 1.30f, 1.30f, 1.20f, 0.60f, 1, 1, 1.f },   /* insane */
};

const char *game_difficulty_name(Difficulty d)
{
    static const char *n[DIFF_COUNT] = { "EASY", "NORMAL", "HARD", "INSANE" };
    return (d >= 0 && d < DIFF_COUNT) ? n[d] : "?";
}

void game_set_difficulty(Game *g, Difficulty d)
{
    if (d < 0 || d >= DIFF_COUNT) d = DIFF_NORMAL;
    g->diff = d;
    g->tune = PRESETS[d];
    g->lives = g->tune.spare_ships;
    g->bombs = g->tune.bombs;
}

void game_init(Game *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->rng = seed ? seed : 0x9E3779B9u;
    g->wave = 1;
    g->diff = DIFF_NORMAL;
    g->tune = (Tuning){ 1.f, 1.f, 1.f, 1.f, START_SPARE_SHIPS, START_BOMBS, 1.f };
    g->lives = START_SPARE_SHIPS;
    g->bombs = START_BOMBS;
    g->next_bonus = BONUS_EVERY;
    g->planet_alive = true;
    g->ship.x = 400.f;
    g->ship.y = 120.f;
    g->ship.facing = 1;
    g->ship.alive = true;
    g->ship.invuln = 90;
    g->camx = wrapx(g->ship.x - 100.f);
    place_humanoids(g);
    start_wave(g);
}

/* ======================================================================== */
/* Ship                                                                     */
/* ======================================================================== */

static void update_ship(Game *g, const Controls *c)
{
    Ship *s = &g->ship;

    if (!s->alive) {
        if (s->respawn > 0 && --s->respawn == 0) {
            s->alive = true;
            s->vx = 0.f; s->vy = 0.f;
            s->y = 110.f;
            s->invuln = INVULN_FRAMES;
            s->hyper = 0;
            g->cam_snap = true;
            ev(g, EV_RESPAWN, s->x, s->y, 0);
        }
        return;
    }
    if (s->invuln > 0) s->invuln--;
    if (s->fire_cd > 0) s->fire_cd--;

    /* hidden in hyperspace */
    if (s->hyper > 0) {
        s->hyper--;
        if (s->hyper == 0) {
            if (s->hyper_dies) {
                s->hyper_dies = false;
                ship_die(g);
            } else {
                ev(g, EV_HYPER_BACK, s->x, s->y, 0);
            }
        }
        return;
    }

    /* facing */
    if (c->face != 0)       s->facing = c->face > 0 ? 1 : -1;
    else if (c->reverse)    s->facing = -s->facing;   /* momentum stays */

    /* thrust and drag: this pair of numbers is the heart of the feel */
    s->thrusting = c->thrust != 0;
    if (s->thrusting) {
        s->vx += (float)s->facing * SHIP_THRUST;
        s->vx *= SHIP_DRAG_THRUST;
    } else {
        s->vx *= SHIP_DRAG_COAST;
    }
    s->x = wrapx(s->x + s->vx);

    /* vertical: follows the stick with a little smoothing */
    float target = ((float)c->down - (float)c->up) * SHIP_VSPEED;
    s->vy += (target - s->vy) * SHIP_VSMOOTH;
    s->y += s->vy;
    float lo = (float)PLAY_TOP + 8.f;
    float hi = floor_y(g, s->x);
    s->y = clampf(s->y, lo, hi);

    /* laser */
    if (c->fire && s->fire_cd == 0) {
        for (int i = 0; i < MAX_PLAYER_SHOTS; i++) {
            Shot *sh = &g->shot[i];
            if (sh->active) continue;
            sh->active = true;
            sh->x  = wrapx(s->x + (float)s->facing * 10.f);
            sh->y  = s->y + 1.f;
            sh->vx = (float)s->facing * LASER_SPEED + s->vx;
            sh->life = LASER_LIFE;
            s->fire_cd = LASER_COOLDOWN;
            ev(g, EV_LASER, sh->x, sh->y, 0);
            break;
        }
    }

    /* smart bomb */
    if (c->bomb && g->bombs > 0) {
        g->bombs--;
        smart_bomb(g);
    }

    /* hyperspace */
    if (c->hyper) {
        s->hyper = HYPER_HIDDEN;
        s->hyper_dies = (rndi(g, 1, HYPER_DEATH_ODDS) == 1);
        ev(g, EV_HYPER, s->x, s->y, 0);
        s->x = rndf(g) * WORLD_W;
        s->y = PLAY_TOP + 20.f + rndf(g) * 100.f;
        s->vx = 0.f;
        g->cam_snap = true;
        /* passengers stay with the ship */
    }

    /* passengers ride below the ship; landing drops them */
    int n = game_carried(g), k = 0;
    bool low = g->planet_alive && s->y >= terrain_y(s->x) - 20.f;
    for (int i = 0; i < HUMANOID_COUNT; i++) {
        Humanoid *h = &g->hum[i];
        if (h->state != H_CARRIED) continue;
        if (low) {
            h->state = H_STAND;
            h->x = wrapx(s->x + ((float)k - (float)(n - 1) * 0.5f) * 8.f);
            h->y = terrain_y(h->x) - 5.f;
            add_score(g, SCORE_LANDING);
            ev(g, EV_LANDING, h->x, h->y, 0);
        } else {
            h->x = wrapx(s->x + ((float)k - (float)(n - 1) * 0.5f) * 7.f);
            h->y = s->y + 9.f;
        }
        k++;
    }
}

/* ======================================================================== */
/* Shots                                                                    */
/* ======================================================================== */

static void update_shots(Game *g)
{
    for (int i = 0; i < MAX_PLAYER_SHOTS; i++) {
        Shot *sh = &g->shot[i];
        if (!sh->active) continue;
        float x0 = sh->x;
        sh->x = wrapx(sh->x + sh->vx);
        if (--sh->life <= 0) { sh->active = false; continue; }

        /* swept test: the laser covers everything it passed through */
        for (int j = 0; j < MAX_ENEMIES; j++) {
            Enemy *e = &g->en[j];
            if (!e->alive) continue;
            float er = enemy_radius(e->type);
            float d0 = wrapdx(x0, e->x);
            float lo = (sh->vx < 0 ? sh->vx : 0.f) - er;
            float hi = (sh->vx > 0 ? sh->vx : 0.f) + er;
            if (d0 >= lo && d0 <= hi && fabsf(e->y - sh->y) < er + 1.5f) {
                kill_enemy(g, j);
                sh->active = false;
                break;
            }
        }
    }
}

/* ======================================================================== */
/* Enemies                                                                  */
/* ======================================================================== */

static int pick_target(Game *g, const Enemy *e)
{
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < HUMANOID_COUNT; i++) {
        if (g->hum[i].state != H_STAND) continue;
        /* skip humanoids another Lander already hunts */
        bool taken = false;
        for (int j = 0; j < MAX_ENEMIES; j++) {
            const Enemy *o = &g->en[j];
            if (o != e && o->alive && o->type == E_LANDER && o->target == i) { taken = true; break; }
        }
        if (taken) continue;
        float d = fabsf(wrapdx(e->x, g->hum[i].x));
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void update_lander(Game *g, int idx, float ws)
{
    Enemy *e = &g->en[idx];
    float spd = 0.55f * ws;

    if (!g->planet_alive) {            /* no one left to take */
        e->type = E_MUTANT; e->state = 0; e->target = -1;
        return;
    }

    if (e->state == L_SEEK) {
        if (e->target < 0 || g->hum[e->target].state != H_STAND)
            e->target = pick_target(g, e);
        if (e->target < 0) {                       /* nothing to hunt: drift */
            e->vx = sinf(e->phase) * spd;
            e->vy = 0.f;
        } else {
            float dx = wrapdx(e->x, g->hum[e->target].x);
            e->vx = clampf(dx * 0.04f, -spd * 1.6f, spd * 1.6f);
            float hover = PLAY_TOP + 50.f;
            e->vy = clampf((hover - e->y) * 0.03f, -0.4f, 0.4f);
            if (fabsf(dx) < 5.f) e->state = L_DESCEND;
        }
    } else if (e->state == L_DESCEND) {
        if (e->target < 0 || g->hum[e->target].state != H_STAND) { e->state = L_SEEK; e->target = -1; }
        else {
            Humanoid *h = &g->hum[e->target];
            float dx = wrapdx(e->x, h->x);
            e->vx = clampf(dx * 0.1f, -0.6f, 0.6f);
            e->vy = spd * 1.3f;
            if (e->y >= h->y - 12.f && fabsf(dx) < 8.f) {
                h->state = H_GRABBED;
                h->carrier = idx;
                e->state = L_ABDUCT;
                ev(g, EV_ABDUCT, e->x, e->y, 0);
            }
        }
    } else { /* L_ABDUCT */
        Humanoid *h = &g->hum[e->target];
        e->vx = 0.f;
        e->vy = -0.6f * ws;
        h->x = e->x;
        h->y = e->y + 12.f;
        if (e->y <= PLAY_TOP + 4.f) {              /* made it: Mutant! */
            h->state = H_DEAD;
            e->type = E_MUTANT;
            e->state = 0;
            e->target = -1;
            ev(g, EV_HUMANOID_DIE, h->x, h->y, 1);
            ev(g, EV_MUTANT, e->x, e->y, 0);
            check_planet(g);
        }
    }

    if (shoot_tick(g, e)) {
        e->shoot = (int)((float)rndi(g, 100, 280) / ws);
        if (on_screen(g, e->x) && e->state != L_ABDUCT) fire_bullet(g, e->x, e->y, 1.5f, 70.f);
    }
}

static void update_mutant(Game *g, Enemy *e, float ws)
{
    const Ship *s = &g->ship;
    float tvx = 0.f, tvy = 0.f;
    float ms = 1.7f * ws;
    if (s->alive && s->hyper == 0) {
        float dx = wrapdx(e->x, s->x), dy = s->y - e->y;
        float d = sqrtf(dx * dx + dy * dy); if (d < 1.f) d = 1.f;
        tvx = dx / d * ms;
        tvy = dy / d * ms + sinf(e->phase * 3.f) * 1.1f;
    } else {
        tvx = sinf(e->phase) * 1.0f;
        tvy = cosf(e->phase * 1.3f) * 1.0f;
    }
    e->vx += (tvx - e->vx) * 0.07f;
    e->vy += (tvy - e->vy) * 0.07f;

    if (shoot_tick(g, e)) {
        e->shoot = (int)((float)rndi(g, 70, 190) / ws);
        if (on_screen(g, e->x)) fire_bullet(g, e->x, e->y, 2.0f, 50.f);
    }
}

static void update_baiter(Game *g, Enemy *e, float ws)
{
    const Ship *s = &g->ship;
    float top = 3.4f + 0.03f * ws;
    if (s->alive && s->hyper == 0) {
        float dx = wrapdx(e->x, s->x);
        e->vx += clampf(dx * 0.02f, -0.25f, 0.25f);
        e->vx = clampf(e->vx, -top, top);
        e->vy += clampf((s->y - e->y) * 0.03f, -0.2f, 0.2f);
        e->vy = clampf(e->vy, -2.2f, 2.2f);
    }
    if (shoot_tick(g, e)) {
        e->shoot = rndi(g, 45, 100);
        if (on_screen(g, e->x)) fire_bullet(g, e->x, e->y, 2.6f, 25.f);
    }
}

static void update_bomber(Game *g, Enemy *e)
{
    e->vy = sinf(e->phase * 1.5f) * 0.7f;
    if (e->vx == 0.f) e->vx = 0.7f;
    if (--e->timer <= 0) {
        e->timer = rndi(g, 35, 70);
        for (int i = 0; i < MAX_MINES; i++) {
            Mine *m = &g->mine[i];
            if (m->active) continue;
            m->active = true; m->x = e->x; m->y = e->y; m->life = 900;
            ev(g, EV_MINE_DROP, e->x, e->y, 0);
            break;
        }
    }
}

static void update_pod(Enemy *e)
{
    e->vy = sinf(e->phase) * 0.5f;
    if (e->vx == 0.f) e->vx = 0.3f;
}

static void update_swarmer(Game *g, Enemy *e, float ws)
{
    const Ship *s = &g->ship;
    if (s->alive && s->hyper == 0) {
        float dx = wrapdx(e->x, s->x);
        e->vx += signf(dx) * 0.06f;
        e->vx = clampf(e->vx, -2.6f * ws, 2.6f * ws);
        e->vy += clampf((s->y - e->y) * 0.01f, -0.1f, 0.1f);
        e->vy = clampf(e->vy, -1.4f, 1.4f) + sinf(e->phase * 4.f) * 0.05f;
    }
    if (shoot_tick(g, e)) {
        e->shoot = rndi(g, 60, 150);
        if (on_screen(g, e->x) && fabsf(s->y - e->y) < 40.f) fire_bullet(g, e->x, e->y, 2.4f, 20.f);
    }
}

static void update_enemies(Game *g)
{
    int wv = g->wave > 16 ? 15 : g->wave - 1;
    float ws = (1.f + 0.045f * (float)wv) * g->tune.enemy_speed * g->tune.adapt;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g->en[i];
        if (!e->alive) continue;
        e->phase += 0.05f;

        switch (e->type) {
        case E_LANDER:  update_lander(g, i, ws); break;
        case E_MUTANT:  update_mutant(g, e, ws); break;
        case E_BAITER:  update_baiter(g, e, ws); break;
        case E_BOMBER:  update_bomber(g, e);     break;
        case E_POD:     update_pod(e);           break;
        case E_SWARMER: update_swarmer(g, e, ws);break;
        default: break;
        }
        /* update_lander may have turned it into a Mutant: that is fine */

        e->x = wrapx(e->x + e->vx);
        e->y += e->vy;

        /* keep inside the play area */
        float lo = (float)PLAY_TOP + 3.f;
        float hi = floor_y(g, e->x);
        if (e->type != E_LANDER || e->state != L_DESCEND) {
            if (e->y < lo) { e->y = lo; if (e->vy < 0.f) e->vy = 0.f; }
            if (e->y > hi) { e->y = hi; if (e->vy > 0.f) e->vy = 0.f; }
        }
    }
}

/* ======================================================================== */
/* Bullets, mines, humanoids                                                */
/* ======================================================================== */

static void update_hazards(Game *g)
{
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &g->eb[i];
        if (!b->active) continue;
        b->x = wrapx(b->x + b->vx);
        b->y += b->vy;
        if (--b->life <= 0 || b->y < PLAY_TOP - 4 || b->y > SCREEN_H) b->active = false;
    }
    for (int i = 0; i < MAX_MINES; i++) {
        Mine *m = &g->mine[i];
        if (m->active && --m->life <= 0) m->active = false;
    }
}

static void update_humanoids(Game *g)
{
    const Ship *s = &g->ship;
    for (int i = 0; i < HUMANOID_COUNT; i++) {
        Humanoid *h = &g->hum[i];
        if (h->state == H_GRABBED) {
            /* its carrier moves it; if the carrier vanished, drop */
            if (h->carrier < 0 || !g->en[h->carrier].alive || g->en[h->carrier].state != L_ABDUCT) {
                h->state = H_FALLING; h->vy = 0.f; h->fall_from = h->y;
            }
        }
        if (h->state != H_FALLING) continue;

        h->vy += 0.045f;
        if (h->vy > 2.6f) h->vy = 2.6f;
        h->y += h->vy;

        /* caught by the ship? */
        if (s->alive && s->hyper == 0 &&
            fabsf(wrapdx(s->x, h->x)) < 10.f && fabsf(h->y - s->y) < 11.f) {
            h->state = H_CARRIED;
            add_score(g, SCORE_CATCH);
            ev(g, EV_CATCH, h->x, h->y, 0);
            continue;
        }

        float ground = terrain_y(h->x) - 5.f;
        if (!g->planet_alive || h->y >= ground) {
            float fell = h->y - h->fall_from;
            if (!g->planet_alive || fell > FALL_SAFE_PIXELS) {
                humanoid_die(g, i);
            } else {
                h->state = H_STAND;
                h->y = ground;
                add_score(g, SCORE_SAVED);
                ev(g, EV_LANDING, h->x, h->y, 1);
            }
        }
    }
}

/* ======================================================================== */
/* Collisions with the ship                                                 */
/* ======================================================================== */

static void check_ship_hits(Game *g)
{
    Ship *s = &g->ship;
    if (!s->alive || s->hyper > 0 || s->invuln > 0) return;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g->en[i];
        if (!e->alive) continue;
        float er = enemy_radius(e->type);
        if (fabsf(wrapdx(s->x, e->x)) < er + 7.f && fabsf(e->y - s->y) < er + 3.5f) {
            kill_enemy(g, i);
            ship_die(g);
            return;
        }
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &g->eb[i];
        if (b->active && fabsf(wrapdx(s->x, b->x)) < 8.f && fabsf(b->y - s->y) < 5.f) {
            b->active = false;
            ship_die(g);
            return;
        }
    }
    for (int i = 0; i < MAX_MINES; i++) {
        Mine *m = &g->mine[i];
        if (m->active && fabsf(wrapdx(s->x, m->x)) < 8.f && fabsf(m->y - s->y) < 6.f) {
            m->active = false;
            ship_die(g);
            return;
        }
    }
}

/* ======================================================================== */
/* Camera                                                                   */
/* ======================================================================== */

static void update_camera(Game *g)
{
    const Ship *s = &g->ship;
    /* the ship sits toward the back of the screen so you see ahead */
    float want_sx = s->facing > 0 ? 105.f : (float)SCREEN_W - 105.f;
    float target = wrapx(s->x - want_sx);
    if (g->cam_snap) {
        g->camx = target;
        g->cam_snap = false;
    } else {
        g->camx = wrapx(g->camx + wrapdx(g->camx, target) * 0.075f);
    }
}

/* ======================================================================== */
/* The step                                                                 */
/* ======================================================================== */

static void spawn_baiters(Game *g)
{
    int after = BAITER_AFTER_FRAMES - 60 * (g->wave > 10 ? 10 : g->wave);
    if (after < 1500) after = 1500;
    if (g->wave_timer < after) return;
    if ((g->wave_timer - after) % BAITER_EVERY_FRAMES != 0) return;
    int alive = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) if (g->en[i].alive && g->en[i].type == E_BAITER) alive++;
    if (alive >= 5) return;
    float side = (rndf(g) < 0.5f) ? -1.f : 1.f;
    int i = spawn_enemy(g, E_BAITER, g->ship.x + side * 380.f, g->ship.y);
    if (i >= 0) { g->baiters_spawned++; ev(g, EV_BAITER, g->en[i].x, g->en[i].y, 0); }
}

static void clear_baiters_and_hazards(Game *g)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g->en[i];
        if (e->alive && e->type == E_BAITER) {
            ev(g, EV_EXPLODE_SMALL, e->x, e->y, (int)E_BAITER);
            e->alive = false;
        }
    }
    for (int i = 0; i < MAX_BULLETS; i++) g->eb[i].active = false;
    for (int i = 0; i < MAX_MINES; i++)   g->mine[i].active = false;
}

void game_step(Game *g, const Controls *c)
{
    g->nev = 0;
    g->frame++;
    if (g->banner_timer > 0) g->banner_timer--;

    update_ship(g, c);
    update_shots(g);
    update_enemies(g);
    update_hazards(g);
    update_humanoids(g);
    check_ship_hits(g);

    if (g->phase == PH_PLAY) {
        g->wave_timer++;
        if (g->spawn_cd > 0) g->spawn_cd--;
        if (g->spawn_cd == 0 && (g->q_lander > 0 || g->q_bomber > 0 || g->q_pod > 0)) {
            spawn_batch(g);
            int cd = 260 - 10 * g->wave;
            g->spawn_cd = cd < 140 ? 140 : cd;
        }
        spawn_baiters(g);

        bool quota = g->q_lander + g->q_bomber + g->q_pod > 0;
        if (!quota && game_enemy_count(g, false) == 0 && g->ship.alive) {
            int alive = game_humanoids_alive(g);
            int mult = g->wave > 5 ? 5 : g->wave;
            g->clear_humanoids = alive;
            g->clear_bonus = alive * 100 * mult;
            add_score(g, g->clear_bonus);
            clear_baiters_and_hazards(g);
            g->phase = PH_CLEAR;
            g->phase_timer = 240;
            ev(g, EV_WAVE_CLEAR, 0, 0, g->wave);
        }
    } else if (g->phase == PH_CLEAR) {
        if (--g->phase_timer <= 0) {
            g->wave++;
            start_wave(g);
        }
    } else if (g->phase == PH_OVER) {
        if (--g->phase_timer <= 0) g->finished = true;
    }

    update_camera(g);
}

/* ======================================================================== */
/* Demo autopilot (attract mode)                                            */
/* ======================================================================== */

void game_ai(const Game *g, Controls *c)
{
    memset(c, 0, sizeof *c);
    const Ship *s = &g->ship;
    if (!s->alive || s->hyper > 0) return;

    int best = -1;
    float bd = 1e9f;
    int near = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &g->en[i];
        if (!e->alive) continue;
        float d = fabsf(wrapdx(s->x, e->x));
        if (d < 200.f) near++;
        float score = d;
        if (e->type == E_LANDER && e->state == L_ABDUCT) score -= 500.f;  /* rescue first */
        if (e->type == E_MUTANT || e->type == E_BAITER)  score -= 150.f;
        if (score < bd) { bd = score; best = i; }
    }

    if (game_carried(g) > 0 && g->planet_alive) {        /* take them home */
        c->down = 1;
        c->face = s->vx >= 0.f ? 1 : -1;
        return;
    }
    for (int i = 0; i < HUMANOID_COUNT; i++) {           /* catch falling ones */
        const Humanoid *h = &g->hum[i];
        if (h->state != H_FALLING) continue;
        float dx = wrapdx(s->x, h->x);
        c->face = dx >= 0.f ? 1 : -1;
        c->thrust = fabsf(dx) > 8.f;
        if (h->y > s->y + 3.f) c->down = 1; else if (h->y < s->y - 3.f) c->up = 1;
        return;
    }
    if (best < 0) { c->thrust = ((g->frame / 90) % 2) == 0; c->face = 1; return; }

    const Enemy *e = &g->en[best];
    float dx = wrapdx(s->x, e->x), dy = e->y - s->y;
    int want = dx >= 0.f ? 1 : -1;
    c->face = want;
    c->thrust = fabsf(dx) > 90.f || (s->vx * (float)want < 1.2f && fabsf(dx) > 30.f);
    if (dy > 4.f) c->down = 1; else if (dy < -4.f) c->up = 1;
    if (fabsf(dy) < 7.f && fabsf(dx) < 300.f) c->fire = 1;
    if (near >= 5 && g->bombs > 1 && (g->frame % 30) == 0) c->bomb = 1;
}

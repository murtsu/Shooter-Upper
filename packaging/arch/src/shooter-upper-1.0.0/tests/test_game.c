/*
 * test_game.c - headless tests of the simulation (no SDL, no window).
 * Build target: test_game (see CMakeLists.txt). Exit code 0 = all passed.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../src/game/game.h"

static int g_fail = 0, g_run = 0;
#define CHECK(cond) do { g_run++; if (!(cond)) { g_fail++; \
    printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static Controls none(void) { Controls c; memset(&c, 0, sizeof c); return c; }

static void clear_enemies(Game *g)
{
    for (int i = 0; i < MAX_ENEMIES; i++) g->en[i].alive = false;
    g->q_lander = g->q_bomber = g->q_pod = 0;
    g->spawn_cd = 99999;
}

static void test_wraparound(void)
{
    printf("world wraparound\n");
    CHECK(fabsf(wrapx(-10.f) - (WORLD_W - 10.f)) < 0.001f);
    CHECK(fabsf(wrapx(WORLD_W + 5.f) - 5.f) < 0.001f);
    /* shortest way from 10 to WORLD_W-10 is 20 to the left */
    CHECK(fabsf(wrapdx(10.f, WORLD_W - 10.f) + 20.f) < 0.001f);
    CHECK(fabsf(wrapdx(WORLD_W - 10.f, 10.f) - 20.f) < 0.001f);
    CHECK(fabsf(wrapdx(100.f, 160.f) - 60.f) < 0.001f);
    /* the terrain has no seam */
    CHECK(fabsf(terrain_y(0.f) - terrain_y((float)WORLD_W)) < 0.001f);
    CHECK(fabsf(terrain_y(WORLD_W - 1.f) - terrain_y(0.f)) < 1.5f);
    /* ship flying past the edge comes out the other side */
    Game g; game_init(&g, 1); clear_enemies(&g);
    g.ship.x = WORLD_W - 2.f; g.ship.vx = 5.f;
    Controls c = none();
    game_step(&g, &c);
    CHECK(g.ship.x < 20.f && g.ship.x >= 0.f);
}

static void test_ship_feel(void)
{
    printf("ship inertia\n");
    Game g; game_init(&g, 2); clear_enemies(&g);
    Controls c = none(); c.thrust = 1;
    for (int i = 0; i < 60; i++) game_step(&g, &c);
    float v = g.ship.vx;
    CHECK(v > 3.f && v < 8.f);
    c.thrust = 0;
    for (int i = 0; i < 10; i++) game_step(&g, &c);
    CHECK(g.ship.vx > 0.5f * v);          /* it drifts, it does not stop */
    float before = g.ship.vx;
    c.reverse = 1; game_step(&g, &c); c.reverse = 0;
    CHECK(g.ship.facing == -1);
    CHECK(g.ship.vx > 0.f && g.ship.vx <= before); /* momentum kept on reverse */
}

static void test_scoring(void)
{
    printf("scoring and extra lives\n");
    Game g; game_init(&g, 3); clear_enemies(&g);
    int lives = g.lives, bombs = g.bombs;
    g.score = 9900;
    /* put a lander right in front of the laser */
    int i = 0;
    g.en[i].alive = true; g.en[i].type = E_LANDER; g.en[i].target = -1;
    g.en[i].x = g.ship.x + 60.f; g.en[i].y = g.ship.y; g.en[i].vx = 0.f; g.en[i].vy = 0.f;
    g.en[i].shoot = 9999;
    /* a second enemy far away keeps the wave from ending (which would add
       the humanoid bonus and muddy this check) */
    g.en[5].alive = true; g.en[5].type = E_POD; g.en[5].shoot = 9999;
    g.en[5].x = wrapx(g.ship.x + 1200.f); g.en[5].y = 100.f;
    Controls c = none(); c.fire = 1;
    for (int k = 0; k < 20 && g.en[i].alive; k++) { g.en[i].y = g.ship.y; game_step(&g, &c); }
    CHECK(!g.en[i].alive);
    CHECK(g.score == 9900 + SCORE_LANDER);
    CHECK(g.lives == lives + 1);
    CHECK(g.bombs == bombs + 1);
    CHECK(g.next_bonus == 2 * BONUS_EVERY);
}

static void test_abduction(void)
{
    printf("abduction, rescue and landing\n");
    Game g; game_init(&g, 4); clear_enemies(&g);
    g.ship.invuln = 100000;
    /* lander right above humanoid 0 */
    Humanoid *h = &g.hum[0];
    int li = 0;
    Enemy *e = &g.en[li];
    e->alive = true; e->type = E_LANDER; e->state = L_SEEK; e->target = -1;
    e->x = h->x; e->y = PLAY_TOP + 50.f; e->shoot = 99999;
    /* keep the ship far away so it does not interfere */
    g.ship.x = wrapx(h->x + 1000.f);
    Controls c = none();
    bool grabbed = false;
    for (int k = 0; k < 900 && !grabbed; k++) {
        game_step(&g, &c);
        grabbed = (h->state == H_GRABBED);
    }
    CHECK(grabbed);
    CHECK(e->state == L_ABDUCT);

    /* let it rise a bit, then shoot it from beside */
    for (int k = 0; k < 60; k++) game_step(&g, &c);
    CHECK(h->state == H_GRABBED);
    g.ship.x = wrapx(e->x - 80.f); g.ship.y = e->y; g.ship.facing = 1; g.ship.vx = 0.f;
    c.fire = 1;
    for (int k = 0; k < 30 && e->alive; k++) { g.ship.y = e->y; game_step(&g, &c); }
    c.fire = 0;
    CHECK(!e->alive);
    CHECK(h->state == H_FALLING);

    /* fly under it and catch it */
    uint32_t score0 = g.score;
    for (int k = 0; k < 300 && h->state == H_FALLING; k++) {
        g.ship.x = h->x; g.ship.y = h->y - 4.f; g.ship.vx = 0.f; g.ship.vy = 0.f;
        game_step(&g, &c);
    }
    CHECK(h->state == H_CARRIED);
    CHECK(g.score >= score0 + SCORE_CATCH);

    /* take it to the ground */
    score0 = g.score;
    for (int k = 0; k < 400 && h->state == H_CARRIED; k++) {
        g.ship.y = terrain_y(g.ship.x) - 8.f;
        game_step(&g, &c);
    }
    CHECK(h->state == H_STAND);
    CHECK(g.score >= score0 + SCORE_LANDING);
}

static void test_fall_death(void)
{
    printf("a long fall kills a humanoid\n");
    Game g; game_init(&g, 5); clear_enemies(&g);
    g.ship.x = wrapx(g.hum[2].x + 1200.f);
    Humanoid *h = &g.hum[2];
    h->state = H_FALLING; h->y = PLAY_TOP + 10.f; h->vy = 0.f; h->fall_from = h->y;
    Controls c = none();
    for (int k = 0; k < 600 && h->state == H_FALLING; k++) game_step(&g, &c);
    CHECK(h->state == H_DEAD);
}

static void test_mutation_and_planet(void)
{
    printf("mutation and planet destruction\n");
    Game g; game_init(&g, 6); clear_enemies(&g);
    g.ship.invuln = 100000;
    g.ship.x = wrapx(g.hum[0].x + 1100.f);
    /* kill all but one humanoid */
    for (int i = 1; i < HUMANOID_COUNT; i++) g.hum[i].state = H_DEAD;
    CHECK(g.planet_alive);
    Humanoid *h = &g.hum[0];
    Enemy *e = &g.en[0];
    e->alive = true; e->type = E_LANDER; e->state = L_SEEK; e->target = -1;
    e->x = h->x; e->y = PLAY_TOP + 20.f; e->shoot = 99999;
    /* a second lander that will be turned into a mutant by the blast */
    Enemy *e2 = &g.en[1];
    e2->alive = true; e2->type = E_LANDER; e2->state = L_SEEK; e2->target = -1;
    e2->x = wrapx(h->x + 500.f); e2->y = 60.f; e2->shoot = 99999;
    Controls c = none();
    for (int k = 0; k < 2500 && g.planet_alive; k++) game_step(&g, &c);
    CHECK(!g.planet_alive);
    CHECK(h->state == H_DEAD);
    CHECK(e->type == E_MUTANT);
    CHECK(e2->type == E_MUTANT);
}

static void test_smart_bomb(void)
{
    printf("smart bomb\n");
    Game g; game_init(&g, 7); clear_enemies(&g);
    g.ship.invuln = 100000;
    int n = 0;
    for (int i = 0; i < 6; i++) {            /* six on screen */
        Enemy *e = &g.en[i];
        e->alive = true; e->type = E_BOMBER; e->shoot = 9999;
        e->x = wrapx(g.camx + 40.f + i * 40.f); e->y = 80.f + i * 5.f; n++;
    }
    for (int i = 6; i < 8; i++) {            /* two far away */
        Enemy *e = &g.en[i];
        e->alive = true; e->type = E_BOMBER; e->shoot = 9999;
        e->x = wrapx(g.camx + 1300.f); e->y = 100.f;
    }
    int b0 = g.bombs;
    Controls c = none(); c.bomb = 1;
    game_step(&g, &c);
    CHECK(g.bombs == b0 - 1);
    int left = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) if (g.en[i].alive) left++;
    CHECK(left == 2);
    (void)n;
}

static void test_pod(void)
{
    printf("pods burst into swarmers\n");
    Game g; game_init(&g, 8); clear_enemies(&g);
    Enemy *e = &g.en[0];
    e->alive = true; e->type = E_POD; e->x = g.ship.x + 60.f; e->y = g.ship.y;
    e->shoot = 9999;
    Controls c = none(); c.fire = 1;
    for (int k = 0; k < 20 && e->type == E_POD && e->alive; k++) { e->y = g.ship.y; game_step(&g, &c); }
    int sw = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) if (g.en[i].alive && g.en[i].type == E_SWARMER) sw++;
    CHECK(sw >= 4 && sw <= 6);
    CHECK(g.score >= (uint32_t)SCORE_POD);
}

static void test_waves(void)
{
    printf("wave progression\n");
    Game g; game_init(&g, 9);
    CHECK(g.wave == 1);
    CHECK(g.q_lander == 15 && g.q_bomber == 0 && g.q_pod == 0);
    g.ship.invuln = 1000000;

    /* clear wave 1 by deleting enemies and the quota */
    g.q_lander = 0;
    clear_enemies(&g);
    Controls c = none();
    game_step(&g, &c);
    CHECK(g.phase == PH_CLEAR);
    CHECK(g.clear_humanoids == HUMANOID_COUNT);
    CHECK(g.clear_bonus == 10 * 100 * 1);
    for (int k = 0; k < 260 && g.wave == 1; k++) game_step(&g, &c);
    CHECK(g.wave == 2);
    CHECK(g.q_lander == 20 && g.q_bomber == 3 && g.q_pod == 1);

    /* wave 5 restores a destroyed planet */
    g.wave = 4;
    g.planet_alive = false;
    for (int i = 0; i < HUMANOID_COUNT; i++) g.hum[i].state = H_DEAD;
    clear_enemies(&g);
    g.q_lander = 0;
    game_step(&g, &c);                       /* clears wave 4 */
    for (int k = 0; k < 260 && g.wave == 4; k++) game_step(&g, &c);
    CHECK(g.wave == 5);
    CHECK(g.planet_alive);
    CHECK(game_humanoids_alive(&g) == HUMANOID_COUNT);
}

static void test_ship_death_and_gameover(void)
{
    printf("lives and game over\n");
    Game g; game_init(&g, 10); clear_enemies(&g);
    g.ship.invuln = 0;
    int lives = g.lives;
    Enemy *e = &g.en[0];
    e->alive = true; e->type = E_MUTANT; e->x = g.ship.x; e->y = g.ship.y; e->shoot = 9999;
    Controls c = none();
    game_step(&g, &c);
    CHECK(!g.ship.alive);
    CHECK(g.lives == lives - 1);
    for (int k = 0; k < RESPAWN_FRAMES + 5; k++) game_step(&g, &c);
    CHECK(g.ship.alive && g.ship.invuln > 0);

    g.lives = 0;
    g.ship.invuln = 0;
    e = &g.en[1];
    e->alive = true; e->type = E_MUTANT; e->x = g.ship.x; e->y = g.ship.y; e->shoot = 9999;
    game_step(&g, &c);
    CHECK(g.phase == PH_OVER);
    for (int k = 0; k < 300; k++) game_step(&g, &c);
    CHECK(g.finished);
}

static uint64_t hash_game(const Game *g)
{
    uint64_t h = 1469598103934665603ull;
    const unsigned char *p = (const unsigned char *)&g->ship;
    for (size_t i = 0; i < sizeof g->ship; i++) { h ^= p[i]; h *= 1099511628211ull; }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g->en[i].alive) continue;
        int x = (int)(g->en[i].x * 100.f), y = (int)(g->en[i].y * 100.f);
        h ^= (uint64_t)x; h *= 1099511628211ull;
        h ^= (uint64_t)y; h *= 1099511628211ull;
    }
    h ^= g->score; h *= 1099511628211ull;
    return h;
}

static void test_determinism(void)
{
    printf("determinism (same seed + same input = same game)\n");
    Game a, b;
    game_init(&a, 1234); game_init(&b, 1234);
    for (int k = 0; k < 3000; k++) {
        Controls c;
        game_ai(&a, &c);
        game_step(&a, &c);
        game_step(&b, &c);            /* b receives a's inputs */
    }
    CHECK(hash_game(&a) == hash_game(&b));
    Game d; game_init(&d, 99);
    for (int k = 0; k < 3000; k++) { Controls c; game_ai(&d, &c); game_step(&d, &c); }
    CHECK(hash_game(&a) != hash_game(&d));
}

static void test_soak(void)
{
    printf("soak: demo AI plays 20 minutes without breaking\n");
    Game g; game_init(&g, 4242);
    int games = 0;
    for (int k = 0; k < 20 * 60 * 60; k++) {
        Controls c; game_ai(&g, &c);
        game_step(&g, &c);
        if (g.finished) { game_init(&g, 4242 + (uint32_t)(++games)); }
        /* sanity: everything stays in range */
        if (!(g.ship.x >= 0.f && g.ship.x < WORLD_W) ||
            !(g.ship.y >= 0.f && g.ship.y <= SCREEN_H) ||
            g.lives < 0 || g.bombs < 0) { CHECK(0); break; }
    }
    CHECK(1);
    printf("  (%d games ended, final wave %d)\n", games, g.wave);
}

/* How many enemy shots happen in 40 seconds, over five games, while the
 * ship sits still and cannot die. Same seeds for every grade. */
static int shots_for(Difficulty d, float adapt)
{
    int shots = 0;
    for (uint32_t seed = 11; seed < 16; seed++) {
        Game g;
        game_init(&g, seed);
        game_set_difficulty(&g, d);
        g.tune.adapt = adapt;
        Controls c = {0};
        for (int f = 0; f < 2400; f++) {
            g.ship.invuln = 100000;
            game_step(&g, &c);
            for (int i = 0; i < g.nev; i++) if (g.ev[i].type == EV_ENEMY_SHOT) shots++;
        }
    }
    return shots;
}

static void test_difficulty(void)
{
    printf("difficulty\n");
    Game g;
    game_init(&g, 1);
    CHECK(g.diff == DIFF_NORMAL && g.lives == START_SPARE_SHIPS && g.bombs == START_BOMBS);
    game_set_difficulty(&g, DIFF_EASY);
    CHECK(g.lives == 3 && g.bombs == 4);
    game_set_difficulty(&g, DIFF_INSANE);
    CHECK(g.lives == 1 && g.bombs == 1);
    game_set_difficulty(&g, (Difficulty)99);                 /* nonsense falls back to normal */
    CHECK(g.diff == DIFF_NORMAL);
    CHECK(!strcmp(game_difficulty_name(DIFF_HARD), "HARD"));

    int e = shots_for(DIFF_EASY, 1.f), n = shots_for(DIFF_NORMAL, 1.f);
    int h = shots_for(DIFF_HARD, 1.f), i = shots_for(DIFF_INSANE, 1.f);
    printf("  enemy shots in 40 s: easy %d, normal %d, hard %d, insane %d\n", e, n, h, i);
    CHECK(e < n && n < h && h < i);

    int lo = shots_for(DIFF_NORMAL, 0.92f), hi = shots_for(DIFF_NORMAL, 1.08f);
    printf("  sensor nudge: 0.92 -> %d shots, 1.08 -> %d shots (normal %d)\n", lo, hi, n);
    CHECK(lo < n && n < hi);
}

int main(void)
{
    test_wraparound();
    test_ship_feel();
    test_scoring();
    test_abduction();
    test_fall_death();
    test_mutation_and_planet();
    test_smart_bomb();
    test_pod();
    test_waves();
    test_ship_death_and_gameover();
    test_determinism();
    test_soak();
    test_difficulty();
    printf("\n%d checks, %d failed\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}

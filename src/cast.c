/*
 * cast.c - see cast.h.
 */
#include "cast.h"
#include "gfx.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct { ActorKind kind; const char *name; const char *role; const char *pts; } CastDef;

/* roles are at most 17 letters: that is what fits under each picture */
static const CastDef CAST[] = {
    { ACTOR_SHIP,    "YOUR SHIP", "FAST AND BRAVE",  ""         },
    { ACTOR_LANDER,  "LANDER",    "STEALS HUMANOIDS","150 POINTS" },
    { ACTOR_MUTANT,  "MUTANT",    "WAS A HUMANOID",  "150 POINTS" },
    { ACTOR_BAITER,  "BAITER",    "LOVES DAWDLERS",  "200 POINTS" },
    { ACTOR_BOMBER,  "BOMBER",    "DROPS MINES",     "250 POINTS" },
    { ACTOR_POD,     "POD",       "FULL OF SWARMERS","1000 POINTS"},
    { ACTOR_SWARMER, "SWARMER",   "ANGRY AND FAST",  "150 POINTS" },
    { ACTOR_HUMAN,   "HUMANOID",  "PLEASE SAVE ME",  "500 POINTS" },
    { ACTOR_MINE,    "MINE",      "DO NOT TOUCH",    ""         },
};
#define N ((int)(sizeof CAST / sizeof CAST[0]))

#define START_GAP   45      /* frames between one actor and the next        */
#define HOLD_FRAMES 260     /* how long the finished picture stays          */
#define CELL_W 106
#define CELL_H 60
#define TOP    34

typedef struct {
    float x, y, vx, vy, tx, ty;
    float jx, jy;            /* the kid's current joystick push            */
    int   timer;             /* frames until the kid changes his mind      */
    int   age;
    bool  active, locked;
    int   lock_t;
    bool  flip;
} Actor;

static Actor g_a[N];
static int   g_t;
static int   g_done_t;
static unsigned g_rng = 1;

/*
 * A tiny random number generator (xorshift). It has its own seed, so the same
 * seed always gives the same parade. That makes the parade testable:
 * test_cast.c can run many seeds and check that every one of them ends.
 */
static unsigned rnd(void) { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
/* A random number between 0 and 1, made from rnd(). */
static float rndf(void) { return (float)(rnd() >> 8) / 16777216.f; }

/*
 * Starts a new parade. Clears all actors and works out where each one should
 * end up: a grid of 3 columns, one cell per actor. The actors themselves
 * appear later, one at a time, in cast_step().
 */
void cast_start(unsigned seed)
{
    g_rng = seed ? seed : 1;
    g_t = 0;
    g_done_t = -1;
    memset(g_a, 0, sizeof g_a);
    for (int i = 0; i < N; i++) {
        g_a[i].tx = (float)((i % 3) * CELL_W + CELL_W / 2 + 1);
        g_a[i].ty = (float)(TOP + (i / 3) * CELL_H + 20);
    }
}

/*
 * Puts one actor on the screen at a random edge (left, right, top or bottom),
 * already moving inwards at speed 3.
 */
static void spawn(Actor *a)
{
    int side = (int)(rnd() % 4);
    a->active = true;
    switch (side) {
    case 0: a->x = -24.f;  a->y = 40.f + rndf() * 170.f; a->vx = 3.f; a->vy = 0.f; break;
    case 1: a->x = 344.f;  a->y = 40.f + rndf() * 170.f; a->vx = -3.f; a->vy = 0.f; break;
    case 2: a->x = rndf() * 320.f; a->y = 14.f;  a->vx = 0.f; a->vy = 3.f; break;
    default: a->x = rndf() * 320.f; a->y = 240.f; a->vx = 0.f; a->vy = -3.f; break;
    }
    a->timer = 4;
}

/* The kid decides what to push the stick towards. Mostly the right way, but
 * not always, and sometimes he lets go and just watches it drift. */
static void new_push(Actor *a)
{
    float r = rndf();
    float dx = a->tx - a->x, dy = a->ty - a->y;
    if (r < 0.52f) {                              /* aim for the spot */
        a->jx = dx > 4.f ? 1.f : (dx < -4.f ? -1.f : 0.f);
        a->jy = dy > 4.f ? 1.f : (dy < -4.f ? -1.f : 0.f);
    } else if (r < 0.82f) {                       /* mash in a random direction */
        a->jx = (float)((int)(rnd() % 3) - 1);
        a->jy = (float)((int)(rnd() % 3) - 1);
    } else {                                      /* let go */
        a->jx = a->jy = 0.f;
    }
    if (rndf() < 0.07f) { a->vx *= -1.3f; a->vy *= -1.3f; }       /* panic: full reverse */
    a->timer = 5 + (int)(rnd() % 14);
}

/*
 * Moves one actor one frame, as if a kid were steering it with a joystick. The
 * kid pushes in random directions at first, so the actor swings around wildly.
 * After about 2.5 seconds (age 150) the random pushes fade out and a gentle
 * pull towards the target spot takes over. That is what guarantees that every
 * parade ends. When the actor is close to its spot and slow enough, it locks
 * in place and its name starts to type itself in.
 */
static void step_actor(Actor *a)
{
    a->age++;
    if (a->locked) { a->lock_t++; return; }

    /* Time goes by and the kid gets better at it: from about 2.5 seconds
     * on, his random pushes fade out and a gentle pull towards the spot grows.
     * That keeps the show messy at the start and makes sure it always ends. */
    float p = (float)(a->age - 150) / 150.f;
    if (p < 0.f) p = 0.f;
    if (p > 1.f) p = 1.f;

    if (--a->timer <= 0) new_push(a);
    a->vx = (a->vx + a->jx * 0.95f * (1.f - p)) * (0.93f - 0.05f * p);
    a->vy = (a->vy + a->jy * 0.95f * (1.f - p)) * (0.93f - 0.05f * p);
    a->vx += (a->tx - a->x) * 0.012f * p;
    a->vy += (a->ty - a->y) * 0.012f * p;
    float sp = sqrtf(a->vx * a->vx + a->vy * a->vy);
    if (sp > 6.5f) { a->vx *= 6.5f / sp; a->vy *= 6.5f / sp; }

    a->x += a->vx;
    a->y += a->vy;
    if (fabsf(a->vx) > 0.3f) a->flip = a->vx < 0.f;

    /* after the first moments the screen edges are solid: bounce with a bump */
    if (a->age > 30) {
        if (a->x < 12.f)  { a->x = 12.f;  a->vx = fabsf(a->vx) * 0.8f; }
        if (a->x > 308.f) { a->x = 308.f; a->vx = -fabsf(a->vx) * 0.8f; }
        if (a->y < 30.f)  { a->y = 30.f;  a->vy = fabsf(a->vy) * 0.8f; }
        if (a->y > 212.f) { a->y = 212.f; a->vy = -fabsf(a->vy) * 0.8f; }
    }

    float dx = a->tx - a->x, dy = a->ty - a->y;
    if (a->age > 90 && dx * dx + dy * dy < 64.f && sp < 3.2f) {
        a->locked = true; a->x = a->tx; a->y = a->ty; a->lock_t = 0; a->flip = false;
    }
}

/*
 * Runs the parade one frame. Starts the next actor when it is its turn (every
 * START_GAP frames), moves all active actors, and notes the moment when all of
 * them are locked in place.
 */
void cast_step(void)
{
    g_t++;
    int locked = 0;
    for (int i = 0; i < N; i++) {
        Actor *a = &g_a[i];
        if (!a->active && g_t >= i * START_GAP + 10) spawn(a);
        if (a->active) step_actor(a);
        if (a->locked) locked++;
    }
    if (locked == N && g_done_t < 0) g_done_t = g_t;
}

/*
 * True when everyone has been locked in place for HOLD_FRAMES frames (about 4
 * seconds). main.c then moves on to the next screen.
 */
bool cast_done(void) { return g_done_t >= 0 && g_t - g_done_t > HOLD_FRAMES; }
/* How many actors have reached their spot so far. Used by the tests. */
int  cast_locked(void) { int c = 0; for (int i = 0; i < N; i++) if (g_a[i].locked) c++; return c; }
/* How many actors there are in total. */
int  cast_count(void) { return N; }

/*
 * Draws the parade: a dark layer over the background, the title, then each
 * actor. A locked actor does a little hop when it lands and then wobbles
 * gently. Its name, role and points are drawn a couple of letters at a time so
 * the text looks like it is being typed.
 */
void cast_draw(int frame)
{
    gfx_blend_rect(0, 0, SCREEN_W, SCREEN_H, RGB(4,2,20), 110);
    gfx_amiga_wave((SCREEN_W - gfx_amiga_width("MEET THE CAST", 1, 1)) / 2, 8, "MEET THE CAST", RAMP_GOLD, 1, 1, frame, 2);

    for (int i = 0; i < N; i++) {
        const Actor *a = &g_a[i];
        if (!a->active) continue;
        int bump = 0;
        if (a->locked && a->lock_t < 10) bump = -(int)(sinf((float)a->lock_t / 10.f * 3.14159f) * 6.f);   /* little hop on landing */
        int wob = a->locked ? (int)(sinf((float)(frame + i * 9) * 0.1f) * 1.5f) : 0;
        int sc = 2;
        if (CAST[i].kind == ACTOR_SWARMER || CAST[i].kind == ACTOR_MINE || CAST[i].kind == ACTOR_HUMAN) sc = 3;
        gfx_draw_actor(CAST[i].kind, (int)a->x, (int)a->y + bump + wob, frame, a->flip, sc);

        if (a->locked) {
            int shown = a->lock_t / 2;                  /* the text types itself in */
            char buf[32];
            int cx = (int)a->tx;
            int ny = (int)a->ty + 21;

            snprintf(buf, sizeof buf, "%.*s", shown, CAST[i].name);
            gfx_amiga_text(cx - gfx_amiga_width(CAST[i].name, 0, 1) / 2, ny, buf, RAMP_GOLD, 0, 1);
            if (shown > (int)strlen(CAST[i].name)) {
                int k = shown - (int)strlen(CAST[i].name);
                snprintf(buf, sizeof buf, "%.*s", k, CAST[i].role);
                gfx_text(cx - gfx_text_width(CAST[i].role, 1) / 2, ny + 11, buf, RGB(190,200,245), 1);
                if (CAST[i].pts[0] && k > (int)strlen(CAST[i].role)) {
                    snprintf(buf, sizeof buf, "%.*s", k - (int)strlen(CAST[i].role), CAST[i].pts);
                    gfx_text(cx - gfx_text_width(CAST[i].pts, 1) / 2, ny + 21, buf, RGB(120,235,255), 1);
                }
            }
        }
    }
}

/*
 * sense.c - see sense.h. Event times are kept in small ring buffers, and every
 * number is worked out from those times, so nothing drifts over a long session.
 */
#include "sense.h"
#include <string.h>

#define FRAMES 120
#define STEPS  120
#define EVENTS 256

typedef struct { double t[EVENTS]; int head, n; } Ring;

static double  g_frame_t[FRAMES]; static int g_frame_n, g_frame_head;
static double  g_step_t[STEPS];   static int g_step_n, g_step_head;
static Ring    g_press, g_death;
static SenseInfo g_info = { 0, 0, 0, 0, 1.f, true, 0 };

/*
 * Adds an event time to a ring buffer. A ring buffer is a fixed-size list
 * where the newest entry overwrites the oldest once it is full, so memory use
 * never grows however long you play.
 */
static void ring_add(Ring *r, double t)
{
    r->t[r->head] = t;
    r->head = (r->head + 1) % EVENTS;
    if (r->n < EVENTS) r->n++;
}

/* number of events newer than (now - window_ms) */
static int ring_count(const Ring *r, double now, double window_ms)
{
    int c = 0;
    for (int i = 0; i < r->n; i++) {
        int idx = (r->head - 1 - i + EVENTS * 2) % EVENTS;
        if (now - r->t[idx] <= window_ms) c++; else break;
    }
    return c;
}

/* Forgets everything measured so far. Called when a new game starts. */
void sense_reset(void)
{
    memset(g_frame_t, 0, sizeof g_frame_t); g_frame_n = g_frame_head = 0;
    memset(g_step_t, 0, sizeof g_step_t);   g_step_n = g_step_head = 0;
    memset(&g_press, 0, sizeof g_press);
    memset(&g_death, 0, sizeof g_death);
    g_info = (SenseInfo){ 0, 0, 0, 0, 1.f, true, 0 };
}

/*
 * Call once for every picture drawn, with the time in milliseconds. Keeps the
 * last 120 frame times and works out two things: pictures per second (fps) and
 * the longest gap between two pictures (worst_ms). Nothing is reported until
 * 10 frames have been seen.
 */
void sense_frame(double now)
{
    g_frame_t[g_frame_head] = now;
    g_frame_head = (g_frame_head + 1) % FRAMES;
    if (g_frame_n < FRAMES) g_frame_n++;

    if (g_frame_n >= 10) {
        double newest = now;
        double oldest = g_frame_t[(g_frame_head - g_frame_n + FRAMES * 2) % FRAMES];
        if (newest > oldest) g_info.fps = (float)((g_frame_n - 1) * 1000.0 / (newest - oldest));
        double worst = 0;
        for (int i = 1; i < g_frame_n; i++) {
            double a = g_frame_t[(g_frame_head - g_frame_n + i - 1 + FRAMES * 2) % FRAMES];
            double b = g_frame_t[(g_frame_head - g_frame_n + i + FRAMES * 2) % FRAMES];
            if (b - a > worst) worst = b - a;
        }
        g_info.worst_ms = (float)worst;
    }
}

/*
 * Call once for every game step. Measures how many steps per second the game
 * really runs. It should be 60. smooth is true while it is within 8 percent of
 * that.
 */
void sense_sim_step(double now)
{
    g_step_t[g_step_head] = now;
    g_step_head = (g_step_head + 1) % STEPS;
    if (g_step_n < STEPS) g_step_n++;

    if (g_step_n >= 30) {
        double oldest = g_step_t[(g_step_head - g_step_n + STEPS * 2) % STEPS];
        if (now > oldest) g_info.sim = (float)((g_step_n - 1) * 1000.0 / (now - oldest));
        g_info.smooth = g_info.sim > 60.f * 0.92f && g_info.sim < 60.f * 1.08f;
    }
}

/*
 * Call when the player presses a key or button. Used to work out presses per
 * second.
 */
void sense_press(double now) { ring_add(&g_press, now); }
/*
 * Call when the player's ship is lost. Recent deaths make the game a little
 * easier.
 */
void sense_death(double now) { ring_add(&g_death, now); }

/* Keeps a number between lo and hi. */
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/*
 * Works out the adaptive level. It starts from 1.0 (no change). A busy player
 * (more than about 4 presses per second) raises the target a little, and every
 * death in the last 40 seconds lowers it. The result is kept between
 * SENSE_ADAPT_MIN and SENSE_ADAPT_MAX (0.92 to 1.08), so the game can never
 * change enemy speed by more than 8 percent. The level moves only a small step
 * towards the target each time, so changes are slow and the player never
 * notices them happening.
 */
void sense_update(double now, bool playing, bool adaptive)
{
    g_info.press_s = (float)ring_count(&g_press, now, 8000.0) / 8.0f;
    g_info.deaths  = ring_count(&g_death, now, 40000.0);

    float target = 1.f;
    if (adaptive && playing) {
        /* around 4 presses a second is a calm player; more means a busy,
           confident one. Each death in the last 40 s takes a little away. */
        target += clampf((g_info.press_s - 4.f) * 0.015f, -0.06f, 0.10f);
        target -= 0.04f * (float)g_info.deaths;
    }
    target = clampf(target, SENSE_ADAPT_MIN, SENSE_ADAPT_MAX);
    g_info.adapt += (target - g_info.adapt) * 0.002f;   /* about 8 s time constant at 60 steps/s */
}

/*
 * Gives read-only access to all the measured numbers. The F9/Tab meter and the
 * game both read from here.
 */
const SenseInfo *sense_info(void) { return &g_info; }

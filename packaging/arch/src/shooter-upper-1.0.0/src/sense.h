/*
 * sense.h - the Senseability sensor.
 *
 * It watches two things while you play and does something useful with them:
 *
 *  1. TIMING. It counts how many pictures per second reach the screen and how
 *     many simulation steps run per second (the target is exactly 60 of each),
 *     and it remembers the slowest frame of the last two seconds. If the
 *     machine cannot keep up it says so, so the game can warn the player.
 *
 *  2. THE PLAYER. It counts button presses per second and deaths. When the
 *     adaptive mode is on, it nudges the difficulty by at most +-8 percent of enemy speed:
 *     a player who is busy and not dying gets a little more pressure, a player
 *     who keeps dying gets a little relief. The nudge moves slowly (about
 *     eight seconds) so you never feel the game jump.
 *
 * This file knows nothing about SDL: callers pass in the time in milliseconds.
 * That makes it testable with made-up clocks.
 */
#ifndef SENSE_H
#define SENSE_H

#include <stdbool.h>

#define SENSE_ADAPT_MIN 0.92f
#define SENSE_ADAPT_MAX 1.08f

typedef struct {
    float fps;        /* pictures per second, last second or so            */
    float sim;        /* simulation steps per second (target 60)           */
    float worst_ms;   /* slowest frame in the last 120 frames              */
    float press_s;    /* button presses per second over the last 8 seconds */
    float adapt;      /* difficulty factor, 1.0 = no change                */
    bool  smooth;     /* true when sim is within 8 percent of 60           */
    int   deaths;     /* deaths in the last 40 seconds                     */
} SenseInfo;

void sense_reset(void);
void sense_frame(double now_ms);        /* once per drawn picture        */
void sense_sim_step(double now_ms);     /* once per simulation step      */
void sense_press(double now_ms);        /* once per player button press  */
void sense_death(double now_ms);        /* when the player loses a ship  */
void sense_update(double now_ms, bool playing, bool adaptive);  /* once per step */
const SenseInfo *sense_info(void);

#endif

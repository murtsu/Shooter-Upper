/*
 * cast.h - the idle parade: "MEET THE CAST".
 *
 * After 20 seconds without input the title screen introduces every ship,
 * enemy and humanoid. They do not glide in politely. Each one is steered by a
 * make-believe kid with a joystick: it shoots in from a random edge, wobbles,
 * overshoots, reverses at the wrong moment, bangs into the screen border, and
 * finally, with some luck, lands in its place. Then its name and job appear.
 *
 * The motion is made by feeding random, short "joystick pushes" into a normal
 * speed-and-friction model, which is why it looks like someone is really
 * controlling it badly rather than like a canned animation.
 */
#ifndef CAST_H
#define CAST_H

#include <stdbool.h>

void cast_start(unsigned seed);
void cast_step(void);            /* once per simulation step */
void cast_draw(int frame);       /* on top of the space background */
bool cast_done(void);            /* the parade has finished and held its pose */
int  cast_locked(void);          /* how many actors have landed (for tests) */
int  cast_count(void);

#endif

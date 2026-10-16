/*
 * gfx.h - drawing into a 320x224 pixel buffer, in a 16-bit console style.
 *
 * Everything is drawn by code: sprites are small grids of palette indices
 * (16 colors each), backgrounds are computed, text uses a built-in 5x7 font.
 * No image files are loaded, so there are no art licenses to worry about.
 * main.c uploads the finished buffer to the screen and SDL scales it up with
 * crisp nearest-neighbor pixels.
 */
#ifndef GFX_H
#define GFX_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "game/game.h"

#define RGB(r, g, b) (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

extern uint32_t g_fb[SCREEN_W * SCREEN_H];

void gfx_init(void);

/* primitives */
void gfx_clear(uint32_t c);
void gfx_pixel(int x, int y, uint32_t c);
void gfx_rect(int x, int y, int w, int h, uint32_t c);
void gfx_rect_outline(int x, int y, int w, int h, uint32_t c);
void gfx_blend_rect(int x, int y, int w, int h, uint32_t c, int alpha /*0..255*/);
void gfx_hline(int x0, int x1, int y, uint32_t c);

/* text (5x7 font, upper case) */
int  gfx_text_width(const char *s, int scale);
void gfx_text(int x, int y, const char *s, uint32_t c, int scale);
void gfx_text_shadow(int x, int y, const char *s, uint32_t c, int scale);
void gfx_text_center(int y, const char *s, uint32_t c, int scale);
void gfx_text_grad(int x, int y, const char *s, int scale, uint32_t top, uint32_t bottom);
void gfx_text_grad_center(int y, const char *s, int scale, uint32_t top, uint32_t bottom);

/* Amiga-style fonts: chunky letters with a chrome gradient, a dark outline and
 * a drop shadow. 'big' is 12x14 pixels per letter, small is 6x7. */
enum { RAMP_GOLD, RAMP_ICE, RAMP_RED, RAMP_WHITE, RAMP_GREEN, RAMP_DIM, RAMP_COUNT };
int  gfx_amiga_width(const char *s, int big, int scale);
void gfx_amiga_text(int x, int y, const char *s, int ramp, int big, int scale);
void gfx_amiga_wave(int x, int y, const char *s, int ramp, int big, int scale, int frame, int amp);
void gfx_amiga_center(int y, const char *s, int ramp, int big, int scale);
void gfx_title(int y, const char *s, int ramp);                     /* big and centred */
void gfx_menu_item(int y, const char *s, bool selected, int frame); /* small and centred */

/* raster effects */
void gfx_copper_bars(int y, int h, int frame);
void gfx_sine_scroller(int y, const char *text, int frame);
int  gfx_scroller_length(const char *text);                          /* frames for one full pass */

/* the cast of the game, drawn at any size (used by the idle parade) */
typedef enum {
    ACTOR_SHIP, ACTOR_LANDER, ACTOR_MUTANT, ACTOR_BAITER, ACTOR_BOMBER,
    ACTOR_POD, ACTOR_SWARMER, ACTOR_HUMAN, ACTOR_MINE, ACTOR_COUNT
} ActorKind;
void gfx_draw_actor(ActorKind k, int cx, int cy, int frame, bool flip, int scale);

/* whole scenes */
void gfx_draw_space_bg(float camx, bool planet, int frame);   /* sky, nebula, stars, mountains */
void gfx_draw_game(const Game *g, int frame);                 /* world + HUD + overlays */
void gfx_draw_logo(int y, int frame);                         /* the title logo */
#define SPLASH_FRAMES 270                                     /* the start-up splash lasts 4.5 s */
void gfx_draw_splash(int t);                                  /* start-up animation, t = frames since start */
void gfx_draw_ship_icon(int x, int y);                        /* tiny ship for lives */

/* effects: feed game events in, step once per simulation step */
void gfx_fx_reset(void);
void gfx_fx_event(const Event *e, const Game *g);
void gfx_fx_step(const Game *g);

/* post effects and output */
void gfx_crt(uint32_t *dst);                                  /* g_fb -> dst with scanlines */
bool gfx_save_png(const char *path, const uint32_t *px);

#endif

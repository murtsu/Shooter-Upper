/*
 * gfx.c - everything that draws. See gfx.h for the overview.
 */
#include "gfx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWO_PI 6.2831853f

uint32_t g_fb[SCREEN_W * SCREEN_H];

/* ======================================================================== */
/* Primitives                                                               */
/* ======================================================================== */

void gfx_clear(uint32_t c)
{
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) g_fb[i] = c;
}

/*
 * Sets one pixel. Anything outside the picture is ignored, so callers do not
 * have to check the edges. Colors are 0xAARRGGBB, made with the RGB() macro.
 */
void gfx_pixel(int x, int y, uint32_t c)
{
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H) g_fb[y * SCREEN_W + x] = c;
}

/* Draws a filled rectangle. The parts outside the picture are cut off. */
void gfx_rect(int x, int y, int w, int h, uint32_t c)
{
    for (int j = y; j < y + h; j++) {
        if ((unsigned)j >= SCREEN_H) continue;
        for (int i = x; i < x + w; i++)
            if ((unsigned)i < SCREEN_W) g_fb[j * SCREEN_W + i] = c;
    }
}

/* Draws a horizontal line from x0 to x1 at row y. */
void gfx_hline(int x0, int x1, int y, uint32_t c)
{
    if ((unsigned)y >= SCREEN_H) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
    for (int x = x0; x <= x1; x++) g_fb[y * SCREEN_W + x] = c;
}

/* Draws only the frame of a rectangle, one pixel thick. */
void gfx_rect_outline(int x, int y, int w, int h, uint32_t c)
{
    gfx_rect(x, y, w, 1, c);
    gfx_rect(x, y + h - 1, w, 1, c);
    gfx_rect(x, y, 1, h, c);
    gfx_rect(x + w - 1, y, 1, h, c);
}

/*
 * Blends color a with color b. alpha 0 gives a, 255 gives b, and in between is
 * a mix.
 */
static uint32_t mix(uint32_t a, uint32_t b, int alpha /*0..255 of b*/)
{
    uint32_t ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    uint32_t br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    uint32_t r = (ar * (255 - alpha) + br * alpha) / 255;
    uint32_t g = (ag * (255 - alpha) + bg * alpha) / 255;
    uint32_t bl = (ab * (255 - alpha) + bb * alpha) / 255;
    return RGB(r, g, bl);
}

/*
 * Draws a see-through rectangle: the color c is blended over what is already
 * there. alpha 0 is invisible, 255 is solid.
 */
void gfx_blend_rect(int x, int y, int w, int h, uint32_t c, int alpha)
{
    for (int j = y; j < y + h; j++) {
        if ((unsigned)j >= SCREEN_H) continue;
        for (int i = x; i < x + w; i++) {
            if ((unsigned)i >= SCREEN_W) continue;
            g_fb[j * SCREEN_W + i] = mix(g_fb[j * SCREEN_W + i], c, alpha);
        }
    }
}

/* A color between a and b. t = 0 gives a, t = 1 gives b. */
static uint32_t lerp_col(uint32_t a, uint32_t b, float t)
{
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    return mix(a, b, (int)(t * 255.f));
}

/* ======================================================================== */
/* Font: classic 5x7, one byte per column, bit0 = top row.                  */
/* Covers ASCII 32 (space) to 90 (Z). Lower case is drawn as upper case.    */
/* ======================================================================== */

static const uint8_t FONT[59][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14}, {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x56,0x20,0x50}, {0x00,0x05,0x03,0x00,0x00}, {0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},
    {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00}, {0x08,0x14,0x22,0x41,0x00},
    {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
};

/*
 * Finds a letter's place in the font table. Lower case is treated as upper
 * case. Gives -1 for characters the font does not have.
 */
static int glyph_index(char ch)
{
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    if (ch < 32 || ch > 90) return -1;
    return ch - 32;
}

/*
 * How many pixels wide a text is at the given size (6 pixels per letter, minus
 * the gap after the last).
 */
int gfx_text_width(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? (n * 6 - 1) * scale : 0;
}

/*
 * Draws one letter from the 5x7 font. The color fades from top to bottom, or
 * stays flat if top and bottom are the same.
 */
static void draw_glyph(int x, int y, char ch, uint32_t top, uint32_t bottom, int scale)
{
    if (ch == '_') { gfx_rect(x, y + 8 * scale, 5 * scale, scale, top); return; }
    int gi = glyph_index(ch);
    if (gi < 0) return;
    for (int col = 0; col < 5; col++) {
        uint8_t bits = FONT[gi][col];
        for (int row = 0; row < 7; row++) {
            if (!((bits >> row) & 1)) continue;
            uint32_t c = (top == bottom) ? top : lerp_col(top, bottom, (float)row / 6.f);
            gfx_rect(x + col * scale, y + row * scale, scale, scale, c);
        }
    }
}

/* Draws text with a color that fades from top to bottom. */
void gfx_text_grad(int x, int y, const char *s, int scale, uint32_t top, uint32_t bottom)
{
    for (; *s; s++, x += 6 * scale) draw_glyph(x, y, *s, top, bottom, scale);
}

/* Draws plain text in one color. */
void gfx_text(int x, int y, const char *s, uint32_t c, int scale)
{
    gfx_text_grad(x, y, s, scale, c, c);
}

/*
 * Draws text with a dark shadow one step down and to the right, so it reads on
 * any background.
 */
void gfx_text_shadow(int x, int y, const char *s, uint32_t c, int scale)
{
    gfx_text(x + scale, y + scale, s, RGB(10, 8, 30), scale);
    gfx_text(x, y, s, c, scale);
}

/* Draws text with a shadow, centered on the screen sideways. */
void gfx_text_center(int y, const char *s, uint32_t c, int scale)
{
    gfx_text_shadow((SCREEN_W - gfx_text_width(s, scale)) / 2, y, s, c, scale);
}

/* Draws centered text with a color fade and a dark outline. */
void gfx_text_grad_center(int y, const char *s, int scale, uint32_t top, uint32_t bottom)
{
    int x = (SCREEN_W - gfx_text_width(s, scale)) / 2;
    uint32_t dark = RGB(14, 8, 40);
    for (int dy = -1; dy <= 1; dy++)               /* outline */
        for (int dx = -1; dx <= 1; dx++)
            if (dx || dy) gfx_text(x + dx * (scale > 3 ? 2 : 1), y + dy * (scale > 3 ? 2 : 1), s, dark, scale);
    gfx_text(x + scale, y + scale + 1, s, RGB(8, 4, 24), scale);
    gfx_text_grad(x, y, s, scale, top, bottom);
}

/* ======================================================================== */
/* Sprites: grids of palette indices. '.' = see-through, 1-9 / a-f = color. */
/* ======================================================================== */

typedef struct { int w, h; const char *const *rows; const uint32_t *pal; } Sprite;

#define C(r, g, b) RGB(r, g, b)
#define PAL(name, ...) static const uint32_t name[16] = { 0, __VA_ARGS__ }

/* ---- player ship, 16x8, nose to the right ---- */
PAL(pal_ship, C(18,22,60), C(214,230,255), C(128,158,230), C(64,86,160), C(70,235,255),
    C(255,255,255), C(240,70,80), C(150,30,60), C(255,170,50), C(255,225,90));
static const char *const rows_ship[8] = {
    "..11............",
    ".1771....111....",
    "1788111112221...",
    "1899233333355611",
    "1899444444444411",
    ".1884411111111..",
    "..1771..........",
    "..11............"
};

/* ---- Lander, 14x12 ---- */
PAL(pal_lander, C(10,50,20), C(130,255,130), C(50,200,70), C(20,120,50), C(255,240,80),
    C(255,70,70), C(190,200,215));
static const char *const rows_lander[12] = {
    "....111111....",
    "...12222221...",
    "..1223333221..",
    ".1233355333321",
    "12333333333321",
    "14444444444441",
    ".111111111111.",
    "..1.77..77.1..",
    ".1..7....7..1.",
    ".1.7......7.1.",
    "17..........71",
    "11..........11"
};

/* second animation frame: legs scuttle the other way */
static const char *const rows_lander_b[12] = {
    "....111111....", "...12222221...", "..1223333221..", ".1233355333321",
    "12333333333321", "14444444444441", ".111111111111.", "..1.77..77.1..",
    "..1.7....7.1..", "...17....71...", "...11....11...", "...1......1..."
};

/* ---- Mutant, 12x12 (two palettes flicker) ---- */
PAL(pal_mutant_a, C(50,10,60), C(235,70,225), C(150,40,200), C(255,160,255), C(255,255,80), C(255,40,40));
PAL(pal_mutant_b, C(60,10,40), C(255,120,120), C(200,40,120), C(255,210,160), C(120,255,255), C(255,255,255));
static const char *const rows_mutant[12] = {
    "1..1....1..1",
    "1.11.11.11.1",
    ".1223333221.",
    "122333333221",
    "123556655321",
    "123333333321",
    "122333333221",
    ".1223111322.",
    ".1233333321.",
    "..11222211..",
    "...11..11...",
    "..1........1"
};

/* ---- Baiter, 18x8 ---- */
PAL(pal_baiter, C(60,10,20), C(255,185,125), C(230,50,40), C(255,140,40), C(255,240,100), C(100,220,255));
static const char *const rows_baiter[8] = {
    "......111111......",
    ".....16666661.....",
    "...111666666111...",
    ".1122333333332211.",
    "123335353535353321",
    ".1444444444444441.",
    "..11111111111111..",
    "....11......11...."
};

/* ---- Bomber, 14x10 ---- */
PAL(pal_bomber, C(10,30,60), C(140,210,255), C(60,140,230), C(30,80,160), C(255,230,80),
    C(255,255,255), C(255,60,60));
static const char *const rows_bomber[10] = {
    "....111111....",
    "..1122222211..",
    ".123333333321.",
    "12335533553321",
    "14444444444441",
    ".111111111111.",
    "..1177..7711..",
    "...177....771.",
    "....17....71..",
    ".....1....1..."
};

/* ---- Pod, 14x14 ---- */
PAL(pal_pod, C(60,10,10), C(255,150,40), C(255,240,120), C(220,50,30), C(255,255,255));
static const char *const rows_pod[14] = {
    "....111111....",
    "..1122222211..",
    ".122333332221.",
    ".123355553321.",
    "12233555533221",
    "12233355333221",
    "12233333333221",
    "12422222222421",
    "12222222222221",
    "14222222222241",
    ".142222222241.",
    ".114222222411.",
    "..1144444411..",
    "....111111...."
};

/* ---- Swarmer, 7x7 ---- */
PAL(pal_swarmer, C(80,20,0), C(255,230,60), C(255,120,20), C(255,255,255));
static const char *const rows_swarmer[7] = {
    "1..1..1", ".12221.", "1233321", ".23432.", "1233321", ".12221.", "1..1..1"
};

/* ---- Humanoid, 6x10 ---- */
PAL(pal_human, C(20,20,50), C(250,250,255), C(180,190,210), C(80,230,255), C(255,150,40));
static const char *const rows_human[10] = {
    "..11..", ".1221.", ".1441.", "..11..", ".1331.", "133331", ".1331.", ".1551.", ".1..1.", "11..11"
};
/* a falling / abducted humanoid waves its arms */
static const char *const rows_human_up[10] = {
    "..11..", ".1221.", ".1441.", "1.11.1", "113331", ".1331.", ".1331.", ".1551.", ".1..1.", "11..11"
};

/* ---- Mine, 7x7 ---- */
PAL(pal_mine, C(100,10,10), C(255,60,60), C(255,230,80));
static const char *const rows_mine[7] = {
    "...1...", ".1.2.1.", "..222..", "1223221", "..222..", ".1.2.1.", "...1..."
};

/* ---- Smart bomb icon, 5x5 ---- */
PAL(pal_bombicon, C(255,150,40), C(255,235,120), C(255,255,255));
static const char *const rows_bombicon[5] = { ".111.", "12221", "12321", "12221", ".111." };

#define SPR(rows, pal, w, h) { w, h, rows, pal }
static const Sprite S_SHIP    = SPR(rows_ship, pal_ship, 16, 8);
static const Sprite S_LANDER  = SPR(rows_lander, pal_lander, 14, 12);
static const Sprite S_LANDER_B= SPR(rows_lander_b, pal_lander, 14, 12);
static const Sprite S_MUTANT  = SPR(rows_mutant, pal_mutant_a, 12, 12);
static const Sprite S_BAITER  = SPR(rows_baiter, pal_baiter, 18, 8);
static const Sprite S_BOMBER  = SPR(rows_bomber, pal_bomber, 14, 10);
static const Sprite S_POD     = SPR(rows_pod, pal_pod, 14, 14);
static const Sprite S_SWARMER = SPR(rows_swarmer, pal_swarmer, 7, 7);
static const Sprite S_HUMAN   = SPR(rows_human, pal_human, 6, 10);
static const Sprite S_HUMAN_UP= SPR(rows_human_up, pal_human, 6, 10);
static const Sprite S_MINE    = SPR(rows_mine, pal_mine, 7, 7);
static const Sprite S_BOMBICON= SPR(rows_bombicon, pal_bombicon, 5, 5);

/*
 * Turns one character of a sprite drawing ('0' to '9', 'a' to 'f') into a
 * color number from 0 to 15. Anything else gives -1, and 0 means see-through.
 */
static int hexval(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

/* draw with the sprite's top-left at (x, y); half = draw at 50% size */
static void spr_draw(const Sprite *s, const uint32_t *pal, int x, int y, bool flip, bool half)
{
    int step = half ? 2 : 1;
    for (int r = 0; r < s->h; r += step) {
        const char *row = s->rows[r];
        for (int c = 0; c < s->w; c += step) {
            int cc = flip ? (s->w - 1 - c) : c;
            int v = hexval(row[cc]);
            if (v <= 0) continue;
            gfx_pixel(x + (half ? c / 2 : c), y + (half ? r / 2 : r), pal[v]);
        }
    }
}

/* draw with a vertical shear: columns toward the nose move with vy, so the
 * ship tilts like it is banking. nose_left says which way the nose points. */
static void spr_draw_shear(const Sprite *s, int x, int y, bool flip, float vy)
{
    for (int c = 0; c < s->w; c++) {
        int cc = flip ? (s->w - 1 - c) : c;
        float nose = flip ? ((float)(s->w - 1 - c) - 7.5f) : ((float)c - 7.5f);   /* + toward the nose */
        int dy = (int)floorf(vy * nose * 0.11f + 0.5f);
        for (int r = 0; r < s->h; r++) {
            int v = hexval(s->rows[r][cc]);
            if (v > 0) gfx_pixel(x + c, y + r + dy, s->pal[v]);
        }
    }
}

/* draw centered on (cx, cy) */
static void spr_center(const Sprite *s, int cx, int cy, bool flip)
{
    spr_draw(s, s->pal, cx - s->w / 2, cy - s->h / 2, flip, false);
}

/* Draws a tiny ship, used for the spare ships in the top panel. */
void gfx_draw_ship_icon(int x, int y)
{
    spr_draw(&S_SHIP, S_SHIP.pal, x, y, false, true);
}

/* ======================================================================== */
/* Effects: particles, popups, shake                                        */
/* ======================================================================== */

typedef struct { float x, y, vx, vy; int life, max; uint8_t kind; } Particle;
typedef struct { float x, y; int life; char text[8]; } Popup;

#define MAXP 1100
#define MAXPOP 10
enum { K_FIRE = 0, K_CYAN, K_PINK, K_GREEN, K_DEBRIS, K_FLAME };

static Particle g_p[MAXP];
static Popup    g_pop[MAXPOP];
static int      g_shake, g_flash, g_planet_msg, g_bonus_msg;
static uint32_t g_fxrng = 12345;

/*
 * A random number for the visual effects only. It is kept apart from the
 * game's own generator, so the effects can never change how a game plays out.
 */
static uint32_t frnd(void)
{
    g_fxrng ^= g_fxrng << 13; g_fxrng ^= g_fxrng >> 17; g_fxrng ^= g_fxrng << 5;
    return g_fxrng;
}
/* A random decimal number from 0 up to 1, for effects. */
static float frndf(void) { return (float)(frnd() >> 8) * (1.0f / 16777216.0f); }

/*
 * Clears all particles, pop-up texts and screen effects. Called when a new
 * game starts.
 */
void gfx_fx_reset(void)
{
    memset(g_p, 0, sizeof g_p);
    memset(g_pop, 0, sizeof g_pop);
    g_shake = g_flash = g_planet_msg = g_bonus_msg = 0;
}

/*
 * Creates one particle (a speck of fire or debris) in the first free slot.
 * kind decides its colors.
 */
static void spawn_p(float x, float y, float vx, float vy, int life, int kind)
{
    for (int i = 0; i < MAXP; i++) {
        if (g_p[i].life > 0) continue;
        g_p[i] = (Particle){ x, y, vx, vy, life, life, (uint8_t)kind };
        return;
    }
}

/* Creates n particles at once that fly out in all directions, for explosions. */
static void burst(float x, float y, int n, float smin, float smax, int lmin, int lmax, int kind)
{
    for (int i = 0; i < n; i++) {
        float a = frndf() * TWO_PI;
        float s = smin + frndf() * (smax - smin);
        spawn_p(x, y, cosf(a) * s, sinf(a) * s, lmin + (int)(frndf() * (float)(lmax - lmin)), kind);
    }
}

/* Starts a small floating text, for example the points for saving a humanoid. */
static void popup(float x, float y, const char *text)
{
    for (int i = 0; i < MAXPOP; i++) {
        if (g_pop[i].life > 0) continue;
        g_pop[i].x = x; g_pop[i].y = y; g_pop[i].life = 50;
        snprintf(g_pop[i].text, sizeof g_pop[i].text, "%s", text);
        return;
    }
}

/*
 * Turns a game event into something to look at: explosions, screen shake, a
 * white flash, pop-up points. The game logic only reports what happened, and
 * this is where it gets a look.
 */
void gfx_fx_event(const Event *e, const Game *g)
{
    switch (e->type) {
    case EV_EXPLODE_SMALL: burst(e->x, e->y, 22, 0.4f, 2.6f, 16, 34, K_FIRE); break;
    case EV_EXPLODE_BIG:   burst(e->x, e->y, 60, 0.5f, 3.6f, 26, 52, K_FIRE); g_shake = g_shake < 3 ? 3 : g_shake; break;
    case EV_SMARTBOMB:
        burst(e->x, e->y, 110, 5.f, 9.f, 22, 32, K_CYAN);
        g_flash = 10; g_shake = 8;
        break;
    case EV_SHIP_DEATH:
        burst(e->x, e->y, 110, 0.5f, 4.5f, 30, 70, K_FIRE);
        burst(e->x, e->y, 30, 0.5f, 3.f, 40, 80, K_DEBRIS);
        g_shake = 10; g_flash = 4;
        break;
    case EV_PLANET_BLOW:
        g_planet_msg = 170; g_shake = 18; g_flash = 14;
        for (int i = 0; i < 420; i++) {
            float x = g->camx + frndf() * SCREEN_W;
            float y = terrain_y(x) - frndf() * 6.f;
            spawn_p(x, y, (frndf() - 0.5f) * 3.f, -(0.4f + frndf() * 4.6f), 40 + (int)(frndf() * 80.f), K_FIRE);
        }
        for (int i = 0; i < 120; i++) {
            float x = g->camx + frndf() * SCREEN_W;
            spawn_p(x, terrain_y(x), (frndf() - 0.5f) * 2.f, -(1.f + frndf() * 3.f), 60 + (int)(frndf() * 80.f), K_DEBRIS);
        }
        break;
    case EV_HUMANOID_DIE:  burst(e->x, e->y, 14, 0.4f, 1.6f, 16, 30, K_PINK); break;
    case EV_MUTANT:        burst(e->x, e->y, 20, 1.f, 2.5f, 14, 26, K_PINK); break;
    case EV_CATCH:         burst(e->x, e->y, 10, 0.3f, 1.4f, 20, 34, K_GREEN); popup(e->x, e->y - 10.f, "500"); break;
    case EV_LANDING:       burst(e->x, e->y, 8, 0.3f, 1.2f, 20, 34, K_GREEN);
                           popup(e->x, e->y - 12.f, e->arg ? "250" : "500"); break;
    case EV_HYPER:         burst(e->x, e->y, 26, 1.f, 4.f, 10, 22, K_CYAN); break;
    case EV_HYPER_BACK:    burst(e->x, e->y, 34, 1.f, 4.f, 12, 24, K_CYAN); break;
    case EV_RESPAWN:       burst(e->x, e->y, 30, 0.5f, 2.5f, 16, 30, K_CYAN); break;
    case EV_EXTRALIFE:     g_bonus_msg = 120; break;
    default: break;
    }
}

/*
 * Moves all particles and counts down the effects once per game step. Also
 * adds sparks behind the ship while it thrusts.
 */
void gfx_fx_step(const Game *g)
{
    for (int i = 0; i < MAXP; i++) {
        Particle *p = &g_p[i];
        if (p->life <= 0) continue;
        p->x = wrapx(p->x + p->vx);
        p->y += p->vy;
        p->vx *= 0.985f;
        p->vy *= 0.985f;
        if (p->kind == K_DEBRIS) p->vy += 0.06f;
        p->life--;
    }
    for (int i = 0; i < MAXPOP; i++) if (g_pop[i].life > 0) { g_pop[i].life--; g_pop[i].y -= 0.4f; }
    if (g_shake > 0 && (g->frame & 1)) g_shake--;
    if (g_flash > 0) g_flash--;
    if (g_planet_msg > 0) g_planet_msg--;
    if (g_bonus_msg > 0) g_bonus_msg--;

    const Ship *s = &g->ship;                      /* engine flame sparks */
    if (s->alive && s->hyper == 0 && s->thrusting) {
        float bx = s->x - (float)s->facing * 9.f;
        spawn_p(bx, s->y + (frndf() - 0.5f) * 2.f, -(float)s->facing * (0.8f + frndf()) + s->vx * 0.5f,
                (frndf() - 0.5f) * 0.4f, 8 + (int)(frndf() * 6.f), K_FLAME);
    }
}

/* ======================================================================== */
/* Background: sky, nebula, stars, mountains                                */
/* ======================================================================== */

#define NEB_W 512
static uint32_t g_neb[NEB_W * SCREEN_H];          /* ARGB, alpha = strength */
typedef struct { int16_t x, y; uint8_t flags; } Star;
#define STARS 70
static Star g_star[3][STARS];

static const uint8_t BAYER[4][4] = {
    { 0, 8, 2, 10}, {12, 4, 14, 6}, { 3, 11, 1, 9}, {15, 7, 13, 5}
};

#define BANDS 14
static uint32_t g_sky_band[2][BANDS + 2];         /* [planet?][band] */

void gfx_init(void)
{
    /* sky bands: a banded gradient, dithered between neighbours */
    static const uint32_t stops_world[4] = { RGB(6,5,26), RGB(26,12,62), RGB(86,30,96), RGB(168,66,84) };
    static const uint32_t stops_void[4]  = { RGB(2,2,10), RGB(10,5,26), RGB(30,10,46), RGB(52,14,46) };
    for (int v = 0; v < 2; v++) {
        const uint32_t *st = v == 0 ? stops_world : stops_void;
        for (int b = 0; b <= BANDS; b++) {
            float t = (float)b / (float)BANDS * 3.f;
            int i = (int)t; if (i > 2) i = 2;
            g_sky_band[v][b] = lerp_col(st[i], st[i + 1], t - (float)i);
        }
        g_sky_band[v][BANDS + 1] = g_sky_band[v][BANDS];
    }

    /* nebula: smooth sines whose frequencies divide NEB_W, so it tiles */
    for (int y = 0; y < SCREEN_H; y++) {
        float v = (float)y / (float)SCREEN_H * 3.14159f;
        for (int x = 0; x < NEB_W; x++) {
            float u = (float)x / (float)NEB_W * TWO_PI;
            float f = sinf(u * 2.f + sinf(v * 2.f) * 1.5f)
                    + sinf(u * 3.f + 1.f + v * 1.3f) * 0.7f
                    + sinf(u * 5.f - v * 2.f) * 0.4f;
            float a = (f - 0.3f) / 1.6f;
            if (a < 0.f) a = 0.f;
            if (a > 1.f) a = 1.f;
            a *= sinf(v) * 0.55f;
            float m = sinf(u * 4.f + v) * 0.5f + 0.5f;
            uint32_t col = lerp_col(RGB(130,40,180), RGB(40,120,210), m);
            g_neb[y * NEB_W + x] = (col & 0x00FFFFFFu) | ((uint32_t)(a * 120.f) << 24);
        }
    }

    uint32_t seed = 0xC0FFEE;
    for (int l = 0; l < 3; l++) {
        for (int i = 0; i < STARS; i++) {
            seed = seed * 1664525u + 1013904223u; g_star[l][i].x = (int16_t)((seed >> 8) % NEB_W);
            seed = seed * 1664525u + 1013904223u; g_star[l][i].y = (int16_t)(PLAY_TOP + (seed >> 8) % (SCREEN_H - PLAY_TOP));
            seed = seed * 1664525u + 1013904223u; g_star[l][i].flags = (uint8_t)((seed >> 8) & 3);
        }
    }
    gfx_fx_reset();
}

/*
 * Draws the whole background: the sky, a slow nebula, three layers of stars
 * and mountains. camx is where the camera is, and each layer moves at its own
 * speed, which gives the feeling of depth.
 */
void gfx_draw_space_bg(float camx, bool planet, int frame)
{
    const uint32_t *band = g_sky_band[planet ? 0 : 1];

    /* 1. dithered sky */
    for (int y = 0; y < SCREEN_H; y++) {
        float q = (float)y / (float)(SCREEN_H - 1) * (float)BANDS;
        int b = (int)q;
        float frac = q - (float)b;
        for (int x = 0; x < SCREEN_W; x++) {
            float th = ((float)BAYER[y & 3][x & 3] + 0.5f) / 16.f;
            g_fb[y * SCREEN_W + x] = band[b + (frac > th ? 1 : 0)];
        }
    }

    /* 2. nebula, very slow parallax */
    int nx = (int)(camx * 0.06f) % NEB_W;
    for (int y = PLAY_TOP; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            uint32_t n = g_neb[y * NEB_W + ((nx + x) % NEB_W)];
            int a = (int)(n >> 24);
            if (a > 3) g_fb[y * SCREEN_W + x] = mix(g_fb[y * SCREEN_W + x], n, a);
        }
    }

    /* 3. three star layers moving at different speeds */
    static const float spd[3] = { 0.10f, 0.22f, 0.42f };
    static const uint32_t col[3] = { RGB(95,95,150), RGB(160,160,215), RGB(235,235,255) };
    for (int l = 0; l < 3; l++) {
        for (int i = 0; i < STARS; i++) {
            float px = (float)g_star[l][i].x - camx * spd[l];
            int sx = (int)fmodf(px, (float)NEB_W);
            if (sx < 0) sx += NEB_W;
            if (sx >= SCREEN_W) continue;
            uint32_t c = col[l];
            if (g_star[l][i].flags == 0 && ((frame / 10 + i) & 3) == 0) c = RGB(255,255,255);
            gfx_pixel(sx, g_star[l][i].y, c);
            if (l == 2 && g_star[l][i].flags == 1) gfx_pixel(sx + 1, g_star[l][i].y, c);
        }
    }

    /* 4. distant mountains (only while the planet lives) */
    if (planet) {
        for (int c = 0; c < SCREEN_W; c++) {
            float wx = camx * 0.3f + (float)c;
            float t = wx / 640.f * TWO_PI;
            float h = 38.f + 14.f * sinf(t * 3.f) + 9.f * sinf(t * 7.f + 1.f) + 5.f * sinf(t * 19.f + 2.f);
            int top = 206 - (int)h;
            for (int y = top; y < SCREEN_H; y++) {
                float d = (float)(y - top) / 70.f;
                uint32_t cc = lerp_col(RGB(62,34,100), RGB(24,12,48), d);
                if (y == top) cc = RGB(120,80,160);
                gfx_pixel(c, y, cc);
            }
        }
    }
}

/* ======================================================================== */
/* Game scene                                                               */
/* ======================================================================== */

static int s_ox, s_oy;

/* Converts a position in the world to a position on the screen. */
static int sx_of(const Game *g, float wx) { return (int)floorf(wrapdx(g->camx, wx)) + s_ox; }

/*
 * Draws the ground column by column, in bands of color from a bright edge to
 * dark rock, with a shimmer along the top.
 */
static void draw_terrain(const Game *g, int frame)
{
    float t = (float)frame * 0.04f;
    for (int c = -2; c < SCREEN_W + 2; c++) {
        float wx = g->camx + (float)c;
        int ty = (int)terrain_y(wx) + s_oy;
        int sc = c + s_ox;
        float shimmer = sinf(wx * 0.05f + t) * 0.5f + 0.5f;
        for (int y = ty; y < SCREEN_H; y++) {
            int d = y - ty;
            uint32_t col;
            if (d == 0)      col = lerp_col(RGB(255,170,80), RGB(255,235,150), shimmer);
            else if (d < 3)  col = RGB(226,122,62);
            else if (d < 7)  col = RGB(168,84,52);
            else if (d < 13) col = RGB(118,58,50);
            else             col = RGB(72,36,56);
            /* little rock texture */
            uint32_t h = ((uint32_t)(wx * 0.5f) * 73856093u) ^ ((uint32_t)y * 19349663u);
            if (((h >> 7) & 15) == 0 && d > 1) col = lerp_col(col, RGB(20,10,30), 0.35f);
            gfx_pixel(sc, y, col);
        }
    }
}

/*
 * Draws one enemy with the right sprite and animation frame. Enemies far
 * outside the screen are skipped.
 */
static void draw_enemy(const Game *g, const Enemy *e, int frame)
{
    int x = sx_of(g, e->x), y = (int)e->y + s_oy;
    if (x < -20 || x > SCREEN_W + 20) return;
    switch (e->type) {
    case E_LANDER:  spr_center(((frame / 10) & 1) ? &S_LANDER_B : &S_LANDER, x, y + (int)(sinf(e->phase * 2.f) * 1.2f), false); break;
    case E_MUTANT: {
        const uint32_t *pal = ((frame / 4) & 1) ? pal_mutant_b : pal_mutant_a;
        spr_draw(&S_MUTANT, pal, x - 6, y - 6, (frame / 8) & 1, false);
        break;
    }
    case E_BAITER:  spr_center(&S_BAITER, x, y, e->vx < 0.f); break;
    case E_BOMBER:  spr_center(&S_BOMBER, x, y, e->vx < 0.f); break;
    case E_POD:     spr_center(&S_POD, x, y, false); break;
    case E_SWARMER: spr_center(&S_SWARMER, x, y, (frame / 6) & 1); break;
    default: break;
    }
}

/*
 * Draws the player's ship, with an engine flame when it thrusts. It blinks
 * while the ship is protected and is hidden during hyperspace.
 */
static void draw_ship(const Game *g, int frame)
{
    const Ship *s = &g->ship;
    if (!s->alive || s->hyper > 0) return;
    if (s->invuln > 0 && ((frame / 3) & 1)) return;           /* blink while protected */
    int x = sx_of(g, s->x), y = (int)s->y + s_oy;
    bool left = s->facing < 0;
    int f = left ? -1 : 1;

    if (s->thrusting) {                                         /* flame */
        int len = 6 + (frame % 3) * 2 + (int)(fabsf(s->vx) * 0.5f);
        for (int i = 0; i < len; i++) {
            int spread = (len - i) / 4;
            uint32_t c = i < len / 3 ? RGB(255,255,230) : (i < 2 * len / 3 ? RGB(255,220,80) : RGB(255,120,30));
            for (int j = -spread; j <= spread; j++) gfx_pixel(x - f * (9 + i), y + j, c);
        }
    }
    spr_draw_shear(&S_SHIP, x - 8, y - 4, left, s->vy);
}

/*
 * Draws one laser shot as a bright line with a glow that fades towards the
 * tail.
 */
static void draw_laser(const Game *g, const Shot *sh)
{
    int x = sx_of(g, sh->x), y = (int)sh->y + s_oy;
    int dir = sh->vx >= 0.f ? 1 : -1;
    int len = 26;
    for (int i = 0; i < len; i++) {
        float t = (float)i / (float)len;
        uint32_t core = lerp_col(RGB(255,255,255), RGB(120,230,255), t);
        uint32_t halo = lerp_col(RGB(80,200,255), RGB(20,60,140), t);
        int px = x - dir * i;
        gfx_pixel(px, y, core);
        gfx_pixel(px, y - 1, halo);
        gfx_pixel(px, y + 1, halo);
    }
}

/*
 * Draws the top panel: score, spare ships, smart bombs, wave number, humanoids
 * left and the radar that shows the whole world.
 */
static void draw_hud(const Game *g, int frame)
{
    /* panel */
    for (int y = 0; y < PLAY_TOP - 1; y++) {
        uint32_t c = lerp_col(RGB(8,10,34), RGB(4,5,18), (float)y / (float)PLAY_TOP);
        gfx_hline(0, SCREEN_W - 1, y, c);
    }
    gfx_hline(0, SCREEN_W - 1, PLAY_TOP - 1, RGB(60,90,200));
    gfx_hline(0, SCREEN_W - 1, PLAY_TOP - 2, RGB(24,36,110));

    /* score */
    char buf[32];
    snprintf(buf, sizeof buf, "%06u", g->score);
    gfx_text_grad(8, 5, buf, 2, RGB(255,245,170), RGB(255,160,60));

    /* spare ships and smart bombs */
    for (int i = 0; i < g->lives && i < 6; i++) gfx_draw_ship_icon(8 + i * 10, 21);
    for (int i = 0; i < g->bombs && i < 9; i++) spr_draw(&S_BOMBICON, S_BOMBICON.pal, 8 + i * 7, 25, false, false);

    /* wave and humanoids */
    snprintf(buf, sizeof buf, "WAVE %d", g->wave);
    gfx_text(240, 5, buf, RGB(150,220,255), 1);
    int hum = game_humanoids_alive(g);
    for (int i = 0; i < hum && i < HUMANOID_COUNT; i++) {
        spr_draw(&S_HUMAN, S_HUMAN.pal, 240 + i * 7, 15, false, true);
    }
    if (hum == 0) gfx_text(240, 17, "NONE LEFT", RGB(255,90,90), 1);

    /* radar */
    const int rx = 100, ry = 4, rw = 120, rh = 24;
    gfx_rect(rx, ry, rw, rh, RGB(2,8,22));
    gfx_rect_outline(rx - 1, ry - 1, rw + 2, rh + 2, RGB(50,170,210));
    gfx_rect_outline(rx - 2, ry - 2, rw + 4, rh + 4, RGB(22,70,110));
    if (g->planet_alive) {
        for (int x = 0; x < rw; x++) {
            float wx = (float)x / (float)rw * WORLD_W;
            int yy = ry + (int)((terrain_y(wx) - PLAY_TOP) / (float)(SCREEN_H - PLAY_TOP) * (float)rh);
            gfx_pixel(rx + x, yy, RGB(110,70,60));
            gfx_pixel(rx + x, yy + 1, RGB(70,40,50));
        }
    }
    float scale = (float)rw / (float)WORLD_W;
    /* view window */
    int vx0 = rx + (int)(wrapx(g->camx) * scale);
    int vw = (int)(SCREEN_W * scale);
    for (int k = 0; k < 2; k++) {
        int bx = vx0 - k * rw;
        if (bx + vw < rx || bx > rx + rw) continue;
        for (int i = 0; i <= vw; i++) {
            int px = bx + i;
            if (px < rx || px >= rx + rw) continue;
            gfx_pixel(px, ry, RGB(200,220,255));
            gfx_pixel(px, ry + rh - 1, RGB(200,220,255));
        }
    }
    #define RADAR_XY(wx, wy, px, py) \
        int px = rx + (int)(wrapx(wx) * scale); \
        int py = ry + 1 + (int)(((wy) - PLAY_TOP) / (float)(SCREEN_H - PLAY_TOP) * (float)(rh - 3)); \
        if (px >= rx + rw) { px = rx + rw - 1; } \
        if (py < ry) { py = ry; } \
        if (py > ry + rh - 3) { py = ry + rh - 3; }
    for (int i = 0; i < HUMANOID_COUNT; i++) {
        const Humanoid *h = &g->hum[i];
        if (h->state == H_DEAD) continue;
        RADAR_XY(h->x, h->y, px, py)
        gfx_pixel(px, py, RGB(190,255,255)); gfx_pixel(px, py + 1, RGB(190,255,255));
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &g->en[i];
        if (!e->alive) continue;
        uint32_t c = RGB(255,255,255);
        switch (e->type) {
        case E_LANDER:  c = RGB(90,255,90); break;
        case E_MUTANT:  c = RGB(255,70,230); break;
        case E_BAITER:  c = RGB(255,60,60); break;
        case E_BOMBER:  c = RGB(90,150,255); break;
        case E_POD:     c = RGB(255,160,40); break;
        case E_SWARMER: c = RGB(255,235,70); break;
        default: break;
        }
        RADAR_XY(e->x, e->y, px, py)
        gfx_rect(px, py, 2, 2, c);
    }
    if (g->ship.alive && ((frame / 6) & 1) == 0) {
        RADAR_XY(g->ship.x, g->ship.y, px, py)
        gfx_rect(px - 1, py, 3, 2, RGB(255,255,255));
    }
    #undef RADAR_XY
}

/*
 * Draws the big messages on top of the game: wave number, wave complete, game
 * over, planet destroyed, extra ship, and the floating point texts.
 */
static void draw_overlays(const Game *g, int frame)
{
    char buf[64];
    if (g->phase == PH_PLAY && g->banner_timer > 0) {
        bool vis = g->banner_timer > 30 || ((g->banner_timer / 3) & 1);
        if (vis) {
            snprintf(buf, sizeof buf, "WAVE %d", g->wave);
            gfx_amiga_center(80, buf, RAMP_GOLD, 1, 2);
            if (g->wave == 1) gfx_text_center(116, "PROTECT THE HUMANOIDS", RGB(150,230,255), 1);
        }
    }
    if (g->phase == PH_CLEAR) {
        snprintf(buf, sizeof buf, "WAVE %d COMPLETE", g->wave);
        gfx_title(66, buf, RAMP_GOLD);
        snprintf(buf, sizeof buf, "HUMANOIDS SAVED  %d", g->clear_humanoids);
        gfx_text_center(100, buf, RGB(190,255,255), 1);
        snprintf(buf, sizeof buf, "BONUS  %d", g->clear_bonus);
        gfx_text_center(114, buf, RGB(255,235,120), 1);
    }
    if (g->phase == PH_OVER) {
        gfx_amiga_center(84, "GAME OVER", RAMP_RED, 1, 2);
    }
    if (g_planet_msg > 0 && ((frame / 6) & 1) == 0) {
        gfx_title(52, "PLANET DESTROYED", RAMP_RED);
    }
    if (g_bonus_msg > 0) {
        gfx_text_center(46, "EXTRA SHIP AND SMART BOMB", RGB(255,240,120), 1);
    }
    for (int i = 0; i < MAXPOP; i++) {
        if (g_pop[i].life <= 0) continue;
        int x = sx_of(g, g_pop[i].x) - gfx_text_width(g_pop[i].text, 1) / 2;
        gfx_text_shadow(x, (int)g_pop[i].y + s_oy, g_pop[i].text, RGB(255,255,150), 1);
    }
}

/*
 * The color of a particle. It changes as the particle ages, for example from
 * white through yellow and orange to dark red for fire.
 */
static uint32_t particle_color(const Particle *p)
{
    float f = (float)p->life / (float)p->max;
    switch (p->kind) {
    case K_CYAN:
        return f > 0.6f ? RGB(255,255,255) : (f > 0.3f ? RGB(120,235,255) : RGB(40,120,230));
    case K_PINK:
        return f > 0.6f ? RGB(255,230,255) : (f > 0.3f ? RGB(255,100,230) : RGB(150,30,150));
    case K_GREEN:
        return f > 0.6f ? RGB(240,255,230) : (f > 0.3f ? RGB(110,255,120) : RGB(30,150,60));
    case K_DEBRIS:
        return f > 0.5f ? RGB(190,110,70) : RGB(110,60,50);
    case K_FLAME:
        return f > 0.6f ? RGB(255,255,200) : (f > 0.3f ? RGB(255,170,50) : RGB(200,60,30));
    default:
        if (f > 0.8f) return RGB(255,255,255);
        if (f > 0.6f) return RGB(255,240,110);
        if (f > 0.35f) return RGB(255,150,40);
        if (f > 0.15f) return RGB(220,50,25);
        return RGB(100,25,25);
    }
}

/*
 * Draws one whole picture of the game: background, ground, humanoids, mines,
 * enemies, bullets, lasers, ship, particles, flash and then the panel and
 * messages. Screen shake moves the world but never the panel.
 */
void gfx_draw_game(const Game *g, int frame)
{
    s_ox = s_oy = 0;
    if (g_shake > 0) {
        s_ox = (int)(frnd() % (uint32_t)(2 * g_shake + 1)) - g_shake;
        s_oy = (int)(frnd() % (uint32_t)(2 * g_shake + 1)) - g_shake;
        if (s_oy > 3) s_oy = 3;
        if (s_oy < -3) s_oy = -3;
    }

    gfx_draw_space_bg(g->camx + (float)s_ox, g->planet_alive, frame);
    if (g->planet_alive) draw_terrain(g, frame);

    /* humanoids */
    for (int i = 0; i < HUMANOID_COUNT; i++) {
        const Humanoid *h = &g->hum[i];
        if (h->state == H_DEAD) continue;
        int x = sx_of(g, h->x), y = (int)h->y + s_oy;
        if (x < -10 || x > SCREEN_W + 10) continue;
        bool waving = (h->state == H_GRABBED || h->state == H_FALLING) && ((frame / 5) & 1);
        spr_center(waving ? &S_HUMAN_UP : &S_HUMAN, x, y, false);
    }
    for (int i = 0; i < MAX_MINES; i++) {
        const Mine *m = &g->mine[i];
        if (!m->active) continue;
        if (m->life < 120 && ((m->life / 4) & 1)) continue;        /* blink before it vanishes */
        spr_center(&S_MINE, sx_of(g, m->x), (int)m->y + s_oy, false);
    }
    for (int i = 0; i < MAX_ENEMIES; i++) if (g->en[i].alive) draw_enemy(g, &g->en[i], frame);
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet *b = &g->eb[i];
        if (!b->active) continue;
        int x = sx_of(g, b->x), y = (int)b->y + s_oy;
        uint32_t c = ((frame / 2) & 1) ? RGB(255,255,255) : RGB(255,220,70);
        gfx_rect(x - 1, y - 1, 3, 3, c);
        gfx_pixel(x, y, RGB(255,90,40));
    }
    for (int i = 0; i < MAX_PLAYER_SHOTS; i++) if (g->shot[i].active) draw_laser(g, &g->shot[i]);
    draw_ship(g, frame);

    /* particles */
    for (int i = 0; i < MAXP; i++) {
        const Particle *p = &g_p[i];
        if (p->life <= 0) continue;
        int x = sx_of(g, p->x), y = (int)p->y + s_oy;
        if ((unsigned)x >= SCREEN_W || y < PLAY_TOP || y >= SCREEN_H) continue;
        uint32_t c = particle_color(p);
        bool big = (p->kind == K_FIRE || p->kind == K_CYAN) && p->life * 2 > p->max;
        gfx_pixel(x, y, c);
        if (big) { gfx_pixel(x + 1, y, c); gfx_pixel(x, y + 1, c); gfx_pixel(x + 1, y + 1, c); }
    }

    if (g_flash > 0) gfx_blend_rect(0, PLAY_TOP, SCREEN_W, SCREEN_H - PLAY_TOP, RGB(255,255,255), g_flash * 14);

    s_ox = s_oy = 0;                    /* the HUD never shakes */
    draw_hud(g, frame);
    draw_overlays(g, frame);
}

/* ======================================================================== */
/* Amiga-style fonts                                                        */
/* Built once from the 5x7 font: make the strokes one pixel bolder, and for */
/* the big size smooth the corners with the EPX scaling rule (the one used  */
/* by many 16-bit era emulators), which turns blocky diagonals into the     */
/* rounded shapes that Amiga title screens had.                             */
/* ======================================================================== */

typedef struct { uint8_t w, h; uint8_t px[14][12]; } Glyph;
static Glyph g_big[59], g_small[59];
static bool  g_font_ready;

/* eight colors per ramp, top of the letter to the bottom; the fourth is a
   bright line, which is what makes the letters look like polished metal */
static const uint32_t RAMPS[RAMP_COUNT][8] = {
    { RGB(255,255,215), RGB(255,240,130), RGB(255,208,64),  RGB(255,255,235), RGB(236,150,32),  RGB(196,96,20),  RGB(136,58,16),  RGB(84,32,12) },   /* gold  */
    { RGB(235,255,255), RGB(150,235,255), RGB(70,180,255),  RGB(240,255,255), RGB(40,120,235),  RGB(30,80,190),  RGB(24,48,130),  RGB(16,28,84) },   /* ice   */
    { RGB(255,235,215), RGB(255,170,130), RGB(255,96,70),   RGB(255,225,200), RGB(220,40,40),   RGB(170,24,40),  RGB(112,16,40),  RGB(64,10,30) },   /* red   */
    { RGB(255,255,255), RGB(240,240,250), RGB(215,215,235), RGB(255,255,255), RGB(170,170,205), RGB(130,130,170), RGB(90,90,130),  RGB(54,54,90) },   /* white */
    { RGB(230,255,215), RGB(150,255,130), RGB(80,220,90),   RGB(235,255,225), RGB(40,170,70),   RGB(24,120,60),  RGB(16,80,50),   RGB(10,48,36) },   /* green */
    { RGB(150,160,215), RGB(125,135,195), RGB(105,115,175), RGB(160,170,225), RGB(85,95,155),   RGB(70,78,135),  RGB(55,62,110),  RGB(40,46,86) },   /* dim   */
};

/*
 * Builds the two big fonts from the plain 5x7 one when first needed: the
 * strokes are made bolder, and for the big size the corners are smoothed with
 * the EPX rule so diagonals look round.
 */
static void font_build(void)
{
    for (int gi = 0; gi < 59; gi++) {
        /* 6x7 bold source: a pixel is set if it, or its left neighbour, is set in the 5x7 font */
        uint8_t t[7][6] = {{0}};
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if ((FONT[gi][col] >> row) & 1) { t[row][col] = 1; t[row][col + 1] = 1; }
        Glyph *sm = &g_small[gi];
        sm->w = 6; sm->h = 7;
        for (int y = 0; y < 7; y++) for (int x = 0; x < 6; x++) sm->px[y][x] = t[y][x];

        Glyph *bg = &g_big[gi];
        bg->w = 12; bg->h = 14;
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 6; x++) {
                uint8_t P = t[y][x];
                uint8_t A = y > 0 ? t[y - 1][x] : 0, D = y < 6 ? t[y + 1][x] : 0;
                uint8_t C = x > 0 ? t[y][x - 1] : 0, B = x < 5 ? t[y][x + 1] : 0;
                uint8_t o1 = P, o2 = P, o3 = P, o4 = P;
                if (C == A && C != D && A != B) o1 = A;
                if (A == B && A != C && B != D) o2 = B;
                if (D == C && D != B && C != A) o3 = C;
                if (B == D && B != A && D != C) o4 = D;
                bg->px[y * 2][x * 2] = o1;     bg->px[y * 2][x * 2 + 1] = o2;
                bg->px[y * 2 + 1][x * 2] = o3; bg->px[y * 2 + 1][x * 2 + 1] = o4;
            }
        }
    }
    g_font_ready = true;
}

/*
 * Finds the letter to draw (small or big). Builds the fonts on first use.
 * Returns NULL for characters that are not in the font.
 */
static const Glyph *glyph_of(char ch, int big)
{
    if (!g_font_ready) font_build();
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    if (ch < 32 || ch > 90) return NULL;
    return big ? &g_big[ch - 32] : &g_small[ch - 32];
}

/*
 * Draws one square of size sc. A pixel of the font is drawn as such a block
 * when the font is scaled up.
 */
static void block(int x, int y, int sc, uint32_t c) { gfx_rect(x, y, sc, sc, c); }

/* pass 0 = shadow, 1 = outline, 2 = fill */
static void amiga_pass(int x, int y, const char *s, int ramp, int big, int sc, int pass, int frame, int amp)
{
    int adv = (big ? 13 : 7) * sc;
    int gx = x;
    for (int i = 0; *s; s++, i++, gx += adv) {
        const Glyph *g = glyph_of(*s, big);
        if (!g) continue;
        int oy = amp ? (int)floorf(sinf((float)frame * 0.09f + (float)i * 0.55f) * (float)amp + 0.5f) : 0;
        for (int py = 0; py < g->h; py++) {
            for (int px = 0; px < g->w; px++) {
                if (!g->px[py][px]) continue;
                int bx = gx + px * sc, by = y + oy + py * sc;
                if (pass == 0) {
                    block(bx + sc * 2, by + sc * 2, sc, RGB(6,4,20));
                } else if (pass == 1) {
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++)
                            if (dx || dy) block(bx + dx * sc, by + dy * sc, sc, RGB(14,8,34));
                } else {
                    int idx = py * 8 / g->h;
                    block(bx, by, sc, RAMPS[ramp][idx]);
                }
            }
        }
    }
}

/* How wide a text will be in the chrome font, in pixels. */
int gfx_amiga_width(const char *s, int big, int scale)
{
    int n = (int)strlen(s);
    return n ? (n * (big ? 13 : 7) - 1) * scale : 0;
}

/*
 * Draws chrome lettering where every letter bobs up and down on a wave. amp is
 * how far, frame moves the wave.
 */
void gfx_amiga_wave(int x, int y, const char *s, int ramp, int big, int sc, int frame, int amp)
{
    for (int pass = 0; pass < 3; pass++) amiga_pass(x, y, s, ramp, big, sc, pass, frame, amp);
}

/* Draws chrome lettering that stands still. */
void gfx_amiga_text(int x, int y, const char *s, int ramp, int big, int sc)
{
    gfx_amiga_wave(x, y, s, ramp, big, sc, 0, 0);
}

/* Draws chrome lettering centered on the screen. */
void gfx_amiga_center(int y, const char *s, int ramp, int big, int sc)
{
    gfx_amiga_text((SCREEN_W - gfx_amiga_width(s, big, sc)) / 2, y, s, ramp, big, sc);
}

/* Draws a big centered heading in chrome lettering. */
void gfx_title(int y, const char *s, int ramp) { gfx_amiga_center(y, s, ramp, 1, 1); }

/*
 * Draws one menu line. The chosen line is gold with little arrows that bob,
 * the others are dim.
 */
void gfx_menu_item(int y, const char *s, bool selected, int frame)
{
    int w = gfx_amiga_width(s, 0, 1);
    int x = (SCREEN_W - w) / 2;
    gfx_amiga_text(x, y, s, selected ? RAMP_GOLD : RAMP_DIM, 0, 1);
    if (selected) {
        int bob = (frame / 8) & 1;
        gfx_amiga_text(x - 14 + bob, y, ">", RAMP_ICE, 0, 1);
        gfx_amiga_text(x + w + 8 - bob, y, "<", RAMP_ICE, 0, 1);
    }
}

/* ======================================================================== */
/* Raster effects                                                           */
/* ======================================================================== */

/* "Copper bars": soft horizontal bands that glide up and down, the classic
 * trick of the Amiga's raster coprocessor. */
void gfx_copper_bars(int y, int h, int frame)
{
    static const uint32_t tint[5] = { RGB(255,60,60), RGB(255,170,40), RGB(60,120,255), RGB(60,230,255), RGB(200,70,255) };
    const int bar_h = 14;
    for (int b = 0; b < 5; b++) {
        float ph = (float)frame * 0.022f + (float)b * 1.25f;
        int cy = y + h / 2 + (int)(sinf(ph) * (float)(h / 2 - bar_h / 2));
        for (int r = 0; r < bar_h; r++) {
            float k = sinf((float)(r + 1) / (float)(bar_h + 1) * 3.14159f);
            uint32_t c = lerp_col(RGB(10,6,30), tint[b], k);
            gfx_blend_rect(0, cy - bar_h / 2 + r, SCREEN_W, 1, c, (int)(40.f + k * 120.f));
        }
    }
}

/* How many frames it takes for the scrolling text to pass once. */
int gfx_scroller_length(const char *text) { return (int)((strlen(text) * 7 + SCREEN_W) / 2); }

/* A text that scrolls sideways while each letter rides a sine wave. */
void gfx_sine_scroller(int y, const char *text, int frame)
{
    int len = (int)strlen(text);
    int total = len * 7 + SCREEN_W;
    int x0 = SCREEN_W - (frame * 2) % total;
    for (int i = 0; i < len; i++) {
        int x = x0 + i * 7;
        if (x < -8 || x > SCREEN_W) continue;
        int yy = y + (int)floorf(sinf((float)x * 0.035f + (float)frame * 0.07f) * 5.f + 0.5f);
        char one[2] = { text[i], 0 };
        int ramp = ((x / 40) + frame / 20) % 3 == 0 ? RAMP_ICE : (((x / 40) + frame / 20) % 3 == 1 ? RAMP_GOLD : RAMP_WHITE);
        gfx_amiga_text(x, yy, one, ramp, 0, 1);
    }
}

/* ======================================================================== */
/* Actors (used by the idle parade) and the logo                            */
/* ======================================================================== */

static void spr_draw_scaled(const Sprite *s, const uint32_t *pal, int cx, int cy, bool flip, int sc)
{
    int x0 = cx - s->w * sc / 2, y0 = cy - s->h * sc / 2;
    for (int r = 0; r < s->h; r++)
        for (int c = 0; c < s->w; c++) {
            int v = hexval(s->rows[r][flip ? (s->w - 1 - c) : c]);
            if (v <= 0) continue;
            gfx_rect(x0 + c * sc, y0 + r * sc, sc, sc, pal[v]);
        }
}

/*
 * Draws one of the game's characters at any size, with its animation. Used by
 * the parade, the splash and similar screens.
 */
void gfx_draw_actor(ActorKind k, int cx, int cy, int frame, bool flip, int sc)
{
    switch (k) {
    case ACTOR_SHIP:    spr_draw_scaled(&S_SHIP, S_SHIP.pal, cx, cy, flip, sc);
                        if (frame % 3) gfx_rect(cx + (flip ? 1 : -1) * (9 * sc) - (flip ? 0 : 3 * sc), cy - sc, 3 * sc, 2 * sc, RGB(255,200,70));
                        break;
    case ACTOR_LANDER:  spr_draw_scaled(((frame / 10) & 1) ? &S_LANDER_B : &S_LANDER, S_LANDER.pal, cx, cy, flip, sc); break;
    case ACTOR_MUTANT:  spr_draw_scaled(&S_MUTANT, ((frame / 4) & 1) ? pal_mutant_b : pal_mutant_a, cx, cy, flip, sc); break;
    case ACTOR_BAITER:  spr_draw_scaled(&S_BAITER, S_BAITER.pal, cx, cy, flip, sc); break;
    case ACTOR_BOMBER:  spr_draw_scaled(&S_BOMBER, S_BOMBER.pal, cx, cy, flip, sc); break;
    case ACTOR_POD:     spr_draw_scaled(&S_POD, S_POD.pal, cx, cy, flip, sc); break;
    case ACTOR_SWARMER: spr_draw_scaled(&S_SWARMER, S_SWARMER.pal, cx, cy, ((frame / 6) & 1) != 0, sc); break;
    case ACTOR_HUMAN:   spr_draw_scaled(((frame / 8) & 1) ? &S_HUMAN_UP : &S_HUMAN, S_HUMAN.pal, cx, cy, flip, sc); break;
    case ACTOR_MINE:    spr_draw_scaled(&S_MINE, S_MINE.pal, cx, cy, flip, sc); break;
    default: break;
    }
}

/*
 * Draws the SHOOTER UPPER logo with bobbing letters and a bright glint that
 * sweeps across.
 */
void gfx_draw_logo(int y, int frame)
{
    /* each letter bobs on its own sine phase, gold on top, ice blue below */
    gfx_amiga_wave((SCREEN_W - gfx_amiga_width("SHOOTER", 1, 2)) / 2, y, "SHOOTER", RAMP_GOLD, 1, 2, frame, 3);
    gfx_amiga_wave((SCREEN_W - gfx_amiga_width("UPPER", 1, 3)) / 2, y + 34, "UPPER", RAMP_ICE, 1, 3, frame + 20, 3);
    /* a glint that sweeps across the lettering */
    int gx = ((frame * 3) % 520) - 100;
    for (int i = 0; i < 40; i++) {
        int x = gx + i;
        for (int yy = y; yy < y + 80 && yy < SCREEN_H; yy++) {
            if (x < 0 || x >= SCREEN_W) break;
            uint32_t px = g_fb[yy * SCREEN_W + x];
            int r = (px >> 16) & 255, g = (px >> 8) & 255;
            if (r > 150 || g > 170) g_fb[yy * SCREEN_W + x] = mix(px, RGB(255,255,255), 150 - i * 3 > 0 ? 150 - i * 3 : 0);
        }
    }
}

/* ======================================================================== */
/* start-up splash: warp stars, the title slides in, credits type themselves */
/* ======================================================================== */

/* t is the number of frames since the splash began (0 to SPLASH_FRAMES). */
void gfx_draw_splash(int t)
{
    const float cx = SCREEN_W / 2.f, cy = 62.f;
    gfx_clear(RGB(2, 2, 14));

    /* 1. a star field flying towards us, slowing down as the title arrives */
    float speed = 0.4f + (t < 140 ? 7.f * (1.f - (float)t / 140.f) : 0.f);
    float travel = t < 140 ? 0.4f * (float)t + 7.f * ((float)t - (float)(t * t) / 280.f)
                           : 0.4f * 140.f + 7.f * 70.f + 0.4f * (float)(t - 140);
    for (int j = 0; j < 110; j++) {
        float ang = (float)j * 2.399963f;                 /* golden angle spreads them evenly */
        float k = 0.6f + (float)(j % 7) * 0.25f;          /* nearer stars move faster */
        float d = fmodf(6.f + (float)((j * 37) % 100) + travel * k, 230.f);
        float dx = cosf(ang), dy = sinf(ang);
        int br = 60 + (int)(d * 0.9f);
        if (br > 255) br = 255;
        int len = (int)(speed * k * 1.6f);
        for (int s = 0; s <= len; s++) {                   /* the streak behind the star */
            float dd = d - (float)s;
            if (dd < 0.f) break;
            int x = (int)(cx + dx * dd * 1.4f), y = (int)(cy + dy * dd);
            int b2 = br - s * 14;
            if (b2 < 20) b2 = 20;
            gfx_pixel(x, y, RGB(b2, b2, b2 > 235 ? 255 : b2 + 20));
        }
    }

    /* 2. the two words slide in from opposite sides and settle with a flash */
    const int logo_y = 30;
    if (t >= 40) {
        float u = (float)(t - 40) / 70.f;
        if (u > 1.f) u = 1.f;
        float e = 1.f - (1.f - u) * (1.f - u) * (1.f - u);
        int w1 = gfx_amiga_width("SHOOTER", 1, 2), w2 = gfx_amiga_width("UPPER", 1, 3);
        int x1 = (int)(-(float)w1 + e * (float)((SCREEN_W - w1) / 2 + w1));
        int x2 = (int)((float)SCREEN_W - e * (float)(SCREEN_W - (SCREEN_W - w2) / 2));
        if (t < 110) {
            gfx_amiga_wave(x1, logo_y, "SHOOTER", RAMP_GOLD, 1, 2, t, 3);
            gfx_amiga_wave(x2, logo_y + 34, "UPPER", RAMP_ICE, 1, 3, t + 20, 3);
        } else {
            gfx_draw_logo(logo_y, t);                      /* same letters, now with the glint */
        }
    }
    if (t >= 110 && t < 140) {                             /* flash and a ring that spreads */
        int a = 150 - (t - 110) * 5;
        if (a > 0) gfx_blend_rect(0, 0, SCREEN_W, SCREEN_H, RGB(255, 255, 255), a);
        float r = (float)(t - 110) * 7.f;
        for (int i = 0; i < 160; i++) {
            float th = (float)i * 6.2831853f / 160.f;
            gfx_pixel((int)(cx + cosf(th) * r * 1.4f), (int)(cy + 14.f + sinf(th) * r), RGB(255, 235, 150));
        }
    }

    /* 3. the ship crosses the screen and the credits type themselves in */
    if (t >= 130) {
        int n = (t - 130) / 2;
        char buf[48];
        static const char *const L1 = "AN IDEA BY MARKO TAHVANAINEN";
        static const char *const L2 = "DESIGNED AND BUILT BY CLAUDE";
        static const char *const L3 = "COPYRIGHT 2026 MARKO TAHVANAINEN AND CLAUDE";
        snprintf(buf, sizeof buf, "%.*s", n, L1);
        gfx_text(( SCREEN_W - gfx_text_width(L1, 1)) / 2, 142, buf, RGB(255, 225, 120), 1);
        snprintf(buf, sizeof buf, "%.*s", n > 30 ? n - 30 : 0, L2);
        gfx_text((SCREEN_W - gfx_text_width(L2, 1)) / 2, 154, buf, RGB(130, 225, 255), 1);
        snprintf(buf, sizeof buf, "%.*s", n > 60 ? n - 60 : 0, L3);
        gfx_text((SCREEN_W - gfx_text_width(L3, 1)) / 2, 176, buf, RGB(120, 130, 190), 1);
    }
    if (t >= 150 && t < 240) {
        int sx = -20 + (t - 150) * 4;
        int sy = 124 + (int)(sinf((float)t * 0.15f) * 4.f);
        for (int i = 1; i < 14; i++)                       /* engine trail */
            gfx_rect(sx - i * 3 - 8, sy + (i & 1), 3, 1, RGB(255, 170 - i * 8 > 0 ? 170 - i * 8 : 0, 40));
        gfx_draw_actor(ACTOR_SHIP, sx, sy, t, false, 2);
    }
    if (t >= 170 && ((t / 24) & 1) == 0)
        gfx_text_center(204, "PRESS ANY KEY", RGB(190, 200, 245), 1);

    /* 4. fade to black at the end */
    if (t > SPLASH_FRAMES - 24) {
        int a = (t - (SPLASH_FRAMES - 24)) * 11;
        gfx_blend_rect(0, 0, SCREEN_W, SCREEN_H, RGB(0, 0, 0), a > 255 ? 255 : a);
    }
}

/* ======================================================================== */
/* CRT effect: curved screen, scanlines, vignette                           */
/* ======================================================================== */

static int32_t g_crt_src[SCREEN_W * SCREEN_H];
static uint8_t g_crt_lum[SCREEN_W * SCREEN_H];
static bool    g_crt_ready;

/*
 * Works out, once, where each screen pixel should read from to bend the
 * picture like an old tube, and how dark it should be (scanlines and darker
 * corners).
 */
static void crt_build(void)
{
    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            float u = ((float)x + 0.5f) / (float)SCREEN_W * 2.f - 1.f;
            float v = ((float)y + 0.5f) / (float)SCREEN_H * 2.f - 1.f;
            float r2 = u * u + v * v;
            float su = u * (1.f + 0.045f * r2), sv = v * (1.f + 0.06f * r2);
            int sx = (int)((su * 0.5f + 0.5f) * (float)SCREEN_W);
            int sy = (int)((sv * 0.5f + 0.5f) * (float)SCREEN_H);
            int i = y * SCREEN_W + x;
            if (sx < 0 || sx >= SCREEN_W || sy < 0 || sy >= SCREEN_H) { g_crt_src[i] = -1; g_crt_lum[i] = 0; continue; }
            g_crt_src[i] = sy * SCREEN_W + sx;
            float vig = 1.f - 0.22f * r2;
            float scan = (y & 1) ? 0.80f : 1.0f;
            float l = vig * scan * 255.f;
            g_crt_lum[i] = (uint8_t)(l < 0.f ? 0.f : (l > 255.f ? 255.f : l));
        }
    }
    g_crt_ready = true;
}

/*
 * Makes the old TV look: copies the picture into dst with a curve, dark
 * scanlines and a soft vignette. The original picture is left alone.
 */
void gfx_crt(uint32_t *dst)
{
    if (!g_crt_ready) crt_build();
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        int s = g_crt_src[i];
        if (s < 0) { dst[i] = RGB(0,0,0); continue; }
        uint32_t c = g_fb[s];
        uint32_t l = g_crt_lum[i];
        uint32_t r = ((c >> 16) & 255) * l / 255, gg = ((c >> 8) & 255) * l / 255, b = (c & 255) * l / 255;
        /* brighten slightly to make up for the dark scanlines */
        r = r * 9 / 8; gg = gg * 9 / 8; b = b * 9 / 8;
        if (r > 255) r = 255;
        if (gg > 255) gg = 255;
        if (b > 255) b = 255;
        dst[i] = RGB(r, gg, b);
    }
}

/* ======================================================================== */
/* PNG writer (uncompressed), for screenshots and tests                     */
/* ======================================================================== */

static uint32_t crc_table[256];
/* Builds the table the PNG checksum (CRC32) needs. */
static void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}
/* Adds some bytes to a running CRC32 checksum. */
static uint32_t crc_update(uint32_t crc, const uint8_t *buf, size_t len)
{
    uint32_t c = crc;
    for (size_t i = 0; i < len; i++) c = crc_table[(c ^ buf[i]) & 255] ^ (c >> 8);
    return c;
}
/*
 * Writes a 32-bit number to the file, most significant byte first, as PNG
 * wants it.
 */
static void put32(FILE *f, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    fwrite(b, 1, 4, f);
}
/* Writes one PNG chunk: length, type, data and checksum. */
static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    put32(f, len);
    fwrite(type, 1, 4, f);
    if (len) fwrite(data, 1, len, f);
    uint32_t crc = crc_update(0xFFFFFFFFu, (const uint8_t *)type, 4);
    if (len) crc = crc_update(crc, data, len);
    put32(f, crc ^ 0xFFFFFFFFu);
}

/*
 * Saves the picture as a PNG file, without compression (simple and always
 * correct). Used for screenshots with F12. Returns false if the file could not
 * be written.
 */
bool gfx_save_png(const char *path, const uint32_t *px)
{
    crc_init();
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    const int w = SCREEN_W, h = SCREEN_H;
    size_t raw_len = (size_t)(w * 3 + 1) * (size_t)h;
    uint8_t *raw = malloc(raw_len);
    if (!raw) { fclose(f); return false; }
    for (int y = 0; y < h; y++) {
        raw[y * (w * 3 + 1)] = 0;
        for (int x = 0; x < w; x++) {
            uint32_t c = px[y * w + x];
            uint8_t *o = &raw[y * (w * 3 + 1) + 1 + x * 3];
            o[0] = (uint8_t)(c >> 16); o[1] = (uint8_t)(c >> 8); o[2] = (uint8_t)c;
        }
    }
    /* zlib stream with stored (uncompressed) blocks */
    size_t blocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + blocks * 5 + 4;
    uint8_t *z = malloc(z_len);
    size_t zp = 0;
    z[zp++] = 0x78; z[zp++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw_len; i++) { a = (a + raw[i]) % 65521u; b = (b + a) % 65521u; }
    for (size_t off = 0; off < raw_len; off += 65535) {
        size_t n = raw_len - off > 65535 ? 65535 : raw_len - off;
        z[zp++] = (off + n >= raw_len) ? 1 : 0;
        z[zp++] = (uint8_t)(n & 255); z[zp++] = (uint8_t)(n >> 8);
        z[zp++] = (uint8_t)(~n & 255); z[zp++] = (uint8_t)((~n >> 8) & 255);
        memcpy(&z[zp], &raw[off], n); zp += n;
    }
    uint32_t ad = (b << 16) | a;
    z[zp++] = (uint8_t)(ad >> 24); z[zp++] = (uint8_t)(ad >> 16); z[zp++] = (uint8_t)(ad >> 8); z[zp++] = (uint8_t)ad;

    static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = { (uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w,
                         (uint8_t)(h >> 24), (uint8_t)(h >> 16), (uint8_t)(h >> 8), (uint8_t)h, 8, 2, 0, 0, 0 };
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)zp);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw); free(z);
    return true;
}

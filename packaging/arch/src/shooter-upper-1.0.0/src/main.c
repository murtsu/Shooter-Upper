/*
 * main.c - window, main loop and all the menus.
 *
 * The loop runs the simulation at a fixed 60 steps per second no matter how
 * fast the screen refreshes, so the game feels the same on a 60 Hz and a
 * 144 Hz monitor. The Senseability sensor (sense.c) watches that, and
 * errors.c turns every problem into a dialog with advice.
 *
 * Command line (mostly for testing):
 *   --windowed | --fullscreen | --scale N | --no-audio | --config-dir DIR
 *   --smoke N          run N frames of the demo headless, then exit 0
 *   --screenshot FILE  save a set of screen pictures (FILE-title.png ...) and exit
 *   --version | --help
 */
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "audio.h"
#include "cast.h"
#include "errors.h"
#include "gfx.h"
#include "input.h"
#include "music.h"
#include "sense.h"
#include "storage.h"
#include "game/game.h"

typedef enum {
    ST_TITLE, ST_DIFF, ST_PLAY, ST_PAUSE, ST_NAME, ST_SETTINGS, ST_CONTROLS,
    ST_SCORES, ST_HELP, ST_CREDITS, ST_CAST, ST_DEMO, ST_SPLASH
} State;

#define IDLE_TO_CAST (60 * 20)         /* 20 seconds without input */

static SDL_Window   *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture  *g_tex;
static bool g_headless;
static bool g_running = true;
static bool g_vsync = true;
static bool g_no_audio;

static State g_state = ST_SPLASH;      /* the game opens with the splash */
static Game  g_game;
static int   g_menu_sel;
static int   g_frame;          /* free-running, for animation              */
static int   g_idle;           /* frames without input on the title screen */
static int   g_state_timer;
static bool  g_attract_scores; /* scores screen shown by the attract loop  */
static int   g_rank;           /* position of the new high score           */
static char  g_name[4] = "AAA";
static int   g_name_pos;
static int   g_rebind = -1;    /* action being rebound, or -1              */
static bool  g_prev_pause;
static State g_return_state = ST_TITLE;   /* where settings go back to    */
static char  g_notice[64];
static int   g_notice_timer;
static int   g_err_sel;        /* selected button of the error dialog      */
static bool  g_show_sense;     /* TAB meter (F9 too)                        */
static bool  g_music_on;
static int   g_slow_frames;
static double g_now_ms;        /* the clock the sensor uses                 */
static bool  g_selftest;       /* --selftest: play a scripted session       */
static bool  g_selftest_failed;
static unsigned g_visited;     /* bit per State seen during the self test   */
static int   g_max_play_frames;

/* ======================================================================== */
/* small helpers                                                            */
/* ======================================================================== */

static void notice(const char *s)
{
    snprintf(g_notice, sizeof g_notice, "%s", s);
    g_notice_timer = 120;
}

/*
 * Plays a sound effect, unless the game runs without a window and sound card
 * (tests and screenshots).
 */
static void sfx(Sfx s) { if (!g_headless) audio_play(s); }

/*
 * Sends the sound and music volume from the settings (0 to 10) to the audio
 * code (0 to 1).
 */
static void apply_volume(void)
{
    audio_set_volume((float)g_set.volume / 10.0f);
    music_set_volume((float)g_set.music / 10.0f);
}

/* saving can fail (full disk, read-only folder): tell the player what to do */
static bool save_settings(void)
{
    bool ok = settings_save();
    if (!ok) err_report(ERR_SETTINGS_SAVE, storage_dir());
    return ok;
}

/*
 * Makes the window match the settings: fullscreen, or windowed at the chosen
 * size, centered on the screen.
 */
static void apply_window(void)
{
    if (!g_win) return;
    if (g_set.fullscreen) {
        SDL_SetWindowFullscreen(g_win, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(g_win, 0);
        SDL_SetWindowSize(g_win, SCREEN_W * g_set.scale, SCREEN_H * g_set.scale);
        SDL_SetWindowPosition(g_win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}

/*
 * Switches to another screen. The menu choice, the idle timer and any waiting
 * key presses are reset, so a key held in the old screen cannot do something
 * in the new one.
 */
static void go(State s)
{
    g_state = s;
    g_menu_sel = 0;
    g_idle = 0;
    g_state_timer = 0;
    input_menu_clear();
}

/*
 * Starts a new game with the chosen difficulty. The random seed comes from the
 * clock, so every game is different.
 */
static void start_game(void)
{
    game_init(&g_game, (uint32_t)time(NULL) ^ (uint32_t)SDL_GetTicks());
    game_set_difficulty(&g_game, (Difficulty)g_set.difficulty);
    gfx_fx_reset();
    sense_reset();
    audio_stop_all();
    go(ST_PLAY);
    g_prev_pause = true;      /* a key still held from the menu must not pause */
}

/*
 * Starts the demo that plays by itself, with a fixed seed that depends on the
 * frame number.
 */
static void start_demo(void)
{
    game_init(&g_game, 20260 + (uint32_t)g_frame);
    gfx_fx_reset();
    go(ST_DEMO);
}

/* ======================================================================== */
/* events -> sound and visual effects                                       */
/* ======================================================================== */

static const struct { int ev; Sfx sfx; } EVSFX[] = {
    { EV_LASER, SFX_LASER }, { EV_ENEMY_SHOT, SFX_ENEMY_SHOT },
    { EV_EXPLODE_SMALL, SFX_EXPLODE_SMALL }, { EV_EXPLODE_BIG, SFX_EXPLODE_BIG },
    { EV_SMARTBOMB, SFX_SMARTBOMB }, { EV_ABDUCT, SFX_ABDUCT }, { EV_SCREAM, SFX_SCREAM },
    { EV_CATCH, SFX_CATCH }, { EV_LANDING, SFX_LANDING }, { EV_HUMANOID_DIE, SFX_HUMANOID_DIE },
    { EV_MUTANT, SFX_MUTANT }, { EV_HYPER, SFX_HYPER }, { EV_HYPER_BACK, SFX_HYPER_BACK },
    { EV_EXTRALIFE, SFX_EXTRALIFE }, { EV_WAVE_START, SFX_WAVE_START },
    { EV_WAVE_CLEAR, SFX_WAVE_CLEAR }, { EV_PLANET_BLOW, SFX_PLANET_BLOW },
    { EV_SHIP_DEATH, SFX_SHIP_DEATH }, { EV_BAITER, SFX_BAITER }, { EV_GAMEOVER, SFX_GAMEOVER },
    { EV_RESPAWN, SFX_RESPAWN }, { EV_MINE_DROP, SFX_MINE_DROP },
};

/*
 * Hands this step's game events to the visual effects and, in a real game, to
 * the sound code and the sensor (a death counts for the sensor). Then the list
 * is emptied.
 */
static void drain_events(bool real_game)
{
    for (int i = 0; i < g_game.nev; i++) {
        const Event *e = &g_game.ev[i];
        gfx_fx_event(e, &g_game);
        if (!real_game) continue;
        if (e->type == EV_SHIP_DEATH) sense_death(g_now_ms);
        for (size_t k = 0; k < sizeof EVSFX / sizeof EVSFX[0]; k++)
            if (EVSFX[k].ev == e->type) { sfx(EVSFX[k].sfx); break; }
    }
    g_game.nev = 0;
}

/* one simulation step of the running game (or the demo) */
static void sim_step(bool demo)
{
    Controls c;
    if (demo) {
        game_ai(&g_game, &c);
        g_game.tune.adapt = 1.f;
    } else {
        input_make_controls(&c, (Scheme)g_set.scheme);
        g_game.tune.adapt = sense_info()->adapt;      /* the sensor's gentle nudge */
    }
    game_step(&g_game, &c);
    if (!demo) input_clear_pulses();
    drain_events(!demo);
    gfx_fx_step(&g_game);
}

/* ======================================================================== */
/* menus                                                                    */
/* ======================================================================== */

/* returns +1 / -1 for left/right, 0 for nothing */
static int menu_nav(int count)
{
    if (input_menu(MN_UP))   { g_menu_sel = (g_menu_sel + count - 1) % count; sfx(SFX_MENU_MOVE); g_idle = 0; }
    if (input_menu(MN_DOWN)) { g_menu_sel = (g_menu_sel + 1) % count;         sfx(SFX_MENU_MOVE); g_idle = 0; }
    if (input_menu(MN_LEFT))  { g_idle = 0; return -1; }
    if (input_menu(MN_RIGHT)) { g_idle = 0; return 1; }
    return 0;
}

/*
 * Draws a list of menu lines, with the chosen one highlighted. y0 is the top
 * and step the distance between lines.
 */
static void draw_menu(const char *const *items, int n, int y0, int step)
{
    for (int i = 0; i < n; i++) gfx_menu_item(y0 + i * step, items[i], i == g_menu_sel, g_frame);
}

/*
 * Draws the line at the bottom that tells which button selects and which goes
 * back, with names that match the keyboard or the controller in use.
 */
static void draw_hint(void)
{
    char buf[80];
    snprintf(buf, sizeof buf, "%s SELECT    %s BACK", input_confirm_label(), input_back_label());
    gfx_text_center(SCREEN_H - 14, buf, RGB(110,120,170), 1);
}

/* Draws the moving space background used behind most menus. */
static void draw_bg(void)
{
    gfx_draw_space_bg((float)g_frame * 0.7f, true, g_frame);
}

/*
 * Draws a dark see-through box that makes text easy to read over the
 * background.
 */
static void panel(int x, int y, int w, int h)
{
    gfx_blend_rect(x, y, w, h, RGB(4,4,24), 185);
}

/* ---- title ---- */
static const char *const TITLE_ITEMS[] = {
    "START GAME", "HOW TO PLAY", "HIGH SCORES", "SETTINGS", "CONTROLS", "CREDITS", "QUIT"
};
#define N_TITLE 7

static const char SCROLL_TEXT[] =
    "SHOOTER UPPER ...  WRITTEN IN C ...  NO PICTURE FILES ...  EVERY SOUND IS MATH ...  "
    "SAVE THE HUMANOIDS ...  THEY ARE SMALL AND THEY DO NOT SHOOT BACK ...  "
    "GREETINGS TO EVERYONE WHO EVER WAITED FOR A TAPE TO LOAD ...         ";

/*
 * The title screen each frame: move in the menu, start what was chosen, and
 * after 20 seconds of nothing begin the parade of characters.
 */
static void update_title(void)
{
    menu_nav(N_TITLE);
    if (input_menu(MN_CONFIRM)) {
        sfx(SFX_MENU_SELECT);
        switch (g_menu_sel) {
        case 0: go(ST_DIFF); g_menu_sel = g_set.difficulty; return;
        case 1: go(ST_HELP); return;
        case 2: g_attract_scores = false; go(ST_SCORES); return;
        case 3: g_return_state = ST_TITLE; go(ST_SETTINGS); return;
        case 4: g_return_state = ST_TITLE; go(ST_CONTROLS); return;
        case 5: go(ST_CREDITS); return;
        case 6: g_running = false; return;
        }
    }
    if (input_menu(MN_BACK)) { g_menu_sel = N_TITLE - 1; sfx(SFX_MENU_BACK); }
    if (++g_idle > IDLE_TO_CAST) { cast_start((unsigned)time(NULL) + (unsigned)g_frame); go(ST_CAST); }
}

/*
 * Draws the title screen: moving color bars, the logo, the menu, the scrolling
 * text and the version number.
 */
static void draw_title(void)
{
    draw_bg();
    gfx_copper_bars(4, 92, g_frame);
    gfx_draw_logo(12, g_frame);
    draw_menu(TITLE_ITEMS, N_TITLE, 100, 12);
    gfx_sine_scroller(188, SCROLL_TEXT, g_frame);
    draw_hint();
    char buf[48];
    snprintf(buf, sizeof buf, "V%s", GAME_VERSION);
    gfx_text(2, SCREEN_H - 9, buf, RGB(70,75,120), 1);
}

/* ---- start-up splash ---- */
static void update_splash(void)
{
    bool any = input_held(ACT_FIRE);
    for (int k = 0; k < MN_COUNT; k++) if (input_menu((MenuKey)k)) any = true;
    if (any || ++g_state_timer > SPLASH_FRAMES) go(ST_TITLE);
}

/*
 * Draws the start-up animation. How far along it is comes from the screen's
 * timer.
 */
static void draw_splash(void)
{
    gfx_draw_splash(g_state_timer);
}

/* ---- the idle parade ---- */
static void update_cast(void)
{
    bool any = false;
    for (int k = 0; k < MN_COUNT; k++) if (input_menu((MenuKey)k)) any = true;
    if (any || input_held(ACT_FIRE)) { go(ST_TITLE); return; }
    cast_step();
    if (cast_done()) start_demo();
}

/* Draws the parade of characters on top of the space background. */
static void draw_cast(void)
{
    draw_bg();
    cast_draw(g_frame);
}

/* ---- difficulty ---- */
static const struct { const char *name; int ramp; const char *line; } DIFFS[DIFF_COUNT] = {
    { "EASY",   RAMP_GREEN, "SLOWER ENEMIES, FEWER SHOTS. 4 SHIPS, 4 BOMBS." },
    { "NORMAL", RAMP_ICE,   "THE WAY IT WAS MEANT. 3 SHIPS, 3 BOMBS." },
    { "HARD",   RAMP_GOLD,  "FASTER AND SHARPER. 3 SHIPS, 2 BOMBS." },
    { "INSANE", RAMP_RED,   "GOOD LUCK. YOU WILL NEED IT. 2 SHIPS, 1 BOMB." },
};

/* The grade screen each frame. Choosing a grade saves it and starts the game. */
static void update_diff(void)
{
    menu_nav(DIFF_COUNT);
    if (input_menu(MN_CONFIRM)) {
        g_set.difficulty = g_menu_sel;
        save_settings();
        sfx(SFX_MENU_SELECT);
        start_game();
        return;
    }
    if (input_menu(MN_BACK)) { sfx(SFX_MENU_BACK); go(ST_TITLE); }
}

/*
 * Draws the grade screen with the four grades and a line about each. The
 * bottom line tells if the senseability is on.
 */
static void draw_diff(void)
{
    draw_bg();
    panel(14, 6, SCREEN_W - 28, SCREEN_H - 12);
    gfx_title(14, "CHOOSE YOUR GRADE", RAMP_GOLD);
    for (int i = 0; i < DIFF_COUNT; i++) {
        bool sel = i == g_menu_sel;
        int y = 48 + i * 38;
        int ramp = sel ? DIFFS[i].ramp : RAMP_DIM;
        int w = gfx_amiga_width(DIFFS[i].name, 1, 1);
        int x = (SCREEN_W - w) / 2;
        if (sel) gfx_amiga_wave(x, y, DIFFS[i].name, ramp, 1, 1, g_frame, 2);
        else     gfx_amiga_text(x, y, DIFFS[i].name, ramp, 1, 1);
        gfx_text_center(y + 19, DIFFS[i].line, sel ? RGB(215,225,255) : RGB(100,108,160), 1);
        if (sel) {
            int bob = (g_frame / 8) & 1;
            gfx_amiga_text(x - 20 + bob, y, ">", RAMP_ICE, 1, 1);
            gfx_amiga_text(x + w + 8 - bob, y, "<", RAMP_ICE, 1, 1);
        }
    }
    gfx_text_center(SCREEN_H - 30, g_set.adaptive ? "SENSEABILITY ON: THE GAME TUNES ITSELF TO YOU"
                                                  : "SENSEABILITY OFF: THE GRADE STAYS AS CHOSEN",
                    RGB(120,200,235), 1);
    draw_hint();
}

/* ---- demo ---- */
static void update_demo(void)
{
    bool any = false;
    for (int k = 0; k < MN_COUNT; k++) if (input_menu((MenuKey)k)) any = true;
    if (any || input_held(ACT_FIRE)) { go(ST_TITLE); return; }
    sim_step(true);
    if (++g_state_timer > 60 * 40 || g_game.finished) { g_attract_scores = true; go(ST_SCORES); }
}

/* Draws the demo as an ordinary game with a blinking PRESS ANY BUTTON on top. */
static void draw_demo(void)
{
    gfx_draw_game(&g_game, g_frame);
    gfx_blend_rect(0, SCREEN_H - 26, SCREEN_W, 14, RGB(0,0,0), 110);
    if ((g_frame / 30) & 1) gfx_text_center(SCREEN_H - 23, "PRESS ANY BUTTON", RGB(255,240,130), 1);
}

/* ---- high scores ---- */
static void update_scores(void)
{
    bool any = false;
    for (int k = 0; k < MN_COUNT; k++) if (input_menu((MenuKey)k)) any = true;
    if (any) { sfx(SFX_MENU_BACK); go(ST_TITLE); return; }
    if (g_attract_scores && ++g_state_timer > 60 * 8) go(ST_TITLE);
}

/* Draws the high score table. */
static void draw_scores(void)
{
    draw_bg();
    panel(20, 12, SCREEN_W - 40, SCREEN_H - 24);
    gfx_title(20, "HIGH SCORES", RAMP_GOLD);
    gfx_text(70, 54, "RANK  NAME     SCORE    WAVE", RGB(120,200,255), 1);
    for (int i = 0; i < SCORE_SLOTS; i++) {
        char buf[128];
        snprintf(buf, sizeof buf, " %d    %-3.3s   %8u    %2d", i + 1, g_scores[i].name, g_scores[i].score, g_scores[i].wave);
        uint32_t col = i == 0 ? RGB(255,235,110) : RGB(205,210,245);
        gfx_text(70, 72 + i * 15, buf, col, 1);
    }
    if (!g_attract_scores) draw_hint();
}

/* ---- how to play ---- */
static void update_simple_back(void)
{
    if (input_menu(MN_BACK) || input_menu(MN_CONFIRM)) { sfx(SFX_MENU_BACK); go(ST_TITLE); }
}

/* Draws the how-to-play page. */
static void draw_help(void)
{
    draw_bg();
    panel(10, 6, SCREEN_W - 20, SCREEN_H - 12);
    gfx_title(12, "HOW TO PLAY", RAMP_GOLD);
    const uint32_t H = RGB(120,220,255), T = RGB(205,210,245);
    gfx_text(22, 40,  "THE GOAL", H, 1);
    gfx_text(22, 51,  "SHOOT THE ALIENS BEFORE THEY CARRY AWAY", T, 1);
    gfx_text(22, 61,  "YOUR HUMANOIDS. A LOST HUMANOID TURNS INTO", T, 1);
    gfx_text(22, 71,  "A MUTANT. LOSE THEM ALL AND THE PLANET BLOWS.", T, 1);
    gfx_text(22, 88,  "SAVING HUMANOIDS", H, 1);
    gfx_text(22, 99,  "SHOOT A LANDER WHILE IT CARRIES ONE, THEN", T, 1);
    gfx_text(22, 109, "CATCH THE HUMANOID AS IT FALLS (500) AND SET", T, 1);
    gfx_text(22, 119, "IT DOWN ON THE GROUND (500).", T, 1);
    gfx_text(22, 136, "POINTS", H, 1);
    gfx_text(22, 147, "LANDER 150  MUTANT 150  SWARMER 150", T, 1);
    gfx_text(22, 157, "BAITER 200  BOMBER 250  POD 1000", T, 1);
    gfx_text(22, 174, "EXTRA SHIP AND SMART BOMB EVERY 10000.", T, 1);
    gfx_text(22, 184, "SMART BOMB CLEARS THE SCREEN. HYPERSPACE IS", T, 1);
    gfx_text(22, 194, "A GAMBLE.", T, 1);
    draw_hint();
}

/* ---- credits ---- */
static void draw_credits(void)
{
    draw_bg();
    panel(30, 14, SCREEN_W - 60, SCREEN_H - 28);
    gfx_title(22, "CREDITS", RAMP_GOLD);
    gfx_text_center(58,  GAME_TITLE, RGB(255,255,255), 1);
    gfx_text_center(76,  "ALL GRAPHICS DRAWN BY CODE", RGB(205,210,245), 1);
    gfx_text_center(88,  "ALL SOUND EFFECTS MADE BY CODE", RGB(205,210,245), 1);
    char buf[80];
    const char *src = music_source() == MS_CUSTOM ? "YOUR OWN SONG" :
                      music_source() == MS_BUNDLED ? "THEME SONG: THE FILE THAT SHIPS WITH THE GAME" :
                      "THEME SONG: BUILT-IN CHIP TUNE";
    snprintf(buf, sizeof buf, "%s", src);
    gfx_text_center(100, buf, RGB(205,210,245), 1);
    gfx_text_center(116, "BUILT WITH C AND SDL2", RGB(120,220,255), 1);
    gfx_text_center(126, "SDL2 IS FREE SOFTWARE UNDER THE ZLIB LICENSE", RGB(120,130,190), 1);
    gfx_text_center(136, "MP3 DECODER: MINIMP3, PUBLIC DOMAIN", RGB(120,130,190), 1);
    gfx_text_center(156, "IDEA, DIRECTION AND TESTING", RGB(255,225,120), 1);
    gfx_text_center(166, "MARKO TAHVANAINEN", RGB(255,255,255), 1);
    gfx_text_center(180, "DESIGN AND CODE: CLAUDE, ANTHROPIC", RGB(130,225,255), 1);
    gfx_text_center(192, "COPYRIGHT 2026 MARKO TAHVANAINEN AND CLAUDE", RGB(120,130,190), 1);
    draw_hint();
}

/* ---- settings ---- */
#define N_SET 8

/*
 * The settings screen each frame. Left and right change the chosen value (or
 * confirm toggles it), the changes take effect at once, and leaving the screen
 * saves them.
 */
static void update_settings(void)
{
    int d = menu_nav(N_SET);
    bool conf = input_menu(MN_CONFIRM);
    bool changed = false;
    switch (g_menu_sel) {
    case 0: if (d || conf) { g_set.fullscreen ^= 1; apply_window(); changed = true; } break;
    case 1: if (d) { g_set.scale += d; if (g_set.scale < 1) g_set.scale = 1; if (g_set.scale > 8) g_set.scale = 8;
                     apply_window(); changed = true; } break;
    case 2: if (d) { g_set.volume += d; if (g_set.volume < 0) g_set.volume = 0; if (g_set.volume > 10) g_set.volume = 10;
                     apply_volume(); sfx(SFX_LASER); changed = true; } break;
    case 3: if (d) { g_set.music += d; if (g_set.music < 0) g_set.music = 0; if (g_set.music > 10) g_set.music = 10;
                     apply_volume(); changed = true; } break;
    case 4: if (d || conf) { g_set.crt ^= 1; changed = true; } break;
    case 5: if (d || conf) { g_set.scheme ^= 1; changed = true; } break;
    case 6: if (d || conf) { g_set.adaptive ^= 1; changed = true; } break;
    case 7: if (conf) { save_settings(); sfx(SFX_MENU_BACK); go(g_return_state); return; } break;
    }
    if (changed && g_menu_sel != 2) sfx(SFX_MENU_MOVE);
    if (input_menu(MN_BACK)) { save_settings(); sfx(SFX_MENU_BACK); go(g_return_state); }
}

/* Makes a little bar like ++++++.... for a value from 0 to 10. */
static void bar_text(char *out, size_t n, int v)
{
    char bar[16];
    for (int i = 0; i < 10; i++) bar[i] = i < v ? '+' : '.';
    bar[10] = 0;
    snprintf(out, n, "%s", bar);
}

/* Draws the settings screen with every setting and its current value. */
static void draw_settings(void)
{
    draw_bg();
    panel(24, 10, SCREEN_W - 48, SCREEN_H - 20);
    gfx_title(18, "SETTINGS", RAMP_GOLD);
    char rows[N_SET][64], bar[16];
    snprintf(rows[0], 64, "FULLSCREEN       %s", g_set.fullscreen ? "ON" : "OFF");
    snprintf(rows[1], 64, "WINDOW SIZE      %dX", g_set.scale);
    bar_text(bar, sizeof bar, g_set.volume);
    snprintf(rows[2], 64, "SOUND EFFECTS    %s", bar);
    bar_text(bar, sizeof bar, g_set.music);
    snprintf(rows[3], 64, "MUSIC            %s", bar);
    snprintf(rows[4], 64, "CRT EFFECT       %s", g_set.crt ? "ON" : "OFF");
    snprintf(rows[5], 64, "STEERING         %s", g_set.scheme == SCHEME_CLASSIC ? "CLASSIC" : "MODERN");
    snprintf(rows[6], 64, "SENSEABILITY     %s", g_set.adaptive ? "ON" : "OFF");
    snprintf(rows[7], 64, "BACK");
    for (int i = 0; i < N_SET; i++) {
        bool sel = i == g_menu_sel;
        int y = 52 + i * 14;
        if (sel) gfx_text(44, y, ">", RGB(255,220,90), 1);
        gfx_text(58, y, rows[i], sel ? RGB(255,255,255) : RGB(120,130,190), 1);
    }
    const char *help = "";
    if (g_menu_sel == 1 && g_set.fullscreen) help = "WINDOW SIZE APPLIES IN WINDOWED MODE";
    if (g_menu_sel == 3) help = "PUT YOUR OWN SONG IN THE SETTINGS FOLDER AS THEME.MP3";
    if (g_menu_sel == 5)
        help = g_set.scheme == SCHEME_CLASSIC ? "CLASSIC: THRUST BUTTON AND REVERSE BUTTON"
                                              : "MODERN: LEFT OR RIGHT FLIES THAT WAY";
    if (g_menu_sel == 6) help = "THE GAME TUNES ITSELF UP TO 8 PERCENT TO FIT YOU";
    gfx_text_center(170, help, RGB(150,200,230), 1);
    gfx_text_center(184, "TAB SHOWS THE SENSEABILITY METER", RGB(90,100,150), 1);
    draw_hint();
}

/* ---- controls ---- */
static void draw_controls(void)
{
    draw_bg();
    panel(10, 6, SCREEN_W - 20, SCREEN_H - 12);
    gfx_title(10, "CONTROLS", RAMP_GOLD);
    gfx_text(30, 38, "ACTION", RGB(120,200,255), 1);
    gfx_text(104, 38, "KEY 1", RGB(120,200,255), 1);
    gfx_text(170, 38, "KEY 2", RGB(120,200,255), 1);
    gfx_text(236, 38, "PAD", RGB(120,200,255), 1);
    for (int a = 0; a < ACT_COUNT; a++) {
        bool sel = a == g_menu_sel;
        int y = 50 + a * 12;
        uint32_t col = sel ? RGB(255,255,255) : RGB(140,150,205);
        if (sel) gfx_text(18, y, ">", RGB(255,220,90), 1);
        gfx_text(30, y, input_action_name((Action)a), col, 1);
        if (g_rebind == a) {
            if ((g_frame / 15) & 1) gfx_text(104, y, "PRESS A KEY OR BUTTON", RGB(255,220,90), 1);
        } else {
            gfx_text(104, y, input_key_label(g_set.bind[a].key[0]), col, 1);
            gfx_text(170, y, input_key_label(g_set.bind[a].key[1]), col, 1);
            gfx_text(236, y, input_button_label(g_set.bind[a].pad), col, 1);
        }
    }
    int y = 50 + ACT_COUNT * 12 + 4;
    bool s1 = g_menu_sel == ACT_COUNT, s2 = g_menu_sel == ACT_COUNT + 1;
    if (s1) gfx_text(18, y, ">", RGB(255,220,90), 1);
    gfx_text(30, y, "RESET TO DEFAULTS", s1 ? RGB(255,255,255) : RGB(140,150,205), 1);
    if (s2) gfx_text(18, y + 12, ">", RGB(255,220,90), 1);
    gfx_text(30, y + 12, "BACK", s2 ? RGB(255,255,255) : RGB(140,150,205), 1);
    if (g_rebind >= 0) gfx_text_center(SCREEN_H - 24, "ESC CANCELS", RGB(150,200,230), 1);
    else gfx_text_center(SCREEN_H - 24, "SELECT A ROW TO CHANGE KEY 1 OR THE PAD BUTTON", RGB(110,120,170), 1);
    draw_hint();
}

/*
 * The controls screen each frame. Choose a row, press confirm, then press the
 * new key or button. If it is already used by another action the two swap
 * places, so nothing is left without a key.
 */
static void update_controls(void)
{
    if (g_rebind >= 0) {
        SDL_Scancode k = input_take_key();
        int b = input_take_button();
        if (k == SDL_SCANCODE_ESCAPE) { g_rebind = -1; sfx(SFX_MENU_BACK); input_menu_clear(); return; }
        if (k != SDL_SCANCODE_UNKNOWN) {
            /* a key already used by another action moves over (swap) */
            for (int a = 0; a < ACT_COUNT; a++) {
                if (a == g_rebind) continue;
                for (int s = 0; s < 2; s++)
                    if (g_set.bind[a].key[s] == k) g_set.bind[a].key[s] = g_set.bind[g_rebind].key[0];
            }
            g_set.bind[g_rebind].key[0] = k;
            g_rebind = -1; sfx(SFX_MENU_SELECT); input_menu_clear();
        } else if (b >= 0) {
            for (int a = 0; a < ACT_COUNT; a++)
                if (a != g_rebind && g_set.bind[a].pad == b) g_set.bind[a].pad = g_set.bind[g_rebind].pad;
            g_set.bind[g_rebind].pad = b;
            g_rebind = -1; sfx(SFX_MENU_SELECT); input_menu_clear();
        }
        return;
    }
    menu_nav(ACT_COUNT + 2);
    if (input_menu(MN_CONFIRM)) {
        if (g_menu_sel < ACT_COUNT) {
            g_rebind = g_menu_sel;
            input_take_key(); input_take_button();
            sfx(SFX_MENU_SELECT);
        } else if (g_menu_sel == ACT_COUNT) {
            input_default_bindings(g_set.bind);
            sfx(SFX_MENU_SELECT);
            notice("DEFAULTS RESTORED");
        } else {
            save_settings(); sfx(SFX_MENU_BACK); go(g_return_state); return;
        }
    }
    if (input_menu(MN_BACK)) { save_settings(); sfx(SFX_MENU_BACK); go(g_return_state); }
}

/* ---- play and pause ---- */
static void update_play(void)
{
    bool p = input_held(ACT_PAUSE);
    if (p && !g_prev_pause) { g_prev_pause = true; go(ST_PAUSE); return; }
    g_prev_pause = p;
    sim_step(false);
    if (g_game.phase == PH_OVER && g_game.finished) {
        int r = scores_rank(g_game.score);
        if (r >= 0) {
            g_rank = r;
            strcpy(g_name, "AAA");
            g_name_pos = 0;
            input_take_key();
            go(ST_NAME);
        } else {
            g_attract_scores = false;
            go(ST_SCORES);
        }
    }
}

static const char *const PAUSE_ITEMS[] = { "RESUME", "SETTINGS", "QUIT TO TITLE" };

/*
 * The pause menu each frame. Resume with the pause key, back or the first
 * choice. Settings and quit to title are the other choices.
 */
static void update_pause(void)
{
    g_menu_sel %= 3;
    menu_nav(3);
    bool p = input_held(ACT_PAUSE);
    bool resume = false;
    if (p && !g_prev_pause) resume = true;
    g_prev_pause = p;
    if (input_menu(MN_BACK)) resume = true;
    if (input_menu(MN_CONFIRM)) {
        sfx(SFX_MENU_SELECT);
        if (g_menu_sel == 0) resume = true;
        else if (g_menu_sel == 1) { g_return_state = ST_PAUSE; go(ST_SETTINGS); return; }
        else { go(ST_TITLE); return; }
    }
    if (resume) { g_state = ST_PLAY; g_prev_pause = true; input_menu_clear(); sense_reset(); }
}

/* Draws the frozen game, darkened, with PAUSED and the pause menu on top. */
static void draw_pause(void)
{
    gfx_draw_game(&g_game, g_frame);
    gfx_blend_rect(0, PLAY_TOP, SCREEN_W, SCREEN_H - PLAY_TOP, RGB(0,0,20), 150);
    gfx_amiga_center(66, "PAUSED", RAMP_GOLD, 1, 2);
    draw_menu(PAUSE_ITEMS, 3, 116, 14);
}

/* ---- name entry ---- */
static void update_name(void)
{
    SDL_Scancode k = input_take_key();
    if (k >= SDL_SCANCODE_A && k <= SDL_SCANCODE_Z) {
        g_name[g_name_pos] = (char)('A' + (k - SDL_SCANCODE_A));
        sfx(SFX_MENU_MOVE);
        if (g_name_pos < 2) g_name_pos++;
    }
    if (input_menu(MN_UP))   { g_name[g_name_pos] = g_name[g_name_pos] == 'Z' ? 'A' : g_name[g_name_pos] + 1; sfx(SFX_MENU_MOVE); }
    if (input_menu(MN_DOWN)) { g_name[g_name_pos] = g_name[g_name_pos] == 'A' ? 'Z' : g_name[g_name_pos] - 1; sfx(SFX_MENU_MOVE); }
    if (input_menu(MN_RIGHT) && g_name_pos < 2) { g_name_pos++; sfx(SFX_MENU_MOVE); }
    if (input_menu(MN_LEFT)  && g_name_pos > 0) { g_name_pos--; sfx(SFX_MENU_MOVE); }
    if (input_menu(MN_BACK)  && g_name_pos > 0) { g_name_pos--; sfx(SFX_MENU_BACK); }
    if (input_menu(MN_CONFIRM)) {
        sfx(SFX_MENU_SELECT);
        if (g_name_pos < 2) { g_name_pos++; }
        else {
            scores_insert(g_rank, g_name, g_game.score, g_game.wave);
            if (!scores_save()) err_report(ERR_SCORES_SAVE, storage_dir());
            g_attract_scores = false;
            go(ST_SCORES);
        }
    }
}

/*
 * Draws the new high score screen with the three letters and a marker under
 * the one being edited.
 */
static void draw_name(void)
{
    draw_bg();
    gfx_title(26, "NEW HIGH SCORE", RAMP_GOLD);
    char buf[48];
    snprintf(buf, sizeof buf, "%u", g_game.score);
    gfx_amiga_center(56, buf, RAMP_WHITE, 1, 1);
    snprintf(buf, sizeof buf, "RANK %d   %s", g_rank + 1, game_difficulty_name(g_game.diff));
    gfx_text_center(88, buf, RGB(120,220,255), 1);
    gfx_text_center(110, "ENTER YOUR INITIALS", RGB(205,210,245), 1);
    for (int i = 0; i < 3; i++) {
        int x = SCREEN_W / 2 - 42 + i * 30;
        char c[2] = { g_name[i], 0 };
        gfx_amiga_text(x, 128, c, i == g_name_pos ? RAMP_GOLD : RAMP_ICE, 1, 2);
        if (i == g_name_pos && ((g_frame / 15) & 1)) gfx_rect(x, 160, 24, 2, RGB(255,220,90));
    }
    gfx_text_center(SCREEN_H - 24, "TYPE LETTERS OR USE UP AND DOWN", RGB(110,120,170), 1);
    draw_hint();
}

/* ======================================================================== */
/* error dialogs                                                            */
/* ======================================================================== */

/* word-wrap text into lines of at most 'cols' characters; returns the count */
static int wrap(const char *text, int cols, char lines[][64], int max_lines)
{
    int n = 0;
    const char *p = text;
    while (*p && n < max_lines) {
        while (*p == ' ') p++;
        int len = 0, last_space = -1;
        while (p[len] && len < cols) { if (p[len] == ' ') last_space = len; len++; }
        if (p[len] && last_space > 0) len = last_space;       /* break at a space if we can */
        memcpy(lines[n], p, (size_t)len);
        lines[n][len] = 0;
        n++;
        p += len;
    }
    return n;
}

/*
 * Draws the error dialog: the title, what happened, the detail, the tips for
 * what the player can try and the buttons.
 */
static void draw_error(void)
{
    ErrCode code = err_current();
    const ErrInfo *e = err_info(code);
    const char *detail = err_current_detail();
    char lines[8][64];

    gfx_blend_rect(0, 0, SCREEN_W, SCREEN_H, RGB(0,0,16), 170);
    const int x = 14, w = SCREEN_W - 28;
    int y = 14;

    /* measure first so the box fits its text */
    int total = 16;
    total += wrap(e->what, 44, lines, 8) * 10 + 6;
    if (detail && *detail) total += wrap(detail, 44, lines, 2) * 10 + 6;
    total += 14;
    for (int i = 0; i < ERR_MAX_TIPS && e->tips[i]; i++) total += wrap(e->tips[i], 42, lines, 3) * 10 + 2;
    total += 26;
    y = (SCREEN_H - total) / 2;
    if (y < 4) y = 4;

    gfx_rect(x - 2, y - 2, w + 4, total + 4, e->fatal ? RGB(120,20,30) : RGB(40,60,140));
    gfx_rect(x, y, w, total, RGB(10,12,40));
    gfx_rect(x, y, w, 14, e->fatal ? RGB(170,30,40) : RGB(60,90,200));
    gfx_amiga_text(x + 6, y + 3, e->title, RAMP_WHITE, 0, 1);

    int ty = y + 20;
    int n = wrap(e->what, 44, lines, 8);
    for (int i = 0; i < n; i++, ty += 10) gfx_text(x + 8, ty, lines[i], RGB(235,238,255), 1);
    ty += 6;
    if (detail && *detail) {
        n = wrap(detail, 44, lines, 2);
        for (int i = 0; i < n; i++, ty += 10) gfx_text(x + 8, ty, lines[i], RGB(255,200,110), 1);
        ty += 6;
    }
    gfx_text(x + 8, ty, "WHAT YOU CAN TRY", RGB(120,225,255), 1);
    ty += 14;
    for (int i = 0; i < ERR_MAX_TIPS && e->tips[i]; i++) {
        n = wrap(e->tips[i], 42, lines, 3);
        gfx_text(x + 8, ty, "-", RGB(255,220,90), 1);
        for (int k = 0; k < n; k++, ty += 10) gfx_text(x + 18, ty, lines[k], RGB(190,200,240), 1);
        ty += 2;
    }

    /* buttons */
    int count = 0;
    while (count < ERR_MAX_BUTTONS && e->button[count]) count++;
    int bw = 78, gap = 10;
    int bx = x + (w - (count * bw + (count - 1) * gap)) / 2;
    int by = y + total - 20;
    for (int i = 0; i < count; i++) {
        bool sel = i == g_err_sel;
        gfx_rect(bx, by, bw, 14, sel ? RGB(255,200,70) : RGB(40,48,100));
        gfx_rect_outline(bx, by, bw, 14, sel ? RGB(255,255,200) : RGB(90,100,170));
        int tw = gfx_text_width(e->button[i], 1);
        gfx_text(bx + (bw - tw) / 2, by + 4, e->button[i], sel ? RGB(40,20,0) : RGB(210,220,255), 1);
        bx += bw + gap;
    }
}

/*
 * Carries out the button the player chose in an error dialog: continue, quit,
 * fix it (smaller window and no CRT effect) or try again (sound, settings or
 * scores).
 */
static void error_action(ErrCode code, ErrAction act)
{
    switch (act) {
    case ERR_ACT_CONTINUE:
        err_dismiss();
        break;
    case ERR_ACT_QUIT:
        err_dismiss();
        g_running = false;
        break;
    case ERR_ACT_FIX:
        if (code == ERR_SLOW) {
            g_set.crt = 0;
            if (g_set.scale > 2) g_set.scale = 2;
            apply_window();
            save_settings();
            notice("CRT OFF AND WINDOW SMALLER");
        }
        err_dismiss();
        break;
    case ERR_ACT_RETRY:
        if (code == ERR_AUDIO_DEVICE) {
            audio_shutdown();
            if (audio_init(!g_no_audio)) {
                apply_volume();
                g_music_on = false;                    /* let the music start again */
                err_dismiss();
                notice("SOUND IS WORKING");
            } else notice("STILL NO SOUND");
        } else if (code == ERR_SETTINGS_SAVE) {
            if (settings_save()) { err_dismiss(); notice("SETTINGS SAVED"); } else notice("STILL CANNOT SAVE");
        } else if (code == ERR_SCORES_SAVE) {
            if (scores_save()) { err_dismiss(); notice("HIGH SCORES SAVED"); } else notice("STILL CANNOT SAVE");
        } else err_dismiss();
        break;
    }
}

/*
 * The error dialog each frame. Left and right choose a button, confirm presses
 * it. Back closes the dialog, except for errors the game cannot go on after.
 */
static void update_error(void)
{
    const ErrInfo *e = err_info(err_current());
    int count = 0;
    while (count < ERR_MAX_BUTTONS && e->button[count]) count++;
    if (g_err_sel >= count) g_err_sel = 0;
    if (input_menu(MN_LEFT))  { g_err_sel = (g_err_sel + count - 1) % count; sfx(SFX_MENU_MOVE); }
    if (input_menu(MN_RIGHT)) { g_err_sel = (g_err_sel + 1) % count; sfx(SFX_MENU_MOVE); }
    if (input_menu(MN_CONFIRM)) {
        ErrCode code = err_current();
        ErrAction act = e->action[g_err_sel];
        sfx(SFX_MENU_SELECT);
        g_err_sel = 0;
        error_action(code, act);
        sense_reset();
        input_menu_clear();
    }
    if (input_menu(MN_BACK) && !e->fatal) { err_dismiss(); g_err_sel = 0; input_menu_clear(); sense_reset(); }
}

/* ======================================================================== */
/* frame                                                                    */
/* ======================================================================== */

static bool music_wanted(void)
{
    return g_state != ST_PLAY && g_state != ST_PAUSE;
}

/*
 * One step of the whole program, called exactly 60 times a second. It runs the
 * current screen, counts the screens visited (for the self test), updates the
 * sensor, starts and stops the music, and offers help if the computer is too
 * slow.
 */
static void update_one(void)
{
    err_tick();
    sense_sim_step(g_now_ms);

    if (err_pending()) { update_error(); return; }        /* a dialog freezes everything else */

    switch (g_state) {
    case ST_TITLE:    update_title(); break;
    case ST_DIFF:     update_diff(); break;
    case ST_PLAY:     update_play(); break;
    case ST_PAUSE:    update_pause(); break;
    case ST_NAME:     update_name(); break;
    case ST_SETTINGS: update_settings(); break;
    case ST_CONTROLS: update_controls(); break;
    case ST_SCORES:   update_scores(); break;
    case ST_HELP:     update_simple_back(); break;
    case ST_CREDITS:  update_simple_back(); break;
    case ST_CAST:     update_cast(); break;
    case ST_DEMO:     update_demo(); break;
    case ST_SPLASH:   update_splash(); break;
    }
    if (g_notice_timer > 0) g_notice_timer--;
    g_visited |= 1u << g_state;
    if (g_state == ST_PLAY && (int)g_game.frame > g_max_play_frames) g_max_play_frames = (int)g_game.frame;

    sense_update(g_now_ms, g_state == ST_PLAY, g_set.adaptive != 0);

    /* the title song plays on every screen except the game itself */
    if (!g_headless) {
        bool want = music_wanted();
        if (want != g_music_on) {
            g_music_on = want;
            if (want) music_play(); else music_stop();
        }
    }

    /* the machine cannot keep up: after three bad seconds offer help */
    if (!g_headless && g_state == ST_PLAY && sense_info()->fps > 0.f && sense_info()->fps < 45.f) {
        if (++g_slow_frames > 60 * 3) {
            char d[64];
            snprintf(d, sizeof d, "%.0f pictures per second", (double)sense_info()->fps);
            err_report(ERR_SLOW, d);
            g_slow_frames = 0;
        }
    } else g_slow_frames = 0;
}

/*
 * Draws the little senseability panel (toggled with Tab or F9): pictures per
 * second, steps per second, the slowest frame, button presses, deaths and the
 * current difficulty nudge.
 */
static void draw_sense_meter(void)
{
    const SenseInfo *s = sense_info();
    char buf[64];
    gfx_blend_rect(2, PLAY_TOP + 2, 150, 44, RGB(0,0,0), 170);
    snprintf(buf, sizeof buf, "FPS %.0f  SIM %.0f  WORST %.0fMS", (double)s->fps, (double)s->sim, (double)s->worst_ms);
    gfx_text(5, PLAY_TOP + 5, buf, RGB(150,255,170), 1);
    snprintf(buf, sizeof buf, "PRESSES/S %.1f  DEATHS %d", (double)s->press_s, s->deaths);
    gfx_text(5, PLAY_TOP + 15, buf, RGB(150,220,255), 1);
    snprintf(buf, sizeof buf, "LEVEL %+.0f PCT  %s", (double)((s->adapt - 1.f) * 100.f), g_set.adaptive ? "ADAPTIVE" : "FIXED");
    gfx_text(5, PLAY_TOP + 25, buf, RGB(255,230,130), 1);
    gfx_text(5, PLAY_TOP + 35, s->smooth ? "SMOOTH" : "NOT SMOOTH", s->smooth ? RGB(120,255,120) : RGB(255,100,100), 1);
}

/*
 * Draws the current screen, then the notice line, the meter and any error
 * dialog on top.
 */
static void draw_all(void)
{
    switch (g_state) {
    case ST_TITLE:    draw_title(); break;
    case ST_DIFF:     draw_diff(); break;
    case ST_PLAY:     gfx_draw_game(&g_game, g_frame); break;
    case ST_PAUSE:    draw_pause(); break;
    case ST_NAME:     draw_name(); break;
    case ST_SETTINGS: draw_settings(); break;
    case ST_CONTROLS: draw_controls(); break;
    case ST_SCORES:   draw_scores(); break;
    case ST_HELP:     draw_help(); break;
    case ST_CREDITS:  draw_credits(); break;
    case ST_CAST:     draw_cast(); break;
    case ST_DEMO:     draw_demo(); break;
    case ST_SPLASH:   draw_splash(); break;
    }
    if (g_notice_timer > 0) {
        gfx_blend_rect(0, SCREEN_H - 40, SCREEN_W, 12, RGB(0,0,0), 150);
        gfx_text_center(SCREEN_H - 38, g_notice, RGB(255,240,130), 1);
    }
    if (g_show_sense) draw_sense_meter();
    if (err_pending()) draw_error();
}

/*
 * Sends the finished picture to the window. With the CRT effect on it is first
 * run through the TV filter. SDL scales it up with sharp pixels.
 */
static void present(void)
{
    static uint32_t crt[SCREEN_W * SCREEN_H];
    const uint32_t *src = g_fb;
    if (g_set.crt) { gfx_crt(crt); src = crt; }
    SDL_UpdateTexture(g_tex, NULL, src, SCREEN_W * (int)sizeof(uint32_t));
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
    SDL_RenderPresent(g_ren);
}

/*
 * Saves the picture as shooter-upper-001.png, 002 and so on, using the first
 * number that is free.
 */
static void save_screenshot(void)
{
    char name[64];
    for (int i = 1; i < 1000; i++) {
        snprintf(name, sizeof name, "shooter-upper-%03d.png", i);
        FILE *f = fopen(name, "rb");
        if (!f) break;
        fclose(f);
    }
    if (gfx_save_png(name, g_fb)) notice("SCREENSHOT SAVED");
    else err_report(ERR_SCREENSHOT, name);
}

/* global keys: F11 / Alt+Enter fullscreen, F10 CRT, Tab or F9 meter, F12 screenshot */
static void global_key(const SDL_Event *e)
{
    if (e->type != SDL_KEYDOWN || e->key.repeat) return;
    SDL_Scancode sc = e->key.keysym.scancode;
    bool alt = (e->key.keysym.mod & KMOD_ALT) != 0;
    if (sc == SDL_SCANCODE_F11 || (alt && (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER))) {
        g_set.fullscreen ^= 1; apply_window(); save_settings();
    } else if (sc == SDL_SCANCODE_F10) {
        g_set.crt ^= 1; save_settings();
    } else if (sc == SDL_SCANCODE_TAB || sc == SDL_SCANCODE_F9) {
        if (g_rebind < 0) g_show_sense = !g_show_sense;     /* not while a key is being chosen */
    } else if (sc == SDL_SCANCODE_F12) {
        save_screenshot();
    }
}

/* ======================================================================== */
/* self test: a scripted player that uses the real menus and the real loop  */
/* ======================================================================== */

typedef struct { int frame; SDL_Scancode sc; int kind; } Step;     /* kind 0 tap, 1 press, 2 release */

static const Step SCRIPT[] = {
    {   2, SDL_SCANCODE_RETURN, 0 },                      /* skip the splash */
    {  10, SDL_SCANCODE_RETURN, 0 },                      /* START GAME      */
    {  30, SDL_SCANCODE_RETURN, 0 },                      /* NORMAL          */
    {  60, SDL_SCANCODE_Z, 1 }, { 60, SDL_SCANCODE_SPACE, 1 },
    { 100, SDL_SCANCODE_UP, 1 }, { 160, SDL_SCANCODE_UP, 2 },
    { 200, SDL_SCANCODE_LEFT, 0 },
    { 400, SDL_SCANCODE_Z, 2 }, { 400, SDL_SCANCODE_SPACE, 2 },
    { 410, SDL_SCANCODE_P, 0 },                           /* pause           */
    { 440, SDL_SCANCODE_P, 0 },                           /* resume          */
    { 470, SDL_SCANCODE_P, 0 },                           /* pause again     */
    { 480, SDL_SCANCODE_DOWN, 0 }, { 485, SDL_SCANCODE_DOWN, 0 },
    { 490, SDL_SCANCODE_RETURN, 0 },                      /* QUIT TO TITLE   */
    { 500, SDL_SCANCODE_DOWN, 0 }, { 505, SDL_SCANCODE_DOWN, 0 }, { 510, SDL_SCANCODE_DOWN, 0 },
    { 515, SDL_SCANCODE_RETURN, 0 },                      /* SETTINGS        */
    { 525, SDL_SCANCODE_DOWN, 0 }, { 530, SDL_SCANCODE_DOWN, 0 }, { 535, SDL_SCANCODE_DOWN, 0 },
    { 540, SDL_SCANCODE_RIGHT, 0 },                       /* music louder    */
    { 550, SDL_SCANCODE_ESCAPE, 0 },                      /* back to title   */
};                                                        /* then 20 s of nothing */

static int g_release_at[SDL_NUM_SCANCODES];

/*
 * Self test only: pretends a key was pressed or released, through the same
 * input code a real keyboard uses.
 */
static void send_key(SDL_Scancode sc, bool down)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.keysym.scancode = sc;
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    if (down) input_handle_event(&e);
    input_test_key(sc, down);
}

/*
 * Self test only: at each frame, sends the key presses the script says should
 * happen then, and releases keys that were only tapped.
 */
static void selftest_feed(int frame)
{
    for (size_t i = 0; i < sizeof SCRIPT / sizeof SCRIPT[0]; i++) {
        if (SCRIPT[i].frame != frame) continue;
        if (SCRIPT[i].kind == 2) send_key(SCRIPT[i].sc, false);
        else { send_key(SCRIPT[i].sc, true); if (SCRIPT[i].kind == 0) g_release_at[SCRIPT[i].sc] = frame + 3; }
    }
    for (int sc = 1; sc < SDL_NUM_SCANCODES; sc++)
        if (g_release_at[sc] && g_release_at[sc] == frame) { send_key((SDL_Scancode)sc, false); g_release_at[sc] = 0; }
}

/* ======================================================================== */
/* startup                                                                  */
/* ======================================================================== */

static void usage(void)
{
    puts(GAME_TITLE " " GAME_VERSION "\n"
         "usage: " GAME_ID " [options]\n"
         "  --windowed | --fullscreen   start mode\n"
         "  --scale N                   window size 1..8\n"
         "  --no-audio                  run without sound\n"
         "  --config-dir DIR            use DIR for settings and scores\n"
         "  --dump-music FILE.wav       write the built-in song as a WAV file\n"
         "  --selftest                  play a scripted session through the real menus, then exit\n"
         "  --smoke N                   run N frames of the demo headless, then exit\n"
         "  --screenshot PREFIX         save pictures of the screens (PREFIX-title.png ...) and exit\n"
         "  --version, --help");
}

/* a problem so bad the game cannot go on: show it in a system message box,
 * which works even when there is no window */
static void fatal(ErrCode code, const char *detail)
{
    char text[1200];
    err_report(code, detail);
    err_format_plain(code, detail, text, sizeof text);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, err_info(code)->title, text, NULL);
}

/* look for the song: the player's own, then the one that ships, then built-in */
static void load_music(void)
{
    char path[900];
    snprintf(path, sizeof path, "%s/%s", storage_dir(), MUSIC_FILE_NAME);
    MusicResult r = music_load_file(path, MS_CUSTOM);
    if (r == MR_OK) return;
    if (r != MR_NOT_FOUND) {                           /* the file is there but no good */
        char d[1000];
        snprintf(d, sizeof d, "%s: %s", path, music_result_text(r));
        err_report(ERR_MUSIC_FILE, d);
    }

    char *base = SDL_GetBasePath();
    const char *rel[] = { "music/theme.mp3", "../share/" GAME_ID "/music/theme.mp3" };
    for (int i = 0; base && i < 2; i++) {
        snprintf(path, sizeof path, "%s%s", base, rel[i]);
        if (music_load_file(path, MS_BUNDLED) == MR_OK) { SDL_free(base); return; }
    }
    if (base) SDL_free(base);
    if (music_load_file("/usr/share/" GAME_ID "/music/theme.mp3", MS_BUNDLED) == MR_OK) return;
    if (music_load_file("music/theme.mp3", MS_BUNDLED) == MR_OK) return;
    music_load_builtin();
}

/*
 * Starts the game. First the command line options, then the special modes that
 * need no window (write the song, smoke test, screenshots). After that the
 * window, sound and input are set up and the main loop runs: collect events,
 * run as many 1/60 second steps as the clock says are due, draw, show. At the
 * end the settings are saved and everything is shut down.
 */
int main(int argc, char **argv)
{
    bool force_fs = false, force_win = false;
    int scale_arg = 0, smoke = 0;
    const char *shot = NULL, *dump_music = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else if (!strcmp(argv[i], "--version")) { puts(GAME_TITLE " " GAME_VERSION); return 0; }
        else if (!strcmp(argv[i], "--windowed")) force_win = true;
        else if (!strcmp(argv[i], "--fullscreen")) force_fs = true;
        else if (!strcmp(argv[i], "--no-audio")) g_no_audio = true;
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale_arg = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--config-dir") && i + 1 < argc) storage_set_dir(argv[++i]);
        else if (!strcmp(argv[i], "--selftest")) g_selftest = true;
        else if (!strcmp(argv[i], "--dump-music") && i + 1 < argc) dump_music = argv[++i];
        else if (!strcmp(argv[i], "--smoke") && i + 1 < argc) smoke = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot = argv[++i];
        else { fprintf(stderr, "unknown option: %s\n", argv[i]); usage(); return 2; }
    }

    if (dump_music) {                      /* write the built-in song as a WAV file, to listen to it */
        audio_init(false);
        music_load_builtin();
        bool ok = music_export_wav(dump_music);
        printf(ok ? "wrote %s (%.1f seconds)\n" : "could not write %s\n", dump_music, (double)music_seconds());
        return ok ? 0 : 1;
    }
    g_headless = smoke > 0 || shot != NULL;
    settings_load();
    scores_load();
    err_log_open(storage_dir());
    if (force_fs) g_set.fullscreen = 1;
    if (force_win) g_set.fullscreen = 0;
    if (scale_arg >= 1 && scale_arg <= 8) g_set.scale = scale_arg;

    gfx_init();

    if (g_headless) {
        /* no window, no sound card: just prove the whole game runs */
        audio_init(false);
        music_load_builtin();
        if (shot) {
            /* PREFIX is a prefix: writes PREFIX-title.png, PREFIX-help.png, ... */
            static const struct { const char *name; State st; } T[] = {
                { "title", ST_TITLE }, { "diff", ST_DIFF }, { "help", ST_HELP }, { "settings", ST_SETTINGS },
                { "controls", ST_CONTROLS }, { "scores", ST_SCORES }, { "credits", ST_CREDITS },
                { "name", ST_NAME }, { "pause", ST_PAUSE }, { "play", ST_PLAY }, { "cast", ST_CAST },
                { "error", ST_TITLE }, { "splash", ST_SPLASH }
            };
            start_demo();
            for (int f = 0; f < 700; f++) { g_frame++; sim_step(true); }
            for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
                g_state = T[i].st;
                g_rank = 0; g_menu_sel = 1;
                g_frame = 200;
                g_state_timer = 190;
                if (T[i].st == ST_CAST) { cast_start(5); for (int f = 0; f < 330; f++) { g_frame++; cast_step(); } }
                if (!strcmp(T[i].name, "error")) err_report(ERR_AUDIO_DEVICE, "no device found");
                if (!strcmp(T[i].name, "play")) g_show_sense = true;
                draw_all();
                char path[512];
                snprintf(path, sizeof path, "%s-%s.png", shot, T[i].name);
                if (!gfx_save_png(path, g_fb)) return 1;
                if (!strcmp(T[i].name, "error")) err_dismiss();
            }
            return 0;
        }
        start_demo();
        for (int f = 0; f < smoke; f++) {
            g_frame++;
            g_now_ms = (double)f * 1000.0 / 60.0;
            sense_frame(g_now_ms);
            update_one();                      /* runs the demo through the real state machine */
            if (g_state == ST_TITLE || g_state == ST_SCORES || g_state == ST_CAST) start_demo();
            draw_all();
        }
        printf("smoke ok: %d frames, score %u, wave %d\n", smoke, g_game.score, g_game.wave);
        music_free();
        audio_shutdown();
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fatal(ERR_SDL_INIT, SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");

    g_win = SDL_CreateWindow(GAME_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             SCREEN_W * g_set.scale, SCREEN_H * g_set.scale,
                             SDL_WINDOW_RESIZABLE | (g_set.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!g_win) { fatal(ERR_WINDOW, SDL_GetError()); SDL_Quit(); return 1; }
    SDL_SetWindowMinimumSize(g_win, SCREEN_W, SCREEN_H);
    if (g_selftest) g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE);   /* no graphics card needed */
    else g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED);
    if (!g_ren) {
        g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE);
        if (g_ren) err_report(ERR_RENDERER_SOFT, SDL_GetError());
    }
    if (!g_ren) { fatal(ERR_WINDOW, SDL_GetError()); SDL_Quit(); return 1; }
    {
        SDL_RendererInfo info;
        g_vsync = SDL_GetRendererInfo(g_ren, &info) == 0 && (info.flags & SDL_RENDERER_PRESENTVSYNC);
    }
    SDL_RenderSetLogicalSize(g_ren, SCREEN_W, SCREEN_H);
    SDL_RenderSetIntegerScale(g_ren, SDL_TRUE);
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    if (!g_tex) { fatal(ERR_WINDOW, SDL_GetError()); SDL_Quit(); return 1; }
    SDL_ShowCursor(SDL_DISABLE);

    if (!audio_init(!g_no_audio)) err_report(ERR_AUDIO_DEVICE, SDL_GetError());
    load_music();
    apply_volume();
    input_init();
    input_menu_clear();
    sense_reset();
    sfx(SFX_TITLE);

    const double dt = 1000.0 / SIM_HZ;
    double acc = 0;
    Uint64 last = SDL_GetPerformanceCounter();
    const double freq = (double)SDL_GetPerformanceFrequency();

    while (g_running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) g_running = false;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                if (g_state == ST_PLAY) { go(ST_PAUSE); g_prev_pause = true; }
            }
            else if (e.type == SDL_CONTROLLERDEVICEREMOVED && g_state == ST_PLAY) {
                err_report(ERR_PAD_LOST, NULL);
            }
            else if (g_state == ST_PLAY &&
                     ((e.type == SDL_KEYDOWN && !e.key.repeat) || e.type == SDL_CONTROLLERBUTTONDOWN)) {
                sense_press(g_now_ms);
            }
            global_key(&e);
            input_handle_event(&e);
        }
        input_update();

        Uint64 now = SDL_GetPerformanceCounter();
        if (g_selftest) {                          /* a made-up clock: runs as fast as it can */
            g_now_ms += dt;
            acc += dt;
        } else {
            g_now_ms = (double)now * 1000.0 / freq;
            acc += (double)(now - last) * 1000.0 / freq;
        }
        last = now;
        if (acc > 250.0) acc = 250.0;             /* after a stall do not fast-forward */

        while (acc >= dt) {
            g_frame++;
            if (g_selftest) selftest_feed(g_frame);
            input_update();
            update_one();
            acc -= dt;
        }
        if (g_selftest) {
            const unsigned need = (1u << ST_TITLE) | (1u << ST_DIFF) | (1u << ST_PLAY) | (1u << ST_PAUSE) |
                                  (1u << ST_SETTINGS) | (1u << ST_CAST) | (1u << ST_DEMO) | (1u << ST_SPLASH);
            if ((g_visited & need) == need) {
                printf("selftest ok: all screens reached, longest game %d frames, music volume %d\n",
                       g_max_play_frames, g_set.music);
                g_running = false;
            } else if (g_frame > 6000 || err_pending()) {
                printf("selftest FAILED: visited mask %x of %x, frame %d, dialog %s\n",
                       g_visited, need, g_frame, err_pending() ? err_info(err_current())->title : "none");
                g_running = false;
                g_selftest_failed = true;
            }
        }
        draw_all();
        present();
        sense_frame(g_now_ms);

        /* without vsync the loop would spin at hundreds of pictures a second:
           sleep until the next step is due so every machine runs the same 60 */
        if (!g_vsync) {
            double wait = dt - acc;
            if (wait > 1.5) SDL_Delay((Uint32)(wait - 1.0));
        } else if (!(SDL_GetWindowFlags(g_win) & SDL_WINDOW_SHOWN)) SDL_Delay(10);
    }

    settings_save();
    input_shutdown();
    music_free();
    audio_shutdown();
    SDL_DestroyTexture(g_tex);
    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(g_win);
    SDL_Quit();
    return g_selftest_failed ? 1 : 0;
}

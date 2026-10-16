/*
 * config.h - the one place for the game's name and its tuning numbers.
 *
 * Why a single file: the title lives here only, so renaming the game is a
 * one-line change. The tuning numbers (thrust, drag, speeds) live here too,
 * because they decide how the game FEELS and you will want to tweak them
 * without hunting through the code.
 */
#ifndef CONFIG_H
#define CONFIG_H

#define GAME_TITLE   "SHOOTER UPPER"
#define GAME_ID      "shooter-upper"   /* folder name for settings and scores */
#define GAME_VERSION "1.0.0"

/* ---- screen and world ------------------------------------------------- */
#define SCREEN_W   320      /* internal resolution, scaled up by SDL        */
#define SCREEN_H   224
#define WORLD_W    2560     /* eight screens wide, wraps around             */
#define PLAY_TOP   32       /* the HUD and radar own the rows above this    */
#define SIM_HZ     60       /* logic steps per second, fixed                */

/* ---- ship feel (all values are per simulation step, 1/60 s) ------------ */
#define SHIP_THRUST      0.24f   /* acceleration while thrust is held        */
#define SHIP_DRAG_THRUST 0.968f  /* speed kept per step while thrusting      */
#define SHIP_DRAG_COAST  0.987f  /* speed kept per step while coasting       */
#define SHIP_VSPEED      2.7f    /* vertical speed at full stick             */
#define SHIP_VSMOOTH     0.30f   /* how fast vertical speed follows the stick*/
#define LASER_SPEED      14.0f
#define LASER_LIFE       26
#define LASER_COOLDOWN   6
#define MAX_PLAYER_SHOTS 10
#define RESPAWN_FRAMES   110
#define INVULN_FRAMES    150
#define HYPER_HIDDEN     30
#define HYPER_DEATH_ODDS 6       /* 1 in N hyperspace jumps kills the ship   */

/* ---- rules ------------------------------------------------------------ */
#define START_SPARE_SHIPS 2      /* plus the ship you start in = 3 total     */
#define START_BOMBS       3
#define BONUS_EVERY       10000  /* extra ship + smart bomb                  */
#define MAX_SPARE_SHIPS   9
#define MAX_BOMBS         9
#define HUMANOID_COUNT    10
#define RESTORE_EVERY     5      /* planet comes back on waves 5, 10, 15...  */
#define FALL_SAFE_PIXELS  55     /* falling further than this kills          */

#define SCORE_LANDER   150
#define SCORE_MUTANT   150
#define SCORE_SWARMER  150
#define SCORE_BAITER   200
#define SCORE_BOMBER   250
#define SCORE_POD      1000
#define SCORE_CATCH    500
#define SCORE_LANDING  500
#define SCORE_SAVED    250       /* humanoid fell a short way and lived     */

#define BAITER_AFTER_FRAMES 2400 /* time on a wave before Baiters show up    */
#define BAITER_EVERY_FRAMES 420

#endif

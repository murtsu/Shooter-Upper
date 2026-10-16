/*
 * errors.h - what the game does when something goes wrong.
 *
 * Rule: a problem never ends in a silent failure or a crash. Every problem
 * has a code, and every code has a plain-language dialog with
 *   - what happened,
 *   - the technical detail (a file name, an SDL message),
 *   - a short list of things the player can try,
 *   - buttons (for example RETRY and CONTINUE).
 * Everything is also written to log.txt in the settings folder, so a player
 * can send it to you when they ask for help.
 *
 * This file has no SDL and no drawing code: main.c shows the dialog, and the
 * tests can check every dialog has real text.
 */
#ifndef ERRORS_H
#define ERRORS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ERR_NONE = 0,
    ERR_SDL_INIT,        /* fatal: SDL could not start                    */
    ERR_WINDOW,          /* fatal: no window                              */
    ERR_RENDERER_SOFT,   /* warning: no graphics acceleration             */
    ERR_AUDIO_DEVICE,    /* warning: no sound                             */
    ERR_MUSIC_FILE,      /* warning: player's theme.mp3 is not usable     */
    ERR_SETTINGS_SAVE,   /* warning: could not write settings             */
    ERR_SCORES_SAVE,     /* warning: could not write the high scores      */
    ERR_PAD_LOST,        /* info: controller unplugged during play        */
    ERR_SLOW,            /* warning: the machine cannot keep 60 steps/s   */
    ERR_SCREENSHOT,      /* warning: screenshot could not be saved        */
    ERR_COUNT
} ErrCode;

typedef enum { ERR_ACT_CONTINUE = 0, ERR_ACT_RETRY, ERR_ACT_FIX, ERR_ACT_QUIT } ErrAction;

#define ERR_MAX_TIPS 4
#define ERR_MAX_BUTTONS 3

typedef struct {
    ErrCode     code;
    bool        fatal;
    const char *title;
    const char *what;                          /* what happened, one or two sentences */
    const char *tips[ERR_MAX_TIPS];            /* NULL ends the list                  */
    const char *button[ERR_MAX_BUTTONS];       /* NULL ends the list                  */
    ErrAction   action[ERR_MAX_BUTTONS];
} ErrInfo;

const ErrInfo *err_info(ErrCode c);

/* log file: call once at start with the folder; messages go to <dir>/log.txt */
void err_log_open(const char *dir);

/* report a problem. detail may be NULL. The same code is not queued again
 * within 30 seconds (frames are counted by err_tick). */
void err_report(ErrCode c, const char *detail);
void err_tick(void);                    /* once per simulation step */

bool          err_pending(void);
ErrCode       err_current(void);
const char   *err_current_detail(void);
void          err_dismiss(void);        /* remove the dialog that is showing */

/* the same dialog as plain text, for the system message box and the log */
void err_format_plain(ErrCode c, const char *detail, char *out, size_t n);

#endif

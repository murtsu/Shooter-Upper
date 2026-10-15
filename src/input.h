/*
 * input.h - one layer between devices and the game.
 *
 * The game only knows ACTIONS (thrust, fire, ...). Keyboard keys, controller
 * buttons, sticks and triggers are all mapped onto actions here, through a
 * table that the player can edit (rebinding). That is why adding a new
 * controller or changing a key never touches the game code.
 */
#ifndef INPUT_H
#define INPUT_H

#include <SDL.h>
#include <stdbool.h>
#include "game/game.h"

typedef enum {
    ACT_UP = 0, ACT_DOWN, ACT_LEFT, ACT_RIGHT,
    ACT_THRUST, ACT_REVERSE, ACT_FIRE, ACT_BOMB, ACT_HYPER, ACT_PAUSE,
    ACT_COUNT
} Action;

typedef struct {
    SDL_Scancode key[2];     /* two keyboard keys per action             */
    int          pad;        /* SDL_GameControllerButton, or -1 for none */
} Binding;

typedef enum { SCHEME_CLASSIC = 0, SCHEME_MODERN = 1 } Scheme;

typedef enum {
    MN_UP = 0, MN_DOWN, MN_LEFT, MN_RIGHT, MN_CONFIRM, MN_BACK, MN_COUNT
} MenuKey;

typedef enum { DEV_KEYBOARD = 0, DEV_PAD } Device;

void  input_default_bindings(Binding *b);
void  input_init(void);
void  input_shutdown(void);

/* call for every SDL event, then once per frame */
void  input_handle_event(const SDL_Event *e);
void  input_update(void);

/* gameplay */
void  input_make_controls(Controls *c, Scheme scheme);   /* held + pending pulses */
void  input_clear_pulses(void);                          /* after the first sim step of a frame */
bool  input_held(Action a);

/* menus: true once per press */
bool  input_menu(MenuKey k);
void  input_menu_clear(void);

/* rebinding: returns the next pressed key / pad button, then forgets it */
SDL_Scancode input_take_key(void);
int           input_take_button(void);

/* for the self test only: pretend a key is held down or let go */
void  input_test_key(SDL_Scancode sc, bool down);

Device input_last_device(void);
bool   input_pad_connected(void);

/* text for prompts, matching the controller in use (A/B or Cross/Circle...) */
const char *input_button_label(int sdl_button);
const char *input_key_label(SDL_Scancode sc);
const char *input_confirm_label(void);    /* "ENTER" or "A" / "CROSS"       */
const char *input_back_label(void);       /* "ESC" or "B" / "CIRCLE"        */
const char *input_action_name(Action a);

#endif

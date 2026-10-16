/*
 * input.c - keyboards, controllers, rebinding. See input.h.
 */
#include "input.h"
#include "storage.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define MAX_PADS 4
#define STICK_DEAD   0.35f      /* ignore stick movement below this      */
#define TRIGGER_ON   8000       /* 0..32767                              */

static SDL_GameController *g_pad[MAX_PADS];
static bool   g_held[ACT_COUNT], g_prev[ACT_COUNT], g_pulse[ACT_COUNT];
static bool   g_menu[MN_COUNT];
static bool   g_menu_axis_prev[4];
static SDL_Scancode g_last_key = SDL_SCANCODE_UNKNOWN;
static int    g_last_button = -1;
static Device g_device = DEV_KEYBOARD;
static bool   g_forced[SDL_NUM_SCANCODES];     /* keys held by the self test */

void input_test_key(SDL_Scancode sc, bool down)
{
    if (sc > 0 && sc < SDL_NUM_SCANCODES) g_forced[sc] = down;
}

/*
 * Fills in the factory key and controller buttons for every action. Each
 * action gets two keyboard keys and one pad button. BIND is a small macro that
 * saves typing the same three lines ten times.
 */
void input_default_bindings(Binding *b)
{
    #define BIND(a, k0, k1, p) b[a].key[0] = (k0); b[a].key[1] = (k1); b[a].pad = (p)
    BIND(ACT_UP,      SDL_SCANCODE_UP,     SDL_SCANCODE_W, SDL_CONTROLLER_BUTTON_DPAD_UP);
    BIND(ACT_DOWN,    SDL_SCANCODE_DOWN,   SDL_SCANCODE_S, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    BIND(ACT_LEFT,    SDL_SCANCODE_LEFT,   SDL_SCANCODE_A, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    BIND(ACT_RIGHT,   SDL_SCANCODE_RIGHT,  SDL_SCANCODE_D, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    BIND(ACT_THRUST,  SDL_SCANCODE_Z,      SDL_SCANCODE_K, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    BIND(ACT_REVERSE, SDL_SCANCODE_X,      SDL_SCANCODE_L, SDL_CONTROLLER_BUTTON_B);
    BIND(ACT_FIRE,    SDL_SCANCODE_SPACE,  SDL_SCANCODE_J, SDL_CONTROLLER_BUTTON_A);
    BIND(ACT_BOMB,    SDL_SCANCODE_C,      SDL_SCANCODE_I, SDL_CONTROLLER_BUTTON_X);
    BIND(ACT_HYPER,   SDL_SCANCODE_V,      SDL_SCANCODE_O, SDL_CONTROLLER_BUTTON_Y);
    BIND(ACT_PAUSE,   SDL_SCANCODE_ESCAPE, SDL_SCANCODE_P, SDL_CONTROLLER_BUTTON_START);
    #undef BIND
}

/* ---- controllers -------------------------------------------------------- */

static void open_pad(int joy_index)
{
    if (!SDL_IsGameController(joy_index)) return;
    for (int i = 0; i < MAX_PADS; i++) {
        if (g_pad[i]) continue;
        g_pad[i] = SDL_GameControllerOpen(joy_index);
        return;
    }
}

/*
 * Closes the controller with the given SDL id, for example after it was
 * unplugged, and frees its slot so another one can take its place.
 */
static void close_pad(SDL_JoystickID id)
{
    for (int i = 0; i < MAX_PADS; i++) {
        if (g_pad[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_pad[i])) == id) {
            SDL_GameControllerClose(g_pad[i]);
            g_pad[i] = NULL;
        }
    }
}

/*
 * Starts the controller part of SDL and opens every controller that is already
 * plugged in. Controllers plugged in later arrive as events.
 */
void input_init(void)
{
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_GameControllerEventState(SDL_ENABLE);
    int n = SDL_NumJoysticks();
    for (int i = 0; i < n; i++) open_pad(i);
}

/* Closes all open controllers. Called once when the game quits. */
void input_shutdown(void)
{
    for (int i = 0; i < MAX_PADS; i++)
        if (g_pad[i]) { SDL_GameControllerClose(g_pad[i]); g_pad[i] = NULL; }
}

/* True if at least one controller is open right now. */
bool input_pad_connected(void)
{
    for (int i = 0; i < MAX_PADS; i++) if (g_pad[i]) return true;
    return false;
}

/*
 * Reads one analog stick or trigger from all controllers. Returns -1 to 1 (0
 * to 1 for triggers). If several controllers move, the one pushed furthest
 * wins.
 */
static float axis(SDL_GameControllerAxis ax)
{
    float best = 0.f;
    for (int i = 0; i < MAX_PADS; i++) {
        if (!g_pad[i]) continue;
        float v = (float)SDL_GameControllerGetAxis(g_pad[i], ax) / 32767.f;
        if (v * v > best * best) best = v;
    }
    return best;
}

/*
 * True if the given controller button is held on any controller. A negative
 * number means no button is bound, so the answer is false.
 */
static bool button(int b)
{
    if (b < 0) return false;
    for (int i = 0; i < MAX_PADS; i++)
        if (g_pad[i] && SDL_GameControllerGetButton(g_pad[i], (SDL_GameControllerButton)b)) return true;
    return false;
}

/* ---- action state -------------------------------------------------------- */

static bool axis_for(Action a)
{
    switch (a) {
    case ACT_UP:      return axis(SDL_CONTROLLER_AXIS_LEFTY) < -STICK_DEAD;
    case ACT_DOWN:    return axis(SDL_CONTROLLER_AXIS_LEFTY) >  STICK_DEAD;
    case ACT_LEFT:    return axis(SDL_CONTROLLER_AXIS_LEFTX) < -STICK_DEAD;
    case ACT_RIGHT:   return axis(SDL_CONTROLLER_AXIS_LEFTX) >  STICK_DEAD;
    case ACT_THRUST:  return axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) * 32767.f > TRIGGER_ON;
    case ACT_REVERSE: return axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) * 32767.f > TRIGGER_ON;
    default:          return false;
    }
}

/*
 * Called once per frame. Works out which actions are held (keyboard or
 * controller), sets a one-frame pulse when an action was just pressed, and
 * turns stick flicks into menu moves. It also remembers if the player last
 * used the keyboard or a pad.
 */
void input_update(void)
{
    const Uint8 *ks = SDL_GetKeyboardState(NULL);
    for (int a = 0; a < ACT_COUNT; a++) {
        const Binding *b = &g_set.bind[a];
        bool h = false;
        for (int k = 0; k < 2; k++)
            if (b->key[k] != SDL_SCANCODE_UNKNOWN && (ks[b->key[k]] || g_forced[b->key[k]])) h = true;
        bool padh = button(b->pad) || axis_for((Action)a);
        if (padh && !h && !g_prev[a]) g_device = DEV_PAD;
        g_held[a] = h || padh;
        if (g_held[a] && !g_prev[a]) g_pulse[a] = true;
        g_prev[a] = g_held[a];
    }
    /* stick flicks for menu navigation */
    bool st[4] = {
        axis(SDL_CONTROLLER_AXIS_LEFTY) < -0.6f, axis(SDL_CONTROLLER_AXIS_LEFTY) > 0.6f,
        axis(SDL_CONTROLLER_AXIS_LEFTX) < -0.6f, axis(SDL_CONTROLLER_AXIS_LEFTX) > 0.6f
    };
    static const MenuKey map[4] = { MN_UP, MN_DOWN, MN_LEFT, MN_RIGHT };
    for (int i = 0; i < 4; i++) {
        if (st[i] && !g_menu_axis_prev[i]) { g_menu[map[i]] = true; g_device = DEV_PAD; }
        g_menu_axis_prev[i] = st[i];
    }
}

/*
 * Called for every SDL event. Handles controllers being plugged in or out, and
 * turns key and button presses into menu moves (up, down, confirm, back). It
 * also remembers the last key or button, which the rebinding screen uses.
 */
void input_handle_event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_CONTROLLERDEVICEADDED:    open_pad(e->cdevice.which); break;
    case SDL_CONTROLLERDEVICEREMOVED:  close_pad(e->cdevice.which); break;

    case SDL_KEYDOWN: {
        g_device = DEV_KEYBOARD;
        SDL_Scancode sc = e->key.keysym.scancode;
        if (sc == SDL_SCANCODE_UP)    g_menu[MN_UP] = true;
        if (sc == SDL_SCANCODE_DOWN)  g_menu[MN_DOWN] = true;
        if (sc == SDL_SCANCODE_LEFT)  g_menu[MN_LEFT] = true;
        if (sc == SDL_SCANCODE_RIGHT) g_menu[MN_RIGHT] = true;
        if (e->key.repeat) break;                        /* the rest: first press only */
        g_last_key = sc;
        if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE)
            g_menu[MN_CONFIRM] = true;
        if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE) g_menu[MN_BACK] = true;
        if (sc == SDL_SCANCODE_W) g_menu[MN_UP] = true;
        if (sc == SDL_SCANCODE_S) g_menu[MN_DOWN] = true;
        break;
    }
    case SDL_CONTROLLERBUTTONDOWN: {
        g_device = DEV_PAD;
        int b = e->cbutton.button;
        g_last_button = b;
        if (b == SDL_CONTROLLER_BUTTON_DPAD_UP)    g_menu[MN_UP] = true;
        if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN)  g_menu[MN_DOWN] = true;
        if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT)  g_menu[MN_LEFT] = true;
        if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) g_menu[MN_RIGHT] = true;
        if (b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_START) g_menu[MN_CONFIRM] = true;
        if (b == SDL_CONTROLLER_BUTTON_B)          g_menu[MN_BACK] = true;
        break;
    }
    default: break;
    }
}

/*
 * Packs the current input into the Controls struct the game logic reads. The
 * game never sees keys or pads, only this struct. In MODERN steering the
 * arrows fly that way, in CLASSIC they do nothing and thrust and reverse have
 * their own buttons.
 */
void input_make_controls(Controls *c, Scheme scheme)
{
    memset(c, 0, sizeof *c);
    c->up   = g_held[ACT_UP];
    c->down = g_held[ACT_DOWN];
    c->fire = g_held[ACT_FIRE];
    c->bomb = g_pulse[ACT_BOMB];
    c->hyper = g_pulse[ACT_HYPER];

    if (scheme == SCHEME_MODERN) {
        bool l = g_held[ACT_LEFT], r = g_held[ACT_RIGHT];
        if (l && !r) c->face = -1;
        if (r && !l) c->face = 1;
        c->thrust = g_held[ACT_THRUST] || l || r;
        c->reverse = g_pulse[ACT_REVERSE];          /* still works as a quick flip */
    } else {
        c->thrust = g_held[ACT_THRUST];
        c->reverse = g_pulse[ACT_REVERSE];
    }
}

/*
 * Forgets the one-frame pulses once the game has used them, so a bomb press
 * counts only once.
 */
void input_clear_pulses(void) { memset(g_pulse, 0, sizeof g_pulse); }
/* True while the given action is held down. */
bool input_held(Action a) { return g_held[a]; }

/*
 * True once for each menu key press. Reading it also clears it, so one press
 * moves the menu one step.
 */
bool input_menu(MenuKey k)
{
    bool v = g_menu[k];
    g_menu[k] = false;
    return v;
}
/* Throws away all waiting menu presses, for example when a new screen opens. */
void input_menu_clear(void) { memset(g_menu, 0, sizeof g_menu); }

/*
 * Returns the last key pressed and then forgets it. Gives SDL_SCANCODE_UNKNOWN
 * if nothing was pressed. Used when the player picks a new key.
 */
SDL_Scancode input_take_key(void)
{
    SDL_Scancode k = g_last_key;
    g_last_key = SDL_SCANCODE_UNKNOWN;
    return k;
}
/*
 * Returns the last controller button pressed and then forgets it. Gives -1 if
 * nothing was pressed.
 */
int input_take_button(void)
{
    int b = g_last_button;
    g_last_button = -1;
    return b;
}

/*
 * Tells if the player used the keyboard or a controller last, so the screens
 * can show the matching button names.
 */
Device input_last_device(void) { return g_device; }

/* ---- labels ------------------------------------------------------------- */

static SDL_GameControllerType pad_type(void)
{
    for (int i = 0; i < MAX_PADS; i++)
        if (g_pad[i]) return SDL_GameControllerGetType(g_pad[i]);
    return SDL_CONTROLLER_TYPE_XBOX360;
}

/*
 * Gives the name of a controller button as printed on that kind of pad, for
 * example CROSS on PlayStation and A on Xbox.
 */
const char *input_button_label(int b)
{
    if (b < 0) return "NONE";
    SDL_GameControllerType t = pad_type();
    bool ps = (t == SDL_CONTROLLER_TYPE_PS3 || t == SDL_CONTROLLER_TYPE_PS4 || t == SDL_CONTROLLER_TYPE_PS5);
    bool nin = (t == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO);
    switch (b) {
    case SDL_CONTROLLER_BUTTON_A: return ps ? "CROSS"    : (nin ? "B" : "A");
    case SDL_CONTROLLER_BUTTON_B: return ps ? "CIRCLE"   : (nin ? "A" : "B");
    case SDL_CONTROLLER_BUTTON_X: return ps ? "SQUARE"   : (nin ? "Y" : "X");
    case SDL_CONTROLLER_BUTTON_Y: return ps ? "TRIANGLE" : (nin ? "X" : "Y");
    case SDL_CONTROLLER_BUTTON_BACK:  return ps ? "SHARE" : "BACK";
    case SDL_CONTROLLER_BUTTON_START: return ps ? "OPTIONS" : "START";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:  return "L STICK";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "R STICK";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return ps ? "L1" : "LB";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return ps ? "R1" : "RB";
    case SDL_CONTROLLER_BUTTON_DPAD_UP:    return "D UP";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return "D DOWN";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return "D LEFT";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "D RIGHT";
    default: return "BUTTON";
    }
}

/*
 * Gives a key name in capital letters for the screen. It cycles through four
 * small buffers, so a few labels can be used in the same line of code.
 */
const char *input_key_label(SDL_Scancode sc)
{
    static char buf[4][24];
    static int n = 0;
    char *out = buf[n++ & 3];
    if (sc == SDL_SCANCODE_UNKNOWN) { snprintf(out, 24, "NONE"); return out; }
    const char *name = SDL_GetScancodeName(sc);
    if (!name || !*name) { snprintf(out, 24, "KEY %d", (int)sc); return out; }
    int i = 0;
    for (; name[i] && i < 22; i++) out[i] = (char)toupper((unsigned char)name[i]);
    out[i] = 0;
    return out;
}

/* The name of the confirm button for the device in use: the A button or ENTER. */
const char *input_confirm_label(void)
{
    if (g_device == DEV_PAD) return input_button_label(SDL_CONTROLLER_BUTTON_A);
    return "ENTER";
}
/* The name of the back button for the device in use: the B button or ESC. */
const char *input_back_label(void)
{
    if (g_device == DEV_PAD) return input_button_label(SDL_CONTROLLER_BUTTON_B);
    return "ESC";
}

/* The text shown for an action on the controls screen. */
const char *input_action_name(Action a)
{
    static const char *n[ACT_COUNT] = {
        "UP", "DOWN", "LEFT", "RIGHT", "THRUST", "REVERSE", "FIRE", "SMART BOMB", "HYPERSPACE", "PAUSE"
    };
    return n[a];
}

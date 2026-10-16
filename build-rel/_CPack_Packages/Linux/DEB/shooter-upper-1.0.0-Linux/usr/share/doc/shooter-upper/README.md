# Shooter Upper

An arcade defense shooter. Fly over a wrapping planet, shoot the aliens, and
stop them from carrying your humanoids away. 16-bit looks, 8-bit chip sound.
Every picture is drawn by code and every sound is synthesized by code, so the
game ships as one small program with no art or audio files.

Written in C11 with SDL2. Built and packaged here for Arch Linux.

## Play

    shooter-upper                 start normally
    shooter-upper --fullscreen    start fullscreen
    shooter-upper --help          all options

Keyboard (all keys can be changed in CONTROLS):

| Action        | Keys              | Pad (default)     |
|---------------|-------------------|-------------------|
| Move up/down  | Up/Down or W/S    | D-pad / left stick|
| Thrust        | Z or K            | RB / right trigger|
| Reverse       | X or L            | B / left trigger  |
| Fire          | Space or J        | A                 |
| Smart bomb    | C or I            | X                 |
| Hyperspace    | V or O            | Y                 |
| Pause         | Esc or P          | Start             |

Steering can be CLASSIC (thrust and reverse buttons) or MODERN (left or right
flies that way). Change it in SETTINGS.

Global keys: F11 or Alt+Enter fullscreen, F10 CRT effect, F9 Senseability
meter, F12 screenshot.

**Grades.** START GAME opens a screen with four grades: Easy, Normal, Hard and
Insane. They change enemy speed, how often they shoot, how well they aim, and
how many ships and smart bombs you start with.

**Senseability.** The game watches its own frame rate (target: a steady 60)
and how you play (button presses per second, deaths). With SENSEABILITY on, it
nudges enemy speed by at most 8 percent to suit you. Press F9 to see the
numbers. Details in `docs/SENSEABILITY.md`.

**Music.** The title song plays on every screen except the game itself. Put
your own MP3 at `~/.config/shooter-upper/theme.mp3` to replace it. See
`docs/MUSIC.md`.

**Idle parade.** Leave the title screen alone for 20 seconds and the whole
cast shows up, steered by an imaginary kid with a joystick.

**Problems.** If something goes wrong (no sound device, a bad song file, a
disk that cannot be written, a controller that disappears, a computer that is
too slow) the game shows a dialog that says what happened and what to try.
Everything is also written to `~/.config/shooter-upper/log.txt`.

Controllers: anything SDL2 knows as a game controller (Xbox, PlayStation,
Switch Pro, 8BitDo and so on), hot-plugging supported. Button names in the
menus follow the controller type.

Settings and high scores live in `~/.config/shooter-upper/`.

## Docs

- `docs/BUILDING.md` build, test, package for Arch and Ubuntu
- `docs/MUSIC.md` the song, ACE-Step prompts, file format
- `docs/SENSEABILITY.md` what the sensor measures and does
- `docs/LEGAL.md` name, license and trademark notes (read before selling)
- `docs/STORE.md` store page text and screenshot list
- `docs/PLAN.md`, `docs/RESEARCH.md` design plan and the rules research

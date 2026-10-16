# Senseability

A small sensor in the game (`src/sense.c`). It does two jobs.

## 1. Keeping the game steady at 60

The game logic always runs in fixed steps of 1/60 second. If a picture takes
longer to draw, the game runs more steps to catch up, so the game itself never
slows down. The sensor measures:

| Number     | Meaning                                         |
|------------|-------------------------------------------------|
| FPS        | pictures reaching the screen per second         |
| SIM        | game steps per second (the target is 60)        |
| WORST      | the slowest frame of the last 120 frames, in ms |

On a screen without vertical sync the game also sleeps between pictures, so a
fast computer does not burn power drawing 500 pictures a second.

If the picture rate stays under 45 for three seconds while you play, the game
stops and shows a dialog: "GAME RUNS TOO SLOWLY". FIX IT turns off the CRT
effect and makes the window smaller.

## 2. Tuning to the player

It counts your button presses per second over the last 8 seconds and your
deaths over the last 40 seconds. With SENSEABILITY ON in Settings:

- around 4 presses per second is a calm player and changes nothing
- busy and not dying: enemies get a little faster, up to 8 percent
- dying a lot: enemies get a little slower, down to 8 percent less

The change moves slowly (about 8 seconds), so you never feel it jump. It only
changes enemy speed. The grade you chose (Easy to Insane) sets the base, and
the sensor only nudges around it. Turn it off and the grade is exact.

Press **Tab** in the game (F9 works too) to see all the numbers.

## Why it is built this way

The sensor knows nothing about SDL or the screen. Callers hand it the time in
milliseconds. That is why the test can drive it with a made-up clock and check
exact outcomes, such as "a perfect 60 Hz clock reads 60" or "10 deaths pull the
level down but never below the floor".

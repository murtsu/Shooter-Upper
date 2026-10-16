# Contributing

Bug reports are welcome. Pull requests are welcome too, with one condition:
the code is under an all-rights-reserved license (see `LICENSE`), so by sending
a pull request you agree that the copyright holder may use your change as part
of the game, including in paid releases. If that is not OK with you, open an
issue instead and describe the idea.

## Before you send anything

    cmake -S . -B build -DSU_SANITIZE=ON
    cmake --build build -j
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure

All tests must pass, with no new compiler warnings.

## Rules of the code

- C11, no compiler extensions.
- `src/game/` must stay free of SDL. It runs a fixed 60 Hz step and uses the
  game's own random number generator, so replays stay identical.
- All art and sound are made by code. Do not add pictures or samples copied
  from other games.
- No em-dashes in docs or comments. Short sentences.

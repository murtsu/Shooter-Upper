# Building on Arch Linux

## Dependencies

    sudo pacman -S --needed base-devel cmake sdl2

## Build and run

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/shooter-upper

## Tests

    SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy ctest --test-dir build --output-on-failure

Eight tests:

- `game` rules and difficulty grades (headless, 61 checks)
- `audio` the sound chip: every effect audible, nothing clips
- `music` the MP3 loader, the loop and the built-in song (needs `ffmpeg` to
  make sample files; those parts are skipped without it)
- `sense` the Senseability sensor, driven with made-up clocks
- `errors` every error dialog has real text, the queue and the log work
- `cast` the idle parade always ends, for 25 different random seeds
- `selftest` a scripted player that uses the real menus: title, grade screen,
  play, pause, settings, 20 seconds idle, parade, demo
- `smoke` two minutes of demo play through the whole state machine Add `-DSU_SANITIZE=ON` to the first cmake command to run them under
AddressSanitizer and UBSan.

## Arch package

    packaging/make-source-tarball.sh
    cd packaging/arch
    updpkgsums          # fills in the checksum
    makepkg -si

Before you publish: set `url` and `license` in the PKGBUILD.

## Tuning the feel

All the numbers that decide how the ship handles are in `src/config.h`
(thrust, drag, vertical speed, laser speed). Enemy speeds and spawn timing are
in `src/game/game.c`. The original arcade values are not public, so these were
tuned by estimate. Play it and adjust.

## Layout

    src/game/   the rules, no SDL, runs headless
    src/gfx.c   all drawing into a 320x224 buffer
    src/audio.c software sound chip
    src/input.c keyboard and controllers, rebinding
    src/main.c  window, loop, menus
    tests/      game, audio tests and a screenshot tool

# Building on Ubuntu / Debian

## Dependencies

    sudo apt install build-essential cmake libsdl2-dev

## Build, test, run

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy ctest --test-dir build --output-on-failure
    ./build/shooter-upper

## Make a .deb

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
    cd build && cpack
    sudo apt install ./shooter-upper_1.0.0_amd64.deb

Removing it again: `sudo apt remove shooter-upper`.

Which Ubuntu versions it runs on is decided by the machine you build on. The
package demands the same C library version it was built against, so a .deb
built on Ubuntu 24.04 will not install on 22.04. To support an older release,
build on that release (a container or a VM is enough) and ship one .deb per
release.

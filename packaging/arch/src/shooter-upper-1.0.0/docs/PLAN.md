# Shooter Upper: design plan (C11 + SDL2)

## 1. Why these choices

- **C11.** Fits the project, builds fast, tiny binaries, and the game is small enough that C is not a burden. No hidden allocation, no runtime to ship.
- **SDL2.** One library covers window, input, audio output and a GPU-backed 2D renderer on Windows, Ubuntu and Arch. Its GameController API handles Xbox, PlayStation, Switch Pro and 8BitDo pads through one button layout, so we do not write per-controller code.
- **Software-style rendering at 320x224.** We draw into a small pixel buffer (or render target) and let SDL scale it with integer scaling and nearest filtering. This gives crisp 16-bit pixels and makes screen shake, palette tricks and CRT filters easy.
- **Fixed timestep (60 Hz).** The game logic updates in steps of exactly 1/60 s, no matter how fast the screen refreshes. This keeps behavior identical on every machine, and lets us test logic without a window.
- **Runtime audio synthesis.** We fill the audio buffer in a callback by mixing four channels: two pulse waves with variable duty, one triangle, one noise. No audio files means no licensing problems and a true chip sound.

## 2. Source layout

```
shooter-upper/
  CMakeLists.txt
  src/
    main.c            entry, init, main loop
    config.h          title, constants, tuning tables (single place for name)
    game/             PURE logic, no SDL calls
      world.c/.h      wrapping world, terrain heights, coordinate math
      ship.c/.h       thrust, inertia, reverse, laser, smart bomb, hyperspace
      enemies.c/.h    Lander, Mutant, Baiter, Bomber, Pod, Swarmer behavior
      humanoids.c/.h  grab, carry, fall, catch, land
      waves.c/.h      wave tables, planet restore rules, baiter timer
      score.c/.h      scoring and extra lives
      rng.c/.h        seeded random, so tests are repeatable
    platform/         SDL specific
      video.c/.h      window, scaling, pixel buffer
      input.c/.h      keyboard, controllers, remapping
      audio.c/.h      mixer and chip emulation
      storage.c/.h    per-OS config and high score path
    render/
      sprites.c/.h    original procedural sprite data and palettes
      draw.c/.h       world, radar, HUD, particles, parallax
    ui/
      menus.c/.h      title, settings, pause, name entry
  tests/              headless tests of src/game
  docs/
  packaging/          deb, AppImage, PKGBUILD, Windows scripts
```

The rule that matters: `src/game/` never includes SDL. That is why we can unit test it, and why the feel stays the same on every platform.

## 3. Key technical decisions

- **World coordinates.** World width about 8 screens (2560 pixels at 320 wide). Positions are stored as floats in world space. The camera follows the ship with a lead in the facing direction, like the original, so you see more ahead than behind. Distance between two objects uses the shortest path around the wrap.
- **Radar.** Draws every object scaled down into a strip. Needs the same wrap-aware math.
- **Ship feel.** Thrust adds acceleration in the facing direction, drag is mild, reverse flips facing instantly but keeps momentum. Vertical movement is direct. These numbers are the heart of the feel and will be tuned by hand with you.
- **Enemy AI** as small state machines, one function per type.
- **Particles** for explosions, thrust flame and the planet explosion. Fixed-size pool, no allocations during play.
- **Graphics.** Original sprites defined in code or generated sheets, 16-color palette per sprite, parallax in three layers, optional CRT effect.
- **Audio.** Sound effects described as small recipes (waveform, pitch envelope, duration), mixed from a table so new sounds are easy to add.
- **Input.** An action layer sits between devices and the game: THRUST, REVERSE, FIRE, BOMB, HYPERSPACE, UP, DOWN. Keyboard and controllers both map to actions, so rebinding is just editing a table.
- **Saving.** High scores and settings in a small text or binary file under the user config folder (XDG on Linux, AppData on Windows).

## 4. Controller default layout (proposal)

- Left stick or D-pad: up and down move, left and right set facing and thrust direction.
- A: fire. X: smart bomb. Y: hyperspace. Right trigger: thrust. Left trigger or B: reverse.
- Keyboard: arrows or WASD for movement, Space fire, Z thrust, X reverse, C smart bomb, V hyperspace. All remappable.

Two schemes will be offered: **Classic** (separate thrust and reverse buttons, like the arcade) and **Modern** (stick left and right does thrust and facing together, which is easier for new players).

## 5. Phases

1. Research and plan (this document). **Awaiting your approval.**
2. Engine core: window, loop, input actions, world scroll, ship flight with inertia, placeholder shapes. You can fly around and feel the thrust.
3. Gameplay: enemies, humanoids, abductions, mutants, radar, scoring, waves, bombs, hyperspace.
4. Graphics and audio: 16-bit art, parallax, particles, chip sound, music jingles.
5. Menus, settings, high scores, controller rebinding, attract mode.
6. Packaging for Windows, Ubuntu (deb and AppImage) and Arch (PKGBUILD), CI, legal docs, store text.

## 6. Risks to tell you about now

- **Trademark.** The game is now called "Shooter Upper", which avoids the registered "Defender" name. I have not checked whether "Shooter Upper" itself is taken, so a trademark search is still worth doing before you sell.
- **Feel needs your hands.** Numbers like thrust, drag, enemy speed and spawn timing cannot be copied from a spec. I will run tests, but you need to play each phase build and tell me what feels wrong.
- **Windows testing.** I can cross-compile a Windows build here, but I cannot run it, so a real Windows test by you or a friend is needed before selling.
- **Controller testing.** I can test the code path, not your physical pads.

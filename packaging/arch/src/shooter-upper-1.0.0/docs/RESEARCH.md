# Defender research notes (written in my own words)

Purpose: capture what the 1981 Williams arcade game does and why it feels the way it does, so "Shooter Upper" can reproduce the design with original code, art and sound. Nothing here comes from ROMs or original assets.

## 1. Basic facts

- Designed by Eugene Jarvis with Larry DeMar, Sam Dicker and Paul Dussault. Released early 1981.
- Original screen was 320x256, driven by a 6809 CPU with a separate 6800 for sound. Sound was mono.
- Famous for brutal difficulty. Trade-show visitors were scared off by the complexity. One source quotes an average arcade session of 37 seconds.
- The control layout on the cabinet: a two-way joystick (up and down only) and five buttons: thrust, fire, reverse, smart bomb, hyperspace. The left hand moves the stick, the right hand works the buttons. Jarvis copied the feel of Space Invaders (stick) and Asteroids (buttons) on purpose, since players already knew them.

## 2. World and camera

- The world is a wrapping planet, wider than the screen. The player can scroll either way and it loops. (Steve Ritchie talked Jarvis out of a left-to-right-only scroll.)
- Terrain is a thin line at the bottom. Background stars move slower than the ship, which gives depth.
- A scanner (radar) strip at the top shows every enemy and humanoid in the whole world. Critics called the game unplayable without it.

## 3. Ship and weapons

- Free vertical movement, horizontal movement through thrust with inertia, plus a reverse button that flips facing.
- Laser: unlimited, fires in the facing direction.
- Smart bomb: kills everything visible. Start with 3, gain one per 10,000 points (adjustable by the operator). Strategy guides say it works most reliably when the ship is not moving much.
- Hyperspace: random teleport, can kill the ship on arrival. Guides say save it for emergencies such as many Baiters, or after the planet blows up.
- Start with 3 ships. Extra ship every 10,000 points, together with the smart bomb.

## 4. Humanoids and the abduction loop

- Ten humanoids stand on the planet at the start.
- A Lander descends, grabs a humanoid and carries it upward.
- If the Lander reaches the top with it, the two fuse into a Mutant.
- If you shoot the Lander in time, the humanoid falls. Catch it before it hits the ground, and then lower it to the surface. A fall from too high kills it.
- The ship can carry several caught humanoids.
- If all humanoids are lost, the planet explodes, the terrain vanishes, and play continues in empty space. Reports say all Landers then turn into Mutants.
- The planet and all ten humanoids come back later. Sources disagree on exactly when (see section 9).
- Good players stay low, near the ground, to guard humanoids.

## 5. Enemies

| Enemy | Behavior (from sources) | Points |
|---|---|---|
| Lander | Slow, hunts humanoids, fires shots | 150 |
| Mutant | Fast, homes in on the player at constant speed, fires | 150 |
| Baiter | Appears if the player takes too long on a wave. Very fast, chases the ship, fires | 200 |
| Bomber | Drifts around and leaves stationary mines | 250 |
| Pod | Floats around, bursts into Swarmers when shot (one source says 1 to 7) | 1000 |
| Swarmer | Released by Pods, tracks the player closely | 150 (arcade) or 200 (Atari manual) |

Mutants and Swarmers have an "international date line": they chase the player in a way that avoids crossing certain lines in the world. This keeps them from simply looping across the whole map. Players call it a known exploit. One home-port document says every alien except Landers can leave through the bottom and return from the top.

Destroying a Pod with a smart bomb gives a bonus on top of the Swarmers it releases (one source lists 1,150 to 2,050).

## 6. Scoring (arcade values, cross-checked)

- Humanoid rescue: sources conflict (see section 9). The Atari 800 manual says 250 if a humanoid falls and lives, 500 if caught, 1000 if caught and returned.
- End of wave bonus: 100 per surviving humanoid times the wave number, capped at 500 from wave 5 on.
- One more life and one smart bomb per 10,000 points.

## 7. Wave composition (one source, arcade-history.com)

- Wave 1: 15 Landers
- Wave 2: 20 Landers, 3 Bombers, 1 Pod
- Wave 3: 20 Landers, 4 Bombers, 3 Pods
- Wave 4 and up: 20 Landers, 5 Bombers, 4 Pods

I have only one source with exact numbers. Treat these as a starting point to playtest and tune.

## 8. What the feel comes from (design intent)

Jarvis wanted both offense and defense at once, so the player constantly ranks priorities: hunt Landers, protect humanoids, avoid hazards, manage the radar. The planet blowing up and coming back was meant to mimic life's ups and downs, a comeback for a good player. Baiters exist to stop the player from camping. Pods and Swarmers go after the ship, not the humanoids, which spreads the pressure. The result is a game where something is always going wrong somewhere off screen. Exploding enemies used a particle algorithm by Sam Dicker, so every explosion looks different.

## 9. Conflicts and unknowns (to resolve by playtest or more research)

1. **When the planet returns.** Arcade-history and Giant Bomb say every fifth wave. A Wikipedia text on the sequel says waves 6, 11, 16. An itch.io fan rebuild says every 4 waves. Home-port manuals say every fifth. Plan: make this a constant, default to every fifth wave (waves 5, 10, 15 as arcade-history states), test it.
2. **Swarmer score.** 150 versus 200. Plan: use 150 (arcade-history and Launchbox), keep it in a table.
3. **Humanoid rescue bonus.** Atari values are 500 caught, 1000 returned. Original arcade values are not confirmed in my sources. Plan: 500 for catching, 500 more for a safe landing, then adjust by feel.
4. **Exact speeds, spawn timers, Baiter timing.** No reliable numbers found. These must be tuned by playing. This is where the feel is won or lost.
5. **Mutant behavior after planet loss.** Sources agree Landers become Mutants, but not on details such as spawn timing. Plan: implement as written and tune.

## 10. What we will not copy

Sprites, sounds, music, ROM data, logos, cabinet art, the exact attract-mode screens. Game rules and mechanics are ideas and are free to reuse. The name "Defender" is a registered trademark (filed December 1980), which is a legal risk for the title, see docs/LEGAL.md later.

## 11. Sources

- Wikipedia: Defender (video game), https://en.wikipedia.org/wiki/Defender_(video_game)
- Arcade-History, Defender page, https://www.arcade-history.com/game/614/
- StrategyWiki, Defender walkthrough and gameplay, https://strategywiki.org/wiki/Defender/Walkthrough
- Giant Bomb, Defender, https://www.giantbomb.com/defender/3030-5536/
- Shmups wiki, Defender, https://shmup.fandom.com/wiki/Defender
- Atari 8-bit manual text, https://ctrl-alt-rees.com/archive/www.cyou.com-~richard/DEFENDER.TXT
- Atari magazine review (Creative Computing / Video & Arcade), https://atarimagazines.com/cva/v1n2/defender.php
- Stargate/Defender II page on arcade-history, https://www.arcade-history.com/game/50391/defender-ii
- Fan rebuild description with basic rules, https://rcorney.itch.io/defender

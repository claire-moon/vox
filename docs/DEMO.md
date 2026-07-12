<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# VOX + DIGS v0.0.1 demo guide

The `v0.0.1` demo is an executable statement of intent: a small match in which
the same bounded voxel world drives terrain destruction, granular motion,
liquids, gases, heat, hazards, lighting, projectile impacts, and transient
effects. It is deliberately a complete vertical slice rather than a promise of
the eventual world scale or platform matrix.

## Start a match

From the title screen:

- **Start Match** opens setup. Choose zero to three bots, Coal Ridge,
  Deepworks, or Furnace Yard, a visible 32-bit seed, and an arsenal. Full Works
  enables all ten demo weapons; Miner Kit groups direct mining/firearms, while
  Powder Keg groups thrown explosives and material deposits.
- **VOX Foundry Lab** opens a no-bot sandbox with the same authoritative
  material and weapon rules and a longer experimentation timer.
- **Options** selects the presentation frame cap, Lightfield tier, debug
  overlay, and desktop fullscreen.

Classic FFA runs for two minutes or until the score limit. Rising lava enters
the physical world during the final thirty seconds; it is not a UI-only height
counter. A dead miner leaves voxel effects, then respawns after a deterministic
delay unless the score limit has ended the match.

## Controls

| Context | Input | Action |
|---|---|---|
| Menus | arrows | Move or change a value |
| Menus | `Enter` | Confirm |
| Menus | `Esc` | Back |
| Seed row | `R` | Generate another deterministic seed |
| Match | `A`/`D` or left/right | Run |
| Match | `W`, up, or `Space` | Jump |
| Match | either `Shift` | Rechargeable steam jet |
| Match | mouse | Aim |
| Match | left mouse | Fire/use selected weapon |
| Match | `1`..`0` | Select weapons 1 through 10 |
| Match | mouse wheel | Cycle weapons |
| Match | `R` | Restart the current seed and mode |
| Match | `Esc` | Pause |
| Anywhere | `F1` | Toggle debug overlay |
| Anywhere | `F11` | Toggle desktop fullscreen |

## Full Works arsenal

Every weapon enters the authoritative game path. Melee edits, projectiles,
deposited material, blast damage, cooldowns, kills, and resulting effects feed
the deterministic match state rather than being presentation-only particles.

| Key | Weapon | Demonstrated system |
|---|---|---|
| `1` | Pick | Precise close-range excavation and melee damage |
| `2` | Blast Charge | Fused gravity projectile, terrain blast, smoke, and high damage |
| `3` | Smoke Pot | Fused gravity projectile that deposits buoyant smoke |
| `4` | Cinder Flask | Fused gravity projectile that deposits hot lava and starts heat reactions |
| `5` | Pressure Hose | Fast water stream for displacement, damage, cooling, and steam |
| `6` | Sledge | Broad close-range terrain and player impact |
| `7` | Nail Gun | Fast direct projectile and compact impact effect |
| `8` | Boiler Shotgun | Short-lived multi-projectile spread |
| `9` | Concussion Grenade | Fused gravity projectile with the widest terrain shock |
| `0` | Nail Bomb | Fused explosive with blast damage and nail-like voxel effects |

The pools are fixed and bounded: up to 64 authoritative projectiles and 192
transient voxel effects. A stable slot order, integer positions/velocities, and
bounded projectile substeps make exhaustion and collision behavior repeatable.

## Systemic material playground

The demo uses fourteen stable material IDs: air, bedrock, stone, soil, coal,
biomass, sand, water, lava, metal, flesh, blood, smoke, and firedamp.

- Sand, water, lava, and blood fall; smoke, firedamp, and gaseous water rise.
- Water touching lava converts the lava contact to stone and emits hot smoke
  as the demo's steam representation.
- Lava or sufficiently hot neighbors ignite flammable biomass, coal, and
  firedamp. Burning solid material accumulates damage and becomes smoke.
- Ignited firedamp triggers a bounded terrain blast.
- Bedrock anchors the arena and cannot be removed by ordinary weapons.
- Rising lava is inserted into the bottom of the world and damages miners that
  contact the hazard.

Reactions are intentionally local, ordered, integer-only, and bounded. This is
the first systemic vocabulary, not a chemically complete simulation.

## Lightfield and presentation

The software renderer first resolves the frontmost occupied cell across the
four-cell depth slab, injects skylight and RGB emission from lava/hot cells,
then propagates that light through a `128 x 80` world-space field. Occupied
cells attenuate light more strongly than air. The field is sampled while
writing a caller-owned `320 x 200` RGB24 framebuffer.

The option changes propagation work, not simulation:

| Tier | Propagation passes | Intended use |
|---|---:|---|
| Compatibility | 1 | Lowest-cost CPU presentation and future constrained adapters |
| Balanced | 3 | Default desktop setting |
| Showcase | 6 | Wider glow where CPU headroom permits |

The host builds a render-only snapshot for miners, projectiles, and effects so
they receive the same Lightfield treatment without mutating authoritative
terrain or replay hashes. SDL2 uploads the completed RGB texture and scales it
to the window with letterboxing.

The selectable `30`, `60`, `90`, `120`, `144`, and `Unlimited` caps govern
presentation only. A fixed-step accumulator advances the game in exact
one-sixtieth-second authoritative steps, with at most eight catch-up steps per
presented frame. If a host is overwhelmed, it drops excess wall-clock debt
rather than using a variable timestep: the match may run slower than real time,
but simulated step size and ordering remain unchanged.

Audio is generated procedurally as short queued signed-16-bit mono effects at
22,050 Hz. The demo has no sampled sound dependency. If SDL2 cannot open an
audio device, gameplay and verification continue silently.

## Non-windowed acceptance

After configuring with `-DVOX_BUILD_SDL2_DEMO=ON`, run:

```sh
./build-demo/digs_demo --smoke-test /tmp/digs-demo-smoke.ppm
./build-demo/digs_demo --benchmark 240
```

The smoke scenario uses a fixed map, seed, inputs, bots, weapons, and tick
count, then prints the state and frame hashes. Matching builds should produce
the same hashes; release notes record canonical values only after the release
commit is frozen.

The benchmark prints milliseconds per frame, frames per second, and a frame
hash for each Lightfield tier. It is a CPU renderer microbenchmark, not a GPU,
input-latency, whole-game, or cross-platform performance guarantee. Record the
CPU model, OS, compiler, build type, SDL version, frame count, and power state
with every published result.

For the complete local gate:

```sh
tools/vox-verify.sh
```

Interactive release acceptance also checks menu navigation, all ten weapon
bindings, bot combat, damage/death/respawn, the three map styles, fixed-step
behavior at every frame cap, Lightfield differences, fullscreen transitions,
pause/results flow, audio fallback, debug values, and rising-lava damage.

## Deliberate v0.0.1 limits

- The world is a bounded demo slab, not the planned large or streamed profile.
- Physics provides terrain-colliding fixed-point body proxies and projectile
  impacts, not angular rigid bodies, joints, stacking, or Havok parity.
- Bots are deterministic target-selection demonstrators, not navigation or
  competitive-AI research agents.
- The SDL2 host is a Linux-first acceptance path. The only NASM code is an
  optional Linux x86-64 FNV-1a contract probe; it does not accelerate gameplay
  or rendering. Windows, macOS, historical APIs, production SIMD/assembly, and
  GPU renderers remain evidence-gated work.
- There is no network multiplayer, replay file format, asset pack, mod loader,
  editor, or compatibility promise for later ABI changes yet.

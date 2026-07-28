<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# v0.0.3 foundation

The first demo proves a complete bounded path from input to authoritative
materials and bodies to a lit graphical frame. It does not establish the final
world scale, renderer backend, or platform matrix.

```text
SDL2 input / deterministic bots
              |
              v
      DIGS C89 match rules
       |               |
       v               v
C89 material world   C++98 fixed-point bodies
       |               |
       +-------+-------+
               v
    render-only voxel snapshot
               |
               v
 C89 RGB Lightfield software renderer
               |
               v
 SDL2 texture / custom UI / VOX Audio v2
```

The active v0.0.3 profile is `512 x 320 x 10`, split into 640
`16 x 16 x 10` chunks. Its 1,638,400 cells are exactly forty times the
original demo volume and four times the v0.0.1 dense profile. That expanded
space supports seed-selected archipelagos, continents, twin hills, broad sky,
and a permanent lava basin while still exercising bounded static storage,
cross-chunk material motion, sleeping, dirty regions, projectiles, reactions,
hazards, and multiple miners. Future larger or historical profiles use the
compile-time dimension boundary until cold-chunk compression and streaming
have their own determinism and platform evidence.

Cells remain in one canonical, stable `z/y/x` array. Parallel chunk metadata
tracks occupied and awake counts, active state, noncanonical dirty/revision
information for presentation, and an incremental cell signature. Simulation
skips cold chunks. Canonical hashes include cell signatures and scheduler
metadata, but exclude dirty flags and revisions: clearing a render-upload bit
must not change the replay state.

The material IDs are stable for this ABI: air, bedrock, stone, soil, coal,
biomass, sand, water, lava, metal, flesh, blood, smoke, and firedamp. Occupancy,
wake state, phase, temperature, and accumulated damage are distinct cell
properties. Sand/liquids fall, gases rise, local heat/reaction rules transform
material, and all work is bounded.

DIGS builds Coal Ridge, Deepworks, and Furnace Yard from integer-only
coordinate hashes and a visible seed. The match adds fixed-point players,
health, damage, cooldowns, deterministic bots, fixed projectile/effect pools,
scoring, configurable respawn, authoritative winner/draw results, and rising
lava. The graphical host changes presentation options and submits input; it
does not own a second set of game rules.

The renderer consumes a snapshot. Transient miners, projectiles, and effects
are voxelized into a render-only copy so they can receive the same RGB
Lightfield as terrain without changing the canonical world or state hash. The
UI remains a separate custom RGB drawing layer, as documented by the original
scope.

Names, killfeed entries, hit markers, multikill/spree tracking, speech bubbles,
and audio scheduling live in bounded caller-owned presentation state. The host
drains existing simulation events into that state after each completed tick;
it does not inject feedback-only events into the ring read by AI. These values
remain outside canonical hashes while producing the same result from the same
ordered event stream.

## Determinism rule

Authoritative state changes are integer-only and ordered by stable world, pool,
and player indices. A mutable random stream is not consulted for diagonal
fallback, maps, bots, or effects; decisions derive from stable state such as
seed, tick, slot, and position. The scalar implementation is the oracle for
the included optional NASM FNV-1a contract probe and every future SIMD, worker,
or GPU routine. The probe is not part of the canonical world hash; it first
proves conditional linkage and scalar parity without putting assembly on the
correctness path.

Presentation timing is outside the oracle. The host advances exact 60 Hz
simulation ticks through a fixed-step accumulator whether frames are capped at
15, 30, 60, 90, 120, 144, or unlimited. Lightfield quality, fullscreen, UI,
procedural audio, and frame dropping are excluded from canonical state; an
identical per-tick input sequence must hash identically at every cap.
Simulation deadlines take priority over presentation. An overloaded host skips
or reduces presentation work rather than changing timestep size or discarding
authoritative tick debt. Startup qualification gates a presentation cap the
local machine cannot sustain at its minimum visual tier.

A replay is valid only when ABI, rules, map generator, simulation profile,
data-pack identity, initial seed, input sequence, and state hashes match. The
demo exposes hashes and deterministic smoke scenarios but does not yet define a
stable replay-file format.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# v0.0.1 foundation

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
 SDL2 texture / custom UI / procedural audio
```

The bounded dense profile is `256 x 160 x 10`, split into 160
`16 x 16 x 10` chunks. Its 409,600 cells are exactly ten times the original
demo volume while remaining practical for strict-language test cycles and
exercising map-sized storage, cross-chunk material motion, sleeping, dirty
regions, projectiles, reactions, hazards, and multiple miners. A future large
profile may use a `2048 x 1024 x 4` shallow slab, cold-chunk compression, and
streaming only after memory, determinism, and platform evidence exists.

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
scoring, respawn, and rising lava. The graphical host changes presentation
options and submits input; it does not own a second set of game rules.

The renderer consumes a snapshot. Transient miners, projectiles, and effects
are voxelized into a render-only copy so they can receive the same RGB
Lightfield as terrain without changing the canonical world or state hash. The
UI remains a separate custom RGB drawing layer, as documented by the original
scope.

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
30, 60, 90, 120, 144, or unlimited. Lightfield quality, fullscreen, UI,
procedural audio, and frame dropping are excluded from canonical state; an
identical per-tick input sequence must hash identically at every cap.
Catch-up is bounded at eight ticks per presented frame; an overloaded host
drops excess wall-clock debt instead of changing authoritative timestep size.

A replay is valid only when ABI, rules, map generator, simulation profile,
data-pack identity, initial seed, input sequence, and state hashes match. The
demo exposes hashes and deterministic smoke scenarios but does not yet define a
stable replay-file format.

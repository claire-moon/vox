<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Foundation slice

The first code slice intentionally proves only the ABI and deterministic
headless loop. It is not the final world size or material catalogue.

The production profile will use a `2048 x 1024 x 4` shallow slab, chunked
activity queues, fixed-point body/particle pools, and a renderer-neutral C
ABI. The current bounded development profile is `128 x 80 x 4`, split into
forty `16 x 16 x 4` chunks. It is deliberately small enough for strict C89
tests while exercising map-sized storage, cross-chunk motion, sleeping, and
dirty-region rendering contracts.

Cells remain in one canonical, stable `z/y/x` array. Parallel chunk metadata
tracks occupied and awake cells, an active bit, a noncanonical dirty/revision
pair for render uploads, and an incremental cell signature. Simulation skips
cold chunks. Canonical hashes include cell signatures and scheduler metadata,
but deliberately exclude dirty flags and revisions: clearing a render dirty
bit must not change the authoritative replay state.

The material IDs are stable across profiles: air, bedrock, stone, soil, coal,
biomass, sand, water, lava, metal, flesh, blood, smoke, and firedamp. A cell
can remain occupied while changing phase, such as water becoming a gaseous
steam state. Occupancy and wake state are tracked independently.

The first cellular pass is deterministic and bounded: sand, water, lava, and
blood fall one cell per tick; smoke, firedamp, and gaseous water rise one cell
per tick. Diagonal fallback is selected from the match tick and cell position,
not a mutable random stream. A moved voxel remains awake for the next tick;
settled voxels sleep.

DIGS creates its initial world with integer-only coordinate hashes. Coal
Ridge, Deepworks, and Furnace Yard are deterministic map styles selected by a
visible seed. The generator initially emits static terrain only, then sleeps
the world without clearing dirty revisions; dynamic water, lava, fire, and
collapse enter through subsequent simulation events rather than a hidden
initial settling pass.

## Determinism rule

State changes are integer-only and ordered by stable cell index. The scalar
implementation is the oracle for every future NASM or SIMD routine. A replay
is valid only when its rules, data-pack hashes, simulation profile, and state
hashes match.

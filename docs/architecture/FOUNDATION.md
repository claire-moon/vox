<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Foundation slice

The first code slice intentionally proves only the ABI and deterministic
headless loop. It is not the final world size or material catalogue.

The production profile will use a `2048 x 1024 x 4` shallow slab, chunked
activity queues, fixed-point body/particle pools, and a renderer-neutral C
ABI. The small `32 x 24 x 4` world in this slice keeps the first build easy to
run on every compiler while the interfaces stabilize.

The material IDs are stable across profiles: air, bedrock, stone, soil, coal,
biomass, sand, water, lava, metal, flesh, blood, smoke, and firedamp. A cell
can remain occupied while changing phase, such as water becoming a gaseous
steam state. Occupancy and wake state are tracked independently.

The first cellular pass is deterministic and bounded: sand, water, lava, and
blood fall one cell per tick; smoke, firedamp, and gaseous water rise one cell
per tick. Diagonal fallback is selected from the match tick and cell position,
not a mutable random stream. A moved voxel remains awake for the next tick;
settled voxels sleep.

## Determinism rule

State changes are integer-only and ordered by stable cell index. The scalar
implementation is the oracle for every future NASM or SIMD routine. A replay
is valid only when its rules, data-pack hashes, simulation profile, and state
hashes match.

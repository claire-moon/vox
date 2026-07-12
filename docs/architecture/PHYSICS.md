<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Physics foundation

`vox_physics` is a C++98 implementation behind a C ABI. It reads the C89
world but does not allocate, mutate terrain, use floating point, throw
exceptions, or expose C++ types across the boundary.

The first body profile is an axis-aligned fixed-point capsule proxy with
Q16.16 position, velocity, and half extents. Each tick applies saturating
gravity, clamps speed, then resolves horizontal movement before vertical
movement through deterministic one-sixteenth-cell substeps. A projected
side-view collision query scans all four depth layers. Only materials marked
with `VOX_MATERIAL_SOLID` block bodies, so granular sand can support a miner
while water, lava, blood, smoke, and firedamp remain nonblocking. Terrain
bounds are solid. Ground, ceiling, left, and right impact flags describe the
result of the current step.

This creates the basis for DIGS miners, bullets, casings, and detached voxel
clusters. It is not yet a general rigid-body engine: there are no angular
constraints, contacts between dynamic bodies, fracture clusters, or
Havok-equivalent features. Those remain bounded follow-on systems, all with
the C89 world as their authoritative terrain source.

DIGS v0.0.1 currently instantiates one body for every living match slot. Its
seeded spawn search chooses a non-overlapping position above the first solid
terrain column, then the authoritative match tick advances the body against
the read-only voxel terrain. Player input, jumping, steam-jet thrust, damage,
and projectile impacts are the next game-facing layers; physics does not edit
terrain by itself.

The headless physics test covers gravity landing on sand, high-speed wall
blocking, terrain-read-only stepping, safe terrain-free legacy stepping, and
repeatable collision results from the same input. A separate C89 target proves
the public header and exported C ABI link cleanly to the C++98 implementation.

The current type configuration intentionally requires 8-bit bytes, 16-bit
shorts, and 32-bit ints at compile time. A true 16-bit compatibility port must
first provide a separate fixed-width type policy; it must not silently run the
Q16.16 simulation with a 16-bit `int`.

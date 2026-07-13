<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Physics contract

`vox_physics` is a strict C++98 implementation behind a versioned C ABI. It
reads the C89 world but does not allocate, mutate terrain, use floating-point
state, throw exceptions, enable RTTI, or expose C++ types across the boundary.

The v0.0.1 body is an axis-aligned fixed-point capsule proxy with Q16.16
position, velocity, and half extents. Each 60 Hz tick applies saturating
gravity, clamps speed, then resolves horizontal movement before vertical
movement through bounded one-sixteenth-cell substeps. A projected side-view
query scans all ten depth layers. Only materials marked solid block a body, so
terrain and granular sand support a miner while water, lava, blood, smoke, and
firedamp remain nonblocking. World bounds are solid. Ground, ceiling, left, and
right flags report the current step's contacts.

DIGS owns the game-facing layer around those bodies:

- deterministic spawn search and delayed respawn;
- run, grounded jump, and rechargeable steam-jet acceleration;
- health, damage, last-attacker attribution, death effects, and score limit;
- lava contact damage and match hazards; and
- fixed-pool C89 projectiles with Q16.16 motion, optional gravity/fuse,
  bounded substeps, terrain/player impacts, material deposits, and blasts.

Terrain destruction is never an implicit side effect of the body solver.
The C89 world pass marks awake structural cells unstable when they lose direct
or diagonal support, then moves them through the deterministic gravity solver.
Weapons call bounded C89 world primitives after their authoritative collision
or fuse condition. Conversely, liquids and gases can move without becoming
rigid bodies; the cellular world remains their authority.

This gives DIGS a systemic collision and combat playground, but it is not a
general rigid-body engine and must not be described as Havok-equivalent. The
demo has no angular dynamics, constraints, joints, broadphase for arbitrary
convex bodies, stable object stacking, continuous collision proof, or detached
rigid voxel clusters. Those are future systems with their own bounded-memory,
determinism, and low-core-count acceptance requirements.

Headless tests cover gravity landing, high-speed wall blocking, terrain
read-only stepping, safe terrain-free stepping, repeatable collision results,
the public C ABI, projectile/damage/respawn behavior, and canonical match
hashes. Future SIMD or NASM collision paths must match these scalar results
before they can be selected at runtime.

The current type policy requires 8-bit bytes, 16-bit shorts, and 32-bit ints at
compile time. A true 16-bit target needs an explicit fixed-width type/profile
port; it must not silently compile Q16.16 state with a 16-bit `int`.

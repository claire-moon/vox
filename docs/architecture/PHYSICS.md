<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Physics contract

`vox_physics` is a strict C89 implementation behind a versioned C ABI. It
reads the C89 world but does not allocate, mutate terrain, or use
floating-point state.

Through v0.0.3 this was the project's only C++98 translation unit, compiled
with `-fno-exceptions -fno-rtti` and marked `extern "C"` at every definition.
v0.0.4 is ISO C only, so it was converted to C in place: the seven `extern "C"`
markers were removed and nothing else changed. The conversion is verifiable —
`vox_headless`, `digs_headless`, and the 600-tick load regression all reproduce
their pre-conversion hashes exactly. Dropping the C++ language also removed the
`libstdc++` and `libgcc_s` runtime dependencies from every shipped binary.

The v0.0.4 body is an axis-aligned fixed-point capsule proxy with Q16.16
position, velocity, and half extents. Each 60 Hz tick applies saturating
gravity, clamps speed, then resolves horizontal movement before vertical
movement through bounded one-sixteenth-cell substeps. A projected side-view
query scans all ten depth layers. Only materials marked solid block a body, so
terrain and granular sand support a miner while water, lava, blood, smoke, and
firedamp remain nonblocking. World bounds are solid. Ground, ceiling, left, and
right flags report the current step's contacts. If material motion embeds a
body before its step, the solver searches a deterministic two-cell
neighborhood for the smallest safe correction and reports recovery. Failure is
a distinct collision result that DIGS converts into one normal crush death;
it never invokes spawn search for a living body.

DIGS owns the game-facing layer around those bodies:

- deterministic spawn search and delayed respawn;
- run, grounded jump, and rechargeable lift-and-glide steam acceleration;
- health, damage, last-attacker attribution, death effects, and score limit;
- lava contact damage and match hazards; and
- fixed-pool C89 projectiles with Q16.16 motion, optional gravity/fuse,
  swept one-eighth-cell contact, visible-anatomy impacts, material deposits,
  and blasts.

## v0.0.5 in-tree physics increment

The v0.0.5 work adds three independent authoritative pools. `vox_fluid_world`
stores global x/y/z coordinates, bounded Q16.16 volume, pressure/head,
temperature, and flow for water, lava, and blood. A stable coordinate sort
processes gravity-increasing-y transfer before lateral equalization, carries a
bounded frontier cursor between ticks, and accounts water/lava reaction loss
explicitly. Terrain blocking is checked at the exact destination x/y/z cell,
so a solid in another depth slice cannot silently dam the current slice. DIGS
deposits corpse blood and tool fluids through this API; the host does not
synthesize a second liquid state.

`vox_rigid_world` stores fixed-point oriented rectangular bodies with angle,
angular velocity, mass, inertia, friction, restitution, sleep state, and
bounded joints. Its deterministic solver uses a fixed iteration count, stable
body-pair ordering, integer angle sectors, terrain contacts, body contacts,
joint distance/angle limits, and fixed pool overflow. Killed miners become
connected anatomy assemblies; smoke and gore remain presentation effects,
player-cut terrain uses material-backed effect fragments, and structural debris
and fixture scrap use the rigid pool. A body may sample the authoritative fluid
pool for bounded drag and buoyancy, and long-sleeping corpse/debris/scrap
bodies are recycled through a stable release policy. Settled detached terrain
restores up to sixteen loose cells of its recorded material in stable
radius/depth order. Any compact remainder increments the authoritative, hashed
discard counter instead of silently vanishing. Fixture scrap never restores a
fixture or a replacement fixture rope anchor: only a sleeping metal component
with at least sixteen source cells may place at most four ordinary, stable
metal cells in deterministic nearby air cells. Smaller components stay visual
and recycle; any unplaceable source material is accounted in the same hashed
discard counter. Resting scrap is pass-through to miners. A moving scrap body
can only cause its bounded, nonlethal 1–10 HP direct impact at high speed; it
cannot sever, bury, or kill a miner.

Authored metal fixtures remain static, collision-solid rope targets until a
game-routed explosive fracture removes them. Kernel-owned material-reaction
blasts are intentionally nonqualifying and leave fixtures intact: they cannot
produce the game-owned component scrap, rope event, or player attribution.
Fixtures are explicitly excluded from
support-bearing checks, terrain gravity, unstable marking, structural-risk
scans, and cluster extraction, so they never enter structural work and a
fixture-only blast cannot create a cave-in. Player-caused removal of soil,
stone, coal, biomass, sand, or ordinary metal captures real cleared cells into
bounded fixed-point fragments. They sweep, bounce, and tumble without
colliding with miners; once slow, each retries deterministic nearby placement
as loose terrain while rejecting a living miner's bounds. A saturated effect
pool skips a new fragment instead of evicting a live one. Non-fragment solid
and gore effects use swept contact with the first terrain surface, stop at the
last reachable point, and remain as visible nonblocking landed marks for 180
ticks. They never create terrain, hazards, anchors, or movement blockers, and
an airborne TTL expiry simply removes them. Blood follows the same impact rule
but stains only the frontmost struck terrain voxel dark red and adds its
nonblocking persistent-fluid residue on that actual impact; it never stains
terrain at a wound, death, or airborne expiry and never changes material,
collision, support, or player rendering.

`vox_cluster_extract` performs a stable six-neighbour structural extraction and
`vox_cluster_spawn_debris` hands qualifying detached material to the rigid
pool. The match-owned `vox_structure_state` tracks per-chunk support, load,
collapse risk, and a bounded deferred frontier; invalidation is hashed and
processed in stable chunk order. DIGS consumes up to two queued unsupported
fragments per tick, but emits a cave-in event, award, attribution, and rigid
debris only for an independently detached, non-fixture terrain component of at
least sixteen cells. Smaller components restore as loose nonblocking material,
with no cave-in event or structural award. Qualifying edges are re-invalidated
so a long cut proceeds through bounded, deterministic work. A wide-roof
regression proves successive 128-cell fragments cross chunk boundaries rather
than leaving a far half floating forever. This is a real deferred cascade, not
a claim of an unbounded whole-map solver: complete connected-volume
support/load analysis and exact mixed-material fragment reconstruction remain
open acceptance work.

Rope hooks and weapon rays use the same fixed-point swept-contact policy. The
attached rope is a bounded segmented constraint with terrain wrapping; cable
contact is presentation-only and non-damaging. A rejected wall correction may
not inject radial velocity or store launch energy.

A fired projectile begins at the ray intersection with its owner's body bounds
plus a small muzzle clearance. Its public state records that launch envelope
and a six-tick owner-clear timer. While still inside the envelope it ignores
the owner and overlapping launch cells; leaving the envelope restores normal
collision immediately. External walls still collide, and blast self-damage is
not globally disabled.

Terrain destruction is never an implicit side effect of the body solver.
Legacy world motion still marks awake structural cells and moves granular
materials through the deterministic gravity solver. v0.0.5 weapons additionally
query structural clusters before their blast, then hand detached groups to the
rigid pool. Conversely, fluid volumes move through `vox_fluid_world` without
becoming rigid bodies; the cellular world remains terrain authority.

This gives DIGS a systemic collision and combat playground, but it is not a
general rigid-body engine and must not be described as Havok-equivalent. The
current pool has coarse integer angle sectors, bounded OBB contacts, and no
general convex broadphase. It does have deterministic swept terrain contacts
for its oriented-AABB bounds (including high-speed walls and ceilings), but no
general continuous body/body collision proof. Full-volume fluid coupling under
large-world pressure, exact mixed-material fragment reconstruction, and
complete connected support/load analysis remain future bounded-memory and
low-core-count acceptance work.

Headless tests cover gravity landing, high-speed wall blocking, embedded-body
recovery, failed-crush lifecycle, terrain read-only stepping, rope wall energy,
swept projectile/anatomy contact, deposition, damage/respawn behavior,
fluid conservation and ordering, drowning, rigid angular/joint state, local
cluster extraction, fixture breakage, dropship transitions, and canonical match
hashes. Future SIMD or NASM collision paths must match these scalar results
before they can be selected at runtime.

The current type policy requires 8-bit bytes, 16-bit shorts, and 32-bit ints at
compile time. A true 16-bit target needs an explicit fixed-width type/profile
port; it must not silently compile Q16.16 state with a 16-bit `int`.

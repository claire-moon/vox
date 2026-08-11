<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Weapons, terrain, and material effects

`vox_world_blast` is the C89 terrain-destruction primitive. It removes a
guaranteed destructive core plus a deterministic irregular fracture shell
across the shallow depth layers, marks affected chunks dirty, wakes the
neighboring material ring, and leaves hot smoke at the blast center. Bedrock
remains the arena boundary.

Exposed soil, stone, coal, biomass, and metal voxels enter the bounded gravity
pass when direct and diagonal support is severed, producing cascading cave-ins.
Explosions sample nearby terrain before removal and emit mixed rubble, smoke,
embers, and weapon fragments; settled rubble can rejoin the canonical world.

Weapon IDs are stable across releases; the names on them are not. The enum
constants below were fixed in `v0.0.1` and have never been renumbered, while
the gameplay names have changed twice. Anything persisted -- a saved binding,
a replay, an arsenal mask -- refers to the ID, so renaming a weapon costs
nothing.

| ID | Enum constant | v0.0.4 name | Delivery | Primary world interaction |
|---:|---|---|---|---|
| 0 | `TOOL_PICK` | PULASKI | Melee | Precise excavation |
| 1 | `TOOL_BLAST_CHARGE` | POPPER | Gravity projectile and fuse | Blast, smoke, terrain/player damage |
| 2 | `TOOL_SMOKE_POT` | SMOKER | Gravity projectile and fuse | Buoyant smoke deposit |
| 3 | `TOOL_CINDER_FLASK` | HOT RAIL | Gravity projectile and fuse | Hot lava deposit and ignition |
| 4 | `TOOL_PRESSURE_HOSE` | HYDROSHOT | Fast projectile stream | Water deposit and cooling |
| 5 | `TOOL_SLEDGE` | GIANT FUCKING HAMMER | Melee | Broad short-range impact |
| 6 | `TOOL_NAIL_GUN` | BOLT ACTION | Fast projectile | Direct damage and small terrain impact |
| 7 | `TOOL_BOILER_SHOTGUN` | SCATTERBRAIN | Projectile spread | Multiple short-lived impacts |
| 8 | `TOOL_CONCUSSION_GRENADE` | FIRECRACKER | Gravity projectile and fuse | Wide terrain blast |
| 9 | `TOOL_NAIL_BOMB` | BORING DRILL | Gravity projectile and fuse | Blast plus voxel debris effects |
| 10 | `TOOL_RAIL_GUN` | RAILSHOT | Charged hitscan ray | Piercing line that loses energy through terrain and bodies |

The `v0.0.4` names are `#define` aliases onto the same constants rather than a
second enum, so there is exactly one numbering and no mapping table to drift.

Properties are table-driven: name, cooldown, damage, blast radius, projectile
speed, fuse, and melee/projectile/explosive/deposit/gravity flags. Match rules
carry an arsenal mask, so a host can expose a curated game style without
renumbering weapons. The demo's Full Works style enables all eleven, while
Miner Kit and Powder Keg provide complementary subsets.

`vox_digs_fire_weapon` is the match-facing command. It validates the living
player, weapon mask, cooldown, target, and fixed-pool capacity. Melee resolves
immediately; ranged weapons occupy stable projectile slots and advance through
bounded Q16.16 substeps. Terrain contact, player contact, or fuse expiry then
routes through the normal damage, blast, deposit, and effect primitives.

`vox_digs_use_tool` remains the exact-cell material/terrain primitive used by
focused headless tests and controlled tool paths. Presentation code must not
edit cells directly or spawn decorative projectiles that bypass match rules.

## Reactions after impact

Weapon delivery ends at the material world; subsequent behavior is owned by
the normal C89 step:

- pressure-hose water falls and converts adjacent lava to stone while emitting
  hot smoke/steam;
- cinder-flask lava falls, emits Lightfield energy, and ignites flammable
  neighbors;
- smoke-pot gas rises through open cells;
- hot biomass and coal accumulate burn damage and become smoke; and
- hot firedamp initiates one bounded blast at the stable first ignition site.

Damage, kills, blood/debris effects, material transformations, projectile and
effect slots, cooldowns, and rising lava state enter the canonical match hash.
The render-only voxel snapshot consumes those structures but cannot alter them.

This arsenal demonstrates creative interactions; it is not a final balance or
content promise. Ammunition inventories, pickups, alternate fire, larger rules
packs, weapon scripting, and mod-data formats remain follow-on work.

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

DIGS `v0.0.1` defines ten stable weapon IDs:

| ID | Weapon | Delivery | Primary world interaction |
|---:|---|---|---|
| 0 | Pick | Melee | Precise excavation |
| 1 | Blast Charge | Gravity projectile and fuse | Blast, smoke, terrain/player damage |
| 2 | Smoke Pot | Gravity projectile and fuse | Buoyant smoke deposit |
| 3 | Cinder Flask | Gravity projectile and fuse | Hot lava deposit and ignition |
| 4 | Pressure Hose | Fast projectile stream | Water deposit and cooling |
| 5 | Sledge | Melee | Broad short-range impact |
| 6 | Nail Gun | Fast projectile | Direct damage and small terrain impact |
| 7 | Boiler Shotgun | Projectile spread | Multiple short-lived impacts |
| 8 | Concussion Grenade | Gravity projectile and fuse | Wide terrain blast |
| 9 | Nail Bomb | Gravity projectile and fuse | Blast plus voxel debris effects |

Properties are table-driven: name, cooldown, damage, blast radius, projectile
speed, fuse, and melee/projectile/explosive/deposit/gravity flags. Match rules
carry an arsenal mask, so a host can expose a curated game style without
renumbering weapons. The demo's Full Works style enables all ten, while Miner
Kit and Powder Keg provide complementary five-weapon subsets.

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

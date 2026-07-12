<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Terrain tools foundation

`vox_world_blast` is the C89 terrain-destruction primitive. It removes every
non-bedrock cell inside a bounded side-view radius across the shallow depth
layers, marks affected chunks dirty, wakes the neighboring material ring, and
leaves hot smoke at the blast center when space is available. Bedrock remains
an arena boundary.

DIGS exposes five deterministic v0.0.1 foundation tools through
`vox_digs_use_tool`:

- Pick: one exact editable voxel.
- Blast Charge: a radius-three terrain blast and smoke source.
- Smoke Pot: buoyant smoke.
- Cinder Flask: hot, emissive lava.
- Pressure Hose: falling water.

Every tool validates the player, match phase, target, and bedrock boundary,
then updates the normal match hash. These are not visual shortcuts; smoke,
water, lava, terrain removal, chunk dirtiness, and subsequent movement all
flow through the same C89 world. Damage attribution, ammunition, projectile
delivery, heat reactions, and the remaining weapon catalogue are follow-on
game systems.

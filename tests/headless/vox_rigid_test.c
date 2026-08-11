/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>

#include "vox/vox_rigid.h"

static int same_bodies(const vox_rigid_world *a, const vox_rigid_world *b)
{
    vox_u16 i;
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        if (a->bodies[i].position_x_q16 != b->bodies[i].position_x_q16 ||
            a->bodies[i].position_y_q16 != b->bodies[i].position_y_q16 ||
            a->bodies[i].velocity_x_q16 != b->bodies[i].velocity_x_q16 ||
            a->bodies[i].angle_q16 != b->bodies[i].angle_q16 ||
            a->bodies[i].angular_velocity_q16 !=
                b->bodies[i].angular_velocity_q16 ||
            a->bodies[i].flags != b->bodies[i].flags) return 0;
    }
    return 1;
}

int main(void)
{
    static vox_world terrain;
    static vox_world enclosure;
    vox_rigid_world first;
    vox_rigid_world second;
    vox_rigid_world oriented;
    vox_rigid_world impulse_world;
    vox_rigid_world wet_world;
    vox_rigid_world wall_world;
    vox_rigid_world ceiling_world;
    vox_rigid_world jointed_world;
    vox_fluid_world fluids;
    vox_u16 a;
    vox_u16 b;
    vox_u16 joint;
    vox_u16 tick;
    vox_world_init(&terrain);
    for (a = 0U; a < VOX_WORLD_WIDTH; ++a) {
        if (vox_world_set(&terrain, a, 10U, 0U, VOX_MAT_STONE,
                          20L << 16) != VOX_OK) return 1;
    }
    vox_rigid_init(&first);
    if (vox_rigid_spawn(&first, &a, 20L << 16, 3L << 16,
                        32768L, 32768L, 65536L,
                        VOX_RIGID_BODY_CORPSE) != VOX_OK ||
        vox_rigid_spawn(&first, &b, 21L << 16, 3L << 16,
                        32768L, 32768L, 65536L,
                        VOX_RIGID_BODY_CORPSE) != VOX_OK ||
        vox_rigid_joint_add(&first, &joint, a, b, 65536L,
                            -32768L, 32768L) != VOX_OK) return 2;
    first.bodies[a].angular_velocity_q16 = 2048L;
    second = first;
    for (tick = 0U; tick < 90U; ++tick) {
        if (vox_rigid_step(&first, &terrain, 4096L) != VOX_OK ||
            vox_rigid_step(&second, &terrain, 4096L) != VOX_OK) return 3;
    }
    if (!same_bodies(&first, &second) ||
        vox_rigid_hash(&first) != vox_rigid_hash(&second) ||
        first.bodies[a].position_y_q16 >= 10L << 16 ||
        first.bodies[a].angle_q16 == 0L ||
        first.bodies[a].sleep_ticks == 0U) {
        fprintf(stderr, "rigid deterministic body scenario failed\n");
        return 4;
    }
    /* A quarter-turn uses the integer orientation table: the long side is
     * vertical and the terrain contact must use its rotated support extent. */
    vox_rigid_init(&oriented);
    if (vox_rigid_spawn(&oriented, &a, 40L << 16, 8L << 16,
                        2L << 16, 32768L, 65536L,
                        VOX_RIGID_BODY_DEBRIS) != VOX_OK) return 5;
    oriented.bodies[a].angle_q16 = VOX_RIGID_ANGLE_FULL_TURN_Q16 / 4L;
    if (vox_rigid_step(&oriented, &terrain, 0L) != VOX_OK ||
        oriented.bodies[a].position_y_q16 + (2L << 16) > 10L << 16) {
        fprintf(stderr, "oriented terrain contact failed\n");
        return 6;
    }
    /* An external blast impulse wakes a sleeping body and changes velocity. */
    vox_rigid_init(&impulse_world);
    if (vox_rigid_spawn(&impulse_world, &a, 50L << 16, 2L << 16,
                        32768L, 32768L, 65536L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK ||
        vox_rigid_apply_impulse(&impulse_world, a, 3L << 16,
                                -(2L << 16)) != VOX_OK ||
        impulse_world.bodies[a].velocity_x_q16 != 3L << 16 ||
        impulse_world.bodies[a].velocity_y_q16 != -(2L << 16) ||
        (impulse_world.bodies[a].flags & VOX_RIGID_BODY_SLEEPING) != 0U) {
        fprintf(stderr, "rigid impulse interface failed\n");
        return 7;
    }
    if (vox_rigid_release(&impulse_world, a) != VOX_OK ||
        (impulse_world.bodies[a].flags & VOX_RIGID_BODY_ACTIVE) != 0U ||
        vox_rigid_spawn(&impulse_world, &b, 51L << 16, 2L << 16,
                        32768L, 32768L, 65536L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK || b != a) {
        fprintf(stderr, "rigid release/reuse interface failed\n");
        return 8;
    }
    /* Fluid forces are a separate deterministic coupling, not a hidden
     * second liquid representation in the rigid pool. */
    vox_fluid_init(&fluids);
    if (vox_fluid_add_at(&fluids, 60U, 3U, 0U, VOX_FLUID_WATER,
                         VOX_FLUID_CELL_CAPACITY_Q16, 20L << 16) != VOX_OK) {
        return 9;
    }
    vox_rigid_init(&wet_world);
    if (vox_rigid_spawn(&wet_world, &a, 60L << 16, 3L << 16,
                        32768L, 32768L, 65536L,
                        VOX_RIGID_BODY_CORPSE) != VOX_OK ||
        vox_rigid_step_fluids(&wet_world, &terrain, &fluids,
                              4096L) != VOX_OK ||
        wet_world.bodies[a].velocity_y_q16 >= 4096L) {
        fprintf(stderr, "rigid fluid coupling failed\n");
        return 10;
    }
    /* High-speed scrap must collide with the leading wall/ceiling cell, not
     * merely test its final location after crossing a one-cell obstacle. */
    vox_world_init(&enclosure);
    for (tick = 2U; tick < 10U; ++tick) {
        if (vox_world_set(&enclosure, 70U, tick, 0U, VOX_MAT_STONE,
                          20L << 16) != VOX_OK) return 11;
    }
    for (a = 80U; a < 85U; ++a) {
        if (vox_world_set(&enclosure, a, 2U, 0U, VOX_MAT_STONE,
                          20L << 16) != VOX_OK) return 12;
    }
    vox_rigid_init(&wall_world);
    if (vox_rigid_spawn(&wall_world, &a, 66L << 16, 5L << 16,
                        32768L, 32768L, 65536L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK) return 13;
    wall_world.bodies[a].velocity_x_q16 = 5L << 16;
    if (vox_rigid_step(&wall_world, &enclosure, 0L) != VOX_OK ||
        wall_world.bodies[a].position_x_q16 > (70L << 16) - 32768L ||
        wall_world.bodies[a].velocity_x_q16 >= 0L) {
        fprintf(stderr, "rigid swept wall contact failed\n");
        return 14;
    }
    vox_rigid_init(&ceiling_world);
    if (vox_rigid_spawn(&ceiling_world, &a, 82L << 16, 6L << 16,
                        32768L, 32768L, 65536L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK) return 15;
    ceiling_world.bodies[a].velocity_y_q16 = -(5L << 16);
    if (vox_rigid_step(&ceiling_world, &enclosure, 0L) != VOX_OK ||
        ceiling_world.bodies[a].position_y_q16 < (3L << 16) + 32768L ||
        ceiling_world.bodies[a].velocity_y_q16 <= 0L) {
        fprintf(stderr, "rigid swept ceiling contact failed\n");
        return 16;
    }
    /* Adjacent anatomy segments are constrained by a joint, not pushed apart
     * by a second self-contact solver.  This prevents corpse segments from
     * jittering indefinitely and looking like suspended organs. */
    vox_rigid_init(&jointed_world);
    if (vox_rigid_spawn(&jointed_world, &a, 100L << 16, 3L << 16,
                        32768L, 32768L, 65536L,
                        VOX_RIGID_BODY_CORPSE) != VOX_OK ||
        vox_rigid_spawn(&jointed_world, &b, 100L << 16, 3L << 16,
                        32768L, 32768L, 65536L,
                        VOX_RIGID_BODY_CORPSE) != VOX_OK ||
        vox_rigid_joint_add(&jointed_world, &joint, a, b, 0L,
                            -32768L, 32768L) != VOX_OK ||
        vox_rigid_step(&jointed_world, &terrain, 4096L) != VOX_OK ||
        jointed_world.bodies[a].position_x_q16 !=
        jointed_world.bodies[b].position_x_q16 ||
        jointed_world.bodies[a].position_y_q16 !=
        jointed_world.bodies[b].position_y_q16) {
        fprintf(stderr, "jointed corpse self-contact exclusion failed\n");
        return 17;
    }
    printf("rigid body gravity, joints, angular state and determinism passed\n");
    return 0;
}

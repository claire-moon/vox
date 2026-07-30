/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>

#include "vox/vox_physics.h"

#define TEST_FLOOR_Y 30U
#define TEST_WALL_X 30U

static int build_collision_world(vox_world *world)
{
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    vox_world_init(world);
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            if (vox_world_set(world, x, TEST_FLOOR_Y, z, VOX_MAT_SAND,
                              20L << 16) != VOX_OK) {
                return 1;
            }
        }
        for (y = 0U; y < TEST_FLOOR_Y; ++y) {
            if (vox_world_set(world, TEST_WALL_X, y, z, VOX_MAT_METAL,
                              20L << 16) != VOX_OK) {
                return 2;
            }
        }
    }
    return vox_world_sleep_all(world) == VOX_OK ? 0 : 3;
}

static int test_landing_and_wall(void)
{
    static vox_world world;
    vox_physics_body body;
    vox_physics_step_config config;
    vox_u32 i;
    vox_u32 world_hash;
    if (build_collision_world(&world) != 0) {
        return 1;
    }
    world_hash = vox_world_hash(&world);
    vox_physics_body_init(&body);
    vox_physics_step_config_default(&config);
    body.position_x.value_q16 = 10L << 16;
    body.position_y.value_q16 = 5L << 16;
    for (i = 0U; i < 120U; ++i) {
        if (vox_physics_step_world(&body, &world, &config) != VOX_OK) {
            return 2;
        }
    }
    if (!(body.flags & VOX_PHYSICS_BODY_GROUNDED) ||
        body.velocity_y.value_q16 != 0 ||
        body.position_y.value_q16 + body.half_height_q16 >
            (vox_i32)(TEST_FLOOR_Y << 16)) {
        return 3;
    }
    config.gravity_q16 = 0;
    body.position_x.value_q16 = 20L << 16;
    body.position_y.value_q16 = (vox_i32)(TEST_FLOOR_Y << 16) -
                                body.half_height_q16;
    body.velocity_x.value_q16 = 4L << 16;
    body.velocity_y.value_q16 = 0;
    body.flags = 0U;
    for (i = 0U; i < 4U; ++i) {
        if (vox_physics_step_world(&body, &world, &config) != VOX_OK) {
            return 4;
        }
        if (body.flags & VOX_PHYSICS_BODY_BLOCKED_X) {
            break;
        }
    }
    if (!(body.flags & VOX_PHYSICS_BODY_BLOCKED_X) ||
        !(body.flags & VOX_PHYSICS_BODY_HIT_RIGHT) ||
        body.velocity_x.value_q16 != 0 ||
        body.position_x.value_q16 + body.half_width_q16 >
            (vox_i32)(TEST_WALL_X << 16)) {
        return 5;
    }
    body.position_x.value_q16 = 26L << 16;
    body.velocity_x.value_q16 = 4L << 16;
    body.flags = 0U;
    if (vox_physics_step_world(&body, &world, &config) != VOX_OK ||
        !(body.flags & VOX_PHYSICS_BODY_HIT_RIGHT) ||
        body.position_x.value_q16 + body.half_width_q16 >
            (vox_i32)(TEST_WALL_X << 16)) {
        return 6;
    }
    if (vox_world_hash(&world) != world_hash) {
        return 7;
    }
    return 0;
}

static int test_determinism(void)
{
    static vox_world world;
    vox_physics_body first;
    vox_physics_body second;
    vox_physics_step_config config;
    vox_u32 i;
    vox_u32 world_hash;
    if (build_collision_world(&world) != 0) {
        return 1;
    }
    world_hash = vox_world_hash(&world);
    vox_physics_body_init(&first);
    vox_physics_body_init(&second);
    vox_physics_step_config_default(&config);
    first.position_x.value_q16 = 14L << 16;
    first.position_y.value_q16 = 8L << 16;
    first.velocity_x.value_q16 = 16384L;
    second = first;
    for (i = 0U; i < 90U; ++i) {
        if (vox_physics_step_world(&first, &world, &config) != VOX_OK ||
            vox_physics_step_world(&second, &world, &config) != VOX_OK) {
            return 2;
        }
    }
    if (first.position_x.value_q16 != second.position_x.value_q16 ||
        first.position_y.value_q16 != second.position_y.value_q16 ||
        first.velocity_x.value_q16 != second.velocity_x.value_q16 ||
        first.velocity_y.value_q16 != second.velocity_y.value_q16 ||
        first.flags != second.flags) {
        return 3;
    }
    if (vox_world_hash(&world) != world_hash) {
        return 4;
    }
    return 0;
}

static int test_overlap_recovery(void)
{
    static vox_world world;
    vox_physics_body body;
    vox_physics_body before;
    vox_physics_step_config config;
    if (build_collision_world(&world) != 0) {
        return 1;
    }
    vox_physics_body_init(&body);
    vox_physics_step_config_default(&config);
    body.position_x.value_q16 = TEST_WALL_X << 16;
    body.position_y.value_q16 = 10L << 16;
    body.velocity_x.value_q16 = 2L << 16;
    body.velocity_y.value_q16 = 12345L;
    body.flags = VOX_PHYSICS_BODY_GROUNDED;
    before = body;
    if (vox_physics_recover_overlap(&body, &world) != VOX_OK ||
        !(body.flags & VOX_PHYSICS_BODY_RECOVERED) ||
        !(body.flags & VOX_PHYSICS_BODY_GROUNDED) ||
        body.position_x.value_q16 >= before.position_x.value_q16 ||
        body.position_y.value_q16 != before.position_y.value_q16 ||
        body.position_x.value_q16 + body.half_width_q16 >
            (vox_i32)(TEST_WALL_X << 16) ||
        body.velocity_x.value_q16 != 0L ||
        body.velocity_y.value_q16 != before.velocity_y.value_q16 ||
        body.abi_version != before.abi_version ||
        body.struct_size != before.struct_size ||
        body.half_width_q16 != before.half_width_q16 ||
        body.half_height_q16 != before.half_height_q16) {
        return 2;
    }
    before = body;
    if (vox_physics_recover_overlap(&body, &world) != VOX_OK ||
        (body.flags & VOX_PHYSICS_BODY_RECOVERED) ||
        body.position_x.value_q16 != before.position_x.value_q16 ||
        body.position_y.value_q16 != before.position_y.value_q16) {
        return 3;
    }
    return 0;
}

static int test_unrecoverable_overlap_is_distinct(void)
{
    static vox_world world;
    vox_physics_body body;
    vox_physics_body before;
    vox_physics_step_config config;
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    vox_world_init(&world);
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (y = 8U; y <= 12U; ++y) {
            for (x = 20U; x <= 24U; ++x) {
                if (vox_world_set(&world, x, y, z, VOX_MAT_METAL,
                                  20L << 16) != VOX_OK) {
                    return 1;
                }
            }
        }
    }
    if (vox_world_sleep_all(&world) != VOX_OK) {
        return 2;
    }
    vox_physics_body_init(&body);
    vox_physics_step_config_default(&config);
    config.gravity_q16 = 0L;
    body.position_x.value_q16 = (22L << 16) + 32768L;
    body.position_y.value_q16 = (10L << 16) + 32768L;
    body.velocity_x.value_q16 = 1234L;
    body.velocity_y.value_q16 = -4321L;
    body.flags = VOX_PHYSICS_BODY_GROUNDED;
    before = body;
    if (vox_physics_step_world(&body, &world, &config) !=
            VOX_ERR_COLLISION ||
        body.position_x.value_q16 != before.position_x.value_q16 ||
        body.position_y.value_q16 != before.position_y.value_q16 ||
        body.velocity_x.value_q16 != before.velocity_x.value_q16 ||
        body.velocity_y.value_q16 != before.velocity_y.value_q16 ||
        body.flags != before.flags) {
        return 3;
    }
    return 0;
}

static int test_recovery_skips_step_assist(void)
{
    static vox_world world;
    vox_physics_body body;
    vox_physics_step_config config;
    vox_u32 x;
    vox_u32 z;
    vox_i32 settled_y;
    vox_world_init(&world);
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            if (vox_world_set(&world, x, TEST_FLOOR_Y, z, VOX_MAT_STONE,
                              20L << 16) != VOX_OK) {
                return 1;
            }
        }
        if (vox_world_set(&world, 20U, TEST_FLOOR_Y - 1U, z,
                          VOX_MAT_STONE, 20L << 16) != VOX_OK) {
            return 2;
        }
    }
    if (vox_world_sleep_all(&world) != VOX_OK) {
        return 3;
    }
    vox_physics_body_init(&body);
    vox_physics_step_config_default(&config);
    config.gravity_q16 = 0L;
    settled_y = (vox_i32)(TEST_FLOOR_Y << 16) - body.half_height_q16;
    body.position_x.value_q16 = 18L << 16;
    body.position_y.value_q16 = settled_y + 4096L;
    body.velocity_x.value_q16 = 2L << 16;
    body.velocity_y.value_q16 = 0L;
    body.flags = VOX_PHYSICS_BODY_GROUNDED;
    if (vox_physics_step_world(&body, &world, &config) != VOX_OK ||
        !(body.flags & VOX_PHYSICS_BODY_RECOVERED) ||
        !(body.flags & VOX_PHYSICS_BODY_BLOCKED_X) ||
        body.position_y.value_q16 != settled_y ||
        body.position_x.value_q16 + body.half_width_q16 > (20L << 16)) {
        return 4;
    }
    return 0;
}

static int test_loose_column_collision(void)
{
    static vox_world world;
    vox_physics_body body;
    vox_physics_step_config config;
    vox_world_init(&world);
    if (vox_world_set(&world, 10U, 10U, 0U, VOX_MAT_FLESH,
                      20L << 16) != VOX_OK ||
        vox_world_set_loose(&world, 10U, 10U, 0U, 1U) != VOX_OK ||
        vox_world_sleep_all(&world) != VOX_OK) {
        return 1;
    }
    vox_physics_body_init(&body);
    vox_physics_step_config_default(&config);
    config.gravity_q16 = 0L;
    body.position_x.value_q16 = 8L << 16;
    body.position_y.value_q16 = 10L << 16;
    body.velocity_x.value_q16 = 4L << 16;
    if (vox_physics_step_world(&body, &world, &config) != VOX_OK ||
        (body.flags & VOX_PHYSICS_BODY_BLOCKED_X) ||
        body.position_x.value_q16 != (12L << 16)) {
        return 2;
    }
    if (vox_world_set_loose(&world, 10U, 10U, 0U, 0U) != VOX_OK) {
        return 3;
    }
    vox_physics_body_init(&body);
    body.position_x.value_q16 = 8L << 16;
    body.position_y.value_q16 = 10L << 16;
    body.velocity_x.value_q16 = 4L << 16;
    if (vox_physics_step_world(&body, &world, &config) != VOX_OK ||
        !(body.flags & VOX_PHYSICS_BODY_BLOCKED_X) ||
        body.position_x.value_q16 >= (10L << 16)) {
        return 4;
    }
    return 0;
}

static int test_rejected_rope_correction_has_no_impulse(void)
{
    static vox_world world;
    vox_physics_body body;
    vox_i32 position_x;
    vox_i32 position_y;
    vox_i32 velocity_x;
    vox_i32 velocity_y;
    vox_i32 tension = 0L;
    vox_u16 broken = 0U;
    if (build_collision_world(&world) != 0) {
        return 1;
    }
    vox_physics_body_init(&body);
    body.position_x.value_q16 = (vox_i32)(TEST_WALL_X << 16) -
                                body.half_width_q16 - 4096L;
    body.position_y.value_q16 = 10L << 16;
    body.velocity_x.value_q16 = 12345L;
    body.velocity_y.value_q16 = -2345L;
    position_x = body.position_x.value_q16;
    position_y = body.position_y.value_q16;
    velocity_x = body.velocity_x.value_q16;
    velocity_y = body.velocity_y.value_q16;
    if (vox_physics_rope_constraint(&body, &world, 32L << 16,
                                    10L << 16, 1L << 16, 32768L,
                                    5L << 16, &tension, &broken) != VOX_OK ||
        broken || tension <= 0L ||
        body.position_x.value_q16 != position_x ||
        body.position_y.value_q16 != position_y ||
        body.velocity_x.value_q16 != velocity_x ||
        body.velocity_y.value_q16 != velocity_y) {
        return 2;
    }
    return 0;
}

static int test_grounded_step_assist(void)
{
    static vox_world world;
    vox_physics_body body;
    vox_physics_step_config config;
    vox_u32 x;
    vox_u32 z;
    vox_world_init(&world);
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            if (vox_world_set(&world, x, TEST_FLOOR_Y, z, VOX_MAT_STONE,
                              20L << 16) != VOX_OK) {
                return 1;
            }
        }
        if (vox_world_set(&world, 20U, TEST_FLOOR_Y - 1U, z,
                          VOX_MAT_STONE, 20L << 16) != VOX_OK) {
            return 2;
        }
    }
    if (vox_world_sleep_all(&world) != VOX_OK) {
        return 3;
    }
    vox_physics_body_init(&body);
    vox_physics_step_config_default(&config);
    config.gravity_q16 = 0L;
    body.position_x.value_q16 = 18L << 16;
    body.position_y.value_q16 = (vox_i32)(TEST_FLOOR_Y << 16) -
                                body.half_height_q16;
    body.velocity_x.value_q16 = 2L << 16;
    body.velocity_y.value_q16 = 0L;
    body.flags = VOX_PHYSICS_BODY_GROUNDED;
    if (vox_physics_step_world(&body, &world, &config) != VOX_OK ||
        body.position_x.value_q16 <= (18L << 16) ||
        (body.flags & VOX_PHYSICS_BODY_BLOCKED_X) ||
        body.position_y.value_q16 >=
            (vox_i32)(TEST_FLOOR_Y << 16) - body.half_height_q16) {
        return 4;
    }
    return 0;
}

static int test_unbounded_legacy_step(void)
{
    vox_physics_body body;
    vox_physics_body_init(&body);
    body.position_x.value_q16 = 1L << 16;
    body.velocity_x.value_q16 = 8192L;
    if (vox_physics_step(&body, 0) != VOX_OK ||
        body.position_x.value_q16 != (1L << 16) + 8192L) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_landing_and_wall();
    if (result != 0) {
        fprintf(stderr, "physics collision scenario failed: %d\n", result);
        return 1;
    }
    result = test_determinism();
    if (result != 0) {
        fprintf(stderr, "physics determinism scenario failed: %d\n", result);
        return 2;
    }
    result = test_unbounded_legacy_step();
    if (result != 0) {
        fprintf(stderr, "physics legacy scenario failed: %d\n", result);
        return 3;
    }
    result = test_overlap_recovery();
    if (result != 0) {
        fprintf(stderr, "physics overlap-recovery scenario failed: %d\n",
                result);
        return 4;
    }
    result = test_unrecoverable_overlap_is_distinct();
    if (result != 0) {
        fprintf(stderr, "physics unrecoverable-overlap scenario failed: %d\n",
                result);
        return 5;
    }
    result = test_recovery_skips_step_assist();
    if (result != 0) {
        fprintf(stderr, "physics recovery step-assist scenario failed: %d\n",
                result);
        return 6;
    }
    result = test_loose_column_collision();
    if (result != 0) {
        fprintf(stderr, "physics loose-column scenario failed: %d\n", result);
        return 7;
    }
    result = test_rejected_rope_correction_has_no_impulse();
    if (result != 0) {
        fprintf(stderr, "physics rope rejection scenario failed: %d\n",
                result);
        return 8;
    }
    result = test_grounded_step_assist();
    if (result != 0) {
        fprintf(stderr, "physics step-assist scenario failed: %d\n", result);
        return 9;
    }
    printf("physics deterministic collision proof passed\n");
    return 0;
}

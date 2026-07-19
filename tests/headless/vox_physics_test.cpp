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

static int test_rejects_embedded_body(void)
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
    before = body;
    if (vox_physics_step_world(&body, &world, &config) != VOX_ERR_INVALID ||
        body.position_x.value_q16 != before.position_x.value_q16 ||
        body.position_y.value_q16 != before.position_y.value_q16 ||
        body.velocity_x.value_q16 != before.velocity_x.value_q16 ||
        body.velocity_y.value_q16 != before.velocity_y.value_q16 ||
        body.flags != before.flags) {
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
    result = test_rejects_embedded_body();
    if (result != 0) {
        fprintf(stderr, "physics embedded-body scenario failed: %d\n", result);
        return 4;
    }
    result = test_grounded_step_assist();
    if (result != 0) {
        fprintf(stderr, "physics step-assist scenario failed: %d\n", result);
        return 5;
    }
    printf("physics deterministic collision proof passed\n");
    return 0;
}

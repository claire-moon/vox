/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>

#include "vox/vox_physics.h"

int main(void)
{
    static vox_world world;
    vox_physics_body body;
    vox_physics_step_config config;
    vox_physics_body_init(&body);
    vox_physics_step_config_default(&config);
    body.position_x.value_q16 = 1L << 16;
    body.velocity_x.value_q16 = 8192L;
    config.gravity_q16 = 0;
    if (body.abi_version != VOX_ABI_VERSION ||
        config.abi_version != VOX_ABI_VERSION ||
        vox_physics_step_world(&body, 0, &config) != VOX_OK ||
        body.position_x.value_q16 != (1L << 16) + 8192L) {
        fprintf(stderr, "C ABI physics smoke failed\n");
        return 1;
    }
    vox_world_init(&world);
    if (VOX_ABI_VERSION != 9U || VOX_ERR_COLLISION == VOX_ERR_INVALID ||
        vox_world_set(&world, 4U, 4U, 0U, VOX_MAT_FLESH,
                      20L << 16) != VOX_OK ||
        vox_world_collision_classify(&world, 4U, 4U) !=
            VOX_WORLD_COLLISION_SOLID ||
        vox_world_set_loose(&world, 4U, 4U, 0U, 1U) != VOX_OK ||
        vox_world_collision_classify(&world, 4U, 4U) !=
            VOX_WORLD_COLLISION_LOOSE) {
        fprintf(stderr, "C ABI collision classification failed\n");
        return 2;
    }
    if (vox_world_set_loose(&world, 4U, 4U, 0U, 0U) != VOX_OK) {
        return 3;
    }
    vox_physics_body_init(&body);
    body.position_x.value_q16 = 4L << 16;
    body.position_y.value_q16 = 4L << 16;
    body.velocity_x.value_q16 = 1234L;
    body.velocity_y.value_q16 = 5678L;
    if (vox_physics_recover_overlap(&body, &world) != VOX_OK ||
        !(body.flags & VOX_PHYSICS_BODY_RECOVERED) ||
        (body.velocity_x.value_q16 != 0L &&
         body.velocity_y.value_q16 != 0L)) {
        fprintf(stderr, "C ABI overlap recovery failed\n");
        return 4;
    }
    printf("C ABI physics smoke passed\n");
    return 0;
}

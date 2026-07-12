/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>

#include "vox/vox_physics.h"

int main(void)
{
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
    printf("C ABI physics smoke passed\n");
    return 0;
}

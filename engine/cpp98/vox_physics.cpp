/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_physics.h"

extern "C" vox_result vox_physics_step(vox_physics_body *body,
                                        vox_i32 gravity_q16)
{
    if (body == 0 || body->abi_version != VOX_ABI_VERSION ||
        body->struct_size < (vox_u32)sizeof(*body)) {
        return VOX_ERR_INVALID;
    }
    body->velocity_y.value_q16 += gravity_q16;
    body->position_x.value_q16 += body->velocity_x.value_q16;
    body->position_y.value_q16 += body->velocity_y.value_q16;
    return VOX_OK;
}

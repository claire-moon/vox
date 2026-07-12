/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_PHYSICS_H
#define VOX_PHYSICS_H

#include "vox_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vox_fixed_point {
    vox_i32 value_q16;
} vox_fixed_point;

typedef struct vox_physics_body {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_fixed_point position_x;
    vox_fixed_point position_y;
    vox_fixed_point velocity_x;
    vox_fixed_point velocity_y;
} vox_physics_body;

vox_result vox_physics_step(vox_physics_body *body, vox_i32 gravity_q16);

#ifdef __cplusplus
}
#endif

#endif

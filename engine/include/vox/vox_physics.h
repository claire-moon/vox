/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_PHYSICS_H
#define VOX_PHYSICS_H

#include "vox_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vox_fixed_point {
    vox_i32 value_q16;
} vox_fixed_point;

#define VOX_PHYSICS_BODY_GROUNDED 1U
#define VOX_PHYSICS_BODY_BLOCKED_X 2U
#define VOX_PHYSICS_BODY_HIT_LEFT 4U
#define VOX_PHYSICS_BODY_HIT_RIGHT 8U
#define VOX_PHYSICS_BODY_HIT_CEILING 16U
#define VOX_PHYSICS_BODY_HIT_FLOOR VOX_PHYSICS_BODY_GROUNDED

typedef struct vox_physics_body {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_fixed_point position_x;
    vox_fixed_point position_y;
    vox_fixed_point velocity_x;
    vox_fixed_point velocity_y;
    vox_i32 half_width_q16;
    vox_i32 half_height_q16;
    vox_u16 flags;
    vox_u16 reserved;
} vox_physics_body;

typedef struct vox_physics_step_config {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_i32 gravity_q16;
    vox_i32 max_speed_q16;
    vox_i32 max_step_q16;
    vox_u16 max_substeps;
    vox_u16 reserved;
} vox_physics_step_config;

void vox_physics_body_init(vox_physics_body *body);
void vox_physics_step_config_default(vox_physics_step_config *config);
vox_result vox_physics_step(vox_physics_body *body, vox_i32 gravity_q16);
vox_result vox_physics_step_world(vox_physics_body *body,
                                  const vox_world *world,
                                  const vox_physics_step_config *config);
void vox_physics_accelerate_x(vox_physics_body *body, vox_i32 target_q16,
                              vox_i32 acceleration_q16,
                              vox_i32 deceleration_q16);
vox_result vox_physics_rope_constraint(vox_physics_body *body,
                                       const vox_world *world,
                                       vox_i32 anchor_x_q16,
                                       vox_i32 anchor_y_q16,
                                       vox_i32 length_q16,
                                       vox_i32 pull_q16,
                                       vox_i32 break_tension_q16,
                                       vox_i32 *tension_q16,
                                       vox_u16 *broken);

#ifdef __cplusplus
}
#endif

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_RIGID_H
#define VOX_RIGID_H

#include "vox_kernel.h"
#include "vox_fluid.h"

#define VOX_RIGID_MAX_BODIES 64U
#define VOX_RIGID_MAX_JOINTS 64U
#define VOX_RIGID_SOLVER_ITERATIONS 4U
#define VOX_RIGID_ANGLE_FULL_TURN_Q16 65536L
#define VOX_RIGID_BODY_ACTIVE 1U
#define VOX_RIGID_BODY_SLEEPING 2U
#define VOX_RIGID_BODY_CORPSE 4U
#define VOX_RIGID_BODY_DEBRIS 8U
#define VOX_RIGID_BODY_SCRAP 16U

typedef struct vox_rigid_body {
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_i32 velocity_x_q16;
    vox_i32 velocity_y_q16;
    vox_i32 angle_q16;
    vox_i32 angular_velocity_q16;
    vox_i32 half_width_q16;
    vox_i32 half_height_q16;
    vox_i32 mass_q16;
    vox_i32 inertia_q16;
    vox_i32 friction_q16;
    vox_i32 restitution_q16;
    vox_u16 flags;
    vox_u16 sleep_ticks;
} vox_rigid_body;

typedef struct vox_rigid_joint {
    vox_u16 body_a;
    vox_u16 body_b;
    vox_i32 rest_length_q16;
    vox_i32 min_angle_q16;
    vox_i32 max_angle_q16;
    vox_u16 active;
    vox_u16 reserved;
} vox_rigid_joint;

typedef struct vox_rigid_world {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 tick;
    vox_u16 body_count;
    vox_u16 joint_count;
    vox_rigid_body bodies[VOX_RIGID_MAX_BODIES];
    vox_rigid_joint joints[VOX_RIGID_MAX_JOINTS];
} vox_rigid_world;

void vox_rigid_init(vox_rigid_world *world);
/* External impulses are authoritative gameplay inputs, not render effects. */
vox_result vox_rigid_apply_impulse(vox_rigid_world *world, vox_u16 body_index,
                                   vox_i32 impulse_x_q16,
                                   vox_i32 impulse_y_q16);
vox_result vox_rigid_spawn(vox_rigid_world *world, vox_u16 *body_index,
                           vox_i32 x_q16, vox_i32 y_q16,
                           vox_i32 half_width_q16,
                           vox_i32 half_height_q16, vox_i32 mass_q16,
                           vox_u16 flags);
/* Release a settled or severed body.  The lowest free slot is reused. */
vox_result vox_rigid_release(vox_rigid_world *world, vox_u16 body_index);
vox_result vox_rigid_joint_add(vox_rigid_world *world, vox_u16 *joint_index,
                               vox_u16 body_a, vox_u16 body_b,
                               vox_i32 rest_length_q16,
                               vox_i32 min_angle_q16,
                               vox_i32 max_angle_q16);
vox_result vox_rigid_step(vox_rigid_world *world, const vox_world *terrain,
                          vox_i32 gravity_q16);
/* As above, with deterministic drag and buoyancy from persistent fluids. */
vox_result vox_rigid_step_fluids(vox_rigid_world *world,
                                 const vox_world *terrain,
                                 const vox_fluid_world *fluids,
                                 vox_i32 gravity_q16);
vox_u32 vox_rigid_hash(const vox_rigid_world *world);

#endif

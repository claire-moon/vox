/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <limits.h>

#include "vox/vox_physics.h"

#define VOX_PHYSICS_ONE_Q16 65536L
#define VOX_PHYSICS_SUBSTEP_Q16 4096L
#define VOX_PHYSICS_MAX_SUBSTEPS 64U
#define VOX_PHYSICS_MAX_HALF_EXTENT_Q16 (8L << 16)
#define VOX_PHYSICS_MAX_STEP_Q16 (2L << 16)

static vox_i32 vox_physics_saturating_add(vox_i32 left, vox_i32 right)
{
    if (right > 0 && left > INT_MAX - right) {
        return INT_MAX;
    }
    if (right < 0 && left < INT_MIN - right) {
        return INT_MIN;
    }
    return left + right;
}

static vox_i32 vox_physics_clamp(vox_i32 value, vox_i32 limit)
{
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

static vox_u32 vox_physics_abs(vox_i32 value)
{
    if (value < 0) {
        return (vox_u32)(-(value + 1)) + 1U;
    }
    return (vox_u32)value;
}

static vox_i32 vox_physics_div_trunc_positive(vox_i32 value,
                                              vox_u32 divisor)
{
    vox_u32 magnitude;
    vox_u32 quotient;
    if (divisor == 0U) {
        return 0;
    }
    magnitude = value < 0 ? (vox_u32)(-(value + 1)) + 1U :
                (vox_u32)value;
    quotient = magnitude / divisor;
    if (value >= 0) {
        return (vox_i32)quotient;
    }
    if (quotient == 0x80000000U) {
        return (vox_i32)(-2147483647L - 1L);
    }
    return -(vox_i32)quotient;
}

static vox_i32 vox_physics_q16_floor(vox_i32 value)
{
    if (value >= 0) {
        return value / VOX_PHYSICS_ONE_Q16;
    }
    return -((-(value + 1) / VOX_PHYSICS_ONE_Q16) + 1);
}

static vox_i32 vox_physics_q16_ceil(vox_i32 value)
{
    if (value == INT_MIN) {
        return INT_MIN / VOX_PHYSICS_ONE_Q16;
    }
    if (value >= 0) {
        return value / VOX_PHYSICS_ONE_Q16 +
               (value % VOX_PHYSICS_ONE_Q16 != 0 ? 1 : 0);
    }
    return -vox_physics_q16_floor(-value);
}

static int vox_physics_cell_is_solid(const vox_world *world, vox_i32 x,
                                     vox_i32 y)
{
    vox_u32 z;
    if (x < 0 || y < 0 || x >= (vox_i32)VOX_WORLD_WIDTH ||
        y >= (vox_i32)VOX_WORLD_HEIGHT) {
        return 1;
    }
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        const vox_cell *cell = vox_world_cell(world, (vox_u32)x,
                                               (vox_u32)y, z);
        const vox_material_properties *properties;
        if (cell == 0 || cell->material == VOX_MAT_AIR) {
            continue;
        }
        properties = vox_material_get(cell->material);
        if (properties == 0 || (properties->flags & VOX_MATERIAL_SOLID)) {
            return 1;
        }
    }
    return 0;
}

static int vox_physics_body_collides(const vox_physics_body *body,
                                     const vox_world *world)
{
    vox_i32 min_x;
    vox_i32 max_x;
    vox_i32 min_y;
    vox_i32 max_y;
    vox_i32 x;
    vox_i32 y;
    if (world == 0) {
        return 0;
    }
    min_x = vox_physics_q16_floor(vox_physics_saturating_add(
        body->position_x.value_q16, -body->half_width_q16));
    max_x = vox_physics_saturating_add(vox_physics_q16_ceil(
        vox_physics_saturating_add(body->position_x.value_q16,
                                    body->half_width_q16)), -1);
    min_y = vox_physics_q16_floor(vox_physics_saturating_add(
        body->position_y.value_q16, -body->half_height_q16));
    max_y = vox_physics_saturating_add(vox_physics_q16_ceil(
        vox_physics_saturating_add(body->position_y.value_q16,
                                    body->half_height_q16)), -1);
    if (min_x < 0 || min_y < 0 || max_x >= (vox_i32)VOX_WORLD_WIDTH ||
        max_y >= (vox_i32)VOX_WORLD_HEIGHT) {
        return 1;
    }
    for (y = min_y; y <= max_y; ++y) {
        for (x = min_x; x <= max_x; ++x) {
            if (vox_physics_cell_is_solid(world, x, y)) {
                return 1;
            }
        }
    }
    return 0;
}

static void vox_physics_move_axis(vox_physics_body *body,
                                  const vox_world *world, vox_i32 delta,
                                  int horizontal, vox_u16 max_substeps)
{
    vox_u32 magnitude;
    vox_u32 steps;
    vox_u32 i;
    vox_i32 base_step;
    vox_i32 remainder;
    vox_i32 *position;
    vox_i32 *velocity;
    if (delta == 0) {
        return;
    }
    magnitude = delta < 0 ? (vox_u32)(-(delta + 1)) + 1U :
                (vox_u32)delta;
    steps = (magnitude + VOX_PHYSICS_SUBSTEP_Q16 - 1U) /
            VOX_PHYSICS_SUBSTEP_Q16;
    if (steps == 0U) {
        steps = 1U;
    }
    if (steps > (vox_u32)max_substeps) {
        steps = (vox_u32)max_substeps;
    }
    base_step = vox_physics_div_trunc_positive(delta, steps);
    remainder = delta - base_step * (vox_i32)steps;
    position = horizontal ? &body->position_x.value_q16 :
                            &body->position_y.value_q16;
    velocity = horizontal ? &body->velocity_x.value_q16 :
                            &body->velocity_y.value_q16;
    for (i = 0U; i < steps; ++i) {
        vox_i32 step = base_step;
        vox_i32 candidate;
        if (remainder > 0) {
            step++;
            remainder--;
        } else if (remainder < 0) {
            step--;
            remainder++;
        }
        candidate = vox_physics_saturating_add(*position, step);
        *position = candidate;
        if (vox_physics_body_collides(body, world)) {
            *position = vox_physics_saturating_add(candidate, -step);
            *velocity = 0;
            if (horizontal) {
                body->flags = (vox_u16)(body->flags |
                                         VOX_PHYSICS_BODY_BLOCKED_X |
                                         (delta > 0 ? VOX_PHYSICS_BODY_HIT_RIGHT :
                                                      VOX_PHYSICS_BODY_HIT_LEFT));
            } else if (delta > 0) {
                body->flags = (vox_u16)(body->flags |
                                         VOX_PHYSICS_BODY_GROUNDED);
            } else {
                body->flags = (vox_u16)(body->flags |
                                         VOX_PHYSICS_BODY_HIT_CEILING);
            }
            return;
        }
    }
}

static int vox_physics_try_step(vox_physics_body *body,
                                const vox_world *world,
                                const vox_physics_body *origin,
                                vox_i32 horizontal_delta,
                                vox_i32 max_step_q16,
                                vox_u16 max_substeps)
{
    vox_i32 elevation = VOX_PHYSICS_SUBSTEP_Q16;
    if (world == 0 || max_step_q16 <= 0 || horizontal_delta == 0) {
        return 0;
    }
    while (elevation <= max_step_q16) {
        vox_physics_body candidate = *origin;
        candidate.position_y.value_q16 = vox_physics_saturating_add(
            candidate.position_y.value_q16, -elevation);
        if (!vox_physics_body_collides(&candidate, world)) {
            vox_physics_move_axis(&candidate, world, horizontal_delta, 1,
                                  max_substeps);
            if (!(candidate.flags & VOX_PHYSICS_BODY_BLOCKED_X)) {
                /* Settle onto the raised surface immediately. */
                vox_physics_move_axis(&candidate, world, elevation, 0,
                                      max_substeps);
                *body = candidate;
                return 1;
            }
        }
        if (max_step_q16 - elevation < VOX_PHYSICS_SUBSTEP_Q16) {
            break;
        }
        elevation += VOX_PHYSICS_SUBSTEP_Q16;
    }
    return 0;
}

static int vox_physics_validate_body(const vox_physics_body *body)
{
    return body != 0 && body->abi_version == VOX_ABI_VERSION &&
           body->struct_size >= (vox_u32)sizeof(*body) &&
           body->half_width_q16 > 0 &&
           body->half_height_q16 > 0 &&
           body->half_width_q16 <= VOX_PHYSICS_MAX_HALF_EXTENT_Q16 &&
           body->half_height_q16 <= VOX_PHYSICS_MAX_HALF_EXTENT_Q16 &&
           body->reserved == 0U;
}

static int vox_physics_validate_config(const vox_physics_step_config *config)
{
    return config != 0 && config->abi_version == VOX_ABI_VERSION &&
           config->struct_size >= (vox_u32)sizeof(*config) &&
           config->max_speed_q16 > 0 &&
           config->max_step_q16 >= 0 &&
           config->max_step_q16 <= VOX_PHYSICS_MAX_STEP_Q16 &&
           config->max_substeps > 0U &&
           config->max_substeps <= VOX_PHYSICS_MAX_SUBSTEPS &&
           config->max_speed_q16 <=
               (vox_i32)(config->max_substeps * VOX_PHYSICS_SUBSTEP_Q16) &&
           config->reserved == 0U;
}

extern "C" void vox_physics_body_init(vox_physics_body *body)
{
    if (body == 0) {
        return;
    }
    body->abi_version = VOX_ABI_VERSION;
    body->struct_size = (vox_u32)sizeof(*body);
    body->position_x.value_q16 = 0;
    body->position_y.value_q16 = 0;
    body->velocity_x.value_q16 = 0;
    body->velocity_y.value_q16 = 0;
    body->half_width_q16 = 24576L;
    body->half_height_q16 = 28672L;
    body->flags = 0U;
    body->reserved = 0U;
}

extern "C" void vox_physics_step_config_default(vox_physics_step_config *config)
{
    if (config == 0) {
        return;
    }
    config->abi_version = VOX_ABI_VERSION;
    config->struct_size = (vox_u32)sizeof(*config);
    config->gravity_q16 = 4096L;
    config->max_speed_q16 = 4L << 16;
    config->max_step_q16 = 1L << 16;
    config->max_substeps = VOX_PHYSICS_MAX_SUBSTEPS;
    config->reserved = 0U;
}

extern "C" void vox_physics_accelerate_x(vox_physics_body *body,
                                           vox_i32 target_q16,
                                           vox_i32 acceleration_q16,
                                           vox_i32 deceleration_q16)
{
    vox_i32 current;
    vox_i32 step;
    if (!vox_physics_validate_body(body) || acceleration_q16 <= 0 ||
        deceleration_q16 <= 0) {
        return;
    }
    current = body->velocity_x.value_q16;
    step = acceleration_q16;
    if (target_q16 == 0 ||
        (current < 0 && target_q16 > 0) ||
        (current > 0 && target_q16 < 0)) {
        step = deceleration_q16;
    }
    if (current < target_q16) {
        if (target_q16 - current < step) {
            current = target_q16;
        } else {
            current = vox_physics_saturating_add(current, step);
        }
    } else if (current > target_q16) {
        if (current - target_q16 < step) {
            current = target_q16;
        } else {
            current = vox_physics_saturating_add(current, -step);
        }
    }
    body->velocity_x.value_q16 = current;
}

extern "C" vox_result vox_physics_rope_constraint(
    vox_physics_body *body, const vox_world *world,
    vox_i32 anchor_x_q16, vox_i32 anchor_y_q16,
    vox_i32 length_q16, vox_i32 pull_q16, vox_i32 break_tension_q16,
    vox_i32 *tension_q16, vox_u16 *broken)
{
    vox_i32 delta_x;
    vox_i32 delta_y;
    vox_u32 absolute_x;
    vox_u32 absolute_y;
    vox_u32 largest;
    vox_u32 smallest;
    vox_i32 distance;
    vox_i32 tension;
    vox_i32 correction;
    vox_i32 divisor;
    vox_i32 direction_x_q8;
    vox_i32 direction_y_q8;
    vox_i32 previous_x;
    vox_i32 previous_y;
    if (tension_q16 != 0) {
        *tension_q16 = 0;
    }
    if (broken != 0) {
        *broken = 0U;
    }
    if (!vox_physics_validate_body(body) || world == 0 ||
        world->abi_version != VOX_ABI_VERSION ||
        world->struct_size < (vox_u32)sizeof(*world) ||
        length_q16 <= 0 || pull_q16 <= 0 ||
        pull_q16 > (8L << 16) || break_tension_q16 <= 0 ||
        broken == 0 || anchor_x_q16 < 0 || anchor_y_q16 < 0 ||
        anchor_x_q16 >= (vox_i32)(VOX_WORLD_WIDTH << 16) ||
        anchor_y_q16 >= (vox_i32)(VOX_WORLD_HEIGHT << 16)) {
        return VOX_ERR_INVALID;
    }
    delta_x = anchor_x_q16 - body->position_x.value_q16;
    delta_y = anchor_y_q16 - body->position_y.value_q16;
    absolute_x = vox_physics_abs(delta_x);
    absolute_y = vox_physics_abs(delta_y);
    largest = absolute_x > absolute_y ? absolute_x : absolute_y;
    smallest = absolute_x > absolute_y ? absolute_y : absolute_x;
    if (largest > (vox_u32)INT_MAX - smallest / 2U) {
        distance = INT_MAX;
    } else {
        distance = (vox_i32)(largest + smallest / 2U);
    }
    if (distance <= length_q16) {
        return VOX_OK;
    }
    tension = distance - length_q16;
    if (tension_q16 != 0) {
        *tension_q16 = tension;
    }
    if (tension > break_tension_q16) {
        *broken = 1U;
        return VOX_OK;
    }
    correction = tension < pull_q16 ? tension : pull_q16;
    divisor = distance / 256L;
    if (divisor <= 0) {
        divisor = 1;
    }
    direction_x_q8 = delta_x / divisor;
    direction_y_q8 = delta_y / divisor;
    direction_x_q8 = vox_physics_clamp(direction_x_q8, 256L);
    direction_y_q8 = vox_physics_clamp(direction_y_q8, 256L);
    previous_x = body->position_x.value_q16;
    previous_y = body->position_y.value_q16;
    body->position_x.value_q16 = vox_physics_saturating_add(
        body->position_x.value_q16,
        (correction * direction_x_q8) / 256L);
    body->position_y.value_q16 = vox_physics_saturating_add(
        body->position_y.value_q16,
        (correction * direction_y_q8) / 256L);
    if (vox_physics_body_collides(body, world)) {
        body->position_x.value_q16 = previous_x;
        body->position_y.value_q16 = previous_y;
    }
    body->velocity_x.value_q16 = vox_physics_saturating_add(
        body->velocity_x.value_q16,
        (correction * direction_x_q8) / 512L);
    body->velocity_y.value_q16 = vox_physics_saturating_add(
        body->velocity_y.value_q16,
        (correction * direction_y_q8) / 512L);
    return VOX_OK;
}

extern "C" vox_result vox_physics_step_world(
    vox_physics_body *body, const vox_world *world,
    const vox_physics_step_config *config)
{
    vox_u16 was_grounded;
    vox_i32 horizontal_delta;
    vox_physics_body horizontal_origin;
    if (!vox_physics_validate_body(body) || !vox_physics_validate_config(config) ||
        (world != 0 && (world->abi_version != VOX_ABI_VERSION ||
                        world->struct_size < (vox_u32)sizeof(*world))) ||
        (world != 0 && vox_physics_body_collides(body, world))) {
        return VOX_ERR_INVALID;
    }
    was_grounded = (vox_u16)(body->flags & VOX_PHYSICS_BODY_GROUNDED);
    body->flags = (vox_u16)(body->flags &
                             (vox_u16)~(VOX_PHYSICS_BODY_GROUNDED |
                                       VOX_PHYSICS_BODY_BLOCKED_X |
                                       VOX_PHYSICS_BODY_HIT_LEFT |
                                       VOX_PHYSICS_BODY_HIT_RIGHT |
                                       VOX_PHYSICS_BODY_HIT_CEILING));
    body->velocity_y.value_q16 = vox_physics_saturating_add(
        body->velocity_y.value_q16, config->gravity_q16);
    body->velocity_x.value_q16 = vox_physics_clamp(
        body->velocity_x.value_q16, config->max_speed_q16);
    body->velocity_y.value_q16 = vox_physics_clamp(
        body->velocity_y.value_q16, config->max_speed_q16);
    horizontal_delta = body->velocity_x.value_q16;
    horizontal_origin = *body;
    vox_physics_move_axis(body, world, horizontal_delta, 1,
                          config->max_substeps);
    if (was_grounded && (body->flags & VOX_PHYSICS_BODY_BLOCKED_X) &&
        vox_physics_try_step(body, world, &horizontal_origin,
                             horizontal_delta, config->max_step_q16,
                             config->max_substeps)) {
        body->flags = (vox_u16)(body->flags &
            (vox_u16)~(VOX_PHYSICS_BODY_BLOCKED_X |
                        VOX_PHYSICS_BODY_HIT_LEFT |
                        VOX_PHYSICS_BODY_HIT_RIGHT));
    }
    vox_physics_move_axis(body, world, body->velocity_y.value_q16, 0,
                          config->max_substeps);
    return VOX_OK;
}

extern "C" vox_result vox_physics_step(vox_physics_body *body,
                                        vox_i32 gravity_q16)
{
    vox_physics_step_config config;
    vox_physics_step_config_default(&config);
    config.gravity_q16 = gravity_q16;
    return vox_physics_step_world(body, 0, &config);
}

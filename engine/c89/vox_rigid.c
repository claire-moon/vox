/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_rigid.h"

#define RIGID_SETTLE_SPEED_Q16 8192L
#define RIGID_SLEEP_SPEED_Q16 RIGID_SETTLE_SPEED_Q16
#define RIGID_SLEEP_TICKS 30U
#define RIGID_MAX_CORRECTION_Q16 (2L << 16)
#define RIGID_ONE_Q16 65536L
#define RIGID_HALF_Q16 32768L

/* 16-sector integer lookup table.  Angles use turns in Q16: 65536 is one
 * complete revolution.  The coarse table is intentional: it is small enough
 * for the legacy profile and every path uses the same integer approximation. */
static const vox_i32 rigid_sin_table[16] = {
    0L, 25080L, 46341L, 60547L, 65536L, 60547L, 46341L, 25080L,
    0L, -25080L, -46341L, -60547L, -65536L, -60547L, -46341L, -25080L
};
static const vox_i32 rigid_cos_table[16] = {
    65536L, 60547L, 46341L, 25080L, 0L, -25080L, -46341L, -60547L,
    -65536L, -60547L, -46341L, -25080L, 0L, 25080L, 46341L, 60547L
};

static vox_u32 rigid_abs(vox_i32 value)
{
    return value < 0L ? (vox_u32)(-(value + 1L)) + 1U : (vox_u32)value;
}

static vox_i32 rigid_distance(vox_i32 dx, vox_i32 dy)
{
    vox_u32 ax = rigid_abs(dx);
    vox_u32 ay = rigid_abs(dy);
    vox_u32 largest = ax > ay ? ax : ay;
    vox_u32 smallest = ax > ay ? ay : ax;
    if (largest > 2147483647U - smallest / 2U) {
        return 2147483647L;
    }
    return (vox_i32)(largest + smallest / 2U);
}

static vox_i32 rigid_mul_q16(vox_i32 a, vox_i32 b)
{
    /* Q8 decomposition keeps the product inside a signed 32-bit C89 int. */
    return (a / 256L) * (b / 256L);
}

static vox_i32 rigid_abs_i32(vox_i32 value)
{
    return value < 0L ? -value : value;
}

static void rigid_basis(vox_i32 angle_q16, vox_i32 *cos_q16,
                        vox_i32 *sin_q16)
{
    vox_i32 normalized = angle_q16 % RIGID_ONE_Q16;
    vox_u16 index;
    if (normalized < 0L) normalized += RIGID_ONE_Q16;
    index = (vox_u16)((vox_u32)normalized >> 12);
    *cos_q16 = rigid_cos_table[index];
    *sin_q16 = rigid_sin_table[index];
}

static vox_i32 rigid_dot(vox_i32 ax, vox_i32 ay, vox_i32 bx, vox_i32 by)
{
    return rigid_mul_q16(ax, bx) + rigid_mul_q16(ay, by);
}

static vox_i32 rigid_radius_on_axis(vox_i32 axis_x, vox_i32 axis_y,
                                    vox_i32 body_cos, vox_i32 body_sin,
                                    vox_i32 half_width,
                                    vox_i32 half_height)
{
    vox_i32 x_projection = rigid_abs_i32(rigid_dot(axis_x, axis_y,
                                                   body_cos, body_sin));
    vox_i32 y_projection = rigid_abs_i32(rigid_dot(axis_x, axis_y,
                                                   -body_sin, body_cos));
    return rigid_mul_q16(x_projection, half_width) +
           rigid_mul_q16(y_projection, half_height);
}

static vox_i32 rigid_project_correction(vox_i32 value, vox_i32 correction,
                                         vox_i32 distance)
{
    vox_i32 denominator = distance / 4096L;
    vox_i32 projected;
    if (denominator <= 0L) denominator = 1L;
    projected = rigid_mul_q16(value, correction);
    return (projected * 16L) / denominator;
}

static vox_i32 rigid_clamp_correction(vox_i32 value)
{
    if (value > RIGID_MAX_CORRECTION_Q16) return RIGID_MAX_CORRECTION_Q16;
    if (value < -RIGID_MAX_CORRECTION_Q16) return -RIGID_MAX_CORRECTION_Q16;
    return value;
}

static int rigid_active(const vox_rigid_body *body)
{
    return body != 0 && (body->flags & VOX_RIGID_BODY_ACTIVE) != 0U;
}

static vox_u32 rigid_mix(vox_u32 hash, vox_u32 value)
{
    hash ^= value;
    hash *= 16777619U;
    return hash;
}

void vox_rigid_init(vox_rigid_world *world)
{
    vox_u16 i;
    if (world == 0) return;
    world->abi_version = VOX_ABI_VERSION;
    world->struct_size = (vox_u32)sizeof(*world);
    world->tick = 0U;
    world->body_count = 0U;
    world->joint_count = 0U;
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        world->bodies[i].position_x_q16 = 0L;
        world->bodies[i].position_y_q16 = 0L;
        world->bodies[i].velocity_x_q16 = 0L;
        world->bodies[i].velocity_y_q16 = 0L;
        world->bodies[i].angle_q16 = 0L;
        world->bodies[i].angular_velocity_q16 = 0L;
        world->bodies[i].half_width_q16 = 0L;
        world->bodies[i].half_height_q16 = 0L;
        world->bodies[i].mass_q16 = 65536L;
        world->bodies[i].inertia_q16 = 65536L;
        world->bodies[i].friction_q16 = 32768L;
        world->bodies[i].restitution_q16 = 8192L;
        world->bodies[i].flags = 0U;
        world->bodies[i].sleep_ticks = 0U;
    }
    for (i = 0U; i < VOX_RIGID_MAX_JOINTS; ++i) {
        world->joints[i].body_a = 0U;
        world->joints[i].body_b = 0U;
        world->joints[i].rest_length_q16 = 0L;
        world->joints[i].min_angle_q16 = -2147483647L;
        world->joints[i].max_angle_q16 = 2147483647L;
        world->joints[i].active = 0U;
        world->joints[i].reserved = 0U;
    }
}

vox_result vox_rigid_apply_impulse(vox_rigid_world *world, vox_u16 body_index,
                                   vox_i32 impulse_x_q16,
                                   vox_i32 impulse_y_q16)
{
    vox_rigid_body *body;
    vox_i32 mass_units;
    if (world == 0 || world->abi_version != VOX_ABI_VERSION ||
        body_index >= world->body_count) return VOX_ERR_INVALID;
    body = &world->bodies[body_index];
    if (!rigid_active(body)) return VOX_ERR_INVALID;
    mass_units = body->mass_q16 / RIGID_ONE_Q16;
    if (mass_units <= 0L) mass_units = 1L;
    body->velocity_x_q16 += impulse_x_q16 / mass_units;
    body->velocity_y_q16 += impulse_y_q16 / mass_units;
    body->sleep_ticks = 0U;
    body->flags = (vox_u16)(body->flags &
                            (vox_u16)~VOX_RIGID_BODY_SLEEPING);
    return VOX_OK;
}

vox_result vox_rigid_spawn(vox_rigid_world *world, vox_u16 *body_index,
                           vox_i32 x_q16, vox_i32 y_q16,
                           vox_i32 half_width_q16,
                           vox_i32 half_height_q16, vox_i32 mass_q16,
                           vox_u16 flags)
{
    vox_u16 index;
    vox_rigid_body *body;
    if (world == 0 || body_index == 0 ||
        world->abi_version != VOX_ABI_VERSION ||
        world->struct_size < (vox_u32)sizeof(*world) ||
        half_width_q16 <= 0L || half_height_q16 <= 0L || mass_q16 <= 0L) {
        return VOX_ERR_INVALID;
    }
    for (index = 0U; index < VOX_RIGID_MAX_BODIES; ++index) {
        if (!rigid_active(&world->bodies[index])) break;
    }
    if (index == VOX_RIGID_MAX_BODIES) return VOX_ERR_CAPACITY;
    body = &world->bodies[index];
    body->position_x_q16 = x_q16;
    body->position_y_q16 = y_q16;
    body->velocity_x_q16 = 0L;
    body->velocity_y_q16 = 0L;
    body->angle_q16 = 0L;
    body->angular_velocity_q16 = 0L;
    body->half_width_q16 = half_width_q16;
    body->half_height_q16 = half_height_q16;
    body->mass_q16 = mass_q16;
    body->inertia_q16 = mass_q16;
    body->friction_q16 = 32768L;
    body->restitution_q16 = 8192L;
    body->flags = (vox_u16)(flags | VOX_RIGID_BODY_ACTIVE);
    body->sleep_ticks = 0U;
    if (index >= world->body_count) world->body_count = (vox_u16)(index + 1U);
    *body_index = index;
    return VOX_OK;
}

vox_result vox_rigid_release(vox_rigid_world *world, vox_u16 body_index)
{
    vox_u16 i;
    if (world == 0 || world->abi_version != VOX_ABI_VERSION ||
        body_index >= world->body_count ||
        !rigid_active(&world->bodies[body_index])) return VOX_ERR_INVALID;
    world->bodies[body_index].position_x_q16 = 0L;
    world->bodies[body_index].position_y_q16 = 0L;
    world->bodies[body_index].velocity_x_q16 = 0L;
    world->bodies[body_index].velocity_y_q16 = 0L;
    world->bodies[body_index].angle_q16 = 0L;
    world->bodies[body_index].angular_velocity_q16 = 0L;
    world->bodies[body_index].half_width_q16 = 0L;
    world->bodies[body_index].half_height_q16 = 0L;
    world->bodies[body_index].flags = 0U;
    world->bodies[body_index].sleep_ticks = 0U;
    for (i = 0U; i < world->joint_count; ++i) {
        if (world->joints[i].active &&
            (world->joints[i].body_a == body_index ||
             world->joints[i].body_b == body_index)) {
            world->joints[i].active = 0U;
        }
    }
    while (world->joint_count > 0U &&
           world->joints[world->joint_count - 1U].active == 0U) {
        world->joint_count--;
    }
    while (world->body_count > 0U &&
           !rigid_active(&world->bodies[world->body_count - 1U])) {
        world->body_count--;
    }
    return VOX_OK;
}

vox_result vox_rigid_joint_add(vox_rigid_world *world, vox_u16 *joint_index,
                               vox_u16 body_a, vox_u16 body_b,
                               vox_i32 rest_length_q16,
                               vox_i32 min_angle_q16,
                               vox_i32 max_angle_q16)
{
    vox_u16 index;
    if (world == 0 || joint_index == 0 ||
        world->abi_version != VOX_ABI_VERSION || body_a >= world->body_count ||
        body_b >= world->body_count || body_a == body_b ||
        !rigid_active(&world->bodies[body_a]) ||
        !rigid_active(&world->bodies[body_b]) || rest_length_q16 < 0L ||
        min_angle_q16 > max_angle_q16) return VOX_ERR_INVALID;
    for (index = 0U; index < VOX_RIGID_MAX_JOINTS; ++index) {
        if (!world->joints[index].active) break;
    }
    if (index == VOX_RIGID_MAX_JOINTS) return VOX_ERR_CAPACITY;
    world->joints[index].body_a = body_a;
    world->joints[index].body_b = body_b;
    world->joints[index].rest_length_q16 = rest_length_q16;
    world->joints[index].min_angle_q16 = min_angle_q16;
    world->joints[index].max_angle_q16 = max_angle_q16;
    world->joints[index].active = 1U;
    if (index >= world->joint_count) world->joint_count = (vox_u16)(index + 1U);
    *joint_index = index;
    return VOX_OK;
}

static void rigid_terrain_sweep(vox_rigid_body *body,
                                const vox_world *terrain,
                                vox_i32 previous_x_q16,
                                vox_i32 previous_y_q16);

static void rigid_integrate(vox_rigid_body *body, vox_i32 gravity_q16)
{
    if (!rigid_active(body) || (body->flags & VOX_RIGID_BODY_SLEEPING) != 0U) {
        return;
    }
    body->velocity_y_q16 += gravity_q16;
    body->position_x_q16 += body->velocity_x_q16;
    body->position_y_q16 += body->velocity_y_q16;
    body->angle_q16 += body->angular_velocity_q16;
    body->angular_velocity_q16 = (body->angular_velocity_q16 * 63L) / 64L;
}

static vox_i32 rigid_q16_floor(vox_i32 value)
{
    vox_u32 magnitude;
    if (value >= 0L) return value / RIGID_ONE_Q16;
    magnitude = (vox_u32)(-(value + 1L)) + 1U;
    return -(vox_i32)((magnitude + (vox_u32)RIGID_ONE_Q16 - 1U) /
                      (vox_u32)RIGID_ONE_Q16);
}

static vox_i32 rigid_cell_q16(vox_i32 cell)
{
    return cell * RIGID_ONE_Q16;
}

static void rigid_terrain_extents(const vox_rigid_body *body,
                                  vox_i32 *extent_x,
                                  vox_i32 *extent_y)
{
    vox_i32 body_cos;
    vox_i32 body_sin;
    rigid_basis(body->angle_q16, &body_cos, &body_sin);
    *extent_x = rigid_abs_i32(rigid_mul_q16(body_cos,
                                             body->half_width_q16)) +
                rigid_abs_i32(rigid_mul_q16(body_sin,
                                             body->half_height_q16));
    *extent_y = rigid_abs_i32(rigid_mul_q16(body_sin,
                                             body->half_width_q16)) +
                rigid_abs_i32(rigid_mul_q16(body_cos,
                                             body->half_height_q16));
}

static int rigid_terrain_span_x(const vox_world *terrain, vox_i32 left,
                                vox_i32 right, vox_i32 y)
{
    vox_i32 sample;
    if (y < 0L || y >= (vox_i32)VOX_WORLD_HEIGHT) return 0;
    for (sample = left; sample <= right; ++sample) {
        if (sample >= 0L && sample < (vox_i32)VOX_WORLD_WIDTH &&
            vox_world_collision_classify(terrain, (vox_u32)sample,
                                         (vox_u32)y) !=
                VOX_WORLD_COLLISION_EMPTY) {
            return 1;
        }
    }
    return 0;
}

static int rigid_terrain_span_y(const vox_world *terrain, vox_i32 x,
                                vox_i32 top, vox_i32 bottom)
{
    vox_i32 sample;
    if (x < 0L || x >= (vox_i32)VOX_WORLD_WIDTH || top > bottom) return 0;
    for (sample = top; sample <= bottom; ++sample) {
        if (sample >= 0L && sample < (vox_i32)VOX_WORLD_HEIGHT &&
            vox_world_collision_classify(terrain, (vox_u32)x,
                                         (vox_u32)sample) !=
                VOX_WORLD_COLLISION_EMPTY) {
            return 1;
        }
    }
    return 0;
}

static void rigid_terrain_wake(vox_rigid_body *body)
{
    body->angular_velocity_q16 = (body->angular_velocity_q16 * 15L) / 16L;
    /* Sleep accumulation is decided from the resolved velocity at the end of
     * the tick.  Resetting it here made a body resting on a floor wake itself
     * every solver iteration even after its impact had settled. */
    body->flags = (vox_u16)(body->flags &
                            (vox_u16)~VOX_RIGID_BODY_SLEEPING);
}

static void rigid_terrain_bounce(vox_i32 *velocity, vox_i32 restitution_q16)
{
    if (*velocity == 0L) return;
    if (rigid_abs(*velocity) <= (vox_u32)RIGID_SETTLE_SPEED_Q16) {
        *velocity = 0L;
    } else {
        *velocity = -rigid_mul_q16(*velocity, restitution_q16);
    }
}

/* Sweep the leading oriented-AABB edge across all cells crossed by the
 * integration step.  Solver iterations can still repair small overlaps from
 * body contacts below, but this pass prevents fast scrap/debris from passing
 * through a one-cell wall before those iterations run. */
static void rigid_terrain_sweep(vox_rigid_body *body,
                                const vox_world *terrain,
                                vox_i32 previous_x_q16,
                                vox_i32 previous_y_q16)
{
    vox_i32 extent_x;
    vox_i32 extent_y;
    vox_i32 left;
    vox_i32 right;
    vox_i32 top;
    vox_i32 bottom;
    vox_i32 previous_edge;
    vox_i32 current_edge;
    vox_i32 sample;
    vox_i32 hit;
    int found;
    if (!rigid_active(body) || terrain == 0) return;
    rigid_terrain_extents(body, &extent_x, &extent_y);

    if (body->velocity_y_q16 > 0L) {
        left = rigid_q16_floor(body->position_x_q16 - extent_x);
        right = rigid_q16_floor(body->position_x_q16 + extent_x);
        previous_edge = rigid_q16_floor(previous_y_q16 + extent_y);
        current_edge = rigid_q16_floor(body->position_y_q16 + extent_y);
        found = 0;
        hit = 0L;
        for (sample = previous_edge; sample <= current_edge; ++sample) {
            if (sample >= (vox_i32)VOX_WORLD_HEIGHT) {
                hit = (vox_i32)VOX_WORLD_HEIGHT;
                found = 1;
                break;
            }
            if (sample >= 0L && rigid_terrain_span_x(terrain, left, right,
                                                      sample)) {
                hit = sample;
                found = 1;
                break;
            }
        }
        if (found) {
            body->position_y_q16 = rigid_cell_q16(hit) - extent_y;
            rigid_terrain_bounce(&body->velocity_y_q16,
                                 body->restitution_q16);
            body->velocity_x_q16 = rigid_mul_q16(body->velocity_x_q16,
                                                  body->friction_q16);
            rigid_terrain_wake(body);
        }
    } else if (body->velocity_y_q16 < 0L) {
        left = rigid_q16_floor(body->position_x_q16 - extent_x);
        right = rigid_q16_floor(body->position_x_q16 + extent_x);
        previous_edge = rigid_q16_floor(previous_y_q16 - extent_y);
        current_edge = rigid_q16_floor(body->position_y_q16 - extent_y);
        found = 0;
        hit = 0L;
        for (sample = previous_edge; sample >= current_edge; --sample) {
            if (sample < 0L) {
                hit = -1L;
                found = 1;
                break;
            }
            if (sample < (vox_i32)VOX_WORLD_HEIGHT &&
                rigid_terrain_span_x(terrain, left, right, sample)) {
                hit = sample;
                found = 1;
                break;
            }
        }
        if (found) {
            body->position_y_q16 = rigid_cell_q16(hit + 1L) + extent_y;
            rigid_terrain_bounce(&body->velocity_y_q16,
                                 body->restitution_q16);
            body->velocity_x_q16 = rigid_mul_q16(body->velocity_x_q16,
                                                  body->friction_q16);
            rigid_terrain_wake(body);
        }
    }

    top = rigid_q16_floor(body->position_y_q16 - extent_y + 1L);
    bottom = rigid_q16_floor(body->position_y_q16 + extent_y - 1L);
    if (body->velocity_x_q16 > 0L) {
        previous_edge = rigid_q16_floor(previous_x_q16 + extent_x);
        current_edge = rigid_q16_floor(body->position_x_q16 + extent_x);
        found = 0;
        hit = 0L;
        for (sample = previous_edge; sample <= current_edge; ++sample) {
            if (sample >= (vox_i32)VOX_WORLD_WIDTH) {
                hit = (vox_i32)VOX_WORLD_WIDTH;
                found = 1;
                break;
            }
            if (sample >= 0L && rigid_terrain_span_y(terrain, sample, top,
                                                      bottom)) {
                hit = sample;
                found = 1;
                break;
            }
        }
        if (found) {
            body->position_x_q16 = rigid_cell_q16(hit) - extent_x;
            rigid_terrain_bounce(&body->velocity_x_q16,
                                 body->restitution_q16);
            body->velocity_y_q16 = rigid_mul_q16(body->velocity_y_q16,
                                                  body->friction_q16);
            rigid_terrain_wake(body);
        }
    } else if (body->velocity_x_q16 < 0L) {
        previous_edge = rigid_q16_floor(previous_x_q16 - extent_x);
        current_edge = rigid_q16_floor(body->position_x_q16 - extent_x);
        found = 0;
        hit = 0L;
        for (sample = previous_edge; sample >= current_edge; --sample) {
            if (sample < 0L) {
                hit = -1L;
                found = 1;
                break;
            }
            if (sample < (vox_i32)VOX_WORLD_WIDTH &&
                rigid_terrain_span_y(terrain, sample, top, bottom)) {
                hit = sample;
                found = 1;
                break;
            }
        }
        if (found) {
            body->position_x_q16 = rigid_cell_q16(hit + 1L) + extent_x;
            rigid_terrain_bounce(&body->velocity_x_q16,
                                 body->restitution_q16);
            body->velocity_y_q16 = rigid_mul_q16(body->velocity_y_q16,
                                                  body->friction_q16);
            rigid_terrain_wake(body);
        }
    }
}

static void rigid_terrain_contact(vox_rigid_body *body,
                                  const vox_world *terrain)
{
    vox_i32 extent_x;
    vox_i32 extent_y;
    vox_i32 left;
    vox_i32 right;
    vox_i32 top;
    vox_i32 bottom;
    if (!rigid_active(body) || terrain == 0) return;
    rigid_terrain_extents(body, &extent_x, &extent_y);

    left = rigid_q16_floor(body->position_x_q16 - extent_x);
    right = rigid_q16_floor(body->position_x_q16 + extent_x);
    bottom = rigid_q16_floor(body->position_y_q16 + extent_y);
    if (bottom >= (vox_i32)VOX_WORLD_HEIGHT ||
        (body->velocity_y_q16 >= 0L && bottom >= 0L &&
         rigid_terrain_span_x(terrain, left, right, bottom))) {
        vox_i32 contact = bottom >= (vox_i32)VOX_WORLD_HEIGHT ?
                          (vox_i32)VOX_WORLD_HEIGHT : bottom;
        body->position_y_q16 = rigid_cell_q16(contact) - extent_y;
        rigid_terrain_bounce(&body->velocity_y_q16,
                             body->restitution_q16);
        body->velocity_x_q16 = rigid_mul_q16(body->velocity_x_q16,
                                              body->friction_q16);
        rigid_terrain_wake(body);
    }

    left = rigid_q16_floor(body->position_x_q16 - extent_x);
    right = rigid_q16_floor(body->position_x_q16 + extent_x);
    top = rigid_q16_floor(body->position_y_q16 - extent_y);
    if (top < 0L || (body->velocity_y_q16 < 0L &&
                     top < (vox_i32)VOX_WORLD_HEIGHT &&
                     rigid_terrain_span_x(terrain, left, right, top))) {
        vox_i32 contact = top < 0L ? 0L : top + 1L;
        body->position_y_q16 = rigid_cell_q16(contact) + extent_y;
        rigid_terrain_bounce(&body->velocity_y_q16,
                             body->restitution_q16);
        body->velocity_x_q16 = rigid_mul_q16(body->velocity_x_q16,
                                              body->friction_q16);
        rigid_terrain_wake(body);
    }

    /* Side contacts use the body's open vertical interval.  Otherwise the
     * floor cell already resolved above is also seen as a wall and shoves a
     * resting body sideways once per solver iteration. */
    top = rigid_q16_floor(body->position_y_q16 - extent_y + 1L);
    bottom = rigid_q16_floor(body->position_y_q16 + extent_y - 1L);
    left = rigid_q16_floor(body->position_x_q16 - extent_x);
    if (left < 0L || (body->velocity_x_q16 < 0L &&
                      left < (vox_i32)VOX_WORLD_WIDTH &&
                      rigid_terrain_span_y(terrain, left, top, bottom))) {
        vox_i32 contact = left < 0L ? 0L : left + 1L;
        body->position_x_q16 = rigid_cell_q16(contact) + extent_x;
        rigid_terrain_bounce(&body->velocity_x_q16,
                             body->restitution_q16);
        body->velocity_y_q16 = rigid_mul_q16(body->velocity_y_q16,
                                              body->friction_q16);
        rigid_terrain_wake(body);
    }

    top = rigid_q16_floor(body->position_y_q16 - extent_y + 1L);
    bottom = rigid_q16_floor(body->position_y_q16 + extent_y - 1L);
    right = rigid_q16_floor(body->position_x_q16 + extent_x);
    if (right >= (vox_i32)VOX_WORLD_WIDTH ||
        (body->velocity_x_q16 > 0L && right >= 0L &&
         rigid_terrain_span_y(terrain, right, top, bottom))) {
        vox_i32 contact = right >= (vox_i32)VOX_WORLD_WIDTH ?
                          (vox_i32)VOX_WORLD_WIDTH : right;
        body->position_x_q16 = rigid_cell_q16(contact) - extent_x;
        rigid_terrain_bounce(&body->velocity_x_q16,
                             body->restitution_q16);
        body->velocity_y_q16 = rigid_mul_q16(body->velocity_y_q16,
                                              body->friction_q16);
        rigid_terrain_wake(body);
    }
}

static void rigid_apply_contact_impulse(vox_rigid_body *a,
                                        vox_rigid_body *b,
                                        vox_i32 normal_x,
                                        vox_i32 normal_y)
{
    vox_i32 relative_x = b->velocity_x_q16 - a->velocity_x_q16;
    vox_i32 relative_y = b->velocity_y_q16 - a->velocity_y_q16;
    vox_i32 normal_speed = rigid_dot(relative_x, relative_y,
                                     normal_x, normal_y);
    vox_i32 impulse;
    vox_i32 tangent_x = -normal_y;
    vox_i32 tangent_y = normal_x;
    vox_i32 tangent_speed;
    if (normal_speed >= 0L) return;
    impulse = rigid_mul_q16(-normal_speed, RIGID_ONE_Q16 +
                            (a->restitution_q16 + b->restitution_q16) /
                            2L);
    impulse /= 2L;
    a->velocity_x_q16 -= rigid_mul_q16(normal_x, impulse);
    a->velocity_y_q16 -= rigid_mul_q16(normal_y, impulse);
    b->velocity_x_q16 += rigid_mul_q16(normal_x, impulse);
    b->velocity_y_q16 += rigid_mul_q16(normal_y, impulse);
    tangent_speed = rigid_dot(relative_x, relative_y, tangent_x, tangent_y);
    tangent_speed /= 8L;
    a->velocity_x_q16 += rigid_mul_q16(tangent_x, tangent_speed);
    a->velocity_y_q16 += rigid_mul_q16(tangent_y, tangent_speed);
    b->velocity_x_q16 -= rigid_mul_q16(tangent_x, tangent_speed);
    b->velocity_y_q16 -= rigid_mul_q16(tangent_y, tangent_speed);
    a->flags = (vox_u16)(a->flags & (vox_u16)~VOX_RIGID_BODY_SLEEPING);
    b->flags = (vox_u16)(b->flags & (vox_u16)~VOX_RIGID_BODY_SLEEPING);
}

static void rigid_body_contact(vox_rigid_body *a, vox_rigid_body *b)
{
    vox_i32 dx;
    vox_i32 dy;
    vox_i32 bound_a;
    vox_i32 bound_b;
    vox_i32 a_cos;
    vox_i32 a_sin;
    vox_i32 b_cos;
    vox_i32 b_sin;
    vox_i32 axes_x[4];
    vox_i32 axes_y[4];
    vox_i32 overlap;
    vox_i32 best_overlap = 2147483647L;
    vox_i32 best_axis_x = 0L;
    vox_i32 best_axis_y = 0L;
    vox_i16 axis;
    if (!rigid_active(a) || !rigid_active(b) ||
        (a->flags & VOX_RIGID_BODY_SLEEPING) != 0U ||
        (b->flags & VOX_RIGID_BODY_SLEEPING) != 0U) return;
    dx = b->position_x_q16 - a->position_x_q16;
    dy = b->position_y_q16 - a->position_y_q16;
    bound_a = rigid_abs_i32(a->half_width_q16) +
              rigid_abs_i32(a->half_height_q16);
    bound_b = rigid_abs_i32(b->half_width_q16) +
              rigid_abs_i32(b->half_height_q16);
    if (rigid_abs_i32(dx) > bound_a + bound_b ||
        rigid_abs_i32(dy) > bound_a + bound_b) return;
    rigid_basis(a->angle_q16, &a_cos, &a_sin);
    rigid_basis(b->angle_q16, &b_cos, &b_sin);
    axes_x[0] = a_cos; axes_y[0] = a_sin;
    axes_x[1] = -a_sin; axes_y[1] = a_cos;
    axes_x[2] = b_cos; axes_y[2] = b_sin;
    axes_x[3] = -b_sin; axes_y[3] = b_cos;
    for (axis = 0; axis < 4; ++axis) {
        vox_i32 center_distance = rigid_abs_i32(
            rigid_dot(dx, dy, axes_x[axis], axes_y[axis]));
        vox_i32 radius_a = rigid_radius_on_axis(
            axes_x[axis], axes_y[axis], a_cos, a_sin,
            a->half_width_q16, a->half_height_q16);
        vox_i32 radius_b = rigid_radius_on_axis(
            axes_x[axis], axes_y[axis], b_cos, b_sin,
            b->half_width_q16, b->half_height_q16);
        overlap = radius_a + radius_b - center_distance;
        if (overlap <= 0L) return;
        if (overlap < best_overlap) {
            best_overlap = overlap;
            best_axis_x = axes_x[axis];
            best_axis_y = axes_y[axis];
        }
    }
    if (rigid_dot(dx, dy, best_axis_x, best_axis_y) < 0L) {
        best_axis_x = -best_axis_x;
        best_axis_y = -best_axis_y;
    }
    overlap = rigid_clamp_correction(best_overlap / 2L);
    a->position_x_q16 -= rigid_mul_q16(best_axis_x, overlap);
    a->position_y_q16 -= rigid_mul_q16(best_axis_y, overlap);
    b->position_x_q16 += rigid_mul_q16(best_axis_x, overlap);
    b->position_y_q16 += rigid_mul_q16(best_axis_y, overlap);
    rigid_apply_contact_impulse(a, b, best_axis_x, best_axis_y);
    a->angular_velocity_q16 += rigid_mul_q16(dy, best_axis_x) / 16L;
    b->angular_velocity_q16 -= rigid_mul_q16(dx, best_axis_y) / 16L;
}

static void rigid_joint_solve(vox_rigid_world *world,
                              const vox_rigid_joint *joint)
{
    vox_rigid_body *a;
    vox_rigid_body *b;
    vox_i32 dx;
    vox_i32 dy;
    vox_i32 distance;
    vox_i32 correction;
    vox_i32 relative_angle;
    if (joint == 0 || !joint->active || joint->body_a >= world->body_count ||
        joint->body_b >= world->body_count) return;
    a = &world->bodies[joint->body_a];
    b = &world->bodies[joint->body_b];
    if (!rigid_active(a) || !rigid_active(b)) return;
    dx = b->position_x_q16 - a->position_x_q16;
    dy = b->position_y_q16 - a->position_y_q16;
    distance = rigid_distance(dx, dy);
    correction = rigid_clamp_correction((distance - joint->rest_length_q16) / 2L);
    if (distance > 0L) {
        a->position_x_q16 += rigid_project_correction(dx, correction,
                                                       distance);
        a->position_y_q16 += rigid_project_correction(dy, correction,
                                                       distance);
        b->position_x_q16 -= rigid_project_correction(dx, correction,
                                                       distance);
        b->position_y_q16 -= rigid_project_correction(dy, correction,
                                                       distance);
    }
    relative_angle = b->angle_q16 - a->angle_q16;
    while (relative_angle > RIGID_HALF_Q16) relative_angle -= RIGID_ONE_Q16;
    while (relative_angle < -RIGID_HALF_Q16) relative_angle += RIGID_ONE_Q16;
    if (relative_angle < joint->min_angle_q16) {
        a->angular_velocity_q16 -=
            (joint->min_angle_q16 - relative_angle) / 8L;
        b->angular_velocity_q16 +=
            (joint->min_angle_q16 - relative_angle) / 8L;
    }
    if (relative_angle > joint->max_angle_q16) {
        a->angular_velocity_q16 +=
            (relative_angle - joint->max_angle_q16) / 8L;
        b->angular_velocity_q16 -=
            (relative_angle - joint->max_angle_q16) / 8L;
    }
}

/* Connected segments already have a positional and angular constraint.  Let
 * them collide as well and a corpse fights its own joints every solver pass:
 * the result is the suspended, dancing anatomy seen in play. */
static int rigid_bodies_are_jointed(const vox_rigid_world *world,
                                    vox_u16 body_a, vox_u16 body_b)
{
    vox_u16 joint_index;
    if (world == 0) return 0;
    for (joint_index = 0U; joint_index < world->joint_count; ++joint_index) {
        const vox_rigid_joint *joint = &world->joints[joint_index];
        if (!joint->active) continue;
        if ((joint->body_a == body_a && joint->body_b == body_b) ||
            (joint->body_a == body_b && joint->body_b == body_a)) {
            return 1;
        }
    }
    return 0;
}

static void rigid_apply_fluid_forces(vox_rigid_body *body,
                                     const vox_fluid_world *fluids,
                                     vox_i32 gravity_q16)
{
    vox_i32 x;
    vox_i32 y;
    vox_i32 total_q16 = 0L;
    vox_u16 material = VOX_FLUID_NONE;
    if (!rigid_active(body) || fluids == 0) return;
    x = body->position_x_q16 / RIGID_ONE_Q16;
    y = body->position_y_q16 / RIGID_ONE_Q16;
    if (x < 0L || y < 0L || x >= (vox_i32)VOX_WORLD_WIDTH ||
        y >= (vox_i32)VOX_WORLD_HEIGHT) return;
    if (vox_fluid_sample_column(fluids, (vox_u16)x, (vox_u16)y,
                                &total_q16, &material) != VOX_OK) return;
    if (total_q16 <= 0L) return;
    /* Drag is stronger in lava, while blood and water share the same
     * low-cost submerged response. */
    if (material == VOX_FLUID_LAVA) {
        body->velocity_x_q16 = (body->velocity_x_q16 * 3L) / 4L;
        body->velocity_y_q16 = (body->velocity_y_q16 * 3L) / 4L;
    } else {
        body->velocity_x_q16 = (body->velocity_x_q16 * 7L) / 8L;
        body->velocity_y_q16 = (body->velocity_y_q16 * 7L) / 8L;
    }
    if (gravity_q16 > 0L) {
        /* At most one quarter-g of buoyancy for a full cell. */
        vox_i32 buoyancy_q16 = gravity_q16 / 4L;
        if (total_q16 >= VOX_FLUID_CELL_CAPACITY_Q16) {
            body->velocity_y_q16 -= buoyancy_q16;
        } else {
            body->velocity_y_q16 -= rigid_mul_q16(buoyancy_q16,
                                                  total_q16);
        }
    }
}

vox_result vox_rigid_step_fluids(vox_rigid_world *world,
                                 const vox_world *terrain,
                                 const vox_fluid_world *fluids,
                                 vox_i32 gravity_q16)
{
    vox_u16 i;
    vox_u16 j;
    vox_u16 iteration;
    vox_i32 previous_x_q16;
    vox_i32 previous_y_q16;
    if (world == 0 || world->abi_version != VOX_ABI_VERSION ||
        world->struct_size < (vox_u32)sizeof(*world) || terrain == 0 ||
        terrain->abi_version != VOX_ABI_VERSION ||
        (fluids != 0 && (fluids->abi_version != VOX_ABI_VERSION ||
                         fluids->struct_size < (vox_u32)sizeof(*fluids)))) {
        return VOX_ERR_INVALID;
    }
    for (i = 0U; i < world->body_count; ++i) {
        previous_x_q16 = world->bodies[i].position_x_q16;
        previous_y_q16 = world->bodies[i].position_y_q16;
        rigid_apply_fluid_forces(&world->bodies[i], fluids, gravity_q16);
        rigid_integrate(&world->bodies[i], gravity_q16);
        rigid_terrain_sweep(&world->bodies[i], terrain,
                            previous_x_q16, previous_y_q16);
    }
    for (iteration = 0U; iteration < VOX_RIGID_SOLVER_ITERATIONS; ++iteration) {
        for (i = 0U; i < world->body_count; ++i) {
            if (!rigid_active(&world->bodies[i]) ||
                (world->bodies[i].flags & VOX_RIGID_BODY_SLEEPING) != 0U) {
                continue;
            }
            rigid_terrain_contact(&world->bodies[i], terrain);
            for (j = (vox_u16)(i + 1U); j < world->body_count; ++j) {
                if (!rigid_active(&world->bodies[j]) ||
                    (world->bodies[j].flags & VOX_RIGID_BODY_SLEEPING) != 0U) {
                    continue;
                }
                if (rigid_bodies_are_jointed(world, i, j)) continue;
                rigid_body_contact(&world->bodies[i], &world->bodies[j]);
            }
        }
        for (i = 0U; i < world->joint_count; ++i)
            rigid_joint_solve(world, &world->joints[i]);
    }
    for (i = 0U; i < world->body_count; ++i) {
        vox_rigid_body *body = &world->bodies[i];
        if (!rigid_active(body)) continue;
        if (rigid_abs(body->velocity_x_q16) < RIGID_SLEEP_SPEED_Q16 &&
            rigid_abs(body->velocity_y_q16) < RIGID_SLEEP_SPEED_Q16 &&
            rigid_abs(body->angular_velocity_q16) < RIGID_SLEEP_SPEED_Q16) {
            if (body->sleep_ticks < 65535U) body->sleep_ticks++;
            if (body->sleep_ticks >= RIGID_SLEEP_TICKS)
                body->flags = (vox_u16)(body->flags | VOX_RIGID_BODY_SLEEPING);
        } else {
            body->sleep_ticks = 0U;
            body->flags = (vox_u16)(body->flags & (vox_u16)~VOX_RIGID_BODY_SLEEPING);
        }
    }
    world->tick++;
    return VOX_OK;
}

vox_result vox_rigid_step(vox_rigid_world *world, const vox_world *terrain,
                          vox_i32 gravity_q16)
{
    return vox_rigid_step_fluids(world, terrain, 0, gravity_q16);
}

vox_u32 vox_rigid_hash(const vox_rigid_world *world)
{
    vox_u32 hash = 2166136261U;
    vox_u16 i;
    if (world == 0) return 0U;
    hash = rigid_mix(hash, world->tick);
    hash = rigid_mix(hash, world->body_count);
    hash = rigid_mix(hash, world->joint_count);
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        const vox_rigid_body *body = &world->bodies[i];
        hash = rigid_mix(hash, (vox_u32)body->position_x_q16);
        hash = rigid_mix(hash, (vox_u32)body->position_y_q16);
        hash = rigid_mix(hash, (vox_u32)body->velocity_x_q16);
        hash = rigid_mix(hash, (vox_u32)body->velocity_y_q16);
        hash = rigid_mix(hash, (vox_u32)body->angle_q16);
        hash = rigid_mix(hash, (vox_u32)body->angular_velocity_q16);
        hash = rigid_mix(hash, (vox_u32)body->half_width_q16);
        hash = rigid_mix(hash, (vox_u32)body->half_height_q16);
        hash = rigid_mix(hash, (vox_u32)body->mass_q16);
        hash = rigid_mix(hash, (vox_u32)body->inertia_q16);
        hash = rigid_mix(hash, (vox_u32)body->friction_q16);
        hash = rigid_mix(hash, (vox_u32)body->restitution_q16);
        hash = rigid_mix(hash, body->flags);
        hash = rigid_mix(hash, body->sleep_ticks);
    }
    for (i = 0U; i < VOX_RIGID_MAX_JOINTS; ++i) {
        hash = rigid_mix(hash, world->joints[i].body_a);
        hash = rigid_mix(hash, world->joints[i].body_b);
        hash = rigid_mix(hash, (vox_u32)world->joints[i].rest_length_q16);
        hash = rigid_mix(hash, (vox_u32)world->joints[i].min_angle_q16);
        hash = rigid_mix(hash, (vox_u32)world->joints[i].max_angle_q16);
        hash = rigid_mix(hash, world->joints[i].active);
    }
    return hash;
}

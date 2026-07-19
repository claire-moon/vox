/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_game.h"

#define DIGS_DENSITY_SCALE 2U
#define DIGS_SCALE(value) ((value) * DIGS_DENSITY_SCALE)
#define DIGS_RUN_SPEED_Q16 49152L
#define DIGS_GROUND_ACCEL_Q16 8192L
#define DIGS_AIR_ACCEL_Q16 4096L
#define DIGS_GROUND_DECEL_Q16 12288L
#define DIGS_AIR_DECEL_Q16 2048L
#define DIGS_JUMP_SPEED_Q16 (-106496L)
#define DIGS_JUMP_HOLD_ACCEL_Q16 3072L
#define DIGS_JUMP_HOLD_TICKS 8U
#define DIGS_COYOTE_TICKS 6U
#define DIGS_JUMP_BUFFER_TICKS 6U
#define DIGS_STEAM_ACCEL_Q16 16384L
#define DIGS_MAX_VERTICAL_SPEED_Q16 (8L << 16)
#define DIGS_STEAM_USE_Q16 1024U
#define DIGS_STEAM_RECHARGE_Q16 512U
#define DIGS_ROPE_MIN_LENGTH_Q16 (3L << 16)
#define DIGS_ROPE_MAX_LENGTH_Q16 (48L << 16)
#define DIGS_ROPE_REEL_SPEED_Q16 16384L
#define DIGS_ROPE_PULL_Q16 32768L
#define DIGS_ROPE_BREAK_TENSION_Q16 (5L << 16)
#define DIGS_ROPE_RAY_STEPS 96U
#define DIGS_PROJECTILE_SUBSTEPS 4U
#define DIGS_PROJECTILE_GRAVITY_Q16 6144L
#define DIGS_REACTION_SAMPLES 128U
#define DIGS_AI_MEMORY_TICKS 180U
#define DIGS_AI_DECISION_TICKS 8U
#define DIGS_AI_RETREAT_HEALTH 28U
#define DIGS_SPAWN_HEADROOM_CELLS DIGS_SCALE(2U)
#define DIGS_SPAWN_SUPPORT_CELLS DIGS_SCALE(2U)
#define DIGS_LAVA_BASIN_TOP (VOX_WORLD_HEIGHT - DIGS_SCALE(12U))

static const vox_digs_weapon_properties digs_weapons[VOX_DIGS_TOOL_COUNT] = {
    {"PICK", 10U, 28U, 2U, 0U, 0U, VOX_DIGS_WEAPON_MELEE},
    {"BLAST CHARGE", 45U, 72U, 8U, 512U, 60U,
     VOX_DIGS_WEAPON_PROJECTILE | VOX_DIGS_WEAPON_EXPLOSIVE |
     VOX_DIGS_WEAPON_GRAVITY},
    {"SMOKE POT", 35U, 0U, 4U, 640U, 45U,
     VOX_DIGS_WEAPON_PROJECTILE | VOX_DIGS_WEAPON_DEPOSIT |
     VOX_DIGS_WEAPON_GRAVITY},
    {"CINDER FLASK", 30U, 12U, 4U, 768U, 40U,
     VOX_DIGS_WEAPON_PROJECTILE | VOX_DIGS_WEAPON_DEPOSIT |
     VOX_DIGS_WEAPON_GRAVITY},
    {"PRESSURE HOSE", 4U, 4U, 0U, 1536U, 12U,
     VOX_DIGS_WEAPON_PROJECTILE | VOX_DIGS_WEAPON_DEPOSIT},
    {"SLEDGE", 28U, 48U, 4U, 0U, 0U, VOX_DIGS_WEAPON_MELEE},
    {"NAIL GUN", 5U, 18U, 0U, 2048U, 40U,
     VOX_DIGS_WEAPON_PROJECTILE},
    {"BOILER SHOTGUN", 36U, 14U, 2U, 1792U, 20U,
     VOX_DIGS_WEAPON_PROJECTILE},
    {"CONCUSSION GRENADE", 46U, 38U, 10U, 768U, 50U,
     VOX_DIGS_WEAPON_PROJECTILE | VOX_DIGS_WEAPON_EXPLOSIVE |
     VOX_DIGS_WEAPON_GRAVITY},
    {"NAIL BOMB", 56U, 62U, 8U, 640U, 55U,
     VOX_DIGS_WEAPON_PROJECTILE | VOX_DIGS_WEAPON_EXPLOSIVE |
     VOX_DIGS_WEAPON_GRAVITY}
};

static void digs_step_projectiles(vox_digs_match *match);
static void digs_step_effects(vox_digs_match *match);
static void digs_step_reactions(vox_digs_match *match);
static void digs_update_lava(vox_digs_match *match);
static void digs_apply_lava_hazards(vox_digs_match *match);
static void digs_spawn_effect(vox_digs_match *match, vox_u16 material,
                              vox_i32 x_q16, vox_i32 y_q16,
                              vox_i32 velocity_x_q16,
                              vox_i32 velocity_y_q16, vox_u16 ttl);
static void digs_spawn_effect_variant(vox_digs_match *match,
                                      vox_u16 material,
                                      vox_i32 x_q16, vox_i32 y_q16,
                                      vox_i32 velocity_x_q16,
                                      vox_i32 velocity_y_q16, vox_u16 ttl,
                                      vox_u16 source, vox_u16 variant);
static void digs_emit_event(vox_digs_match *match, vox_u16 type,
                            vox_u16 source, vox_u16 target,
                            vox_u16 weapon, vox_u16 material,
                            vox_i32 x_q16, vox_i32 y_q16,
                            vox_u16 magnitude, vox_u16 variant);
static void digs_init_anatomy(vox_digs_match *match, vox_u16 player);
static void digs_step_bleeding(vox_digs_match *match);
static void digs_step_rope(vox_digs_match *match, vox_u16 player);
static void digs_detach_rope(vox_digs_match *match, vox_u16 player,
                             vox_u16 event_type);
static vox_u32 digs_abs_i32(vox_i32 value);
static vox_i32 digs_div_trunc_positive(vox_i32 value, vox_u32 divisor);
static vox_i32 digs_q16_to_cell(vox_i32 value);
static vox_u32 digs_scale_lava_level(vox_u32 numerator,
                                     vox_u32 denominator);

static vox_u32 digs_hash_mix(vox_u32 hash, vox_u32 value)
{
    hash ^= value;
    hash *= 16777619U;
    return hash;
}

static vox_u32 digs_noise(vox_u32 seed, vox_u32 x, vox_u32 y, vox_u32 salt)
{
    vox_u32 hash = 2166136261U;
    hash = digs_hash_mix(hash, seed);
    hash = digs_hash_mix(hash, x);
    hash = digs_hash_mix(hash, y);
    hash = digs_hash_mix(hash, salt);
    hash ^= hash >> 16;
    hash *= 2246822519U;
    hash ^= hash >> 13;
    return hash;
}

vox_u16 vox_digs_map_landform(vox_u16 map_style, vox_u32 seed)
{
    if (map_style >= VOX_DIGS_MAP_COUNT) {
        return VOX_DIGS_LANDFORM_COUNT;
    }
    return (vox_u16)(seed % VOX_DIGS_LANDFORM_COUNT);
}

int vox_digs_player_is_active(const vox_digs_match *match, vox_u16 player)
{
    return match != 0 && player < match->rules.player_count &&
           player < VOX_DIGS_MAX_SLOTS;
}

int vox_digs_player_is_bot(const vox_digs_match *match, vox_u16 player)
{
    return vox_digs_player_is_active(match, player) &&
           (match->rules.bot_mask & (vox_u16)(1U << player)) != 0U;
}

static vox_u16 digs_count_bits(vox_u16 value)
{
    vox_u16 count = 0U;
    while (value != 0U) {
        count = (vox_u16)(count + (value & 1U));
        value = (vox_u16)(value >> 1);
    }
    return count;
}

static void digs_emit_event(vox_digs_match *match, vox_u16 type,
                            vox_u16 source, vox_u16 target,
                            vox_u16 weapon, vox_u16 material,
                            vox_i32 x_q16, vox_i32 y_q16,
                            vox_u16 magnitude, vox_u16 variant)
{
    vox_u16 slot;
    vox_digs_event *event;
    if (match->event_count < VOX_DIGS_MAX_EVENTS) {
        slot = (vox_u16)((match->event_head + match->event_count) %
                         VOX_DIGS_MAX_EVENTS);
        match->event_count++;
    } else {
        slot = match->event_head;
        match->event_head = (vox_u16)((match->event_head + 1U) %
                                      VOX_DIGS_MAX_EVENTS);
    }
    event = &match->events[slot];
    match->event_sequence++;
    event->sequence = match->event_sequence;
    event->tick = match->tick;
    event->position_x_q16 = x_q16;
    event->position_y_q16 = y_q16;
    event->type = type;
    event->source = source;
    event->target = target;
    event->weapon = weapon;
    event->material = material;
    event->magnitude = magnitude;
    event->variant = variant;
    event->reserved = 0U;
}

const vox_digs_event *vox_digs_event_get(const vox_digs_match *match,
                                         vox_u16 ordinal)
{
    vox_u16 slot;
    if (match == 0 || ordinal >= match->event_count) {
        return 0;
    }
    slot = (vox_u16)((match->event_head + ordinal) % VOX_DIGS_MAX_EVENTS);
    return &match->events[slot];
}

vox_result vox_digs_consume_events(vox_digs_match *match, vox_u16 count)
{
    if (match == 0 || count > match->event_count) {
        return VOX_ERR_INVALID;
    }
    match->event_head = (vox_u16)((match->event_head + count) %
                                  VOX_DIGS_MAX_EVENTS);
    match->event_count = (vox_u16)(match->event_count - count);
    return VOX_OK;
}

static vox_u16 digs_part_max_health(vox_u16 part)
{
    static const vox_u16 maximums[VOX_DIGS_ANATOMY_PART_COUNT] = {
        45U, 100U, 70U, 44U, 44U, 36U, 36U, 24U, 24U,
        58U, 58U, 46U, 46U, 28U, 28U
    };
    if (part >= VOX_DIGS_ANATOMY_PART_COUNT) {
        return 1U;
    }
    return maximums[part];
}

static void digs_init_anatomy(vox_digs_match *match, vox_u16 player)
{
    vox_u16 part;
    for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
        vox_digs_anatomy_part *anatomy = &match->anatomy[player][part];
        anatomy->max_health = digs_part_max_health(part);
        anatomy->health = anatomy->max_health;
        anatomy->flags = part <= VOX_DIGS_PART_TORSO ?
                         VOX_DIGS_PART_VITAL : VOX_DIGS_PART_LIMB;
        anatomy->bleed_rate_q8 = 0U;
    }
    match->bleed_accumulator_q8[player] = 0U;
    match->clot_ticks[player] = 0U;
}

static vox_u32 digs_abs_difference(vox_u32 left, vox_u32 right)
{
    return left > right ? left - right : right - left;
}

static vox_u32 digs_lerp_height(vox_u32 left, vox_u32 right,
                                vox_u32 offset, vox_u32 span)
{
    if (span == 0U || offset >= span) {
        return right;
    }
    if (right >= left) {
        return left + ((right - left) * offset) / span;
    }
    return left - ((left - right) * offset) / span;
}

static vox_u32 digs_rolling_height(vox_u32 seed, vox_u32 x,
                                   vox_u32 span, vox_u32 base,
                                   vox_u32 radius, vox_u32 salt)
{
    vox_u32 anchor = x / span;
    vox_u32 offset = x % span;
    vox_u32 range = radius * 2U + 1U;
    vox_u32 left_noise = digs_noise(seed, anchor, 0U, salt);
    vox_u32 right_noise = digs_noise(seed, anchor + 1U, 0U, salt);
    vox_u32 left = base + (left_noise % range) - radius;
    vox_u32 right = base + (right_noise % range) - radius;
    return digs_lerp_height(left, right, offset, span);
}

static vox_u32 digs_furnace_level(vox_u32 seed, vox_u32 sector)
{
    return VOX_WORLD_HEIGHT / 2U - 4U +
           (digs_noise(seed, sector, 0U, 413U) % 4U) * DIGS_SCALE(2U);
}

static int digs_land_bounds(vox_u32 seed, vox_u32 x, vox_u16 map_style,
                            vox_u32 *left, vox_u32 *right,
                            vox_u32 *region)
{
    vox_u16 landform = vox_digs_map_landform(map_style, seed);
    vox_u32 edge;
    if (left == 0 || right == 0 || region == 0 ||
        landform >= VOX_DIGS_LANDFORM_COUNT) {
        return 0;
    }
    if (landform == VOX_DIGS_LANDFORM_ARCHIPELAGO) {
        vox_u32 usable;
        vox_u32 span;
        vox_u32 slot;
        vox_u32 gap;
        edge = DIGS_SCALE(8U);
        if (x < edge || x >= VOX_WORLD_WIDTH - edge) {
            return 0;
        }
        usable = VOX_WORLD_WIDTH - edge * 2U;
        span = usable / 4U;
        if (span <= DIGS_SCALE(12U)) {
            return 0;
        }
        slot = (x - edge) / span;
        if (slot > 3U) {
            slot = 3U;
        }
        gap = DIGS_SCALE(7U) +
              digs_noise(seed, slot, map_style, 809U) %
              (DIGS_SCALE(3U) + 1U);
        *left = edge + slot * span + gap / 2U;
        *right = slot == 3U ? VOX_WORLD_WIDTH - edge - gap / 2U - 1U :
                 edge + (slot + 1U) * span - gap / 2U - 1U;
        *region = slot;
        return x >= *left && x <= *right;
    }
    edge = landform == VOX_DIGS_LANDFORM_CONTINENT ?
           VOX_WORLD_WIDTH / 24U : VOX_WORLD_WIDTH / 16U;
    edge += digs_noise(seed, map_style, landform, 821U) %
            (DIGS_SCALE(3U) + 1U);
    *left = edge;
    *right = VOX_WORLD_WIDTH - edge - 1U;
    *region = 0U;
    return x >= *left && x <= *right;
}

static vox_u32 digs_land_edge_distance(vox_u32 seed, vox_u32 x,
                                       vox_u16 map_style,
                                       vox_u32 *region)
{
    vox_u32 left;
    vox_u32 right;
    vox_u32 local_region = 0U;
    if (!digs_land_bounds(seed, x, map_style, &left, &right,
                          &local_region)) {
        if (region != 0) {
            *region = local_region;
        }
        return 0U;
    }
    if (region != 0) {
        *region = local_region;
    }
    return x - left < right - x ? x - left : right - x;
}

static vox_u32 digs_landform_surface(vox_u32 seed, vox_u32 x,
                                     vox_u16 map_style, vox_u32 surface)
{
    vox_u32 left;
    vox_u32 right;
    vox_u32 region;
    vox_u32 edge_distance;
    vox_u16 landform = vox_digs_map_landform(map_style, seed);
    if (!digs_land_bounds(seed, x, map_style, &left, &right, &region)) {
        return VOX_WORLD_HEIGHT;
    }
    edge_distance = x - left < right - x ? x - left : right - x;
    if (edge_distance < DIGS_SCALE(8U)) {
        surface += (DIGS_SCALE(8U) - edge_distance) *
                   DIGS_SCALE(8U) / DIGS_SCALE(8U);
    }
    if (landform == VOX_DIGS_LANDFORM_ARCHIPELAGO) {
        vox_u32 lift = digs_noise(seed, region, map_style, 839U) %
                       (DIGS_SCALE(5U) + 1U);
        if (surface > lift) {
            surface -= lift;
        }
    } else if (landform == VOX_DIGS_LANDFORM_TWIN_HILLS) {
        vox_u32 span = right - left + 1U;
        vox_u32 radius = span / 5U;
        vox_u32 first_center = left + span / 4U;
        vox_u32 second_center = left + (span * 3U) / 4U;
        vox_u32 first_distance = digs_abs_difference(x, first_center);
        vox_u32 second_distance = digs_abs_difference(x, second_center);
        vox_u32 distance = first_distance < second_distance ?
                           first_distance : second_distance;
        if (radius != 0U && distance < radius) {
            vox_u32 lift = (radius - distance) * DIGS_SCALE(10U) / radius;
            if (surface > lift) {
                surface -= lift;
            }
        }
    }
    if (surface < DIGS_SCALE(28U)) {
        surface = DIGS_SCALE(28U);
    }
    if (surface + DIGS_SCALE(18U) >= DIGS_LAVA_BASIN_TOP) {
        surface = DIGS_LAVA_BASIN_TOP - DIGS_SCALE(18U);
    }
    return surface;
}

static vox_u32 digs_surface_y(vox_u32 seed, vox_u32 x, vox_u16 map_style)
{
    vox_u32 surface;
    if (map_style == VOX_DIGS_MAP_COAL_RIDGE) {
        surface = digs_rolling_height(seed, x, DIGS_SCALE(16U),
                                      VOX_WORLD_HEIGHT / 2U +
                                      DIGS_SCALE(2U),
                                      DIGS_SCALE(4U), 101U);
    } else if (map_style == VOX_DIGS_MAP_DEEPWORKS) {
        surface = digs_rolling_height(seed, x, DIGS_SCALE(12U),
                                      VOX_WORLD_HEIGHT / 2U + 1U,
                                      DIGS_SCALE(4U), 211U);
    } else {
        vox_u32 span = DIGS_SCALE(16U);
        vox_u32 blend = DIGS_SCALE(4U);
        vox_u32 sector = x / span;
        vox_u32 offset = x % span;
        vox_u32 current = digs_furnace_level(seed, sector);
        vox_u32 previous = sector == 0U ? current :
                           digs_furnace_level(seed, sector - 1U);
        if (offset < blend) {
            surface = digs_lerp_height(previous, current, offset, blend);
        } else {
            surface = current;
        }
    }
    return digs_landform_surface(seed, x, map_style, surface);
}

static vox_u32 digs_coal_seam_depth(vox_u32 seed, vox_u32 x,
                                    vox_u32 seam)
{
    vox_u32 span = DIGS_SCALE(12U);
    vox_u32 anchor = x / span;
    vox_u32 offset = x % span;
    vox_u32 base = DIGS_SCALE(5U) + seam * DIGS_SCALE(8U);
    vox_u32 left = base + digs_noise(seed, anchor, seam, 307U) %
                          DIGS_SCALE(3U);
    vox_u32 right = base + digs_noise(seed, anchor + 1U, seam, 307U) %
                           DIGS_SCALE(3U);
    return digs_lerp_height(left, right, offset, span);
}

static vox_u32 digs_sand_drift_depth(vox_u32 seed, vox_u32 x)
{
    vox_u32 span = DIGS_SCALE(10U);
    vox_u32 segment = x / span;
    vox_u32 offset = x % span;
    vox_u32 center;
    vox_u32 distance;
    if (((segment + seed) % 3U) != 0U) {
        return 0U;
    }
    center = DIGS_SCALE(3U) +
             digs_noise(seed, segment, 0U, 349U) % DIGS_SCALE(4U);
    distance = digs_abs_difference(offset, center);
    if (distance > DIGS_SCALE(3U)) {
        return 0U;
    }
    return 1U + (DIGS_SCALE(3U) - distance) / DIGS_SCALE(2U);
}

static vox_u32 digs_deep_tunnel_y(vox_u32 seed, vox_u32 x)
{
    return digs_rolling_height(seed, x, DIGS_SCALE(16U),
                               VOX_WORLD_HEIGHT / 2U + DIGS_SCALE(14U),
                               DIGS_SCALE(2U), 503U);
}

static int digs_inside_ellipse(vox_u32 x, vox_u32 y, vox_u32 center_x,
                               vox_u32 center_y, vox_u32 radius_x,
                               vox_u32 radius_y)
{
    vox_u32 delta_x = digs_abs_difference(x, center_x);
    vox_u32 delta_y = digs_abs_difference(y, center_y);
    vox_u32 left;
    vox_u32 right;
    if (delta_x > radius_x || delta_y > radius_y) {
        return 0;
    }
    left = delta_x * delta_x * radius_y * radius_y +
           delta_y * delta_y * radius_x * radius_x;
    right = radius_x * radius_x * radius_y * radius_y;
    return left <= right;
}

static int digs_deep_void(vox_u32 seed, vox_u32 x, vox_u32 y,
                          vox_u32 surface)
{
    vox_u32 tunnel_y = digs_deep_tunnel_y(seed, x);
    vox_u32 center;
    if (x > DIGS_SCALE(3U) && x + DIGS_SCALE(3U) < VOX_WORLD_WIDTH &&
        digs_abs_difference(y, tunnel_y) <= DIGS_SCALE(2U)) {
        return 1;
    }
    for (center = DIGS_SCALE(16U); center < VOX_WORLD_WIDTH;
         center += DIGS_SCALE(32U)) {
        vox_u32 chamber_y = digs_deep_tunnel_y(seed, center);
        if (digs_inside_ellipse(x, y, center, chamber_y,
                                DIGS_SCALE(7U), DIGS_SCALE(5U))) {
            return 1;
        }
        if (digs_abs_difference(x, center) <= DIGS_SCALE(1U) &&
            y >= surface && y <= chamber_y) {
            return 1;
        }
    }
    return 0;
}

static int digs_deep_firedamp(vox_u32 seed, vox_u32 x, vox_u32 y)
{
    vox_u32 center;
    vox_u32 pocket_index = 0U;
    for (center = DIGS_SCALE(16U); center < VOX_WORLD_WIDTH;
         center += DIGS_SCALE(32U)) {
        vox_u32 chamber_y = digs_deep_tunnel_y(seed, center);
        vox_u32 shift = digs_noise(seed, pocket_index, 0U, 557U) %
                        DIGS_SCALE(5U);
        vox_u32 pocket_x = center + shift - DIGS_SCALE(2U);
        vox_u32 pocket_y = chamber_y - DIGS_SCALE(3U);
        if (digs_inside_ellipse(x, y, pocket_x, pocket_y,
                                DIGS_SCALE(2U), DIGS_SCALE(1U))) {
            return 1;
        }
        pocket_index++;
    }
    return 0;
}

static int digs_furnace_pocket(vox_u32 seed, vox_u32 x, vox_u32 y,
                               int *hot)
{
    vox_u32 pocket;
    for (pocket = 0U; pocket < 4U; ++pocket) {
        vox_u32 nominal_x = ((pocket * 2U + 1U) * VOX_WORLD_WIDTH) / 8U;
        vox_u32 jitter = digs_noise(seed, pocket, 0U, 601U) %
                         DIGS_SCALE(5U);
        vox_u32 center_x = nominal_x + jitter - DIGS_SCALE(2U);
        vox_u32 center_y = digs_surface_y(seed, center_x,
                                          VOX_DIGS_MAP_FURNACE_YARD) +
                           DIGS_SCALE(6U);
        if (digs_inside_ellipse(x, y, center_x, center_y,
                                DIGS_SCALE(4U), DIGS_SCALE(3U))) {
            *hot = y >= center_y + DIGS_SCALE(1U);
            return 1;
        }
    }
    return 0;
}

static vox_u16 digs_map_material(vox_u16 map_style, vox_u32 seed,
                                  vox_u32 x, vox_u32 y)
{
    vox_u32 surface = digs_surface_y(seed, x, map_style);
    vox_u32 depth = y >= surface ? y - surface : 0U;
    vox_u32 noise = digs_noise(seed, x, y, (vox_u32)map_style + 17U);
    if (y >= VOX_WORLD_HEIGHT - DIGS_SCALE(2U)) {
        return VOX_MAT_BEDROCK;
    }
    if (y >= DIGS_LAVA_BASIN_TOP) {
        return VOX_MAT_LAVA;
    }
    if (surface >= VOX_WORLD_HEIGHT || y < surface) {
        return VOX_MAT_AIR;
    }
    if (vox_digs_map_landform(map_style, seed) ==
        VOX_DIGS_LANDFORM_ARCHIPELAGO) {
        vox_u32 region;
        vox_u32 edge_distance = digs_land_edge_distance(seed, x, map_style,
                                                        &region);
        vox_u32 thickness = DIGS_SCALE(12U) +
                            (edge_distance < DIGS_SCALE(18U) ?
                             edge_distance : DIGS_SCALE(18U));
        (void)region;
        if (y > surface + thickness) {
            return VOX_MAT_AIR;
        }
    }
    if (map_style == VOX_DIGS_MAP_COAL_RIDGE) {
        vox_u32 drift_depth = digs_sand_drift_depth(seed, x);
        if (depth < drift_depth) {
            return VOX_MAT_SAND;
        }
        if (y == surface) {
            return VOX_MAT_BIOMASS;
        }
        if (depth >= DIGS_SCALE(3U) &&
            (digs_abs_difference(depth,
                                  digs_coal_seam_depth(seed, x, 0U)) <= 1U ||
             digs_abs_difference(depth,
                                  digs_coal_seam_depth(seed, x, 1U)) <= 1U)) {
            return VOX_MAT_COAL;
        }
        return depth > DIGS_SCALE(20U) ? VOX_MAT_STONE : VOX_MAT_SOIL;
    }
    if (map_style == VOX_DIGS_MAP_DEEPWORKS) {
        if (digs_deep_void(seed, x, y, surface)) {
            return digs_deep_firedamp(seed, x, y) ? VOX_MAT_FIREDAMP :
                                                   VOX_MAT_AIR;
        }
        if (y == surface) {
            return VOX_MAT_SOIL;
        }
        if (depth > DIGS_SCALE(3U) &&
            digs_abs_difference(depth,
                                 digs_coal_seam_depth(seed ^ 0x51ed270bU,
                                                      x, 1U)) <= 1U) {
            return VOX_MAT_COAL;
        }
        return VOX_MAT_STONE;
    }
    {
        int hot = 0;
        if (digs_furnace_pocket(seed, x, y, &hot)) {
            return hot ? VOX_MAT_LAVA : VOX_MAT_AIR;
        }
    }
    if (y == surface) {
        if ((x % DIGS_SCALE(16U)) >= DIGS_SCALE(12U)) {
            return VOX_MAT_SAND;
        }
        return VOX_MAT_METAL;
    }
    if ((x % DIGS_SCALE(16U)) < DIGS_SCALE(1U) &&
        depth < DIGS_SCALE(8U)) {
        return VOX_MAT_METAL;
    }
    if (depth > DIGS_SCALE(12U) && (noise % 19U) == 0U) {
        return VOX_MAT_COAL;
    }
    return depth > DIGS_SCALE(14U) ? VOX_MAT_STONE : VOX_MAT_SOIL;
}

static vox_result digs_set_column(vox_world *world, vox_u32 x, vox_u32 y,
                                  vox_u16 material)
{
    vox_u32 z;
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        if (vox_world_set(world, x, y, z, material, 20L << 16) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    }
    return VOX_OK;
}

static vox_result digs_add_metal_span(vox_world *world, vox_u32 left,
                                      vox_u32 right, vox_u32 y)
{
    vox_u32 x;
    if (right >= VOX_WORLD_WIDTH || y >= VOX_WORLD_HEIGHT || left > right) {
        return VOX_ERR_INVALID;
    }
    for (x = left; x <= right; ++x) {
        if (digs_set_column(world, x, y, VOX_MAT_METAL) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    }
    return VOX_OK;
}

static vox_result digs_add_upward_hanger(vox_world *world, vox_u32 x,
                                         vox_u32 rail_y, vox_u32 height)
{
    vox_u32 y;
    vox_u32 top = rail_y > height ? rail_y - height : 1U;
    for (y = top; y < rail_y; ++y) {
        if (digs_set_column(world, x, y, VOX_MAT_METAL) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    }
    return VOX_OK;
}

static int digs_find_fixture_land(vox_u32 seed, vox_u32 nominal_x,
                                  vox_u16 map_style, vox_u32 half_width,
                                  vox_u32 *fixture_x, vox_u32 *surface_y)
{
    vox_u32 radius;
    vox_u32 search_radius = DIGS_SCALE(8U);
    for (radius = 0U; radius <= search_radius; ++radius) {
        vox_u32 candidate;
        vox_u32 surface;
        if (nominal_x >= radius + half_width) {
            candidate = nominal_x - radius;
            surface = digs_surface_y(seed, candidate, map_style);
            if (surface < VOX_WORLD_HEIGHT) {
                *fixture_x = candidate;
                *surface_y = surface;
                return 1;
            }
        }
        if (radius != 0U && nominal_x + radius + half_width <
            VOX_WORLD_WIDTH) {
            candidate = nominal_x + radius;
            surface = digs_surface_y(seed, candidate, map_style);
            if (surface < VOX_WORLD_HEIGHT) {
                *fixture_x = candidate;
                *surface_y = surface;
                return 1;
            }
        }
    }
    return 0;
}

static vox_result digs_add_overhead_fixtures(vox_world *world,
                                             vox_u16 map_style,
                                             vox_u32 seed)
{
    vox_u32 center;
    vox_u32 fixture = 0U;
    vox_u32 previous_fixture_x = 0U;
    int have_previous_fixture = 0;
    for (center = DIGS_SCALE(8U);
         center + DIGS_SCALE(2U) < VOX_WORLD_WIDTH;
         center += DIGS_SCALE(8U)) {
        vox_u32 fixture_x;
        vox_u32 surface;
        vox_u32 half_width = DIGS_SCALE(1U);
        vox_u32 clearance = DIGS_SCALE(18U) +
                            digs_noise(seed, fixture, map_style, 701U) %
                            (DIGS_SCALE(3U) + 1U);
        vox_u32 rail_y;
        vox_u32 left;
        vox_u32 right;
        vox_u32 sample_x;
        vox_u32 hanger_height;
        vox_u32 cap_y;
        if (!digs_find_fixture_land(seed, center, map_style, half_width,
                                    &fixture_x, &surface)) {
            fixture++;
            continue;
        }
        if (have_previous_fixture &&
            fixture_x <= previous_fixture_x + DIGS_SCALE(2U)) {
            fixture++;
            continue;
        }
        left = fixture_x - half_width;
        right = fixture_x + half_width;
        for (sample_x = left; sample_x <= right; ++sample_x) {
            vox_u32 sample_surface = digs_surface_y(seed, sample_x,
                                                    map_style);
            if (sample_surface < surface) {
                surface = sample_surface;
            }
        }
        rail_y = surface > clearance ? surface - clearance : 2U;
        hanger_height = DIGS_SCALE(2U) +
                        digs_noise(seed, fixture, 0U, 733U) %
                        (DIGS_SCALE(2U) + 1U);
        cap_y = rail_y > hanger_height ? rail_y - hanger_height : 1U;
        /*
         * A rope fixture is a small, high T-shaped sky anchor.  Nothing is
         * allowed to descend from it toward the walking surface: long rails
         * and ground-to-rail legs turn useful traversal tools into ceilings
         * and cages.  The 36--42 cell clearance leaves the complete normal
         * jump envelope open while the 16-cell spacing keeps an anchor in
         * reach of the extended rope.
        */
        if (digs_add_metal_span(world, left, right, rail_y) != VOX_OK ||
            digs_add_upward_hanger(world, fixture_x, rail_y,
                                    hanger_height) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
        if (map_style == VOX_DIGS_MAP_FURNACE_YARD) {
            if (digs_add_metal_span(world,
                                    fixture_x - DIGS_SCALE(1U),
                                    fixture_x + DIGS_SCALE(1U),
                                    cap_y) != VOX_OK) {
                return VOX_ERR_INVALID;
            }
        }
        previous_fixture_x = fixture_x;
        have_previous_fixture = 1;
        fixture++;
    }
    return VOX_OK;
}

vox_result vox_digs_generate_map(vox_world *world, vox_u16 map_style,
                                 vox_u32 seed)
{
    vox_u32 x;
    vox_u32 y;
    if (world == 0 || map_style >= VOX_DIGS_MAP_COUNT) {
        return VOX_ERR_INVALID;
    }
    vox_world_init(world);
    for (y = 0U; y < VOX_WORLD_HEIGHT; ++y) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            vox_u16 material = digs_map_material(map_style, seed, x, y);
            if (material != VOX_MAT_AIR &&
                digs_set_column(world, x, y, material) != VOX_OK) {
                return VOX_ERR_INVALID;
            }
        }
    }
    if (digs_add_overhead_fixtures(world, map_style, seed) != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    return vox_world_sleep_all(world);
}

static int digs_cell_is_solid(const vox_world *world, vox_u32 x, vox_u32 y)
{
    vox_u32 z;
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        const vox_cell *cell = vox_world_cell(world, x, y, z);
        const vox_material_properties *properties;
        if (cell == 0 || cell->material == VOX_MAT_AIR) {
            continue;
        }
        properties = vox_material_get(cell->material);
        if (properties != 0 && (properties->flags & VOX_MATERIAL_SOLID)) {
            return 1;
        }
    }
    return 0;
}

static int digs_spawn_floor_is_supported(const vox_world *world,
                                         vox_u32 x, vox_u32 floor_y)
{
    vox_i32 offset_x;
    vox_u32 distance;
    if (x == 0U || x + 1U >= VOX_WORLD_WIDTH ||
        floor_y < DIGS_SPAWN_HEADROOM_CELLS ||
        floor_y + DIGS_SPAWN_SUPPORT_CELLS >= VOX_WORLD_HEIGHT) {
        return 0;
    }
    for (offset_x = -1; offset_x <= 1; ++offset_x) {
        vox_u32 sample_x = (vox_u32)((vox_i32)x + offset_x);
        for (distance = 1U; distance <= DIGS_SPAWN_HEADROOM_CELLS;
             ++distance) {
            if (digs_cell_is_solid(world, sample_x, floor_y - distance)) {
                return 0;
            }
        }
        for (distance = 0U; distance < DIGS_SPAWN_SUPPORT_CELLS;
             ++distance) {
            if (!digs_cell_is_solid(world, sample_x, floor_y + distance)) {
                return 0;
            }
        }
    }
    return 1;
}

static vox_result digs_spawn_player(vox_digs_match *match, vox_u16 player,
                                    vox_u32 preferred_x)
{
    vox_physics_body *body = &match->players[player];
    vox_physics_step_config spawn_config = match->physics_config;
    vox_u32 attempt;
    spawn_config.gravity_q16 = 0;
    for (attempt = 0U; attempt < VOX_WORLD_WIDTH; ++attempt) {
        vox_u32 x = (preferred_x + attempt * 11U) % VOX_WORLD_WIDTH;
        vox_u32 y;
        if (x == 0U || x + 1U >= VOX_WORLD_WIDTH) {
            continue;
        }
        for (y = 1U; y < VOX_WORLD_HEIGHT; ++y) {
            if (!digs_cell_is_solid(&match->world, x, y)) {
                continue;
            }
            if (!digs_spawn_floor_is_supported(&match->world, x, y)) {
                continue;
            }
            vox_physics_body_init(body);
            body->half_width_q16 *= (vox_i32)DIGS_DENSITY_SCALE;
            body->half_height_q16 *= (vox_i32)DIGS_DENSITY_SCALE;
            body->position_x.value_q16 = (vox_i32)(x << 16) + 32768L;
            body->position_y.value_q16 = (vox_i32)(y << 16) -
                                         body->half_height_q16;
            if (vox_physics_step_world(body, &match->world,
                                       &spawn_config) == VOX_OK) {
                return VOX_OK;
            }
        }
    }
    return VOX_ERR_CAPACITY;
}

static int digs_cell_is_rope_anchor(const vox_world *world,
                                    vox_u32 x, vox_u32 y)
{
    vox_u32 z;
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        const vox_cell *cell = vox_world_cell(world, x, y, z);
        if (cell != 0 &&
            (cell->material == VOX_MAT_BEDROCK ||
             cell->material == VOX_MAT_STONE ||
             cell->material == VOX_MAT_METAL ||
             cell->material == VOX_MAT_COAL)) {
            return 1;
        }
    }
    return 0;
}

static vox_i32 digs_distance_approx(vox_i32 delta_x, vox_i32 delta_y)
{
    vox_u32 absolute_x = digs_abs_i32(delta_x);
    vox_u32 absolute_y = digs_abs_i32(delta_y);
    vox_u32 largest = absolute_x > absolute_y ? absolute_x : absolute_y;
    vox_u32 smallest = absolute_x > absolute_y ? absolute_y : absolute_x;
    if (largest > 2147483647U - smallest / 2U) {
        return 2147483647L;
    }
    return (vox_i32)(largest + smallest / 2U);
}

static void digs_detach_rope(vox_digs_match *match, vox_u16 player,
                             vox_u16 event_type)
{
    vox_digs_rope *rope = &match->ropes[player];
    if (!rope->active) {
        return;
    }
    rope->active = 0U;
    rope->tension_q16 = 0L;
    digs_emit_event(match, event_type, player, VOX_DIGS_NO_PLAYER,
                    VOX_DIGS_TOOL_PICK, VOX_MAT_METAL,
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    rope->integrity, 0U);
}

static int digs_attach_rope(vox_digs_match *match, vox_u16 player)
{
    vox_physics_body *body = &match->players[player];
    vox_i32 source_x = body->position_x.value_q16;
    vox_i32 source_y = body->position_y.value_q16;
    vox_i32 delta_x = ((vox_i32)match->aim_x[player] << 16) + 32768L -
                      source_x;
    vox_i32 delta_y = ((vox_i32)match->aim_y[player] << 16) + 32768L -
                      source_y;
    vox_i32 distance = digs_distance_approx(delta_x, delta_y);
    vox_i32 target_x;
    vox_i32 target_y;
    vox_u32 steps;
    vox_u32 step;
    if (distance <= 0) {
        return 0;
    }
    if (distance > DIGS_ROPE_MAX_LENGTH_Q16) {
        vox_i32 divisor = distance / 256L;
        vox_i32 direction_x_q8;
        vox_i32 direction_y_q8;
        if (divisor <= 0) {
            divisor = 1L;
        }
        direction_x_q8 = delta_x / divisor;
        direction_y_q8 = delta_y / divisor;
        target_x = source_x +
                   (DIGS_ROPE_MAX_LENGTH_Q16 * direction_x_q8) / 256L;
        target_y = source_y +
                   (DIGS_ROPE_MAX_LENGTH_Q16 * direction_y_q8) / 256L;
    } else {
        target_x = source_x + delta_x;
        target_y = source_y + delta_y;
    }
    delta_x = target_x - source_x;
    delta_y = target_y - source_y;
    distance = digs_distance_approx(delta_x, delta_y);
    steps = (vox_u32)(distance / 65536L);
    if (steps == 0U) {
        steps = 1U;
    }
    if (steps > DIGS_ROPE_RAY_STEPS) {
        steps = DIGS_ROPE_RAY_STEPS;
    }
    for (step = 1U; step <= steps; ++step) {
        vox_i32 sample_x_q16 = source_x +
            digs_div_trunc_positive(delta_x, steps) * (vox_i32)step;
        vox_i32 sample_y_q16 = source_y +
            digs_div_trunc_positive(delta_y, steps) * (vox_i32)step;
        vox_i32 sample_x = digs_q16_to_cell(sample_x_q16);
        vox_i32 sample_y = digs_q16_to_cell(sample_y_q16);
        if (sample_x < 0 || sample_y < 0 ||
            sample_x >= (vox_i32)VOX_WORLD_WIDTH ||
            sample_y >= (vox_i32)VOX_WORLD_HEIGHT) {
            break;
        }
        if (digs_cell_is_rope_anchor(&match->world, (vox_u32)sample_x,
                                     (vox_u32)sample_y)) {
            vox_digs_rope *rope = &match->ropes[player];
            vox_i32 rope_length;
            rope->anchor_x_q16 = (sample_x << 16) + 32768L;
            rope->anchor_y_q16 = (sample_y << 16) + 32768L;
            rope_length = digs_distance_approx(
                rope->anchor_x_q16 - source_x,
                rope->anchor_y_q16 - source_y);
            if (rope_length < DIGS_ROPE_MIN_LENGTH_Q16) {
                rope_length = DIGS_ROPE_MIN_LENGTH_Q16;
            }
            rope->length_q16 = rope_length;
            rope->tension_q16 = 0L;
            rope->active = 1U;
            rope->integrity = 100U;
            digs_emit_event(match, VOX_DIGS_EVENT_ROPE_ATTACH, player,
                            VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_PICK,
                            VOX_MAT_METAL, rope->anchor_x_q16,
                            rope->anchor_y_q16, (vox_u16)(rope_length >> 16),
                            (vox_u16)(digs_noise(match->rules.seed,
                                                match->tick, player,
                                                0x524F5045U) & 7U));
            return 1;
        }
    }
    return 0;
}

static void digs_step_rope(vox_digs_match *match, vox_u16 player)
{
    vox_digs_rope *rope = &match->ropes[player];
    vox_u16 broken = 0U;
    vox_i16 reel = match->move_y_q15[player];
    vox_i32 anchor_x;
    vox_i32 anchor_y;
    if (!rope->active) {
        return;
    }
    anchor_x = rope->anchor_x_q16 >> 16;
    anchor_y = rope->anchor_y_q16 >> 16;
    if (anchor_x < 0 || anchor_y < 0 ||
        anchor_x >= (vox_i32)VOX_WORLD_WIDTH ||
        anchor_y >= (vox_i32)VOX_WORLD_HEIGHT ||
        !digs_cell_is_rope_anchor(&match->world, (vox_u32)anchor_x,
                                  (vox_u32)anchor_y)) {
        digs_detach_rope(match, player, VOX_DIGS_EVENT_ROPE_BREAK);
        return;
    }
    if (reel < -4096) {
        vox_i32 amount = (DIGS_ROPE_REEL_SPEED_Q16 *
                          -(vox_i32)reel) / 32767L;
        if (rope->length_q16 - amount < DIGS_ROPE_MIN_LENGTH_Q16) {
            rope->length_q16 = DIGS_ROPE_MIN_LENGTH_Q16;
        } else {
            rope->length_q16 -= amount;
        }
    } else if (reel > 4096) {
        vox_i32 amount = (DIGS_ROPE_REEL_SPEED_Q16 *
                          (vox_i32)reel) / 32767L;
        if (rope->length_q16 + amount > DIGS_ROPE_MAX_LENGTH_Q16) {
            rope->length_q16 = DIGS_ROPE_MAX_LENGTH_Q16;
        } else {
            rope->length_q16 += amount;
        }
    }
    if (vox_physics_rope_constraint(&match->players[player], &match->world,
                                    rope->anchor_x_q16,
                                    rope->anchor_y_q16,
                                    rope->length_q16,
                                    DIGS_ROPE_PULL_Q16,
                                    DIGS_ROPE_BREAK_TENSION_Q16,
                                    &rope->tension_q16, &broken) != VOX_OK ||
        broken) {
        digs_detach_rope(match, player, VOX_DIGS_EVENT_ROPE_BREAK);
    } else if (rope->tension_q16 > (3L << 16) && rope->integrity > 0U) {
        rope->integrity--;
        if (rope->integrity == 0U) {
            digs_detach_rope(match, player, VOX_DIGS_EVENT_ROPE_BREAK);
        }
    }
}

static vox_i32 digs_run_speed(const vox_digs_match *match, vox_u16 player)
{
    vox_i32 speed = DIGS_RUN_SPEED_Q16;
    if (match->anatomy[player][VOX_DIGS_PART_LEFT_FOOT].flags &
        VOX_DIGS_PART_SEVERED) {
        speed = (speed * 7L) / 8L;
    }
    if (match->anatomy[player][VOX_DIGS_PART_RIGHT_FOOT].flags &
        VOX_DIGS_PART_SEVERED) {
        speed = (speed * 7L) / 8L;
    }
    if (match->anatomy[player][VOX_DIGS_PART_LEFT_SHIN].flags &
        VOX_DIGS_PART_SEVERED) {
        speed = (speed * 3L) / 4L;
    }
    if (match->anatomy[player][VOX_DIGS_PART_RIGHT_SHIN].flags &
        VOX_DIGS_PART_SEVERED) {
        speed = (speed * 3L) / 4L;
    }
    return speed;
}

static int digs_player_on_slippery_material(const vox_digs_match *match,
                                            vox_u16 player)
{
    vox_i32 x = digs_q16_to_cell(
        match->players[player].position_x.value_q16);
    vox_i32 y = digs_q16_to_cell(
        match->players[player].position_y.value_q16 +
        match->players[player].half_height_q16 + 32768L);
    vox_u32 z;
    if (x < 0 || y < 0 || x >= (vox_i32)VOX_WORLD_WIDTH ||
        y >= (vox_i32)VOX_WORLD_HEIGHT) {
        return 0;
    }
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        const vox_cell *cell = vox_world_cell(&match->world, (vox_u32)x,
                                               (vox_u32)y, z);
        if (cell != 0 && (cell->material == VOX_MAT_BLOOD ||
                          cell->material == VOX_MAT_WATER)) {
            return 1;
        }
    }
    return 0;
}

static void digs_apply_player_controls(vox_digs_match *match, vox_u16 player)
{
    vox_physics_body *body = &match->players[player];
    vox_u16 actions = match->player_actions[player];
    vox_u16 pressed = (vox_u16)(actions &
                                 (vox_u16)~match->previous_actions[player]);
    vox_i32 target_speed = 0L;
    vox_i16 analog_x = match->move_x_q15[player];
    vox_i32 acceleration;
    vox_i32 deceleration;
    if (body->flags & VOX_PHYSICS_BODY_GROUNDED) {
        match->coyote_ticks[player] = DIGS_COYOTE_TICKS;
    } else if (match->coyote_ticks[player] > 0U) {
        match->coyote_ticks[player]--;
    }
    if (pressed & VOX_DIGS_ACTION_JUMP) {
        match->jump_buffer_ticks[player] = DIGS_JUMP_BUFFER_TICKS;
    } else if (match->jump_buffer_ticks[player] > 0U) {
        match->jump_buffer_ticks[player]--;
    }
    if (analog_x != 0) {
        target_speed = (digs_run_speed(match, player) *
                        (vox_i32)analog_x) / 32767L;
        match->facing_right[player] = analog_x > 0 ? 1U : 0U;
    } else if ((actions & (VOX_DIGS_ACTION_LEFT | VOX_DIGS_ACTION_RIGHT)) ==
        VOX_DIGS_ACTION_LEFT) {
        target_speed = -digs_run_speed(match, player);
        match->facing_right[player] = 0U;
    } else if ((actions & (VOX_DIGS_ACTION_LEFT | VOX_DIGS_ACTION_RIGHT)) ==
               VOX_DIGS_ACTION_RIGHT) {
        target_speed = digs_run_speed(match, player);
        match->facing_right[player] = 1U;
    }
    acceleration = (body->flags & VOX_PHYSICS_BODY_GROUNDED) ?
                   DIGS_GROUND_ACCEL_Q16 : DIGS_AIR_ACCEL_Q16;
    deceleration = (body->flags & VOX_PHYSICS_BODY_GROUNDED) ?
                   DIGS_GROUND_DECEL_Q16 : DIGS_AIR_DECEL_Q16;
    if ((body->flags & VOX_PHYSICS_BODY_GROUNDED) &&
        digs_player_on_slippery_material(match, player)) {
        acceleration /= 2L;
        deceleration /= 4L;
    }
    vox_physics_accelerate_x(body, target_speed, acceleration, deceleration);
    if (match->jump_buffer_ticks[player] > 0U &&
        match->coyote_ticks[player] > 0U) {
        body->velocity_y.value_q16 = DIGS_JUMP_SPEED_Q16;
        match->jump_buffer_ticks[player] = 0U;
        match->coyote_ticks[player] = 0U;
        match->jump_hold_ticks[player] = DIGS_JUMP_HOLD_TICKS;
    } else if ((actions & VOX_DIGS_ACTION_JUMP) &&
               match->jump_hold_ticks[player] > 0U &&
               body->velocity_y.value_q16 < 0L) {
        body->velocity_y.value_q16 -= DIGS_JUMP_HOLD_ACCEL_Q16;
        match->jump_hold_ticks[player]--;
    } else if (!(actions & VOX_DIGS_ACTION_JUMP)) {
        match->jump_hold_ticks[player] = 0U;
    }
    if ((actions & VOX_DIGS_ACTION_STEAM) &&
        match->steam_q16[player] != 0U) {
        if (body->velocity_y.value_q16 >
            -DIGS_MAX_VERTICAL_SPEED_Q16 + DIGS_STEAM_ACCEL_Q16) {
            body->velocity_y.value_q16 -= DIGS_STEAM_ACCEL_Q16;
        } else {
            body->velocity_y.value_q16 = -DIGS_MAX_VERTICAL_SPEED_Q16;
        }
        if (match->steam_q16[player] <= DIGS_STEAM_USE_Q16) {
            match->steam_q16[player] = 0U;
        } else {
            match->steam_q16[player] = (vox_u16)(match->steam_q16[player] -
                                                  DIGS_STEAM_USE_Q16);
        }
    } else if (body->flags & VOX_PHYSICS_BODY_GROUNDED) {
        if (match->steam_q16[player] >
            (vox_u16)(65535U - DIGS_STEAM_RECHARGE_Q16)) {
            match->steam_q16[player] = 65535U;
        } else {
            match->steam_q16[player] = (vox_u16)(match->steam_q16[player] +
                                                  DIGS_STEAM_RECHARGE_Q16);
        }
    }
    if ((actions & VOX_DIGS_ACTION_ROPE) && !match->ropes[player].active) {
        (void)digs_attach_rope(match, player);
    } else if (!(actions & VOX_DIGS_ACTION_ROPE) &&
               match->ropes[player].active) {
        digs_detach_rope(match, player, VOX_DIGS_EVENT_ROPE_DETACH);
    }
}

void vox_digs_rules_classic(vox_digs_rules *rules)
{
    if (rules == 0) {
        return;
    }
    rules->abi_version = VOX_ABI_VERSION;
    rules->struct_size = (vox_u32)sizeof(*rules);
    rules->match_ticks = 5U * VOX_DIGS_TICKS_PER_SECOND * 60U;
    rules->score_limit = 10U;
    rules->lava_start_tick = rules->match_ticks -
                             (90U * VOX_DIGS_TICKS_PER_SECOND);
    rules->seed = 0x564F5831U;
    rules->player_count = 2U;
    rules->bot_mask = 0x0002U;
    rules->team_mode = 0U;
    rules->map_style = VOX_DIGS_MAP_COAL_RIDGE;
    rules->weapon_mask = (vox_u16)((1U << VOX_DIGS_TOOL_COUNT) - 1U);
    rules->fx_budget = VOX_DIGS_FX_STANDARD;
    rules->friendly_fire = 0U;
    rules->reserved = 0U;
}

static vox_result digs_validate_rules(const vox_digs_rules *rules)
{
    vox_u16 active_mask;
    vox_u16 bot_count;
    vox_u16 human_count;
    if (rules == 0 || rules->abi_version != VOX_ABI_VERSION ||
        rules->struct_size < (vox_u32)sizeof(*rules)) {
        return VOX_ERR_INVALID;
    }
    if (rules->match_ticks == 0U || rules->score_limit == 0U ||
        rules->score_limit > 65535U ||
        rules->lava_start_tick >= rules->match_ticks ||
        rules->player_count == 0U ||
        rules->player_count > VOX_DIGS_MAX_SLOTS ||
        rules->team_mode > VOX_DIGS_MODE_MINERS_VS_MACHINES ||
        rules->map_style >= VOX_DIGS_MAP_COUNT || rules->weapon_mask == 0U ||
        (rules->weapon_mask &
         (vox_u16)~((1U << VOX_DIGS_TOOL_COUNT) - 1U)) != 0U ||
        (rules->fx_budget != VOX_DIGS_FX_RETRO &&
         rules->fx_budget != VOX_DIGS_FX_STANDARD &&
         rules->fx_budget != VOX_DIGS_FX_CARNAGE) ||
        rules->friendly_fire > 1U || rules->reserved != 0U) {
        return VOX_ERR_INVALID;
    }
    active_mask = (vox_u16)((1U << rules->player_count) - 1U);
    if ((rules->bot_mask & (vox_u16)~active_mask) != 0U) {
        return VOX_ERR_INVALID;
    }
    bot_count = digs_count_bits(rules->bot_mask);
    human_count = (vox_u16)(rules->player_count - bot_count);
    if (bot_count > VOX_DIGS_MAX_BOTS || human_count == 0U ||
        human_count > 2U) {
        return VOX_ERR_INVALID;
    }
    return VOX_OK;
}

vox_result vox_digs_match_init(vox_digs_match *match,
                               const vox_digs_rules *rules)
{
    vox_u16 i;
    vox_result result = digs_validate_rules(rules);
    if (match == 0 || result != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    match->abi_version = VOX_ABI_VERSION;
    match->struct_size = (vox_u32)sizeof(*match);
    match->rules = *rules;
    if (vox_digs_generate_map(&match->world, rules->map_style,
                              rules->seed) != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    match->tick = 0U;
    match->phase = VOX_DIGS_RUNNING;
    match->lava_level_q16 = 0U;
    match->lava_surface_y = (vox_u16)DIGS_LAVA_BASIN_TOP;
    match->projectile_count = 0U;
    match->effect_count = 0U;
    match->effect_cursor = 0U;
    match->event_head = 0U;
    match->event_count = 0U;
    match->event_sequence = 0U;
    match->terrain_hash = vox_world_hash(&match->world);
    vox_physics_step_config_default(&match->physics_config);
    match->physics_config.gravity_q16 *= (vox_i32)DIGS_DENSITY_SCALE;
    match->physics_config.max_step_q16 *= (vox_i32)DIGS_DENSITY_SCALE;
    for (i = 0U; i < VOX_DIGS_MAX_EVENTS; ++i) {
        match->events[i].sequence = 0U;
        match->events[i].tick = 0U;
        match->events[i].position_x_q16 = 0L;
        match->events[i].position_y_q16 = 0L;
        match->events[i].type = VOX_DIGS_EVENT_NONE;
        match->events[i].source = VOX_DIGS_NO_PLAYER;
        match->events[i].target = VOX_DIGS_NO_PLAYER;
        match->events[i].weapon = VOX_DIGS_TOOL_PICK;
        match->events[i].material = VOX_MAT_AIR;
        match->events[i].magnitude = 0U;
        match->events[i].variant = 0U;
        match->events[i].reserved = 0U;
    }
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        match->scores[i] = 0U;
        match->alive[i] = i < rules->player_count ? 1U : 0U;
        match->health[i] = match->alive[i] ? VOX_DIGS_MAX_HEALTH : 0U;
        match->deaths[i] = 0U;
        match->respawn_ticks[i] = 0U;
        match->spawn_shield_ticks[i] = match->alive[i] ?
                                       VOX_DIGS_SPAWN_SHIELD_TICKS : 0U;
        match->player_actions[i] = 0U;
        match->previous_actions[i] = 0U;
        match->aim_x[i] = 0U;
        match->aim_y[i] = 0U;
        match->move_x_q15[i] = 0;
        match->move_y_q15[i] = 0;
        match->coyote_ticks[i] = 0U;
        match->jump_buffer_ticks[i] = 0U;
        match->jump_hold_ticks[i] = 0U;
        match->steam_q16[i] = 65535U;
        match->weapon_cooldown[i] = 0U;
        match->selected_weapon[i] = VOX_DIGS_TOOL_PICK;
        match->facing_right[i] = (vox_u16)(i < 2U ? 1U : 0U);
        match->last_attacker[i] = VOX_DIGS_NO_PLAYER;
        match->ropes[i].anchor_x_q16 = 0L;
        match->ropes[i].anchor_y_q16 = 0L;
        match->ropes[i].length_q16 = DIGS_ROPE_MIN_LENGTH_Q16;
        match->ropes[i].tension_q16 = 0L;
        match->ropes[i].active = 0U;
        match->ropes[i].integrity = 0U;
        match->bots[i].mode = VOX_DIGS_AI_ROAMING;
        match->bots[i].target = VOX_DIGS_NO_PLAYER;
        match->bots[i].memory_ticks = 0U;
        match->bots[i].state_ticks = 0U;
        match->bots[i].roam_direction = (vox_i16)((i & 1U) ? -1 : 1);
        match->bots[i].decision_ticks = (vox_u16)(i * 2U);
        match->bots[i].last_seen_x_q16 = 0L;
        match->bots[i].last_seen_y_q16 = 0L;
        digs_init_anatomy(match, i);
        vox_physics_body_init(&match->players[i]);
        if (match->alive[i] &&
            digs_spawn_player(match, i,
                              (vox_u32)(i + 1U) * VOX_WORLD_WIDTH /
                              (vox_u32)(rules->player_count + 1U)) != VOX_OK) {
            return VOX_ERR_CAPACITY;
        }
        if (match->alive[i]) {
            vox_i32 player_x = digs_q16_to_cell(
                match->players[i].position_x.value_q16);
            vox_i32 player_y = digs_q16_to_cell(
                match->players[i].position_y.value_q16);
            match->aim_x[i] = (vox_u16)(player_x +
                (match->facing_right[i] ? 12L : -12L));
            match->aim_y[i] = (vox_u16)(player_y > 8L ?
                                        player_y - 8L : 0L);
            digs_emit_event(match, VOX_DIGS_EVENT_SPAWN, i,
                            VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_PICK,
                            VOX_MAT_FLESH,
                            match->players[i].position_x.value_q16,
                            match->players[i].position_y.value_q16,
                            VOX_DIGS_SPAWN_SHIELD_TICKS, i);
        }
    }
    for (i = 0U; i < VOX_DIGS_MAX_PROJECTILES; ++i) {
        match->projectiles[i].position_x_q16 = 0L;
        match->projectiles[i].position_y_q16 = 0L;
        match->projectiles[i].velocity_x_q16 = 0L;
        match->projectiles[i].velocity_y_q16 = 0L;
        match->projectiles[i].active = 0U;
        match->projectiles[i].owner = VOX_DIGS_NO_PLAYER;
        match->projectiles[i].weapon = VOX_DIGS_TOOL_PICK;
        match->projectiles[i].material = VOX_MAT_AIR;
        match->projectiles[i].fuse_ticks = 0U;
        match->projectiles[i].age_ticks = 0U;
        match->projectiles[i].damage = 0U;
        match->projectiles[i].blast_radius = 0U;
    }
    for (i = 0U; i < VOX_DIGS_MAX_EFFECTS; ++i) {
        match->effects[i].position_x_q16 = 0L;
        match->effects[i].position_y_q16 = 0L;
        match->effects[i].velocity_x_q16 = 0L;
        match->effects[i].velocity_y_q16 = 0L;
        match->effects[i].active = 0U;
        match->effects[i].material = VOX_MAT_AIR;
        match->effects[i].ttl_ticks = 0U;
        match->effects[i].variant = 0U;
        match->effects[i].source = VOX_DIGS_NO_PLAYER;
        match->effects[i].reserved = 0U;
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_result vox_digs_match_step(vox_digs_match *match)
{
    vox_u32 remaining;
    vox_u16 i;
    if (match == 0 || match->abi_version != VOX_ABI_VERSION ||
        match->struct_size < (vox_u32)sizeof(*match) ||
        match->phase != VOX_DIGS_RUNNING) {
        return VOX_ERR_INVALID;
    }
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        if (!vox_digs_player_is_active(match, i)) {
            continue;
        }
        if (match->weapon_cooldown[i] > 0U) {
            match->weapon_cooldown[i]--;
        }
        if (match->alive[i] && match->spawn_shield_ticks[i] > 0U) {
            match->spawn_shield_ticks[i]--;
            if (match->spawn_shield_ticks[i] == 0U) {
                digs_emit_event(match, VOX_DIGS_EVENT_SHIELD_END, i,
                                VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_PICK,
                                VOX_MAT_METAL,
                                match->players[i].position_x.value_q16,
                                match->players[i].position_y.value_q16,
                                0U, 0U);
            }
        }
        if (!match->alive[i] &&
            match->respawn_ticks[i] > 0U) {
            match->respawn_ticks[i]--;
            if (match->respawn_ticks[i] == 0U) {
                if (digs_spawn_player(match, i,
                                      (vox_u32)(i + 1U) * VOX_WORLD_WIDTH /
                                      (vox_u32)(match->rules.player_count +
                                                1U)) ==
                    VOX_OK) {
                    match->alive[i] = 1U;
                    match->health[i] = VOX_DIGS_MAX_HEALTH;
                    match->steam_q16[i] = 65535U;
                    match->last_attacker[i] = VOX_DIGS_NO_PLAYER;
                    match->spawn_shield_ticks[i] =
                        VOX_DIGS_SPAWN_SHIELD_TICKS;
                    match->player_actions[i] = 0U;
                    match->previous_actions[i] = 0U;
                    match->move_x_q15[i] = 0;
                    match->move_y_q15[i] = 0;
                    match->ropes[i].active = 0U;
                    digs_init_anatomy(match, i);
                    digs_emit_event(match, VOX_DIGS_EVENT_SPAWN, i,
                                    VOX_DIGS_NO_PLAYER,
                                    VOX_DIGS_TOOL_PICK, VOX_MAT_FLESH,
                                    match->players[i].position_x.value_q16,
                                    match->players[i].position_y.value_q16,
                                    VOX_DIGS_SPAWN_SHIELD_TICKS,
                                    (vox_u16)(match->deaths[i] & 7U));
                } else {
                    match->respawn_ticks[i] = 30U;
                }
            }
        }
    }
    for (i = 0U; i < match->rules.player_count; ++i) {
        if (match->phase != VOX_DIGS_RUNNING) {
            break;
        }
        if (match->alive[i] && vox_digs_player_is_bot(match, i) &&
            vox_digs_bot_think(match, i) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    }
    if (match->phase != VOX_DIGS_RUNNING) {
        match->state_hash = vox_digs_hash(match);
        return VOX_OK;
    }
    if (vox_world_step(&match->world, 0) != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        if (match->alive[i]) {
            digs_apply_player_controls(match, i);
        }
        if (match->alive[i] &&
            vox_physics_step_world(&match->players[i], &match->world,
                                   &match->physics_config) != VOX_OK) {
            if (digs_spawn_player(match, i,
                                  (vox_u32)(i + 1U) * VOX_WORLD_WIDTH /
                                  (vox_u32)(match->rules.player_count + 1U)) !=
                VOX_OK) {
                match->alive[i] = 0U;
                match->health[i] = 0U;
                match->respawn_ticks[i] = 30U;
            }
        } else if (match->alive[i]) {
            digs_step_rope(match, i);
        }
        match->previous_actions[i] = match->player_actions[i];
    }
    digs_step_projectiles(match);
    digs_step_effects(match);
    digs_step_bleeding(match);
    digs_step_reactions(match);
    if (match->tick >= match->rules.lava_start_tick) {
        remaining = match->rules.match_ticks - match->rules.lava_start_tick;
        match->lava_level_q16 = digs_scale_lava_level(
            match->tick - match->rules.lava_start_tick, remaining);
    }
    digs_update_lava(match);
    digs_apply_lava_hazards(match);
    match->tick++;
    if (match->tick >= match->rules.match_ticks) {
        match->phase = VOX_DIGS_RESULTS;
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

static void digs_spawn_death_gore(vox_digs_match *match, vox_u16 victim,
                                  vox_u16 killer)
{
    vox_u16 part;
    vox_u16 blood_count;
    vox_u16 weapon = vox_digs_player_is_active(match, killer) ?
                     match->selected_weapon[killer] : VOX_DIGS_TOOL_PICK;
    vox_i32 x_q16 = match->players[victim].position_x.value_q16;
    vox_i32 y_q16 = match->players[victim].position_y.value_q16;
    if (match->rules.fx_budget == VOX_DIGS_FX_RETRO) {
        blood_count = 24U;
    } else if (match->rules.fx_budget == VOX_DIGS_FX_CARNAGE) {
        blood_count = 72U;
    } else {
        blood_count = 44U;
    }
    for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
        vox_u32 noise = digs_noise(match->rules.seed, match->tick,
                                   (vox_u32)victim,
                                   0xA1100000U + part +
                                   (vox_u32)match->deaths[victim] * 37U);
        vox_i32 velocity_x = ((vox_i32)(noise % 21U) - 10L) * 7168L;
        vox_i32 velocity_y = -16384L -
            (vox_i32)((noise >> 8) % 15U) * 5120L;
        if (part >= VOX_DIGS_PART_LEFT_UPPER_ARM) {
            match->anatomy[victim][part].flags = (vox_u16)(
                match->anatomy[victim][part].flags |
                VOX_DIGS_PART_SEVERED);
        }
        digs_spawn_effect_variant(match, VOX_MAT_FLESH, x_q16, y_q16,
                                  velocity_x, velocity_y,
                                  (vox_u16)(70U + noise % 75U), victim,
                                  part);
        if (part >= VOX_DIGS_PART_LEFT_UPPER_ARM) {
            digs_emit_event(match, VOX_DIGS_EVENT_LIMB_SEVER, killer,
                            victim, weapon,
                            VOX_MAT_FLESH, x_q16, y_q16, part,
                            (vox_u16)(noise & 15U));
        }
    }
    for (part = 0U; part < blood_count; ++part) {
        vox_u32 noise = digs_noise(match->rules.seed,
                                   match->tick + part,
                                   (vox_u32)victim,
                                   0xB1000000U +
                                   (vox_u32)match->deaths[victim] * 53U);
        vox_i32 velocity_x = ((vox_i32)(noise % 25U) - 12L) * 6144L;
        vox_i32 velocity_y = -8192L -
            (vox_i32)((noise >> 7) % 18U) * 4608L;
        digs_spawn_effect_variant(match, VOX_MAT_BLOOD, x_q16, y_q16,
                                  velocity_x, velocity_y,
                                  (vox_u16)(38U + noise % 90U), victim,
                                  (vox_u16)(noise & 31U));
    }
}

vox_result vox_digs_record_kill(vox_digs_match *match, vox_u16 killer,
                                vox_u16 victim)
{
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_active(match, killer) ||
        !vox_digs_player_is_active(match, victim) || !match->alive[victim] ||
        killer == victim) {
        return VOX_ERR_INVALID;
    }
    match->scores[killer]++;
    match->alive[victim] = 0U;
    match->health[victim] = 0U;
    match->deaths[victim]++;
    match->respawn_ticks[victim] = VOX_DIGS_RESPAWN_TICKS;
    match->spawn_shield_ticks[victim] = 0U;
    match->player_actions[victim] = 0U;
    match->move_x_q15[victim] = 0;
    match->move_y_q15[victim] = 0;
    match->last_attacker[victim] = killer;
    digs_detach_rope(match, victim, VOX_DIGS_EVENT_ROPE_DETACH);
    digs_spawn_death_gore(match, victim, killer);
    digs_emit_event(match, VOX_DIGS_EVENT_KILL, killer, victim,
                    match->selected_weapon[killer], VOX_MAT_BLOOD,
                    match->players[victim].position_x.value_q16,
                    match->players[victim].position_y.value_q16,
                    match->scores[killer],
                    (vox_u16)(digs_noise(match->rules.seed, match->tick,
                                         killer, victim) & 15U));
    if (match->scores[killer] >= match->rules.score_limit) {
        match->phase = VOX_DIGS_RESULTS;
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_result vox_digs_submit_input(vox_digs_match *match,
                                 const vox_digs_input *input)
{
    if (match == 0 || input == 0 || match->phase != VOX_DIGS_RUNNING ||
        input->abi_version != VOX_ABI_VERSION ||
        input->struct_size < (vox_u32)sizeof(*input) ||
        !vox_digs_player_is_active(match, input->player) ||
        vox_digs_player_is_bot(match, input->player) ||
        !match->alive[input->player] ||
        (input->actions & (vox_u16)~VOX_DIGS_ACTION_MASK) != 0U ||
        input->aim_x >= VOX_WORLD_WIDTH ||
        input->aim_y >= VOX_WORLD_HEIGHT ||
        input->move_x_q15 == (vox_i16)-32768L ||
        input->move_y_q15 == (vox_i16)-32768L) {
        return VOX_ERR_INVALID;
    }
    match->player_actions[input->player] = input->actions;
    match->aim_x[input->player] = input->aim_x;
    match->aim_y[input->player] = input->aim_y;
    match->move_x_q15[input->player] = input->move_x_q15;
    match->move_y_q15[input->player] = input->move_y_q15;
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

static void digs_end_spawn_shield(vox_digs_match *match, vox_u16 player)
{
    if (match->spawn_shield_ticks[player] == 0U) {
        return;
    }
    match->spawn_shield_ticks[player] = 0U;
    digs_emit_event(match, VOX_DIGS_EVENT_SHIELD_END, player,
                    VOX_DIGS_NO_PLAYER, match->selected_weapon[player],
                    VOX_MAT_METAL,
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    0U, 1U);
}

vox_result vox_digs_use_tool(vox_digs_match *match, vox_u16 player,
                             vox_u16 tool, vox_u32 x, vox_u32 y, vox_u32 z)
{
    const vox_cell *target;
    vox_result result;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_active(match, player) || !match->alive[player] ||
        tool >= VOX_DIGS_TOOL_COUNT) {
        return VOX_ERR_INVALID;
    }
    target = vox_world_cell(&match->world, x, y, z);
    if (target == 0 || target->material == VOX_MAT_BEDROCK) {
        return VOX_ERR_INVALID;
    }
    if (tool == VOX_DIGS_TOOL_PICK) {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_AIR,
                               20L << 16);
    } else if (tool == VOX_DIGS_TOOL_BLAST_CHARGE) {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(3U), 700L << 16);
    } else if (tool == VOX_DIGS_TOOL_SMOKE_POT) {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_SMOKE,
                               180L << 16);
    } else if (tool == VOX_DIGS_TOOL_CINDER_FLASK) {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_LAVA,
                               700L << 16);
    } else if (tool == VOX_DIGS_TOOL_PRESSURE_HOSE) {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_WATER,
                               20L << 16);
    } else if (tool == VOX_DIGS_TOOL_SLEDGE) {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(2U), 80L << 16);
    } else if (tool == VOX_DIGS_TOOL_NAIL_GUN) {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(1U), 40L << 16);
    } else if (tool == VOX_DIGS_TOOL_BOILER_SHOTGUN) {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(2U), 180L << 16);
    } else if (tool == VOX_DIGS_TOOL_CONCUSSION_GRENADE) {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(5U), 400L << 16);
    } else {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(4U), 500L << 16);
    }
    if (result != VOX_OK) {
        return result;
    }
    match->selected_weapon[player] = tool;
    digs_end_spawn_shield(match, player);
    digs_emit_event(match, VOX_DIGS_EVENT_WEAPON_FIRE, player,
                    VOX_DIGS_NO_PLAYER, tool, target->material,
                    (vox_i32)(x << 16), (vox_i32)(y << 16),
                    digs_weapons[tool].damage,
                    (vox_u16)(digs_noise(match->rules.seed, match->tick,
                                         player, tool) & 15U));
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

const vox_digs_weapon_properties *vox_digs_weapon_get(vox_u16 weapon)
{
    if (weapon >= VOX_DIGS_TOOL_COUNT) {
        return 0;
    }
    return &digs_weapons[weapon];
}

static vox_u32 digs_abs_i32(vox_i32 value)
{
    if (value < 0) {
        return (vox_u32)(-(value + 1)) + 1U;
    }
    return (vox_u32)value;
}

/* C89 permits either rounding direction for negative signed division. */
static vox_i32 digs_div_trunc_positive(vox_i32 value, vox_u32 divisor)
{
    vox_u32 quotient;
    if (divisor == 0U) {
        return 0;
    }
    quotient = digs_abs_i32(value) / divisor;
    if (value >= 0) {
        return (vox_i32)quotient;
    }
    if (quotient == 0x80000000U) {
        return (vox_i32)(-2147483647L - 1L);
    }
    return -(vox_i32)quotient;
}

static vox_i32 digs_q16_to_cell(vox_i32 value)
{
    return digs_div_trunc_positive(value, 65536U);
}

/* Returns floor(numerator * 65535 / denominator) without a 64-bit type. */
static vox_u32 digs_scale_lava_level(vox_u32 numerator,
                                     vox_u32 denominator)
{
    vox_u32 quotient = 0U;
    vox_u32 remainder;
    vox_u16 bit;
    if (denominator == 0U || numerator == 0U) {
        return 0U;
    }
    if (numerator >= denominator) {
        return 65535U;
    }
    remainder = numerator;
    for (bit = 0U; bit < 16U; ++bit) {
        quotient <<= 1;
        if (remainder >= denominator - remainder) {
            remainder -= denominator - remainder;
            quotient |= 1U;
        } else {
            remainder += remainder;
        }
    }
    if (remainder < numerator && quotient > 0U) {
        quotient--;
    }
    return quotient;
}

static void digs_spawn_effect(vox_digs_match *match, vox_u16 material,
                              vox_i32 x_q16, vox_i32 y_q16,
                              vox_i32 velocity_x_q16,
                              vox_i32 velocity_y_q16, vox_u16 ttl)
{
    vox_u16 variant = (vox_u16)(digs_noise(match->rules.seed,
                                           match->tick,
                                           match->effect_cursor,
                                           material) & 31U);
    digs_spawn_effect_variant(match, material, x_q16, y_q16,
                              velocity_x_q16, velocity_y_q16, ttl,
                              VOX_DIGS_NO_PLAYER, variant);
}

static void digs_spawn_effect_variant(vox_digs_match *match,
                                      vox_u16 material,
                                      vox_i32 x_q16, vox_i32 y_q16,
                                      vox_i32 velocity_x_q16,
                                      vox_i32 velocity_y_q16, vox_u16 ttl,
                                      vox_u16 source, vox_u16 variant)
{
    vox_u16 search;
    vox_u16 slot = match->effect_cursor;
    vox_u16 capacity = match->rules.fx_budget;
    int found = 0;
    for (search = 0U; search < capacity; ++search) {
        vox_u16 candidate = (vox_u16)((match->effect_cursor + search) %
                                       capacity);
        if (!match->effects[candidate].active) {
            slot = candidate;
            found = 1;
            break;
        }
    }
    if (found) {
        match->effect_count++;
    }
    match->effects[slot].position_x_q16 = x_q16;
    match->effects[slot].position_y_q16 = y_q16;
    match->effects[slot].velocity_x_q16 = velocity_x_q16;
    match->effects[slot].velocity_y_q16 = velocity_y_q16;
    match->effects[slot].active = 1U;
    match->effects[slot].material = material;
    match->effects[slot].ttl_ticks = ttl;
    match->effects[slot].variant = variant;
    match->effects[slot].source = source;
    match->effects[slot].reserved = 0U;
    match->effect_cursor = (vox_u16)((slot + 1U) % capacity);
}

static void digs_environment_defeat(vox_digs_match *match, vox_u16 victim)
{
    match->alive[victim] = 0U;
    match->health[victim] = 0U;
    match->deaths[victim]++;
    match->respawn_ticks[victim] = VOX_DIGS_RESPAWN_TICKS;
    match->spawn_shield_ticks[victim] = 0U;
    match->player_actions[victim] = 0U;
    match->move_x_q15[victim] = 0;
    match->move_y_q15[victim] = 0;
    match->last_attacker[victim] = VOX_DIGS_NO_PLAYER;
    digs_detach_rope(match, victim, VOX_DIGS_EVENT_ROPE_DETACH);
    digs_spawn_death_gore(match, victim, VOX_DIGS_NO_PLAYER);
    digs_emit_event(match, VOX_DIGS_EVENT_KILL, VOX_DIGS_NO_PLAYER,
                    victim, VOX_DIGS_TOOL_PICK, VOX_MAT_BLOOD,
                    match->players[victim].position_x.value_q16,
                    match->players[victim].position_y.value_q16,
                    match->deaths[victim],
                    (vox_u16)(digs_noise(match->rules.seed, match->tick,
                                         victim, 0xDEADU) & 15U));
}

static int digs_same_team(const vox_digs_match *match, vox_u16 first,
                          vox_u16 second)
{
    if (match->rules.team_mode != VOX_DIGS_MODE_MINERS_VS_MACHINES) {
        return 0;
    }
    return vox_digs_player_is_bot(match, first) ==
           vox_digs_player_is_bot(match, second);
}

static vox_u16 digs_choose_hit_part(const vox_digs_match *match,
                                    vox_u16 attacker, vox_u16 victim,
                                    vox_u16 weapon, vox_u16 damage)
{
    vox_u32 noise = digs_noise(match->rules.seed, match->tick,
                               (vox_u32)victim,
                               (vox_u32)attacker * 31U +
                               (vox_u32)weapon * 131U + damage);
    if (weapon == VOX_DIGS_TOOL_SLEDGE || weapon == VOX_DIGS_TOOL_PICK) {
        static const vox_u16 melee_parts[6] = {
            VOX_DIGS_PART_HEAD, VOX_DIGS_PART_TORSO,
            VOX_DIGS_PART_PELVIS, VOX_DIGS_PART_LEFT_UPPER_ARM,
            VOX_DIGS_PART_RIGHT_UPPER_ARM, VOX_DIGS_PART_TORSO
        };
        return melee_parts[noise % 6U];
    }
    if (weapon == VOX_DIGS_TOOL_NAIL_GUN ||
        weapon == VOX_DIGS_TOOL_BOILER_SHOTGUN) {
        return (vox_u16)(noise % VOX_DIGS_ANATOMY_PART_COUNT);
    }
    return (vox_u16)((noise + (noise >> 9)) %
                     VOX_DIGS_ANATOMY_PART_COUNT);
}

vox_result vox_digs_apply_hit(vox_digs_match *match, vox_u16 attacker,
                              vox_u16 victim, vox_u16 weapon,
                              vox_u16 part, vox_u16 damage,
                              vox_u16 damage_flags)
{
    vox_digs_anatomy_part *anatomy;
    vox_u16 particle_count;
    vox_u16 i;
    vox_u16 fatal = 0U;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_active(match, victim) ||
        !match->alive[victim] || damage == 0U ||
        weapon >= VOX_DIGS_TOOL_COUNT ||
        (part != VOX_DIGS_NO_PART &&
         part >= VOX_DIGS_ANATOMY_PART_COUNT) ||
        (damage_flags & (vox_u16)~(VOX_DIGS_DAMAGE_BALLISTIC |
                                  VOX_DIGS_DAMAGE_BLUNT |
                                  VOX_DIGS_DAMAGE_EXPLOSIVE |
                                  VOX_DIGS_DAMAGE_HEAT)) != 0U ||
        (attacker != VOX_DIGS_NO_PLAYER &&
         !vox_digs_player_is_active(match, attacker))) {
        return VOX_ERR_INVALID;
    }
    if (attacker != VOX_DIGS_NO_PLAYER && attacker != victim &&
        !match->rules.friendly_fire &&
        digs_same_team(match, attacker, victim)) {
        return VOX_OK;
    }
    if (match->spawn_shield_ticks[victim] > 0U) {
        digs_emit_event(match, VOX_DIGS_EVENT_DAMAGE, attacker, victim,
                        weapon, VOX_MAT_METAL,
                        match->players[victim].position_x.value_q16,
                        match->players[victim].position_y.value_q16,
                        0U, 1U);
        return VOX_OK;
    }
    if (part == VOX_DIGS_NO_PART) {
        part = digs_choose_hit_part(match, attacker, victim, weapon, damage);
    }
    anatomy = &match->anatomy[victim][part];
    if (attacker != VOX_DIGS_NO_PLAYER && attacker != victim) {
        match->last_attacker[victim] = attacker;
    }
    particle_count = (vox_u16)(damage / 6U + 3U);
    if (match->rules.fx_budget == VOX_DIGS_FX_CARNAGE) {
        particle_count = (vox_u16)(particle_count * 2U);
    } else if (match->rules.fx_budget == VOX_DIGS_FX_RETRO &&
               particle_count > 12U) {
        particle_count = 12U;
    }
    if (particle_count > 40U) {
        particle_count = 40U;
    }
    for (i = 0U; i < particle_count; ++i) {
        vox_u32 noise = digs_noise(match->rules.seed, match->tick + i,
                                   (vox_u32)victim,
                                   (vox_u32)part * 257U + damage);
        vox_i32 spread_x = ((vox_i32)(noise % 17U) - 8L) * 5120L;
        vox_i32 spread_y = -10240L -
            (vox_i32)((noise >> 8) % 13U) * 4096L;
        digs_spawn_effect_variant(match, VOX_MAT_BLOOD,
            match->players[victim].position_x.value_q16,
            match->players[victim].position_y.value_q16,
            spread_x, spread_y, (vox_u16)(34U + noise % 60U), victim,
            (vox_u16)((part << 4) | (noise & 15U)));
    }
    if (damage < anatomy->health) {
        anatomy->health = (vox_u16)(anatomy->health - damage);
    } else {
        anatomy->health = 0U;
        if (anatomy->flags & VOX_DIGS_PART_VITAL) {
            fatal = 1U;
        } else if (!(anatomy->flags & VOX_DIGS_PART_SEVERED)) {
            anatomy->flags = (vox_u16)(anatomy->flags |
                                       VOX_DIGS_PART_SEVERED);
            digs_spawn_effect_variant(match, VOX_MAT_FLESH,
                match->players[victim].position_x.value_q16,
                match->players[victim].position_y.value_q16,
                ((vox_i32)(part & 3U) - 1L) * 16384L,
                -32768L - (vox_i32)part * 1024L, 100U, victim, part);
            digs_emit_event(match, VOX_DIGS_EVENT_LIMB_SEVER, attacker,
                            victim, weapon, VOX_MAT_FLESH,
                            match->players[victim].position_x.value_q16,
                            match->players[victim].position_y.value_q16,
                            part, (vox_u16)(damage_flags & 15U));
        }
    }
    if (damage_flags & VOX_DIGS_DAMAGE_HEAT) {
        anatomy->flags = (vox_u16)((anatomy->flags |
                                    VOX_DIGS_PART_CAUTERIZED) &
                                   (vox_u16)~VOX_DIGS_PART_BLEEDING);
        anatomy->bleed_rate_q8 = 0U;
    } else {
        vox_u16 bleed = (vox_u16)(damage / 10U + 1U);
        if (bleed > 16U) {
            bleed = 16U;
        }
        anatomy->flags = (vox_u16)(anatomy->flags |
                                   VOX_DIGS_PART_BLEEDING);
        if (anatomy->bleed_rate_q8 <= (vox_u16)(255U - bleed)) {
            anatomy->bleed_rate_q8 = (vox_u16)(anatomy->bleed_rate_q8 +
                                                bleed);
        } else {
            anatomy->bleed_rate_q8 = 255U;
        }
        match->clot_ticks[victim] = 0U;
    }
    digs_emit_event(match, VOX_DIGS_EVENT_DAMAGE, attacker, victim,
                    weapon, VOX_MAT_BLOOD,
                    match->players[victim].position_x.value_q16,
                    match->players[victim].position_y.value_q16,
                    damage, (vox_u16)((part << 4) |
                                      (damage_flags & 15U)));
    if (damage < match->health[victim]) {
        match->health[victim] = (vox_u16)(match->health[victim] - damage);
    } else {
        match->health[victim] = 0U;
        fatal = 1U;
    }
    if (fatal && attacker != VOX_DIGS_NO_PLAYER && attacker != victim) {
        if (vox_digs_record_kill(match, attacker, victim) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    } else if (fatal &&
               match->last_attacker[victim] != VOX_DIGS_NO_PLAYER &&
               match->last_attacker[victim] != victim) {
        if (vox_digs_record_kill(match, match->last_attacker[victim],
                                 victim) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    } else if (fatal) {
        digs_environment_defeat(match, victim);
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_result vox_digs_apply_damage(vox_digs_match *match, vox_u16 attacker,
                                 vox_u16 victim, vox_u16 damage)
{
    vox_u16 weapon = VOX_DIGS_TOOL_PICK;
    vox_u16 flags = VOX_DIGS_DAMAGE_BLUNT;
    if (attacker != VOX_DIGS_NO_PLAYER && match != 0 &&
        vox_digs_player_is_active(match, attacker)) {
        weapon = match->selected_weapon[attacker];
    }
    if (weapon == VOX_DIGS_TOOL_NAIL_GUN ||
        weapon == VOX_DIGS_TOOL_BOILER_SHOTGUN) {
        flags = VOX_DIGS_DAMAGE_BALLISTIC;
    } else if (weapon == VOX_DIGS_TOOL_BLAST_CHARGE ||
               weapon == VOX_DIGS_TOOL_CONCUSSION_GRENADE ||
               weapon == VOX_DIGS_TOOL_NAIL_BOMB) {
        flags = VOX_DIGS_DAMAGE_EXPLOSIVE;
    } else if (weapon == VOX_DIGS_TOOL_CINDER_FLASK) {
        flags = VOX_DIGS_DAMAGE_HEAT;
    }
    return vox_digs_apply_hit(match, attacker, victim, weapon,
                              VOX_DIGS_NO_PART, damage, flags);
}

static vox_u16 digs_projectile_material(vox_u16 weapon)
{
    if (weapon == VOX_DIGS_TOOL_SMOKE_POT) {
        return VOX_MAT_SMOKE;
    }
    if (weapon == VOX_DIGS_TOOL_CINDER_FLASK) {
        return VOX_MAT_LAVA;
    }
    if (weapon == VOX_DIGS_TOOL_PRESSURE_HOSE) {
        return VOX_MAT_WATER;
    }
    if (weapon == VOX_DIGS_TOOL_BLAST_CHARGE ||
        weapon == VOX_DIGS_TOOL_NAIL_BOMB) {
        return VOX_MAT_COAL;
    }
    return VOX_MAT_METAL;
}

static vox_result digs_spawn_projectile(vox_digs_match *match,
                                        vox_u16 player, vox_u16 weapon,
                                        vox_u32 target_x,
                                        vox_u32 target_y)
{
    const vox_digs_weapon_properties *properties = &digs_weapons[weapon];
    vox_digs_projectile *projectile = 0;
    vox_i32 source_x;
    vox_i32 source_y;
    vox_i32 delta_x;
    vox_i32 delta_y;
    vox_u32 divisor;
    vox_i32 speed_q16;
    vox_u16 i;
    for (i = 0U; i < VOX_DIGS_MAX_PROJECTILES; ++i) {
        if (!match->projectiles[i].active) {
            projectile = &match->projectiles[i];
            break;
        }
    }
    if (projectile == 0) {
        return VOX_ERR_CAPACITY;
    }
    source_x = digs_q16_to_cell(
        match->players[player].position_x.value_q16);
    source_y = digs_q16_to_cell(
        match->players[player].position_y.value_q16);
    delta_x = (vox_i32)target_x - source_x;
    delta_y = (vox_i32)target_y - source_y;
    if (delta_x == 0 && delta_y == 0) {
        delta_x = match->facing_right[player] ? 1L : -1L;
    }
    divisor = digs_abs_i32(delta_x);
    if (digs_abs_i32(delta_y) > divisor) {
        divisor = digs_abs_i32(delta_y);
    }
    if (divisor == 0U) {
        divisor = 1U;
    }
    speed_q16 = (vox_i32)properties->projectile_speed_q8 << 8;
    projectile->position_x_q16 =
        match->players[player].position_x.value_q16;
    projectile->position_y_q16 =
        match->players[player].position_y.value_q16;
    projectile->velocity_x_q16 = digs_div_trunc_positive(
        delta_x * speed_q16, divisor);
    projectile->velocity_y_q16 = digs_div_trunc_positive(
        delta_y * speed_q16, divisor);
    projectile->active = 1U;
    projectile->owner = player;
    projectile->weapon = weapon;
    projectile->material = digs_projectile_material(weapon);
    projectile->fuse_ticks = properties->fuse_ticks;
    projectile->age_ticks = 0U;
    projectile->damage = properties->damage;
    projectile->blast_radius = properties->blast_radius;
    match->projectile_count++;
    return VOX_OK;
}

static void digs_damage_radius(vox_digs_match *match, vox_u16 attacker,
                               vox_u32 x, vox_u32 y, vox_u16 radius,
                               vox_u16 damage, vox_u16 weapon)
{
    vox_u16 player;
    vox_i32 radius_q16 = (vox_i32)radius << 16;
    for (player = 0U; player < match->rules.player_count; ++player) {
        vox_i32 delta_x;
        vox_i32 delta_y;
        vox_u32 distance_squared;
        vox_u32 radius_squared;
        vox_u16 dealt;
        if (!match->alive[player]) {
            continue;
        }
        delta_x = match->players[player].position_x.value_q16 -
                  (vox_i32)(x << 16);
        delta_y = match->players[player].position_y.value_q16 -
                  (vox_i32)(y << 16);
        if (digs_abs_i32(delta_x) > (vox_u32)radius_q16 ||
            digs_abs_i32(delta_y) > (vox_u32)radius_q16) {
            continue;
        }
        delta_x = digs_q16_to_cell(delta_x);
        delta_y = digs_q16_to_cell(delta_y);
        distance_squared = (vox_u32)(delta_x * delta_x +
                                      delta_y * delta_y);
        radius_squared = (vox_u32)radius * (vox_u32)radius;
        if (distance_squared > radius_squared) {
            continue;
        }
        dealt = (vox_u16)((vox_u32)damage *
                (radius_squared - distance_squared + 1U) /
                (radius_squared + 1U));
        if (dealt == 0U) {
            dealt = 1U;
        }
        (void)vox_digs_apply_hit(match, attacker, player, weapon,
                                 VOX_DIGS_NO_PART, dealt,
                                 VOX_DIGS_DAMAGE_EXPLOSIVE);
        if (match->alive[player] && radius != 0U &&
            match->spawn_shield_ticks[player] == 0U) {
            vox_i32 impulse = weapon == VOX_DIGS_TOOL_CONCUSSION_GRENADE ?
                              65536L : 32768L;
            match->players[player].velocity_x.value_q16 +=
                delta_x < 0 ? -impulse : impulse;
            match->players[player].velocity_y.value_q16 = -impulse;
        }
    }
}

static vox_result digs_fire_melee(vox_digs_match *match, vox_u16 player,
                                  vox_u16 weapon, vox_u32 target_x,
                                  vox_u32 target_y)
{
    const vox_digs_weapon_properties *properties = &digs_weapons[weapon];
    vox_i32 player_x = digs_q16_to_cell(
        match->players[player].position_x.value_q16);
    vox_i32 player_y = digs_q16_to_cell(
        match->players[player].position_y.value_q16);
    vox_i32 delta_x = (vox_i32)target_x - player_x;
    vox_i32 delta_y = (vox_i32)target_y - player_y;
    vox_u16 victim;
    if (digs_abs_i32(delta_x) > DIGS_SCALE(4U) ||
        digs_abs_i32(delta_y) > DIGS_SCALE(4U)) {
        return VOX_ERR_INVALID;
    }
    if (target_x > 0U && target_y > 0U &&
        target_x + 1U < VOX_WORLD_WIDTH &&
        target_y + 1U < VOX_WORLD_HEIGHT) {
        (void)vox_world_blast(&match->world, target_x, target_y,
                              VOX_WORLD_DEPTH - 1U,
                              properties->blast_radius, 120L << 16);
    }
    for (victim = 0U; victim < match->rules.player_count; ++victim) {
        vox_i32 victim_x;
        vox_i32 victim_y;
        if (victim == player || !match->alive[victim]) {
            continue;
        }
        victim_x = digs_q16_to_cell(
            match->players[victim].position_x.value_q16);
        victim_y = digs_q16_to_cell(
            match->players[victim].position_y.value_q16);
        if (digs_abs_i32(victim_x - (vox_i32)target_x) <= 2U &&
            digs_abs_i32(victim_y - (vox_i32)target_y) <= 2U) {
            (void)vox_digs_apply_hit(match, player, victim, weapon,
                                     VOX_DIGS_NO_PART,
                                     properties->damage,
                                     VOX_DIGS_DAMAGE_BLUNT);
        }
    }
    digs_spawn_effect(match, VOX_MAT_METAL,
                      (vox_i32)(target_x << 16),
                      (vox_i32)(target_y << 16), 0L, -16384L, 12U);
    return VOX_OK;
}

vox_result vox_digs_fire_weapon(vox_digs_match *match, vox_u16 player,
                                vox_u16 weapon, vox_u32 target_x,
                                vox_u32 target_y)
{
    const vox_digs_weapon_properties *properties;
    vox_result result;
    vox_u16 spawned;
    int offset;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_active(match, player) ||
        !match->alive[player] || weapon >= VOX_DIGS_TOOL_COUNT ||
        target_x >= VOX_WORLD_WIDTH || target_y >= VOX_WORLD_HEIGHT ||
        match->weapon_cooldown[player] != 0U ||
        (match->rules.weapon_mask & (vox_u16)(1U << weapon)) == 0U) {
        return VOX_ERR_INVALID;
    }
    properties = &digs_weapons[weapon];
    result = VOX_OK;
    if (properties->flags & VOX_DIGS_WEAPON_MELEE) {
        result = digs_fire_melee(match, player, weapon, target_x, target_y);
    } else if (weapon == VOX_DIGS_TOOL_BOILER_SHOTGUN) {
        spawned = 0U;
        for (offset = -2; offset <= 2; ++offset) {
            long pellet_y = (long)target_y + (long)offset;
            if (pellet_y < 0L) {
                pellet_y = 0L;
            }
            if (pellet_y >= (long)VOX_WORLD_HEIGHT) {
                pellet_y = (long)VOX_WORLD_HEIGHT - 1L;
            }
            if (digs_spawn_projectile(match, player, weapon, target_x,
                                      (vox_u32)pellet_y) == VOX_OK) {
                spawned++;
            }
        }
        result = spawned == 0U ? VOX_ERR_CAPACITY : VOX_OK;
    } else {
        result = digs_spawn_projectile(match, player, weapon,
                                       target_x, target_y);
    }
    if (result != VOX_OK) {
        return result;
    }
    match->selected_weapon[player] = weapon;
    match->weapon_cooldown[player] = properties->cooldown_ticks;
    match->aim_x[player] = (vox_u16)target_x;
    match->aim_y[player] = (vox_u16)target_y;
    match->facing_right[player] =
        target_x >= (vox_u32)digs_q16_to_cell(
            match->players[player].position_x.value_q16) ? 1U : 0U;
    digs_end_spawn_shield(match, player);
    digs_emit_event(match, VOX_DIGS_EVENT_WEAPON_FIRE, player,
                    VOX_DIGS_NO_PLAYER, weapon,
                    digs_projectile_material(weapon),
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    properties->damage,
                    (vox_u16)(digs_noise(match->rules.seed, match->tick,
                                         player,
                                         (vox_u32)weapon * 19U) & 31U));
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

static int digs_ai_is_enemy(const vox_digs_match *match, vox_u16 player,
                            vox_u16 candidate)
{
    if (candidate == player || !match->alive[candidate]) {
        return 0;
    }
    if (match->rules.team_mode == VOX_DIGS_MODE_MINERS_VS_MACHINES &&
        digs_same_team(match, player, candidate)) {
        return 0;
    }
    return 1;
}

static int digs_ai_line_of_sight(const vox_world *world,
                                 vox_i32 from_x, vox_i32 from_y,
                                 vox_i32 to_x, vox_i32 to_y)
{
    vox_i32 delta_x = to_x - from_x;
    vox_i32 delta_y = to_y - from_y;
    vox_u32 steps = digs_abs_i32(delta_x);
    vox_u32 step;
    if (digs_abs_i32(delta_y) > steps) {
        steps = digs_abs_i32(delta_y);
    }
    if (steps == 0U) {
        return 1;
    }
    for (step = 1U; step < steps; ++step) {
        vox_i32 x = from_x + digs_div_trunc_positive(
            delta_x * (vox_i32)step, steps);
        vox_i32 y = from_y + digs_div_trunc_positive(
            delta_y * (vox_i32)step, steps);
        if (x < 0 || y < 0 || x >= (vox_i32)VOX_WORLD_WIDTH ||
            y >= (vox_i32)VOX_WORLD_HEIGHT ||
            digs_cell_is_solid(world, (vox_u32)x, (vox_u32)y)) {
            return 0;
        }
    }
    return 1;
}

static vox_u16 digs_ai_heard_target(const vox_digs_match *match,
                                    vox_u16 player, vox_i32 bot_x,
                                    vox_i32 bot_y)
{
    vox_u16 checked = 0U;
    while (checked < match->event_count && checked < 32U) {
        vox_u16 ordinal = (vox_u16)(match->event_count - checked - 1U);
        const vox_digs_event *event = vox_digs_event_get(match, ordinal);
        vox_i32 event_x;
        vox_i32 event_y;
        if (event == 0) {
            break;
        }
        if ((event->type == VOX_DIGS_EVENT_WEAPON_FIRE ||
             event->type == VOX_DIGS_EVENT_EXPLOSION) &&
            event->source != VOX_DIGS_NO_PLAYER &&
            vox_digs_player_is_active(match, event->source) &&
            digs_ai_is_enemy(match, player, event->source)) {
            event_x = digs_q16_to_cell(event->position_x_q16);
            event_y = digs_q16_to_cell(event->position_y_q16);
            if (digs_abs_i32(event_x - bot_x) +
                digs_abs_i32(event_y - bot_y) <= 52U) {
                return event->source;
            }
        }
        checked++;
    }
    return VOX_DIGS_NO_PLAYER;
}

static void digs_ai_set_mode(vox_digs_match *match, vox_u16 player,
                             vox_u16 mode)
{
    vox_digs_ai_state *state = &match->bots[player];
    if (state->mode == mode) {
        return;
    }
    state->mode = mode;
    state->state_ticks = 0U;
    digs_emit_event(match, VOX_DIGS_EVENT_AI_STATE, player,
                    state->target, VOX_DIGS_TOOL_PICK, VOX_MAT_METAL,
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    mode, (vox_u16)(digs_noise(match->rules.seed,
                                               match->tick, player,
                                               mode) & 15U));
}

static vox_u16 digs_ai_weapon(const vox_digs_match *match, vox_u16 player,
                              vox_u32 distance)
{
    vox_u16 preferred;
    vox_u16 attempt;
    if (distance <= DIGS_SCALE(3U)) {
        preferred = (digs_noise(match->rules.seed, match->tick, player,
                                11U) & 1U) ?
                    VOX_DIGS_TOOL_SLEDGE : VOX_DIGS_TOOL_PICK;
    } else if (distance <= DIGS_SCALE(14U)) {
        preferred = VOX_DIGS_TOOL_NAIL_GUN;
    } else if (distance <= DIGS_SCALE(26U)) {
        preferred = VOX_DIGS_TOOL_BOILER_SHOTGUN;
    } else {
        preferred = (digs_noise(match->rules.seed, match->tick, player,
                                29U) & 1U) ?
                    VOX_DIGS_TOOL_CONCUSSION_GRENADE :
                    VOX_DIGS_TOOL_BLAST_CHARGE;
    }
    for (attempt = 0U; attempt < VOX_DIGS_TOOL_COUNT; ++attempt) {
        vox_u16 choice = (vox_u16)((preferred + attempt) %
                                    VOX_DIGS_TOOL_COUNT);
        if (match->rules.weapon_mask & (vox_u16)(1U << choice)) {
            return choice;
        }
    }
    return VOX_DIGS_TOOL_PICK;
}

vox_result vox_digs_bot_think(vox_digs_match *match, vox_u16 player)
{
    vox_digs_ai_state *state;
    vox_u16 target = VOX_DIGS_NO_PLAYER;
    vox_u16 candidate;
    vox_u32 nearest = 0xffffffffU;
    vox_i32 bot_x;
    vox_i32 bot_y;
    vox_i32 goal_x;
    vox_i32 goal_y;
    vox_u16 actions = 0U;
    vox_i16 move_x = 0;
    vox_u16 visible = 0U;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_bot(match, player) || !match->alive[player]) {
        return VOX_ERR_INVALID;
    }
    state = &match->bots[player];
    state->state_ticks++;
    if (state->memory_ticks > 0U) {
        state->memory_ticks--;
    }
    if (state->decision_ticks > 0U) {
        state->decision_ticks--;
        return VOX_OK;
    }
    state->decision_ticks = DIGS_AI_DECISION_TICKS;
    bot_x = digs_q16_to_cell(match->players[player].position_x.value_q16);
    bot_y = digs_q16_to_cell(match->players[player].position_y.value_q16);
    for (candidate = 0U; candidate < match->rules.player_count; ++candidate) {
        vox_i32 other_x;
        vox_i32 other_y;
        vox_u32 distance;
        if (!digs_ai_is_enemy(match, player, candidate)) {
            continue;
        }
        other_x = digs_q16_to_cell(
            match->players[candidate].position_x.value_q16);
        other_y = digs_q16_to_cell(
            match->players[candidate].position_y.value_q16);
        distance = digs_abs_i32(other_x - bot_x) +
                   digs_abs_i32(other_y - bot_y);
        if (distance < nearest && distance <= 84U &&
            digs_ai_line_of_sight(&match->world, bot_x, bot_y,
                                  other_x, other_y)) {
            nearest = distance;
            target = candidate;
        }
    }
    if (target != VOX_DIGS_NO_PLAYER) {
        visible = 1U;
        state->target = target;
        state->memory_ticks = DIGS_AI_MEMORY_TICKS;
        state->last_seen_x_q16 = match->players[target].position_x.value_q16;
        state->last_seen_y_q16 = match->players[target].position_y.value_q16;
    } else {
        target = digs_ai_heard_target(match, player, bot_x, bot_y);
        if (target != VOX_DIGS_NO_PLAYER) {
            state->target = target;
            state->memory_ticks = DIGS_AI_MEMORY_TICKS / 2U;
            state->last_seen_x_q16 =
                match->players[target].position_x.value_q16;
            state->last_seen_y_q16 =
                match->players[target].position_y.value_q16;
        } else if (state->memory_ticks > 0U &&
                   state->target != VOX_DIGS_NO_PLAYER) {
            target = state->target;
        } else {
            state->target = VOX_DIGS_NO_PLAYER;
        }
    }
    if (match->health[player] <= DIGS_AI_RETREAT_HEALTH &&
        target != VOX_DIGS_NO_PLAYER) {
        digs_ai_set_mode(match, player, VOX_DIGS_AI_RETREATING);
    } else if (visible) {
        digs_ai_set_mode(match, player, VOX_DIGS_AI_ATTACKING);
    } else if (target != VOX_DIGS_NO_PLAYER) {
        digs_ai_set_mode(match, player, VOX_DIGS_AI_SEARCHING);
    } else {
        digs_ai_set_mode(match, player, VOX_DIGS_AI_ROAMING);
    }
    goal_x = digs_q16_to_cell(state->last_seen_x_q16);
    goal_y = digs_q16_to_cell(state->last_seen_y_q16);
    if (state->mode == VOX_DIGS_AI_ROAMING) {
        if ((match->players[player].flags & VOX_PHYSICS_BODY_BLOCKED_X) ||
            state->state_ticks > 240U) {
            state->roam_direction = (vox_i16)-state->roam_direction;
            state->state_ticks = 0U;
        }
        move_x = state->roam_direction > 0 ? 24575 : -24575;
        goal_x = bot_x + (state->roam_direction > 0 ? 16L : -16L);
        goal_y = bot_y - 8L;
    } else if (state->mode == VOX_DIGS_AI_RETREATING) {
        move_x = goal_x < bot_x ? 32767 : -32767;
    } else if (goal_x + 2L < bot_x) {
        move_x = -32767;
    } else if (goal_x > bot_x + 2L) {
        move_x = 32767;
    }
    if (state->mode == VOX_DIGS_AI_ATTACKING && nearest < 10U) {
        move_x = goal_x < bot_x ? 24575 : -24575;
    }
    if ((match->players[player].flags & VOX_PHYSICS_BODY_BLOCKED_X) ||
        ((match->tick + (vox_u32)player * 31U) % 181U) == 0U) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_JUMP);
    }
    if (goal_y + 4L < bot_y && match->steam_q16[player] > 8192U) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_STEAM);
    }
    if ((goal_y + 9L < bot_y ||
         (match->players[player].flags & VOX_PHYSICS_BODY_BLOCKED_X)) &&
        state->mode != VOX_DIGS_AI_RETREATING) {
        vox_i32 rope_x = bot_x + (move_x >= 0 ? 14L : -14L);
        vox_i32 rope_y = bot_y - 20L;
        if (rope_x < 0L) {
            rope_x = 0L;
        }
        if (rope_x >= (vox_i32)VOX_WORLD_WIDTH) {
            rope_x = (vox_i32)VOX_WORLD_WIDTH - 1L;
        }
        if (rope_y < 0L) {
            rope_y = 0L;
        }
        match->aim_x[player] = (vox_u16)rope_x;
        match->aim_y[player] = (vox_u16)rope_y;
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_ROPE);
    } else if (target != VOX_DIGS_NO_PLAYER) {
        match->aim_x[player] = (vox_u16)goal_x;
        match->aim_y[player] = (vox_u16)goal_y;
    }
    match->player_actions[player] = actions;
    match->move_x_q15[player] = move_x;
    match->move_y_q15[player] = 0;
    if (match->weapon_cooldown[player] == 0U &&
        visible && ((match->tick + (vox_u32)player * 17U) % 24U) <
                   DIGS_AI_DECISION_TICKS) {
        vox_u16 weapon = digs_ai_weapon(match, player, nearest);
        (void)vox_digs_fire_weapon(match, player, weapon,
                                   (vox_u32)goal_x, (vox_u32)goal_y);
    }
    if (((match->tick + (vox_u32)player * 43U) % 240U) <
        DIGS_AI_DECISION_TICKS) {
        digs_emit_event(match, VOX_DIGS_EVENT_AI_BARK, player, target,
                        match->selected_weapon[player], VOX_MAT_SMOKE,
                        match->players[player].position_x.value_q16,
                        match->players[player].position_y.value_q16,
                        state->mode,
                        (vox_u16)(digs_noise(match->rules.seed,
                                             match->tick, player,
                                             0xBA4BU) & 7U));
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

static int digs_projectile_hits_solid(const vox_world *world,
                                      vox_u32 x, vox_u32 y)
{
    vox_u32 z;
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        const vox_cell *cell = vox_world_cell(world, x, y, z);
        const vox_material_properties *properties;
        if (cell == 0 || cell->material == VOX_MAT_AIR) {
            continue;
        }
        properties = vox_material_get(cell->material);
        if (properties != 0 && (properties->flags & VOX_MATERIAL_SOLID)) {
            return 1;
        }
    }
    return 0;
}

static void digs_deposit_projectile(vox_digs_match *match,
                                    const vox_digs_projectile *projectile,
                                    vox_u32 x, vox_u32 y)
{
    long radius = (long)projectile->blast_radius;
    long offset_y;
    long offset_x;
    vox_u32 z;
    for (offset_y = -radius; offset_y <= radius; ++offset_y) {
        for (offset_x = -radius; offset_x <= radius; ++offset_x) {
            long sample_x = (long)x + offset_x;
            long sample_y = (long)y + offset_y;
            if (sample_x < 0L || sample_y < 0L ||
                sample_x >= (long)VOX_WORLD_WIDTH ||
                sample_y >= (long)VOX_WORLD_HEIGHT ||
                offset_x * offset_x + offset_y * offset_y > radius * radius) {
                continue;
            }
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(
                    &match->world, (vox_u32)sample_x, (vox_u32)sample_y, z);
                if (cell == 0 || cell->material == VOX_MAT_BEDROCK) {
                    continue;
                }
                if (projectile->material == VOX_MAT_SMOKE &&
                    cell->material != VOX_MAT_AIR) {
                    continue;
                }
                if (projectile->material == VOX_MAT_WATER &&
                    cell->material != VOX_MAT_AIR &&
                    cell->material != VOX_MAT_LAVA) {
                    continue;
                }
                (void)vox_world_set(&match->world, (vox_u32)sample_x,
                                    (vox_u32)sample_y, z,
                                    projectile->material,
                                    projectile->material == VOX_MAT_LAVA ?
                                    750L << 16 : 80L << 16);
            }
        }
    }
}

static void digs_detonate_projectile(vox_digs_match *match, vox_u16 slot,
                                     vox_u32 x, vox_u32 y,
                                     vox_u16 hit_player)
{
    vox_digs_projectile projectile = match->projectiles[slot];
    const vox_digs_weapon_properties *properties =
        &digs_weapons[projectile.weapon];
    vox_u16 spark;
    vox_u16 particle_count;
    vox_u16 debris_materials[160];
    vox_u32 explosion_noise = digs_noise(match->rules.seed, match->tick,
                                         slot, projectile.weapon * 97U +
                                         projectile.owner);
    vox_u16 terrain_radius = projectile.blast_radius;
    particle_count = (vox_u16)(projectile.blast_radius *
                               projectile.blast_radius * 4U + 16U);
    if (match->rules.fx_budget == VOX_DIGS_FX_RETRO) {
        particle_count = (vox_u16)(particle_count * 3U / 4U);
    } else if (match->rules.fx_budget == VOX_DIGS_FX_CARNAGE) {
        particle_count = (vox_u16)(particle_count * 3U / 2U);
    }
    if (particle_count > 160U) {
        particle_count = 160U;
    }
    if (terrain_radius > 1U) {
        vox_i32 radius_variation = (vox_i32)(explosion_noise % 3U) - 1L;
        terrain_radius = (vox_u16)((vox_i32)terrain_radius +
                                   radius_variation);
        if (terrain_radius > VOX_BLAST_MAX_RADIUS) {
            terrain_radius = VOX_BLAST_MAX_RADIUS;
        }
    }
    for (spark = 0U; spark < particle_count; ++spark) {
        vox_u32 noise = digs_noise(explosion_noise, spark, x, y);
        vox_u32 span = (vox_u32)terrain_radius * 2U + 5U;
        long sample_x = (long)x +
            (long)((vox_i32)(noise % span) - (vox_i32)(span / 2U));
        long sample_y = (long)y +
            (long)((vox_i32)((noise >> 9) % span) -
                   (vox_i32)(span / 2U));
        const vox_cell *sample = 0;
        if (sample_x >= 0L && sample_y >= 0L &&
            sample_x < (long)VOX_WORLD_WIDTH &&
            sample_y < (long)VOX_WORLD_HEIGHT) {
            sample = vox_world_cell(&match->world, (vox_u32)sample_x,
                                    (vox_u32)sample_y,
                                    VOX_WORLD_DEPTH - 1U);
        }
        debris_materials[spark] = sample != 0 &&
                                  sample->material != VOX_MAT_AIR &&
                                  sample->material != VOX_MAT_BEDROCK ?
                                  sample->material : projectile.material;
    }
    match->projectiles[slot].active = 0U;
    if (match->projectile_count > 0U) {
        match->projectile_count--;
    }
    if ((properties->flags & VOX_DIGS_WEAPON_EXPLOSIVE) &&
        projectile.blast_radius > 0U) {
        vox_u16 lobe;
        vox_u16 lobe_count = (vox_u16)(2U + explosion_noise % 4U);
        (void)vox_world_blast(&match->world, x, y,
                              VOX_WORLD_DEPTH - 1U,
                              terrain_radius, 700L << 16);
        for (lobe = 0U; lobe < lobe_count; ++lobe) {
            vox_u32 noise = digs_noise(explosion_noise, lobe, x, y);
            vox_i32 offset_x = (vox_i32)(noise %
                (terrain_radius + 1U)) - (vox_i32)(terrain_radius / 2U);
            vox_i32 offset_y = (vox_i32)((noise >> 8) %
                (terrain_radius + 1U)) - (vox_i32)(terrain_radius / 2U);
            vox_i32 lobe_x = (vox_i32)x + offset_x;
            vox_i32 lobe_y = (vox_i32)y + offset_y;
            vox_u16 lobe_radius = (vox_u16)(terrain_radius / 3U +
                                             1U + (noise >> 16) % 3U);
            if (lobe_x > 0 && lobe_y > 0 &&
                lobe_x < (vox_i32)VOX_WORLD_WIDTH &&
                lobe_y < (vox_i32)VOX_WORLD_HEIGHT) {
                (void)vox_world_blast(&match->world, (vox_u32)lobe_x,
                                      (vox_u32)lobe_y,
                                      VOX_WORLD_DEPTH - 1U,
                                      lobe_radius, 700L << 16);
            }
        }
        digs_damage_radius(match, projectile.owner, x, y,
                           projectile.blast_radius, projectile.damage,
                           projectile.weapon);
        digs_emit_event(match, VOX_DIGS_EVENT_EXPLOSION,
                        projectile.owner, hit_player, projectile.weapon,
                        projectile.material, (vox_i32)(x << 16),
                        (vox_i32)(y << 16), terrain_radius,
                        (vox_u16)(explosion_noise & 31U));
    } else if (hit_player != VOX_DIGS_NO_PLAYER && projectile.damage > 0U) {
        vox_u16 damage_flags =
            projectile.weapon == VOX_DIGS_TOOL_CINDER_FLASK ?
            VOX_DIGS_DAMAGE_HEAT : VOX_DIGS_DAMAGE_BALLISTIC;
        (void)vox_digs_apply_hit(match, projectile.owner, hit_player,
                                 projectile.weapon, VOX_DIGS_NO_PART,
                                 projectile.damage, damage_flags);
    } else if (projectile.blast_radius > 0U &&
               projectile.weapon == VOX_DIGS_TOOL_BOILER_SHOTGUN) {
        (void)vox_world_blast(&match->world, x, y,
                              VOX_WORLD_DEPTH - 1U,
                              DIGS_SCALE(1U), 100L << 16);
    }
    if (properties->flags & VOX_DIGS_WEAPON_DEPOSIT) {
        digs_deposit_projectile(match, &projectile, x, y);
    }
    for (spark = 0U; spark < particle_count; ++spark) {
        vox_u32 noise = digs_noise(explosion_noise, spark,
                                   projectile.owner, projectile.weapon);
        vox_i32 velocity_x = ((vox_i32)(noise % 25U) - 12L) * 6656L;
        vox_i32 velocity_y = -6144L -
                             (vox_i32)((noise >> 7) % 20U) * 4608L;
        vox_u16 material = debris_materials[spark];
        if (projectile.weapon == VOX_DIGS_TOOL_NAIL_BOMB &&
            (noise & 1U) == 0U) {
            material = VOX_MAT_METAL;
        } else if ((properties->flags & VOX_DIGS_WEAPON_EXPLOSIVE) &&
                   noise % 7U == 0U) {
            material = VOX_MAT_SMOKE;
        } else if ((properties->flags & VOX_DIGS_WEAPON_EXPLOSIVE) &&
                   noise % 17U == 0U) {
            material = VOX_MAT_LAVA;
        }
        digs_spawn_effect_variant(match, material, (vox_i32)(x << 16),
                                  (vox_i32)(y << 16), velocity_x, velocity_y,
                                  (vox_u16)(24U + noise % 72U),
                                  projectile.owner,
                                  (vox_u16)(noise & 31U));
    }
}

static void digs_step_projectiles(vox_digs_match *match)
{
    vox_u16 slot;
    for (slot = 0U; slot < VOX_DIGS_MAX_PROJECTILES; ++slot) {
        vox_digs_projectile *projectile = &match->projectiles[slot];
        const vox_digs_weapon_properties *properties;
        vox_u16 substep;
        int detonated = 0;
        if (!projectile->active) {
            continue;
        }
        properties = &digs_weapons[projectile->weapon];
        if (properties->flags & VOX_DIGS_WEAPON_GRAVITY) {
            projectile->velocity_y_q16 += DIGS_PROJECTILE_GRAVITY_Q16;
        }
        for (substep = 0U; substep < DIGS_PROJECTILE_SUBSTEPS; ++substep) {
            vox_i32 x_cell;
            vox_i32 y_cell;
            vox_u16 player;
            projectile->position_x_q16 += digs_div_trunc_positive(
                projectile->velocity_x_q16, DIGS_PROJECTILE_SUBSTEPS);
            projectile->position_y_q16 += digs_div_trunc_positive(
                projectile->velocity_y_q16, DIGS_PROJECTILE_SUBSTEPS);
            x_cell = digs_q16_to_cell(projectile->position_x_q16);
            y_cell = digs_q16_to_cell(projectile->position_y_q16);
            if (x_cell < 0 || y_cell < 0 ||
                x_cell >= (vox_i32)VOX_WORLD_WIDTH ||
                y_cell >= (vox_i32)VOX_WORLD_HEIGHT) {
                match->projectiles[slot].active = 0U;
                if (match->projectile_count > 0U) {
                    match->projectile_count--;
                }
                detonated = 1;
                break;
            }
            for (player = 0U; player < match->rules.player_count; ++player) {
                vox_i32 delta_x;
                vox_i32 delta_y;
                if (!match->alive[player] ||
                    (player == projectile->owner &&
                     projectile->age_ticks < 2U)) {
                    continue;
                }
                delta_x = match->players[player].position_x.value_q16 -
                          projectile->position_x_q16;
                delta_y = match->players[player].position_y.value_q16 -
                          projectile->position_y_q16;
                if (digs_abs_i32(delta_x) <= 32768U &&
                    digs_abs_i32(delta_y) <= 40960U) {
                    digs_detonate_projectile(match, slot,
                                             (vox_u32)x_cell,
                                             (vox_u32)y_cell, player);
                    detonated = 1;
                    break;
                }
            }
            if (detonated) {
                break;
            }
            if (digs_projectile_hits_solid(&match->world,
                                            (vox_u32)x_cell,
                                            (vox_u32)y_cell)) {
                digs_detonate_projectile(match, slot, (vox_u32)x_cell,
                                         (vox_u32)y_cell,
                                         VOX_DIGS_NO_PLAYER);
                detonated = 1;
                break;
            }
        }
        if (detonated) {
            continue;
        }
        projectile->age_ticks++;
        if (projectile->fuse_ticks > 0U) {
            projectile->fuse_ticks--;
            if (projectile->fuse_ticks == 0U) {
                vox_i32 x_cell = digs_q16_to_cell(
                    projectile->position_x_q16);
                vox_i32 y_cell = digs_q16_to_cell(
                    projectile->position_y_q16);
                if (x_cell >= 0 && y_cell >= 0 &&
                    x_cell < (vox_i32)VOX_WORLD_WIDTH &&
                    y_cell < (vox_i32)VOX_WORLD_HEIGHT) {
                    digs_detonate_projectile(match, slot,
                                             (vox_u32)x_cell,
                                             (vox_u32)y_cell,
                                             VOX_DIGS_NO_PLAYER);
                }
            }
        }
    }
}

static void digs_step_effects(vox_digs_match *match)
{
    vox_u16 slot;
    for (slot = 0U; slot < match->rules.fx_budget; ++slot) {
        vox_digs_effect *effect = &match->effects[slot];
        vox_i32 x_cell;
        vox_i32 y_cell;
        int terrain_hit = 0;
        if (!effect->active) {
            continue;
        }
        if (effect->material != VOX_MAT_SMOKE) {
            effect->velocity_y_q16 += 2048L;
        }
        effect->position_x_q16 += effect->velocity_x_q16;
        effect->position_y_q16 += effect->velocity_y_q16;
        if (effect->ttl_ticks > 0U) {
            effect->ttl_ticks--;
        }
        x_cell = digs_q16_to_cell(effect->position_x_q16);
        y_cell = digs_q16_to_cell(effect->position_y_q16);
        if (x_cell >= 0 && y_cell >= 0 &&
            x_cell < (vox_i32)VOX_WORLD_WIDTH &&
            y_cell < (vox_i32)VOX_WORLD_HEIGHT &&
            effect->material != VOX_MAT_SMOKE &&
            digs_projectile_hits_solid(&match->world, (vox_u32)x_cell,
                                       (vox_u32)y_cell)) {
            terrain_hit = 1;
            effect->ttl_ticks = 0U;
        }
        if (effect->ttl_ticks == 0U || x_cell < 0 || y_cell < 0 ||
            x_cell >= (vox_i32)VOX_WORLD_WIDTH ||
            y_cell >= (vox_i32)VOX_WORLD_HEIGHT) {
            if (terrain_hit) {
                while (y_cell > 0 &&
                       digs_projectile_hits_solid(&match->world,
                                                  (vox_u32)x_cell,
                                                  (vox_u32)y_cell)) {
                    y_cell--;
                }
            }
            if (effect->ttl_ticks == 0U && x_cell >= 0 && y_cell >= 0 &&
                x_cell < (vox_i32)VOX_WORLD_WIDTH &&
                y_cell < (vox_i32)VOX_WORLD_HEIGHT &&
                (effect->material == VOX_MAT_BLOOD ||
                 effect->material == VOX_MAT_SMOKE ||
                 effect->material == VOX_MAT_SOIL ||
                 effect->material == VOX_MAT_STONE ||
                 effect->material == VOX_MAT_COAL ||
                 effect->material == VOX_MAT_SAND ||
                 effect->material == VOX_MAT_BIOMASS ||
                 effect->material == VOX_MAT_FLESH)) {
                const vox_cell *cell = vox_world_cell(
                    &match->world, (vox_u32)x_cell, (vox_u32)y_cell,
                    VOX_WORLD_DEPTH - 1U);
                if (cell != 0 && cell->material == VOX_MAT_AIR) {
                    (void)vox_world_set(&match->world, (vox_u32)x_cell,
                                        (vox_u32)y_cell,
                                        VOX_WORLD_DEPTH - 1U,
                                        effect->material,
                                        effect->material == VOX_MAT_BLOOD ?
                                        37L << 16 : 80L << 16);
                }
            }
            effect->active = 0U;
            if (match->effect_count > 0U) {
                match->effect_count--;
            }
        }
    }
}

static void digs_step_bleeding(vox_digs_match *match)
{
    vox_u16 player;
    for (player = 0U; player < match->rules.player_count; ++player) {
        vox_u16 part;
        vox_u16 total_rate = 0U;
        vox_u16 damage;
        if (!match->alive[player]) {
            continue;
        }
        for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
            total_rate = (vox_u16)(total_rate +
                match->anatomy[player][part].bleed_rate_q8);
        }
        if (total_rate == 0U) {
            match->bleed_accumulator_q8[player] = 0U;
            continue;
        }
        match->clot_ticks[player]++;
        if (match->clot_ticks[player] >= 180U) {
            match->clot_ticks[player] = 0U;
            for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
                vox_digs_anatomy_part *anatomy =
                    &match->anatomy[player][part];
                if (anatomy->bleed_rate_q8 > 0U &&
                    !(anatomy->flags & VOX_DIGS_PART_SEVERED)) {
                    anatomy->bleed_rate_q8--;
                    if (anatomy->bleed_rate_q8 == 0U) {
                        anatomy->flags = (vox_u16)(anatomy->flags &
                            (vox_u16)~VOX_DIGS_PART_BLEEDING);
                    }
                }
            }
        }
        if (match->spawn_shield_ticks[player] > 0U) {
            continue;
        }
        if (match->bleed_accumulator_q8[player] >
            (vox_u16)(65535U - total_rate)) {
            match->bleed_accumulator_q8[player] = 65535U;
        } else {
            match->bleed_accumulator_q8[player] = (vox_u16)(
                match->bleed_accumulator_q8[player] + total_rate);
        }
        damage = (vox_u16)(match->bleed_accumulator_q8[player] / 256U);
        match->bleed_accumulator_q8[player] = (vox_u16)(
            match->bleed_accumulator_q8[player] % 256U);
        if (damage > 0U) {
            vox_u16 drops = damage > 8U ? 8U : damage;
            for (part = 0U; part < drops; ++part) {
                vox_u32 noise = digs_noise(match->rules.seed,
                                           match->tick, player,
                                           0xB1EEDU + part);
                digs_spawn_effect_variant(match, VOX_MAT_BLOOD,
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    ((vox_i32)(noise % 9U) - 4L) * 2048L,
                    -4096L - (vox_i32)((noise >> 7) % 5U) * 2048L,
                    (vox_u16)(28U + noise % 36U), player,
                    (vox_u16)(noise & 15U));
            }
            digs_emit_event(match, VOX_DIGS_EVENT_BLEED,
                            match->last_attacker[player], player,
                            VOX_DIGS_TOOL_PICK, VOX_MAT_BLOOD,
                            match->players[player].position_x.value_q16,
                            match->players[player].position_y.value_q16,
                            damage, (vox_u16)(total_rate & 31U));
            if (damage < match->health[player]) {
                match->health[player] = (vox_u16)(match->health[player] -
                                                   damage);
            } else if (match->last_attacker[player] != VOX_DIGS_NO_PLAYER &&
                       match->last_attacker[player] != player) {
                (void)vox_digs_record_kill(match,
                                           match->last_attacker[player],
                                           player);
            } else {
                digs_environment_defeat(match, player);
            }
        }
    }
}

static void digs_step_reactions(vox_digs_match *match)
{
    vox_u16 sample;
    for (sample = 0U; sample < DIGS_REACTION_SAMPLES; ++sample) {
        vox_u32 noise = digs_noise(match->rules.seed, match->world.tick,
                                   sample, 0xB100D5U);
        vox_u32 x = noise % VOX_WORLD_WIDTH;
        vox_u32 y = (noise >> 10) % VOX_WORLD_HEIGHT;
        vox_u32 z = (noise >> 20) % VOX_WORLD_DEPTH;
        const vox_cell *cell = vox_world_cell(&match->world, x, y, z);
        const vox_cell *below;
        if (cell == 0 || cell->material != VOX_MAT_BLOOD ||
            y + 1U >= VOX_WORLD_HEIGHT) {
            continue;
        }
        below = vox_world_cell(&match->world, x, y + 1U, z);
        if (below != 0 && below->material == VOX_MAT_LAVA) {
            (void)vox_world_set(&match->world, x, y, z,
                                VOX_MAT_SMOKE, 220L << 16);
        } else if (below != 0 && below->material == VOX_MAT_WATER &&
                   (noise & 3U) == 0U) {
            (void)vox_world_set(&match->world, x, y, z,
                                VOX_MAT_WATER, 32L << 16);
        }
    }
}

static void digs_update_lava(vox_digs_match *match)
{
    vox_u16 desired_surface;
    if (match->lava_level_q16 == 0U) {
        return;
    }
    desired_surface = (vox_u16)(DIGS_LAVA_BASIN_TOP -
        (match->lava_level_q16 *
         (DIGS_LAVA_BASIN_TOP - DIGS_SCALE(3U)) / 65535U));
    while (match->lava_surface_y > desired_surface) {
        vox_u32 x;
        vox_u32 z;
        match->lava_surface_y--;
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(
                    &match->world, x, match->lava_surface_y, z);
                if (cell != 0 && cell->material != VOX_MAT_BEDROCK) {
                    (void)vox_world_set(&match->world, x,
                                        match->lava_surface_y, z,
                                        VOX_MAT_LAVA, 750L << 16);
                }
            }
        }
    }
}

static void digs_apply_lava_hazards(vox_digs_match *match)
{
    vox_u16 player;
    for (player = 0U; player < match->rules.player_count; ++player) {
        vox_i32 foot_y;
        vox_i32 x_cell;
        int touching_lava = 0;
        vox_u32 z;
        if (!match->alive[player]) {
            continue;
        }
        foot_y = digs_q16_to_cell(
            match->players[player].position_y.value_q16 +
            match->players[player].half_height_q16);
        x_cell = digs_q16_to_cell(
            match->players[player].position_x.value_q16);
        if (foot_y >= (vox_i32)match->lava_surface_y) {
            touching_lava = 1;
        } else if (x_cell >= 0 && foot_y >= 0 &&
                   x_cell < (vox_i32)VOX_WORLD_WIDTH &&
                   foot_y < (vox_i32)VOX_WORLD_HEIGHT) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(
                    &match->world, (vox_u32)x_cell, (vox_u32)foot_y, z);
                if (cell != 0 && cell->material == VOX_MAT_LAVA) {
                    touching_lava = 1;
                    break;
                }
            }
        }
        if (touching_lava) {
            (void)vox_digs_apply_hit(match, VOX_DIGS_NO_PLAYER, player,
                                     VOX_DIGS_TOOL_CINDER_FLASK,
                                     VOX_DIGS_NO_PART, 12U,
                                     VOX_DIGS_DAMAGE_HEAT);
            if (match->alive[player]) {
                digs_spawn_effect(match, VOX_MAT_SMOKE,
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    0L, -24576L, 28U);
            }
        }
    }
}

vox_u32 vox_digs_hash(const vox_digs_match *match)
{
    vox_u32 hash = 2166136261U;
    vox_u16 i;
    if (match == 0) {
        return 0U;
    }
    hash = digs_hash_mix(hash, match->rules.seed);
    hash = digs_hash_mix(hash, match->rules.match_ticks);
    hash = digs_hash_mix(hash, match->rules.score_limit);
    hash = digs_hash_mix(hash, match->rules.lava_start_tick);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.player_count);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.bot_mask);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.team_mode);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.map_style);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.weapon_mask);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.fx_budget);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.friendly_fire);
    hash = digs_hash_mix(hash, VOX_DIGS_MAP_GENERATOR_VERSION);
    hash = digs_hash_mix(hash, match->tick);
    hash = digs_hash_mix(hash, (vox_u32)match->phase);
    hash = digs_hash_mix(hash, match->lava_level_q16);
    hash = digs_hash_mix(hash, (vox_u32)match->lava_surface_y);
    hash = digs_hash_mix(hash, (vox_u32)match->projectile_count);
    hash = digs_hash_mix(hash, (vox_u32)match->effect_count);
    hash = digs_hash_mix(hash, (vox_u32)match->effect_cursor);
    hash = digs_hash_mix(hash, match->terrain_hash);
    hash = digs_hash_mix(hash, vox_world_hash(&match->world));
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.gravity_q16);
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.max_speed_q16);
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.max_step_q16);
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.max_substeps);
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        hash = digs_hash_mix(hash, (vox_u32)match->scores[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->alive[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->health[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->deaths[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->respawn_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->spawn_shield_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->player_actions[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->previous_actions[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->aim_x[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->aim_y[i]);
        hash = digs_hash_mix(hash, (vox_u32)(vox_i32)match->move_x_q15[i]);
        hash = digs_hash_mix(hash, (vox_u32)(vox_i32)match->move_y_q15[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->coyote_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->jump_buffer_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->jump_hold_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->steam_q16[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->weapon_cooldown[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->selected_weapon[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->facing_right[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->last_attacker[i]);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->bleed_accumulator_q8[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->clot_ticks[i]);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].position_x.value_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].position_y.value_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].velocity_x.value_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].velocity_y.value_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].half_width_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].half_height_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->players[i].flags);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].anchor_x_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].anchor_y_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].length_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].tension_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].active);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].integrity);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].mode);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].target);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].memory_ticks);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].state_ticks);
        hash = digs_hash_mix(hash,
                             (vox_u32)(vox_i32)match->bots[i].roam_direction);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].decision_ticks);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->bots[i].last_seen_x_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->bots[i].last_seen_y_q16);
        {
            vox_u16 part;
            for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
                const vox_digs_anatomy_part *anatomy =
                    &match->anatomy[i][part];
                hash = digs_hash_mix(hash, (vox_u32)anatomy->health);
                hash = digs_hash_mix(hash, (vox_u32)anatomy->max_health);
                hash = digs_hash_mix(hash, (vox_u32)anatomy->flags);
                hash = digs_hash_mix(hash,
                                     (vox_u32)anatomy->bleed_rate_q8);
            }
        }
    }
    for (i = 0U; i < VOX_DIGS_MAX_PROJECTILES; ++i) {
        const vox_digs_projectile *projectile = &match->projectiles[i];
        hash = digs_hash_mix(hash, (vox_u32)projectile->active);
        if (projectile->active) {
            hash = digs_hash_mix(hash, (vox_u32)projectile->position_x_q16);
            hash = digs_hash_mix(hash, (vox_u32)projectile->position_y_q16);
            hash = digs_hash_mix(hash, (vox_u32)projectile->velocity_x_q16);
            hash = digs_hash_mix(hash, (vox_u32)projectile->velocity_y_q16);
            hash = digs_hash_mix(hash, (vox_u32)projectile->owner);
            hash = digs_hash_mix(hash, (vox_u32)projectile->weapon);
            hash = digs_hash_mix(hash, (vox_u32)projectile->material);
            hash = digs_hash_mix(hash, (vox_u32)projectile->fuse_ticks);
            hash = digs_hash_mix(hash, (vox_u32)projectile->age_ticks);
            hash = digs_hash_mix(hash, (vox_u32)projectile->damage);
            hash = digs_hash_mix(hash, (vox_u32)projectile->blast_radius);
        }
    }
    for (i = 0U; i < match->rules.fx_budget; ++i) {
        const vox_digs_effect *effect = &match->effects[i];
        hash = digs_hash_mix(hash, (vox_u32)effect->active);
        if (effect->active) {
            hash = digs_hash_mix(hash, (vox_u32)effect->position_x_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->position_y_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->velocity_x_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->velocity_y_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->material);
            hash = digs_hash_mix(hash, (vox_u32)effect->ttl_ticks);
            hash = digs_hash_mix(hash, (vox_u32)effect->variant);
            hash = digs_hash_mix(hash, (vox_u32)effect->source);
        }
    }
    return hash;
}

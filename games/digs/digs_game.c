/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_game.h"

#define DIGS_DENSITY_SCALE 2U
#define DIGS_SCALE(value) ((value) * DIGS_DENSITY_SCALE)
#define DIGS_RUN_SPEED_Q16 32768L
#define DIGS_JUMP_SPEED_Q16 (-98304L)
#define DIGS_STEAM_ACCEL_Q16 16384L
#define DIGS_MAX_VERTICAL_SPEED_Q16 (8L << 16)
#define DIGS_STEAM_USE_Q16 1024U
#define DIGS_STEAM_RECHARGE_Q16 512U
#define DIGS_PROJECTILE_SUBSTEPS 4U
#define DIGS_PROJECTILE_GRAVITY_Q16 6144L
#define DIGS_REACTION_SAMPLES 128U

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

static vox_u32 digs_surface_y(vox_u32 seed, vox_u32 x, vox_u16 map_style)
{
    vox_u32 anchor = x / DIGS_SCALE(8U);
    vox_u32 offset = x % DIGS_SCALE(8U);
    vox_u32 left_noise = digs_noise(seed, anchor, 0U,
                                    (vox_u32)map_style + 1U);
    vox_u32 right_noise = digs_noise(seed, anchor + 1U, 0U,
                                     (vox_u32)map_style + 1U);
    vox_u32 amplitude = VOX_WORLD_HEIGHT / 16U;
    vox_u32 left = VOX_WORLD_HEIGHT / 2U +
                   (left_noise % (amplitude + 1U));
    vox_u32 right = VOX_WORLD_HEIGHT / 2U +
                    (right_noise % (amplitude + 1U));
    if (right >= left) {
        return left + ((right - left) * offset) / DIGS_SCALE(8U);
    }
    return left - ((left - right) * offset) / DIGS_SCALE(8U);
}

static vox_u16 digs_map_material(vox_u16 map_style, vox_u32 seed,
                                  vox_u32 x, vox_u32 y)
{
    vox_u32 surface = digs_surface_y(seed, x, map_style);
    vox_u32 noise = digs_noise(seed, x, y, (vox_u32)map_style + 17U);
    if (y >= VOX_WORLD_HEIGHT - DIGS_SCALE(2U)) {
        return VOX_MAT_BEDROCK;
    }
    if (map_style == VOX_DIGS_MAP_COAL_RIDGE) {
        if (y < surface) {
            return VOX_MAT_AIR;
        }
        if (y == surface) {
            return (noise % 9U) < 2U ? VOX_MAT_SAND : VOX_MAT_BIOMASS;
        }
        if (y <= surface + DIGS_SCALE(2U) && (noise % 31U) == 0U) {
            return VOX_MAT_SAND;
        }
        if (y > surface + DIGS_SCALE(3U) && (noise % 17U) == 0U) {
            return VOX_MAT_COAL;
        }
        return y > surface + DIGS_SCALE(20U) ? VOX_MAT_STONE : VOX_MAT_SOIL;
    }
    if (map_style == VOX_DIGS_MAP_DEEPWORKS) {
        if (y < surface) {
            return VOX_MAT_AIR;
        }
        if (y > surface + DIGS_SCALE(7U) &&
            y < VOX_WORLD_HEIGHT - DIGS_SCALE(5U) &&
            (noise % 19U) == 0U) {
            return (digs_noise(seed, x, y, 91U) % 5U) == 0U ?
                   VOX_MAT_FIREDAMP : VOX_MAT_AIR;
        }
        if (y == surface) {
            return VOX_MAT_SOIL;
        }
        if ((noise % 13U) == 0U) {
            return VOX_MAT_COAL;
        }
        return VOX_MAT_STONE;
    }
    if ((x % DIGS_SCALE(24U)) < DIGS_SCALE(2U) &&
        y + DIGS_SCALE(12U) >= surface && y <= surface) {
        return VOX_MAT_METAL;
    }
    if (y >= surface) {
        if (y == surface && (x % DIGS_SCALE(12U)) < DIGS_SCALE(7U)) {
            return VOX_MAT_METAL;
        }
        if (y == surface && (x % DIGS_SCALE(12U)) >= DIGS_SCALE(9U)) {
            return VOX_MAT_SAND;
        }
        if ((noise % 23U) == 0U) {
            return VOX_MAT_COAL;
        }
        return y > surface + DIGS_SCALE(14U) ? VOX_MAT_STONE : VOX_MAT_SOIL;
    }
    return VOX_MAT_AIR;
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

static void digs_apply_player_controls(vox_digs_match *match, vox_u16 player)
{
    vox_physics_body *body = &match->players[player];
    vox_u16 actions = match->player_actions[player];
    if ((actions & (VOX_DIGS_ACTION_LEFT | VOX_DIGS_ACTION_RIGHT)) ==
        VOX_DIGS_ACTION_LEFT) {
        body->velocity_x.value_q16 = -DIGS_RUN_SPEED_Q16;
        match->facing_right[player] = 0U;
    } else if ((actions & (VOX_DIGS_ACTION_LEFT | VOX_DIGS_ACTION_RIGHT)) ==
               VOX_DIGS_ACTION_RIGHT) {
        body->velocity_x.value_q16 = DIGS_RUN_SPEED_Q16;
        match->facing_right[player] = 1U;
    } else {
        body->velocity_x.value_q16 = 0;
    }
    if ((actions & VOX_DIGS_ACTION_JUMP) &&
        (body->flags & VOX_PHYSICS_BODY_GROUNDED)) {
        body->velocity_y.value_q16 = DIGS_JUMP_SPEED_Q16;
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
    rules->bot_count = 1U;
    rules->team_mode = 0U;
    rules->map_style = VOX_DIGS_MAP_COAL_RIDGE;
    rules->weapon_mask = (vox_u16)((1U << VOX_DIGS_TOOL_COUNT) - 1U);
    rules->seed = 0x564F5831U;
}

static vox_result digs_validate_rules(const vox_digs_rules *rules)
{
    if (rules == 0 || rules->abi_version != VOX_ABI_VERSION ||
        rules->struct_size < (vox_u32)sizeof(*rules)) {
        return VOX_ERR_INVALID;
    }
    if (rules->match_ticks == 0U || rules->score_limit == 0U ||
        rules->score_limit > 65535U ||
        rules->lava_start_tick >= rules->match_ticks ||
        rules->bot_count > VOX_DIGS_MAX_BOTS || rules->team_mode > 1U ||
        rules->map_style >= VOX_DIGS_MAP_COUNT || rules->weapon_mask == 0U ||
        (rules->weapon_mask &
         (vox_u16)~((1U << VOX_DIGS_TOOL_COUNT) - 1U)) != 0U) {
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
    match->lava_surface_y =
        (vox_u16)(VOX_WORLD_HEIGHT - DIGS_SCALE(2U));
    match->projectile_count = 0U;
    match->effect_count = 0U;
    match->effect_cursor = 0U;
    match->terrain_hash = vox_world_hash(&match->world);
    vox_physics_step_config_default(&match->physics_config);
    match->physics_config.gravity_q16 *= (vox_i32)DIGS_DENSITY_SCALE;
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        match->scores[i] = 0U;
        match->alive[i] = i <= rules->bot_count ? 1U : 0U;
        match->health[i] = match->alive[i] ? VOX_DIGS_MAX_HEALTH : 0U;
        match->deaths[i] = 0U;
        match->respawn_ticks[i] = 0U;
        match->player_actions[i] = 0U;
        match->steam_q16[i] = 65535U;
        match->weapon_cooldown[i] = 0U;
        match->selected_weapon[i] = VOX_DIGS_TOOL_PICK;
        match->facing_right[i] = (vox_u16)(i < 2U ? 1U : 0U);
        match->last_attacker[i] = VOX_DIGS_NO_PLAYER;
        vox_physics_body_init(&match->players[i]);
        if (match->alive[i] &&
            digs_spawn_player(match, i,
                              (vox_u32)(i + 1U) * VOX_WORLD_WIDTH /
                              (vox_u32)(rules->bot_count + 2U)) != VOX_OK) {
            return VOX_ERR_CAPACITY;
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
        if (match->weapon_cooldown[i] > 0U) {
            match->weapon_cooldown[i]--;
        }
        if (!match->alive[i] && i <= match->rules.bot_count &&
            match->respawn_ticks[i] > 0U) {
            match->respawn_ticks[i]--;
            if (match->respawn_ticks[i] == 0U) {
                if (digs_spawn_player(match, i,
                                      (vox_u32)(i + 1U) * VOX_WORLD_WIDTH /
                                      (vox_u32)(match->rules.bot_count + 2U)) ==
                    VOX_OK) {
                    match->alive[i] = 1U;
                    match->health[i] = VOX_DIGS_MAX_HEALTH;
                    match->steam_q16[i] = 65535U;
                    match->last_attacker[i] = VOX_DIGS_NO_PLAYER;
                } else {
                    match->respawn_ticks[i] = 30U;
                }
            }
        }
    }
    for (i = 1U; i <= match->rules.bot_count; ++i) {
        if (match->phase != VOX_DIGS_RUNNING) {
            break;
        }
        if (match->alive[i] && vox_digs_bot_think(match, i) != VOX_OK) {
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
                                  (vox_u32)(match->rules.bot_count + 2U)) !=
                VOX_OK) {
                match->alive[i] = 0U;
                match->health[i] = 0U;
                match->respawn_ticks[i] = 30U;
            }
        }
    }
    digs_step_projectiles(match);
    digs_step_effects(match);
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

vox_result vox_digs_record_kill(vox_digs_match *match, vox_u16 killer,
                                vox_u16 victim)
{
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        killer >= VOX_DIGS_MAX_SLOTS || victim >= VOX_DIGS_MAX_SLOTS ||
        killer > match->rules.bot_count || !match->alive[victim] ||
        killer == victim) {
        return VOX_ERR_INVALID;
    }
    match->scores[killer]++;
    match->alive[victim] = 0U;
    match->health[victim] = 0U;
    match->deaths[victim]++;
    match->respawn_ticks[victim] = VOX_DIGS_RESPAWN_TICKS;
    match->player_actions[victim] = 0U;
    match->last_attacker[victim] = killer;
    digs_spawn_effect(match, VOX_MAT_BLOOD,
                      match->players[victim].position_x.value_q16,
                      match->players[victim].position_y.value_q16,
                      -8192L, -24576L, 50U);
    digs_spawn_effect(match, VOX_MAT_BLOOD,
                      match->players[victim].position_x.value_q16,
                      match->players[victim].position_y.value_q16,
                      8192L, -32768L, 55U);
    digs_spawn_effect(match, VOX_MAT_FLESH,
                      match->players[victim].position_x.value_q16,
                      match->players[victim].position_y.value_q16,
                      16384L, -28672L, 64U);
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
        input->player >= VOX_DIGS_MAX_SLOTS ||
        !match->alive[input->player] ||
        (input->actions & (vox_u16)~VOX_DIGS_ACTION_MASK) != 0U) {
        return VOX_ERR_INVALID;
    }
    match->player_actions[input->player] = input->actions;
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_result vox_digs_use_tool(vox_digs_match *match, vox_u16 player,
                             vox_u16 tool, vox_u32 x, vox_u32 y, vox_u32 z)
{
    const vox_cell *target;
    vox_result result;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        player >= VOX_DIGS_MAX_SLOTS || !match->alive[player] ||
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
    vox_u16 search;
    vox_u16 slot = match->effect_cursor;
    int found = 0;
    for (search = 0U; search < VOX_DIGS_MAX_EFFECTS; ++search) {
        vox_u16 candidate = (vox_u16)((match->effect_cursor + search) %
                                       VOX_DIGS_MAX_EFFECTS);
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
    match->effects[slot].reserved = 0U;
    match->effect_cursor = (vox_u16)((slot + 1U) % VOX_DIGS_MAX_EFFECTS);
}

static void digs_environment_defeat(vox_digs_match *match, vox_u16 victim)
{
    match->alive[victim] = 0U;
    match->health[victim] = 0U;
    match->deaths[victim]++;
    match->respawn_ticks[victim] = VOX_DIGS_RESPAWN_TICKS;
    match->player_actions[victim] = 0U;
    match->last_attacker[victim] = VOX_DIGS_NO_PLAYER;
    digs_spawn_effect(match, VOX_MAT_BLOOD,
                      match->players[victim].position_x.value_q16,
                      match->players[victim].position_y.value_q16,
                      0L, -32768L, 60U);
    digs_spawn_effect(match, VOX_MAT_FLESH,
                      match->players[victim].position_x.value_q16,
                      match->players[victim].position_y.value_q16,
                      -12288L, -28672L, 64U);
}

vox_result vox_digs_apply_damage(vox_digs_match *match, vox_u16 attacker,
                                 vox_u16 victim, vox_u16 damage)
{
    vox_u16 particle_count;
    vox_u16 i;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        victim >= VOX_DIGS_MAX_SLOTS || victim > match->rules.bot_count ||
        !match->alive[victim] || damage == 0U ||
        (attacker != VOX_DIGS_NO_PLAYER &&
         (attacker >= VOX_DIGS_MAX_SLOTS ||
          attacker > match->rules.bot_count))) {
        return VOX_ERR_INVALID;
    }
    if (attacker != VOX_DIGS_NO_PLAYER) {
        match->last_attacker[victim] = attacker;
    }
    particle_count = (vox_u16)(damage / 12U + 1U);
    if (particle_count > 8U) {
        particle_count = 8U;
    }
    for (i = 0U; i < particle_count; ++i) {
        vox_i32 spread_x = (vox_i32)((int)(i % 3U) - 1) * 12288L;
        vox_i32 spread_y = -16384L - (vox_i32)(i * 3072U);
        digs_spawn_effect(match, VOX_MAT_BLOOD,
                          match->players[victim].position_x.value_q16,
                          match->players[victim].position_y.value_q16,
                          spread_x, spread_y, (vox_u16)(36U + i * 3U));
    }
    if (damage < match->health[victim]) {
        match->health[victim] = (vox_u16)(match->health[victim] - damage);
    } else if (attacker != VOX_DIGS_NO_PLAYER && attacker != victim) {
        if (vox_digs_record_kill(match, attacker, victim) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    } else if (match->last_attacker[victim] != VOX_DIGS_NO_PLAYER &&
               match->last_attacker[victim] != victim) {
        if (vox_digs_record_kill(match, match->last_attacker[victim],
                                 victim) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    } else {
        digs_environment_defeat(match, victim);
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
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
    for (player = 0U; player <= match->rules.bot_count; ++player) {
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
        (void)vox_digs_apply_damage(match, attacker, player, dealt);
        if (match->alive[player] && radius != 0U) {
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
    for (victim = 0U; victim <= match->rules.bot_count; ++victim) {
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
            (void)vox_digs_apply_damage(match, player, victim,
                                        properties->damage);
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
        player >= VOX_DIGS_MAX_SLOTS || player > match->rules.bot_count ||
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
    match->facing_right[player] =
        target_x >= (vox_u32)digs_q16_to_cell(
            match->players[player].position_x.value_q16) ? 1U : 0U;
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_result vox_digs_bot_think(vox_digs_match *match, vox_u16 player)
{
    vox_u16 target = VOX_DIGS_NO_PLAYER;
    vox_u16 candidate;
    vox_u32 nearest = 0xffffffffU;
    vox_i32 bot_x;
    vox_i32 bot_y;
    vox_i32 target_x;
    vox_i32 target_y;
    vox_u16 actions = 0U;
    vox_u16 weapon;
    vox_u16 attempt;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING || player == 0U ||
        player > match->rules.bot_count || !match->alive[player]) {
        return VOX_ERR_INVALID;
    }
    bot_x = digs_q16_to_cell(match->players[player].position_x.value_q16);
    bot_y = digs_q16_to_cell(match->players[player].position_y.value_q16);
    for (candidate = 0U; candidate <= match->rules.bot_count; ++candidate) {
        vox_i32 other_x;
        vox_u32 distance;
        if (candidate == player || !match->alive[candidate]) {
            continue;
        }
        other_x = digs_q16_to_cell(
            match->players[candidate].position_x.value_q16);
        distance = digs_abs_i32(other_x - bot_x);
        if (distance < nearest) {
            nearest = distance;
            target = candidate;
        }
    }
    if (target == VOX_DIGS_NO_PLAYER) {
        match->player_actions[player] = 0U;
        match->state_hash = vox_digs_hash(match);
        return VOX_OK;
    }
    target_x = digs_q16_to_cell(match->players[target].position_x.value_q16);
    target_y = digs_q16_to_cell(match->players[target].position_y.value_q16);
    if (target_x + 1L < bot_x) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_LEFT);
    } else if (target_x > bot_x + 1L) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_RIGHT);
    }
    if ((match->players[player].flags & VOX_PHYSICS_BODY_BLOCKED_X) ||
        ((match->tick + (vox_u32)player * 31U) % 151U) == 0U) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_JUMP);
    }
    if (target_y + 3L < bot_y && match->steam_q16[player] > 8192U) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_STEAM);
    }
    match->player_actions[player] = actions;
    if (match->weapon_cooldown[player] == 0U &&
        ((match->tick + (vox_u32)player * 17U) % 24U) == 0U) {
        weapon = (vox_u16)(((match->tick / 120U) +
                            (vox_u32)player * 3U) % VOX_DIGS_TOOL_COUNT);
        for (attempt = 0U; attempt < VOX_DIGS_TOOL_COUNT; ++attempt) {
            vox_u16 choice = (vox_u16)((weapon + attempt) %
                                        VOX_DIGS_TOOL_COUNT);
            if (match->rules.weapon_mask & (vox_u16)(1U << choice)) {
                weapon = choice;
                break;
            }
        }
        (void)vox_digs_fire_weapon(match, player, weapon,
                                   (vox_u32)target_x, (vox_u32)target_y);
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
    vox_u16 debris_materials[96];
    particle_count = (vox_u16)(projectile.blast_radius *
                               projectile.blast_radius * 3U + 8U);
    if (particle_count > 96U) {
        particle_count = 96U;
    }
    for (spark = 0U; spark < particle_count; ++spark) {
        long sample_x = (long)x + (long)((int)(spark % 9U) - 4);
        long sample_y = (long)y + (long)((int)((spark * 5U) % 9U) - 4);
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
        (void)vox_world_blast(&match->world, x, y,
                              VOX_WORLD_DEPTH - 1U,
                              projectile.blast_radius, 700L << 16);
        digs_damage_radius(match, projectile.owner, x, y,
                           projectile.blast_radius, projectile.damage,
                           projectile.weapon);
    } else if (hit_player != VOX_DIGS_NO_PLAYER && projectile.damage > 0U) {
        (void)vox_digs_apply_damage(match, projectile.owner, hit_player,
                                    projectile.damage);
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
        vox_i32 velocity_x = (vox_i32)((int)(spark % 11U) - 5) * 11264L;
        vox_i32 velocity_y = -10240L -
                             (vox_i32)((spark * 7U % 9U) * 7168U);
        vox_u16 material = debris_materials[spark];
        if (projectile.weapon == VOX_DIGS_TOOL_NAIL_BOMB &&
            (spark & 1U) == 0U) {
            material = VOX_MAT_METAL;
        } else if ((properties->flags & VOX_DIGS_WEAPON_EXPLOSIVE) &&
                   spark % 7U == 0U) {
            material = VOX_MAT_SMOKE;
        } else if ((properties->flags & VOX_DIGS_WEAPON_EXPLOSIVE) &&
                   spark % 13U == 0U) {
            material = VOX_MAT_LAVA;
        }
        digs_spawn_effect(match, material, (vox_i32)(x << 16),
                          (vox_i32)(y << 16), velocity_x, velocity_y,
                          (vox_u16)(24U + spark % 48U));
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
            for (player = 0U; player <= match->rules.bot_count; ++player) {
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
    for (slot = 0U; slot < VOX_DIGS_MAX_EFFECTS; ++slot) {
        vox_digs_effect *effect = &match->effects[slot];
        vox_i32 x_cell;
        vox_i32 y_cell;
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
        if (effect->ttl_ticks == 0U || x_cell < 0 || y_cell < 0 ||
            x_cell >= (vox_i32)VOX_WORLD_WIDTH ||
            y_cell >= (vox_i32)VOX_WORLD_HEIGHT) {
            if (effect->ttl_ticks == 0U && x_cell >= 0 && y_cell >= 0 &&
                x_cell < (vox_i32)VOX_WORLD_WIDTH &&
                y_cell < (vox_i32)VOX_WORLD_HEIGHT &&
                (effect->material == VOX_MAT_BLOOD ||
                 effect->material == VOX_MAT_SMOKE ||
                 effect->material == VOX_MAT_SOIL ||
                 effect->material == VOX_MAT_STONE ||
                 effect->material == VOX_MAT_COAL ||
                 effect->material == VOX_MAT_SAND ||
                 effect->material == VOX_MAT_BIOMASS)) {
                const vox_cell *cell = vox_world_cell(
                    &match->world, (vox_u32)x_cell, (vox_u32)y_cell,
                    VOX_WORLD_DEPTH - 1U);
                if (cell != 0 && cell->material == VOX_MAT_AIR) {
                    (void)vox_world_set(&match->world, (vox_u32)x_cell,
                                        (vox_u32)y_cell,
                                        VOX_WORLD_DEPTH - 1U,
                                        effect->material, 80L << 16);
                }
            }
            effect->active = 0U;
            if (match->effect_count > 0U) {
                match->effect_count--;
            }
        }
    }
}

static void digs_step_reactions(vox_digs_match *match)
{
    (void)match;
}

static void digs_update_lava(vox_digs_match *match)
{
    vox_u16 desired_surface;
    if (match->lava_level_q16 == 0U) {
        return;
    }
    desired_surface = (vox_u16)((VOX_WORLD_HEIGHT - DIGS_SCALE(3U)) -
        (match->lava_level_q16 *
         (VOX_WORLD_HEIGHT - DIGS_SCALE(3U)) / 65535U));
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
    if (match->lava_level_q16 == 0U) {
        return;
    }
    for (player = 0U; player <= match->rules.bot_count; ++player) {
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
            (void)vox_digs_apply_damage(match, VOX_DIGS_NO_PLAYER,
                                        player, 12U);
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
    hash = digs_hash_mix(hash, (vox_u32)match->rules.bot_count);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.team_mode);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.map_style);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.weapon_mask);
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
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.max_substeps);
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        hash = digs_hash_mix(hash, (vox_u32)match->scores[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->alive[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->health[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->deaths[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->respawn_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->player_actions[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->steam_q16[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->weapon_cooldown[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->selected_weapon[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->facing_right[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->last_attacker[i]);
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
    for (i = 0U; i < VOX_DIGS_MAX_EFFECTS; ++i) {
        const vox_digs_effect *effect = &match->effects[i];
        hash = digs_hash_mix(hash, (vox_u32)effect->active);
        if (effect->active) {
            hash = digs_hash_mix(hash, (vox_u32)effect->position_x_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->position_y_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->velocity_x_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->velocity_y_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->material);
            hash = digs_hash_mix(hash, (vox_u32)effect->ttl_ticks);
        }
    }
    return hash;
}

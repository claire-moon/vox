/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_game.h"

#define DIGS_RUN_SPEED_Q16 16384L
#define DIGS_JUMP_SPEED_Q16 (-49152L)
#define DIGS_STEAM_ACCEL_Q16 8192L
#define DIGS_MAX_VERTICAL_SPEED_Q16 (4L << 16)
#define DIGS_STEAM_USE_Q16 1024U
#define DIGS_STEAM_RECHARGE_Q16 512U

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
    vox_u32 anchor = x / 8U;
    vox_u32 offset = x % 8U;
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
        return left + ((right - left) * offset) / 8U;
    }
    return left - ((left - right) * offset) / 8U;
}

static vox_u16 digs_map_material(vox_u16 map_style, vox_u32 seed,
                                  vox_u32 x, vox_u32 y)
{
    vox_u32 surface = digs_surface_y(seed, x, map_style);
    vox_u32 noise = digs_noise(seed, x, y, (vox_u32)map_style + 17U);
    if (y >= VOX_WORLD_HEIGHT - 2U) {
        return VOX_MAT_BEDROCK;
    }
    if (map_style == VOX_DIGS_MAP_COAL_RIDGE) {
        if (y < surface) {
            return VOX_MAT_AIR;
        }
        if (y == surface) {
            return VOX_MAT_BIOMASS;
        }
        if (y > surface + 3U && (noise % 17U) == 0U) {
            return VOX_MAT_COAL;
        }
        return y > surface + 20U ? VOX_MAT_STONE : VOX_MAT_SOIL;
    }
    if (map_style == VOX_DIGS_MAP_DEEPWORKS) {
        if (y < surface) {
            return VOX_MAT_AIR;
        }
        if (y > surface + 7U && y < VOX_WORLD_HEIGHT - 5U &&
            (noise % 19U) == 0U) {
            return VOX_MAT_AIR;
        }
        if (y == surface) {
            return VOX_MAT_SOIL;
        }
        if ((noise % 13U) == 0U) {
            return VOX_MAT_COAL;
        }
        return VOX_MAT_STONE;
    }
    if ((x % 24U) < 2U && y + 12U >= surface && y <= surface) {
        return VOX_MAT_METAL;
    }
    if (y >= surface) {
        if (y == surface && (x % 12U) < 7U) {
            return VOX_MAT_METAL;
        }
        if ((noise % 23U) == 0U) {
            return VOX_MAT_COAL;
        }
        return y > surface + 14U ? VOX_MAT_STONE : VOX_MAT_SOIL;
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
    } else if ((actions & (VOX_DIGS_ACTION_LEFT | VOX_DIGS_ACTION_RIGHT)) ==
               VOX_DIGS_ACTION_RIGHT) {
        body->velocity_x.value_q16 = DIGS_RUN_SPEED_Q16;
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
    rules->reserved = 0U;
    rules->seed = 0x564F5831U;
}

static vox_result digs_validate_rules(const vox_digs_rules *rules)
{
    if (rules == 0 || rules->abi_version != VOX_ABI_VERSION ||
        rules->struct_size < (vox_u32)sizeof(*rules)) {
        return VOX_ERR_INVALID;
    }
    if (rules->match_ticks == 0U || rules->score_limit == 0U ||
        rules->lava_start_tick >= rules->match_ticks ||
        rules->bot_count > VOX_DIGS_MAX_BOTS || rules->team_mode > 1U ||
        rules->map_style >= VOX_DIGS_MAP_COUNT || rules->reserved != 0U) {
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
    match->terrain_hash = vox_world_hash(&match->world);
    vox_physics_step_config_default(&match->physics_config);
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        match->scores[i] = 0U;
        match->alive[i] = i <= rules->bot_count ? 1U : 0U;
        match->player_actions[i] = 0U;
        match->steam_q16[i] = 65535U;
        vox_physics_body_init(&match->players[i]);
        if (match->alive[i] &&
            digs_spawn_player(match, i,
                              (vox_u32)(i + 1U) * VOX_WORLD_WIDTH /
                              (vox_u32)(rules->bot_count + 2U)) != VOX_OK) {
            return VOX_ERR_CAPACITY;
        }
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
            return VOX_ERR_INVALID;
        }
    }
    if (match->tick >= match->rules.lava_start_tick) {
        remaining = match->rules.match_ticks - match->rules.lava_start_tick;
        match->lava_level_q16 = (match->tick - match->rules.lava_start_tick) *
                                65535U / remaining;
    }
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
        !match->alive[killer] || !match->alive[victim] || killer == victim) {
        return VOX_ERR_INVALID;
    }
    match->scores[killer]++;
    match->alive[victim] = 0U;
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
        result = vox_world_blast(&match->world, x, y, z, 3U, 700L << 16);
    } else if (tool == VOX_DIGS_TOOL_SMOKE_POT) {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_SMOKE,
                               180L << 16);
    } else if (tool == VOX_DIGS_TOOL_CINDER_FLASK) {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_LAVA,
                               700L << 16);
    } else {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_WATER,
                               20L << 16);
    }
    if (result != VOX_OK) {
        return result;
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
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
    hash = digs_hash_mix(hash, VOX_DIGS_MAP_GENERATOR_VERSION);
    hash = digs_hash_mix(hash, match->tick);
    hash = digs_hash_mix(hash, (vox_u32)match->phase);
    hash = digs_hash_mix(hash, match->lava_level_q16);
    hash = digs_hash_mix(hash, match->terrain_hash);
    hash = digs_hash_mix(hash, vox_world_hash(&match->world));
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.gravity_q16);
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.max_speed_q16);
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.max_substeps);
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        hash = digs_hash_mix(hash, (vox_u32)match->scores[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->alive[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->player_actions[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->steam_q16[i]);
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
    return hash;
}

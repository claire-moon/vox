/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_game.h"

static int test_map_generation(void)
{
    static vox_digs_match first;
    static vox_digs_match second;
    static vox_digs_match variant;
    vox_digs_rules rules;
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    vox_digs_rules_classic(&rules);
    rules.seed = 0xC0A1C0DEU;
    if (vox_digs_match_init(&first, &rules) != VOX_OK ||
        vox_digs_match_init(&second, &rules) != VOX_OK ||
        first.terrain_hash != second.terrain_hash ||
        first.terrain_hash != vox_world_hash(&first.world) ||
        first.world.awake_cells != 0U) {
        return 1;
    }
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            const vox_cell *bottom = vox_world_cell(&first.world, x,
                                                     VOX_WORLD_HEIGHT - 1U, z);
            const vox_cell *next_bottom = vox_world_cell(&first.world, x,
                                                          VOX_WORLD_HEIGHT - 2U,
                                                          z);
            if (bottom == 0 || next_bottom == 0 ||
                bottom->material != VOX_MAT_BEDROCK ||
                next_bottom->material != VOX_MAT_BEDROCK) {
                return 2;
            }
        }
    }
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (y = 0U; y < VOX_WORLD_HEIGHT; ++y) {
            for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
                const vox_cell *cell = vox_world_cell(&first.world, x, y, z);
                if (cell == 0 || (cell->material != VOX_MAT_AIR &&
                                  cell->material != VOX_MAT_BEDROCK &&
                                  cell->material != VOX_MAT_STONE &&
                                  cell->material != VOX_MAT_SOIL &&
                                  cell->material != VOX_MAT_COAL &&
                                  cell->material != VOX_MAT_BIOMASS &&
                                  cell->material != VOX_MAT_METAL)) {
                    return 3;
                }
            }
        }
    }
    rules.map_style = VOX_DIGS_MAP_DEEPWORKS;
    if (vox_digs_match_init(&variant, &rules) != VOX_OK ||
        variant.terrain_hash == first.terrain_hash) {
        return 4;
    }
    rules.map_style = VOX_DIGS_MAP_COAL_RIDGE;
    rules.seed ^= 0x11111111U;
    if (vox_digs_match_init(&variant, &rules) != VOX_OK ||
        variant.terrain_hash == first.terrain_hash) {
        return 5;
    }
    return 0;
}

static int run_match(vox_u32 *hash_out)
{
    vox_digs_rules rules;
    static vox_digs_match match;
    vox_u32 i;
    vox_digs_rules_classic(&rules);
    rules.seed = 0xA11CE001U;
    rules.bot_count = 3U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    for (i = 0U; i < rules.match_ticks; ++i) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 2;
        }
        if (i == rules.lava_start_tick && match.lava_level_q16 != 0U) {
            return 3;
        }
        if (i == rules.lava_start_tick + 1U && match.lava_level_q16 == 0U) {
            return 4;
        }
    }
    if (match.phase != VOX_DIGS_RESULTS || match.tick != rules.match_ticks) {
        return 5;
    }
    *hash_out = match.state_hash;
    return 0;
}

static int test_player_physics(void)
{
    static vox_digs_match match;
    vox_digs_rules rules;
    vox_u16 map_style;
    vox_u16 player;
    vox_u32 tick;
    vox_u32 occupied_before;
    vox_u32 terrain_hash;
    vox_digs_rules_classic(&rules);
    rules.bot_count = VOX_DIGS_MAX_BOTS;
    for (map_style = VOX_DIGS_MAP_COAL_RIDGE;
         map_style < VOX_DIGS_MAP_COUNT; ++map_style) {
        rules.map_style = map_style;
        if (vox_digs_match_init(&match, &rules) != VOX_OK) {
            return 1;
        }
        occupied_before = match.world.occupied_cells;
        terrain_hash = match.terrain_hash;
        for (tick = 0U; tick < 120U; ++tick) {
            if (vox_digs_match_step(&match) != VOX_OK) {
                return 2;
            }
        }
        if (match.world.occupied_cells != occupied_before ||
            match.world.awake_cells != 0U || match.terrain_hash != terrain_hash) {
            return 3;
        }
        for (player = 0U; player < VOX_DIGS_MAX_SLOTS; ++player) {
            const vox_physics_body *body = &match.players[player];
            if (!match.alive[player] || body->abi_version != VOX_ABI_VERSION ||
                body->struct_size < (vox_u32)sizeof(*body) ||
                !(body->flags & VOX_PHYSICS_BODY_GROUNDED) ||
                body->position_x.value_q16 <= 0 ||
                body->position_x.value_q16 >=
                    (vox_i32)(VOX_WORLD_WIDTH << 16) ||
                body->position_y.value_q16 <= 0 ||
                body->position_y.value_q16 >=
                    (vox_i32)(VOX_WORLD_HEIGHT << 16)) {
                return 4;
            }
        }
    }
    return 0;
}

static int find_tool_target(const vox_world *world, int require_air,
                            vox_u32 *x_out, vox_u32 *y_out)
{
    vox_u32 x;
    vox_u32 y;
    for (y = 1U; y + 2U < VOX_WORLD_HEIGHT; ++y) {
        for (x = 1U; x + 1U < VOX_WORLD_WIDTH; ++x) {
            const vox_cell *cell = vox_world_cell(world, x, y,
                                                   VOX_WORLD_DEPTH - 1U);
            if (cell == 0) {
                return 0;
            }
            if (require_air ? cell->material == VOX_MAT_AIR :
                              (cell->material != VOX_MAT_AIR &&
                               cell->material != VOX_MAT_BEDROCK)) {
                *x_out = x;
                *y_out = y;
                return 1;
            }
        }
    }
    return 0;
}

static int test_tools(void)
{
    static vox_digs_match match;
    vox_digs_rules rules;
    const vox_cell *cell;
    vox_u32 x;
    vox_u32 y;
    vox_u32 initial_hash;
    vox_digs_rules_classic(&rules);
    rules.bot_count = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        !find_tool_target(&match.world, 0, &x, &y)) {
        return 1;
    }
    initial_hash = match.state_hash;
    if (vox_digs_use_tool(&match, 0U, VOX_DIGS_TOOL_PICK, x, y,
                           VOX_WORLD_DEPTH - 1U) != VOX_OK) {
        return 2;
    }
    cell = vox_world_cell(&match.world, x, y, VOX_WORLD_DEPTH - 1U);
    if (cell == 0 || cell->material != VOX_MAT_AIR ||
        match.state_hash == initial_hash ||
        !find_tool_target(&match.world, 0, &x, &y) ||
        vox_digs_use_tool(&match, 0U, VOX_DIGS_TOOL_BLAST_CHARGE, x, y,
                           VOX_WORLD_DEPTH - 1U) != VOX_OK) {
        return 3;
    }
    if (!find_tool_target(&match.world, 1, &x, &y) ||
        vox_digs_use_tool(&match, 0U, VOX_DIGS_TOOL_SMOKE_POT, x, y,
                           VOX_WORLD_DEPTH - 1U) != VOX_OK) {
        return 4;
    }
    cell = vox_world_cell(&match.world, x, y, VOX_WORLD_DEPTH - 1U);
    if (cell == 0 || cell->material != VOX_MAT_SMOKE ||
        vox_digs_use_tool(&match, 0U, VOX_DIGS_TOOL_CINDER_FLASK, x, y,
                           VOX_WORLD_DEPTH - 1U) != VOX_OK) {
        return 5;
    }
    cell = vox_world_cell(&match.world, x, y, VOX_WORLD_DEPTH - 1U);
    if (cell == 0 || cell->material != VOX_MAT_LAVA ||
        vox_digs_use_tool(&match, 0U, VOX_DIGS_TOOL_PRESSURE_HOSE, x, y,
                           VOX_WORLD_DEPTH - 1U) != VOX_OK) {
        return 6;
    }
    cell = vox_world_cell(&match.world, x, y, VOX_WORLD_DEPTH - 1U);
    if (cell == 0 || cell->material != VOX_MAT_WATER ||
        vox_digs_match_step(&match) != VOX_OK) {
        return 7;
    }
    return 0;
}

static int test_player_controls(void)
{
    static vox_digs_match match;
    vox_digs_rules rules;
    vox_digs_input input;
    vox_i32 start_x;
    vox_i32 start_y;
    vox_u16 steam_before;
    vox_u32 tick;
    vox_digs_rules_classic(&rules);
    rules.bot_count = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    for (tick = 0U; tick < 120U; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 2;
        }
    }
    if (!(match.players[0].flags & VOX_PHYSICS_BODY_GROUNDED)) {
        return 3;
    }
    input.abi_version = VOX_ABI_VERSION;
    input.struct_size = (vox_u32)sizeof(input);
    input.player = 0U;
    start_x = match.players[0].position_x.value_q16;
    input.actions = VOX_DIGS_ACTION_RIGHT;
    if (vox_digs_submit_input(&match, &input) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK ||
        match.players[0].position_x.value_q16 <= start_x) {
        return 4;
    }
    input.actions = 0U;
    if (vox_digs_submit_input(&match, &input) != VOX_OK) {
        return 5;
    }
    for (tick = 0U; tick < 120U; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 6;
        }
    }
    if (!(match.players[0].flags & VOX_PHYSICS_BODY_GROUNDED)) {
        return 7;
    }
    start_y = match.players[0].position_y.value_q16;
    input.actions = VOX_DIGS_ACTION_JUMP;
    if (vox_digs_submit_input(&match, &input) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK ||
        match.players[0].position_y.value_q16 >= start_y) {
        return 8;
    }
    input.actions = 0U;
    if (vox_digs_submit_input(&match, &input) != VOX_OK) {
        return 9;
    }
    for (tick = 0U; tick < 120U; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 10;
        }
    }
    steam_before = match.steam_q16[0];
    start_y = match.players[0].position_y.value_q16;
    input.actions = VOX_DIGS_ACTION_STEAM;
    if (vox_digs_submit_input(&match, &input) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK ||
        match.steam_q16[0] >= steam_before ||
        match.players[0].position_y.value_q16 >= start_y) {
        return 11;
    }
    input.actions = (vox_u16)(VOX_DIGS_ACTION_MASK | 16U);
    if (vox_digs_submit_input(&match, &input) != VOX_ERR_INVALID) {
        return 12;
    }
    return 0;
}

int main(void)
{
    vox_digs_rules rules;
    static vox_digs_match match;
    vox_u32 first;
    vox_u32 second;
    vox_digs_rules_classic(&rules);
    if (rules.match_ticks != 18000U || rules.lava_start_tick != 12600U) {
        fprintf(stderr, "classic timing mismatch\n");
        return 1;
    }
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_record_kill(&match, 0U, 1U) != VOX_OK ||
        match.scores[0] != 1U) {
        fprintf(stderr, "kill attribution mismatch\n");
        return 2;
    }
    if (test_map_generation() != 0) {
        fprintf(stderr, "DIGS map generation mismatch\n");
        return 4;
    }
    if (test_player_physics() != 0) {
        fprintf(stderr, "DIGS player physics mismatch\n");
        return 5;
    }
    if (test_tools() != 0) {
        fprintf(stderr, "DIGS terrain tool mismatch\n");
        return 6;
    }
    if (test_player_controls() != 0) {
        fprintf(stderr, "DIGS player control mismatch\n");
        return 7;
    }
    if (run_match(&first) != 0 || run_match(&second) != 0 || first != second) {
        fprintf(stderr, "DIGS determinism mismatch\n");
        return 3;
    }
    printf("DIGS deterministic hash=%08x\n", (unsigned int)first);
    return 0;
}

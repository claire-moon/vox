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
    if (run_match(&first) != 0 || run_match(&second) != 0 || first != second) {
        fprintf(stderr, "DIGS determinism mismatch\n");
        return 3;
    }
    printf("DIGS deterministic hash=%08x\n", (unsigned int)first);
    return 0;
}

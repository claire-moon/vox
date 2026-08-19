/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>

#include "vox/vox_game.h"

#define V005_ROPE_REACH 48L
#define V005_ROPE_CLEAR 56L

static int saw_event(const vox_digs_match *match, vox_u16 type)
{
    vox_u16 i;
    for (i = 0U; i < match->event_count; ++i) {
        const vox_digs_event *event = &match->events[
            (match->event_head + i) % VOX_DIGS_MAX_EVENTS];
        if (event->type == type) return 1;
    }
    return 0;
}

static vox_u16 count_events(const vox_digs_match *match, vox_u16 type)
{
    vox_u16 i;
    vox_u16 count = 0U;
    for (i = 0U; i < match->event_count; ++i) {
        const vox_digs_event *event = &match->events[
            (match->event_head + i) % VOX_DIGS_MAX_EVENTS];
        if (event->type == type) count++;
    }
    return count;
}

static int cell_overlaps_living_player(const vox_digs_match *match,
                                       vox_u32 x, vox_u32 y)
{
    vox_i32 cell_min_x;
    vox_i32 cell_min_y;
    vox_i32 cell_max_x;
    vox_i32 cell_max_y;
    vox_u16 player;
    if (match == 0) return 0;
    cell_min_x = (vox_i32)x << 16;
    cell_min_y = (vox_i32)y << 16;
    cell_max_x = cell_min_x + 65535L;
    cell_max_y = cell_min_y + 65535L;
    for (player = 0U; player < match->rules.player_count; ++player) {
        const vox_physics_body *body = &match->players[player];
        vox_i32 player_min_x;
        vox_i32 player_min_y;
        vox_i32 player_max_x;
        vox_i32 player_max_y;
        if (!match->alive[player]) continue;
        player_min_x = body->position_x.value_q16 - body->half_width_q16;
        player_min_y = body->position_y.value_q16 - body->half_height_q16;
        player_max_x = body->position_x.value_q16 + body->half_width_q16;
        player_max_y = body->position_y.value_q16 + body->half_height_q16;
        if (cell_max_x >= player_min_x && cell_min_x <= player_max_x &&
            cell_max_y >= player_min_y && cell_min_y <= player_max_y) {
            return 1;
        }
    }
    return 0;
}

static vox_u16 last_bark_stimulus(const vox_digs_match *match,
                                  vox_u16 player)
{
    vox_u16 i;
    vox_u16 stimulus = (vox_u16)VOX_DIGS_STIMULUS_NONE;
    for (i = 0U; i < match->event_count; ++i) {
        const vox_digs_event *event = &match->events[
            (match->event_head + i) % VOX_DIGS_MAX_EVENTS];
        if (event->type == VOX_DIGS_EVENT_AI_BARK &&
            event->source == player) {
            stimulus = event->magnitude;
        }
    }
    return stimulus;
}

/* Arena construction is test-only setup.  It may deliberately replace a
 * generated anchor, but has to do so explicitly now that ordinary world
 * writes reject fixture replacement. */
static vox_result clear_test_cell(vox_world *world, vox_u32 x, vox_u32 y,
                                  vox_u32 z)
{
    if (vox_world_is_fixture(world, x, y, z) &&
        vox_world_set_fixture(world, x, y, z, 0U) != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    return vox_world_set(world, x, y, z, VOX_MAT_AIR, 0L);
}

int main(void)
{
    static vox_digs_match match;
    static vox_digs_match rope_match;
    static vox_digs_match ship_match;
    static vox_digs_match collision_match;
    static vox_digs_match phase_match;
    static vox_digs_match launch_match;
    static vox_digs_match fixture_match;
    static vox_digs_match water_match;
    static vox_digs_match collapse_match;
    static vox_digs_match wide_collapse_match;
    static vox_digs_match impact_match;
    static vox_digs_match bark_match;
    static vox_digs_match ai_match;
    vox_digs_rules rules;
    vox_digs_rules rope_rules;
    vox_digs_rules ship_rules;
    vox_digs_rules launch_rules;
    vox_digs_rules collapse_rules;
    vox_digs_rules wide_collapse_rules;
    vox_digs_rules impact_rules;
    vox_digs_rules bark_rules;
    vox_digs_rules ai_rules;
    vox_digs_rules fixture_rules;
    vox_digs_input input;
    vox_digs_input rope_input;
    vox_digs_input launch_input;
    vox_digs_input bark_input;
    vox_digs_replay_frame replay_frame;
    vox_i32 source_x;
    vox_i32 source_y;
    vox_i32 target_x;
    vox_i32 target_y;
    vox_i32 second_x;
    vox_i32 second_y;
    vox_i32 x;
    vox_i32 y;
    vox_u32 z;
    vox_u16 i;
    vox_u16 body_index;
    vox_u16 settled_cells;
    vox_u16 saw_settle;
    vox_u16 cave_in_events;
    vox_u16 remaining_roof_cells;
    vox_u16 fixture_events;
    vox_u16 scrap_bodies;
    vox_u16 filled_bodies;
    vox_u16 generated_fixture_found;
    vox_u16 stable_metal;
    vox_u16 loose_metal;
    vox_u16 debris_bodies;
    vox_u16 body_index_second;
    vox_u16 anatomy_before;
    vox_i32 first_scrap_x;
    vox_i32 second_scrap_x;
    vox_i32 first_scrap_velocity_x;
    vox_i32 second_scrap_velocity_x;
    vox_u32 generated_fixture_x;
    vox_u32 generated_fixture_y;
    vox_u32 generated_fixture_z;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (VOX_ABI_VERSION != 11U || VOX_DIGS_FX_RETRO != 768U ||
        VOX_DIGS_FX_STANDARD != 1536U || VOX_DIGS_FX_CARNAGE != 3072U) {
        return 92;
    }
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;
    input.abi_version = VOX_ABI_VERSION;
    input.struct_size = (vox_u32)sizeof(input);
    input.player = 0U;
    input.actions = VOX_DIGS_ACTION_DASH;
    input.aim_x = match.aim_x[0];
    input.aim_y = match.aim_y[0];
    input.move_x_q15 = 0;
    input.move_y_q15 = 0;
    input.selected_weapon = VOX_DIGS_TOOL_PICK;
    input.reserved = 0U;
    if (vox_digs_submit_input(&match, &input) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK ||
        match.dash_cooldown[0] == 0U ||
        match.dash_invulnerability[0] == 0U ||
        !saw_event(&match, VOX_DIGS_EVENT_DASH)) return 2;
    match.spawn_shield_ticks[0] = 0U;
    match.spawn_shield_ticks[1] = 0U;
    match.health[0] = 50U;
    if (vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_NAIL_GUN,
                           VOX_DIGS_PART_HEAD, 1U,
                           VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK ||
        match.alive[1] != 0U || match.health[0] != VOX_DIGS_MAX_HEALTH ||
        match.ragdolls.body_count < 2U ||
        match.ragdolls.joint_count < 10U ||
        vox_fluid_cell_get_at(&match.fluids,
            (vox_u16)(match.players[1].position_x.value_q16 >> 16),
            (vox_u16)(match.players[1].position_y.value_q16 >> 16), 0U) != 0 ||
        !saw_event(&match, VOX_DIGS_EVENT_HEADSHOT)) return 3;
    if (vox_world_set(&match.world, 100U, 100U, 0U, VOX_MAT_METAL,
                      20L << 16) != VOX_OK ||
        vox_world_set_fixture(&match.world, 100U, 100U, 0U, 1U) != VOX_OK)
        return 4;
    if (vox_digs_use_tool(&match, 0U, VOX_DIGS_TOOL_FIRECRACKER,
                          100U, 100U, 0U) != VOX_OK) {
        fprintf(stderr, "fixture blast failed\n");
        return 4;
    }
    if (vox_digs_use_tool(&match, 0U, VOX_DIGS_TOOL_CINDER_FLASK,
                          100U, 100U, 0U) != VOX_OK ||
        vox_fluid_cell_get_at(&match.fluids, 100U, 100U, 0U) == 0) return 31;
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        if ((match.ragdolls.bodies[i].flags & VOX_RIGID_BODY_SCRAP) != 0U) {
            break;
        }
    }
    if (i == VOX_RIGID_MAX_BODIES) return 5;
    if (!saw_event(&match, VOX_DIGS_EVENT_FIXTURE_BREAK)) return 32;
    /* Fixtures are static, non-structural rope targets.  Keep one generated
     * target and one hand-built suspended target alive through four seconds
     * of ordinary simulation and a nearby non-qualifying blast. */
    vox_digs_rules_classic(&fixture_rules);
    fixture_rules.player_count = 1U;
    fixture_rules.bot_mask = 0U;
    fixture_rules.score_limit = 0U;
    if (vox_digs_match_init(&fixture_match, &fixture_rules) != VOX_OK) {
        return 80;
    }
    generated_fixture_found = 0U;
    generated_fixture_x = 0U;
    generated_fixture_y = 0U;
    generated_fixture_z = 0U;
    for (y = 0L; y < (vox_i32)VOX_WORLD_HEIGHT &&
         generated_fixture_found == 0U; ++y) {
        for (x = 0L; x < (vox_i32)VOX_WORLD_WIDTH &&
             generated_fixture_found == 0U; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (vox_world_is_fixture(&fixture_match.world, (vox_u32)x,
                                         (vox_u32)y, z)) {
                    generated_fixture_x = (vox_u32)x;
                    generated_fixture_y = (vox_u32)y;
                    generated_fixture_z = z;
                    generated_fixture_found = 1U;
                    break;
                }
            }
        }
    }
    if (generated_fixture_found == 0U) return 81;
    source_x = 64L;
    source_y = 100L;
    if (generated_fixture_x >= 40U && generated_fixture_x <= 80U &&
        generated_fixture_y >= 80U && generated_fixture_y <= 120U) {
        source_x = (vox_i32)VOX_WORLD_WIDTH - 64L;
    }
    for (y = 80L; y <= 120L; ++y) {
        for (x = source_x - 28L; x <= source_x + 16L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&fixture_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 82;
                }
            }
        }
    }
    if (vox_world_set(&fixture_match.world, (vox_u32)source_x,
                      (vox_u32)source_y, 0U,
                      VOX_MAT_METAL, 20L << 16) != VOX_OK ||
        vox_world_set_fixture(&fixture_match.world, (vox_u32)source_x,
                              (vox_u32)source_y, 0U, 1U) != VOX_OK ||
        /* Concussion's radius is ten and its fixture-fracture envelope is
         * twelve.  A nonstructural impact thirteen cells away is therefore
         * a real nearby blast that must leave this fixture untouched. */
        vox_world_set(&fixture_match.world, (vox_u32)(source_x - 13L),
                      (vox_u32)source_y, 0U,
                      VOX_MAT_SMOKE, 20L << 16) != VOX_OK ||
        vox_digs_use_tool(&fixture_match, 0U,
                          VOX_DIGS_TOOL_CONCUSSION_GRENADE,
                          (vox_u32)(source_x - 13L),
                          (vox_u32)source_y, 0U) != VOX_OK ||
        vox_world_sleep_all(&fixture_match.world) != VOX_OK) {
        return 83;
    }
    for (i = 0U; i < 240U; ++i) {
        if (vox_digs_match_step(&fixture_match) != VOX_OK) return 84;
    }
    if (!vox_world_is_fixture(&fixture_match.world, generated_fixture_x,
                              generated_fixture_y, generated_fixture_z) ||
        !vox_world_is_fixture(&fixture_match.world, (vox_u32)source_x,
                              (vox_u32)source_y, 0U) ||
        vox_world_cell(&fixture_match.world, generated_fixture_x,
                       generated_fixture_y, generated_fixture_z)->material !=
            VOX_MAT_METAL ||
        vox_world_cell(&fixture_match.world, (vox_u32)source_x,
                       (vox_u32)source_y, 0U)->material != VOX_MAT_METAL ||
        saw_event(&fixture_match, VOX_DIGS_EVENT_CAVE_IN)) {
        return 85;
    }
    /* Two disconnected components inside one offset blast must become two
     * pieces with distinct starting points and outward velocity.  A
     * connected pair is one component, not two accidental replacement
     * anchors. */
    if (vox_digs_match_init(&fixture_match, &fixture_rules) != VOX_OK) {
        return 86;
    }
    for (y = 80L; y <= 120L; ++y) {
        for (x = 80L; x <= 120L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&fixture_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 87;
                }
            }
        }
    }
    if (vox_world_set(&fixture_match.world, 96U, 100U, 0U,
                      VOX_MAT_STONE, 0L) != VOX_OK ||
        vox_world_set(&fixture_match.world, 101U, 100U, 0U,
                      VOX_MAT_METAL, 0L) != VOX_OK ||
        vox_world_set(&fixture_match.world, 102U, 100U, 0U,
                      VOX_MAT_METAL, 0L) != VOX_OK ||
        vox_world_set(&fixture_match.world, 104U, 100U, 0U,
                      VOX_MAT_METAL, 0L) != VOX_OK ||
        vox_world_set(&fixture_match.world, 105U, 100U, 0U,
                      VOX_MAT_METAL, 0L) != VOX_OK ||
        vox_world_set_fixture(&fixture_match.world, 101U, 100U, 0U, 1U) !=
            VOX_OK ||
        vox_world_set_fixture(&fixture_match.world, 102U, 100U, 0U, 1U) !=
            VOX_OK ||
        vox_world_set_fixture(&fixture_match.world, 104U, 100U, 0U, 1U) !=
            VOX_OK ||
        vox_world_set_fixture(&fixture_match.world, 105U, 100U, 0U, 1U) !=
            VOX_OK ||
        vox_digs_use_tool(&fixture_match, 0U,
                          VOX_DIGS_TOOL_FIRECRACKER,
                          96U, 100U, 0U) != VOX_OK) {
        return 88;
    }
    fixture_events = count_events(&fixture_match, VOX_DIGS_EVENT_FIXTURE_BREAK);
    scrap_bodies = 0U;
    first_scrap_x = 0L;
    second_scrap_x = 0L;
    first_scrap_velocity_x = 0L;
    second_scrap_velocity_x = 0L;
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        if ((fixture_match.ragdolls.bodies[i].flags &
             VOX_RIGID_BODY_SCRAP) != 0U) {
            if (fixture_match.rigid_material[i] != VOX_MAT_METAL ||
                fixture_match.rigid_loose_cells[i] != 2U) return 89;
            if (scrap_bodies == 0U) {
                first_scrap_x =
                    fixture_match.ragdolls.bodies[i].position_x_q16;
                first_scrap_velocity_x =
                    fixture_match.ragdolls.bodies[i].velocity_x_q16;
            } else if (scrap_bodies == 1U) {
                second_scrap_x =
                    fixture_match.ragdolls.bodies[i].position_x_q16;
                second_scrap_velocity_x =
                    fixture_match.ragdolls.bodies[i].velocity_x_q16;
            }
            scrap_bodies++;
        }
    }
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        const vox_cell *first_fixture = vox_world_cell(&fixture_match.world,
                                                        101U, 100U, z);
        const vox_cell *second_fixture = vox_world_cell(&fixture_match.world,
                                                         105U, 100U, z);
        if (first_fixture == 0 || second_fixture == 0 ||
            vox_world_is_fixture(&fixture_match.world, 101U, 100U, z) ||
            vox_world_is_fixture(&fixture_match.world, 102U, 100U, z) ||
            vox_world_is_fixture(&fixture_match.world, 104U, 100U, z) ||
            vox_world_is_fixture(&fixture_match.world, 105U, 100U, z)) {
            return 90;
        }
    }
    if (fixture_events != 2U || scrap_bodies != 2U ||
        first_scrap_x == second_scrap_x || first_scrap_velocity_x <= 0L ||
        second_scrap_velocity_x <= 0L) return 91;
    for (i = 0U; i < 16U; ++i) {
        if (vox_digs_match_step(&fixture_match) != VOX_OK) return 93;
    }
    debris_bodies = 0U;
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        if ((fixture_match.ragdolls.bodies[i].flags &
             VOX_RIGID_BODY_DEBRIS) != 0U) {
            debris_bodies++;
        }
    }
    if (saw_event(&fixture_match, VOX_DIGS_EVENT_CAVE_IN) ||
        debris_bodies != 0U ||
        (fixture_match.awards[0] &
         (1U << VOX_DIGS_AWARD_CAVE_IN_ARTIST)) != 0U) return 94;
    /* Repeat with a full rigid pool to prove overflow removes the fixture
     * without inventing a new anchor, and accounts its source material. */
    filled_bodies = 0U;
    while (vox_rigid_spawn(&fixture_match.ragdolls, &body_index,
                           80L << 16, 80L << 16,
                           8192L, 8192L, 65536L,
                           VOX_RIGID_BODY_DEBRIS) == VOX_OK) {
        filled_bodies++;
    }
    if (filled_bodies == 0U ||
        vox_world_set(&fixture_match.world, 106U, 100U, 0U,
                      VOX_MAT_STONE, 0L) != VOX_OK ||
        vox_world_set(&fixture_match.world, 111U, 100U, 0U,
                      VOX_MAT_METAL, 0L) != VOX_OK ||
        vox_world_set_fixture(&fixture_match.world, 111U, 100U, 0U, 1U) !=
            VOX_OK ||
        vox_digs_use_tool(&fixture_match, 0U,
                          VOX_DIGS_TOOL_FIRECRACKER,
                          106U, 100U, 0U) != VOX_OK) {
        return 95;
    }
    if (vox_world_cell(&fixture_match.world, 111U, 100U, 0U) == 0 ||
        vox_world_cell(&fixture_match.world, 111U, 100U, 0U)->material ==
        VOX_MAT_METAL ||
        vox_world_is_fixture(&fixture_match.world, 111U, 100U, 0U)) return 96;
    fixture_events = count_events(&fixture_match, VOX_DIGS_EVENT_FIXTURE_BREAK);
    scrap_bodies = 0U;
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        if ((fixture_match.ragdolls.bodies[i].flags &
             VOX_RIGID_BODY_SCRAP) != 0U) {
            scrap_bodies++;
        }
    }
    if (fixture_events != 3U || scrap_bodies != 2U ||
        fixture_match.rigid_settle_discarded != 1U) return 97;
    /* A direct blast through a 17-by-10 connected rail proves the private
     * fixture component bound, rather than the smaller terrain-fracture
     * record, owns fixture destruction.  It must become one 170-cell scrap
     * body with a deterministic fallback direction, never a cave-in. */
    if (vox_digs_match_init(&fixture_match, &fixture_rules) != VOX_OK) {
        return 98;
    }
    for (y = 80L; y <= 120L; ++y) {
        for (x = 80L; x <= 130L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&fixture_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 99;
                }
            }
        }
    }
    for (x = 100L; x <= 116L; ++x) {
        for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
            if (vox_world_set(&fixture_match.world, (vox_u32)x, 100U, z,
                              VOX_MAT_METAL, 20L << 16) != VOX_OK ||
                vox_world_set_fixture(&fixture_match.world, (vox_u32)x,
                                      100U, z, 1U) != VOX_OK) {
                return 100;
            }
        }
    }
    if (vox_digs_use_tool(&fixture_match, 0U,
                          VOX_DIGS_TOOL_FIRECRACKER,
                          108U, 100U, 0U) != VOX_OK) {
        return 101;
    }
    scrap_bodies = 0U;
    body_index = VOX_RIGID_MAX_BODIES;
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        if ((fixture_match.ragdolls.bodies[i].flags &
             VOX_RIGID_BODY_SCRAP) != 0U) {
            if (fixture_match.rigid_material[i] != VOX_MAT_METAL ||
                fixture_match.rigid_loose_cells[i] != 170U ||
                (fixture_match.ragdolls.bodies[i].velocity_x_q16 == 0L &&
                 fixture_match.ragdolls.bodies[i].velocity_y_q16 == 0L)) {
                return 102;
            }
            body_index = i;
            scrap_bodies++;
        }
    }
    for (x = 100L; x <= 116L; ++x) {
        for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
            if (vox_world_is_fixture(&fixture_match.world, (vox_u32)x,
                                     100U, z)) {
                return 103;
            }
        }
    }
    if (count_events(&fixture_match, VOX_DIGS_EVENT_FIXTURE_BREAK) != 1U ||
        scrap_bodies != 1U || body_index == VOX_RIGID_MAX_BODIES) return 104;
    for (i = 0U; i < 16U; ++i) {
        if (vox_digs_match_step(&fixture_match) != VOX_OK) return 105;
    }
    debris_bodies = 0U;
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        if ((fixture_match.ragdolls.bodies[i].flags &
             VOX_RIGID_BODY_DEBRIS) != 0U) debris_bodies++;
    }
    if (saw_event(&fixture_match, VOX_DIGS_EVENT_CAVE_IN) ||
        debris_bodies != 0U ||
        (fixture_match.awards[0] &
         (1U << VOX_DIGS_AWARD_CAVE_IN_ARTIST)) != 0U) return 106;
    for (x = 96L; x <= 120L; ++x) {
        for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
            if (vox_world_set(&fixture_match.world, (vox_u32)x, 101U, z,
                              VOX_MAT_BEDROCK, 0L) != VOX_OK) {
                return 107;
            }
        }
    }
    fixture_match.ragdolls.bodies[body_index].position_x_q16 = 108L << 16;
    fixture_match.ragdolls.bodies[body_index].position_y_q16 = 100L << 16;
    fixture_match.ragdolls.bodies[body_index].velocity_x_q16 = 0L;
    fixture_match.ragdolls.bodies[body_index].velocity_y_q16 = 0L;
    fixture_match.ragdolls.bodies[body_index].angular_velocity_q16 = 0L;
    fixture_match.ragdolls.bodies[body_index].flags = (vox_u16)(
        fixture_match.ragdolls.bodies[body_index].flags |
        VOX_RIGID_BODY_SLEEPING);
    fixture_match.ragdolls.bodies[body_index].sleep_ticks = 180U;
    if (vox_digs_match_step(&fixture_match) != VOX_OK) return 108;
    stable_metal = 0U;
    loose_metal = 0U;
    for (y = 96L; y <= 104L; ++y) {
        for (x = 96L; x <= 120L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(
                    &fixture_match.world, (vox_u32)x, (vox_u32)y, z);
                if (cell != 0 && cell->material == VOX_MAT_METAL) {
                    stable_metal++;
                    if ((cell->flags & (VOX_CELL_FIXTURE | VOX_CELL_LOOSE)) !=
                        0U || cell_overlaps_living_player(
                            &fixture_match, (vox_u32)x, (vox_u32)y)) {
                        loose_metal++;
                    }
                }
            }
        }
    }
    if ((fixture_match.ragdolls.bodies[body_index].flags &
         VOX_RIGID_BODY_ACTIVE) != 0U) return 109;
    if (stable_metal != 4U) return 145;
    if (loose_metal != 0U) return 146;
    if (fixture_match.rigid_settle_discarded != 166U) return 147;
    if (fixture_match.rigid_material[body_index] != VOX_MAT_AIR ||
        fixture_match.rigid_loose_cells[body_index] != 0U) return 148;
    /* Fifteen source cells remain a visual/nonblocking remnant and recycle;
     * they may not turn into traversal-changing metal. */
    if (vox_digs_match_init(&fixture_match, &fixture_rules) != VOX_OK) {
        return 110;
    }
    for (y = 96L; y <= 104L; ++y) {
        for (x = 96L; x <= 120L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&fixture_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK ||
                    vox_world_set(&fixture_match.world, (vox_u32)x, 101U, z,
                                  VOX_MAT_BEDROCK, 0L) != VOX_OK) {
                    return 111;
                }
            }
        }
    }
    if (vox_rigid_spawn(&fixture_match.ragdolls, &body_index,
                        108L << 16, 100L << 16,
                        8192L, 8192L, 16384L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK) return 112;
    fixture_match.ragdolls.bodies[body_index].flags = (vox_u16)(
        fixture_match.ragdolls.bodies[body_index].flags |
        VOX_RIGID_BODY_SLEEPING);
    fixture_match.ragdolls.bodies[body_index].sleep_ticks = 180U;
    fixture_match.rigid_material[body_index] = VOX_MAT_METAL;
    fixture_match.rigid_loose_cells[body_index] = 15U;
    if (vox_digs_match_step(&fixture_match) != VOX_OK) return 113;
    stable_metal = 0U;
    for (y = 96L; y <= 104L; ++y) {
        for (x = 96L; x <= 120L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(
                    &fixture_match.world, (vox_u32)x, (vox_u32)y, z);
                if (cell != 0 && cell->material == VOX_MAT_METAL) {
                    stable_metal++;
                }
            }
        }
    }
    if ((fixture_match.ragdolls.bodies[body_index].flags &
         VOX_RIGID_BODY_ACTIVE) != 0U || stable_metal != 0U ||
        fixture_match.rigid_settle_discarded != 15U) return 114;
    /* A qualifying scrap may leave ordinary metal around a living miner, but
     * never in that miner's swept body footprint. */
    if (vox_digs_match_init(&fixture_match, &fixture_rules) != VOX_OK) {
        return 115;
    }
    for (y = 98L; y <= 110L; ++y) {
        for (x = 96L; x <= 120L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&fixture_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK ||
                    vox_world_set(&fixture_match.world, (vox_u32)x, 105U, z,
                                  VOX_MAT_BEDROCK, 0L) != VOX_OK) {
                    return 116;
                }
            }
        }
    }
    fixture_match.players[0].position_x.value_q16 = 108L << 16;
    fixture_match.players[0].position_y.value_q16 = 104L << 16;
    fixture_match.players[0].velocity_x.value_q16 = 0L;
    fixture_match.players[0].velocity_y.value_q16 = 0L;
    if (vox_rigid_spawn(&fixture_match.ragdolls, &body_index,
                        108L << 16, 104L << 16,
                        8192L, 8192L, 16384L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK) return 117;
    fixture_match.ragdolls.bodies[body_index].flags = (vox_u16)(
        fixture_match.ragdolls.bodies[body_index].flags |
        VOX_RIGID_BODY_SLEEPING);
    fixture_match.ragdolls.bodies[body_index].sleep_ticks = 180U;
    fixture_match.rigid_material[body_index] = VOX_MAT_METAL;
    fixture_match.rigid_loose_cells[body_index] = 16U;
    if (vox_digs_match_step(&fixture_match) != VOX_OK) return 118;
    stable_metal = 0U;
    for (y = 98L; y <= 110L; ++y) {
        for (x = 96L; x <= 120L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(
                    &fixture_match.world, (vox_u32)x, (vox_u32)y, z);
                if (cell != 0 && cell->material == VOX_MAT_METAL) {
                    if ((cell->flags & (VOX_CELL_FIXTURE | VOX_CELL_LOOSE)) !=
                        0U || cell_overlaps_living_player(
                            &fixture_match, (vox_u32)x, (vox_u32)y)) {
                        return 119;
                    }
                    stable_metal++;
                }
            }
        }
    }
    if (stable_metal != 4U || fixture_match.rigid_settle_discarded != 12U ||
        (fixture_match.ragdolls.bodies[body_index].flags &
         VOX_RIGID_BODY_ACTIVE) != 0U) return 120;
    vox_digs_rules_classic(&rope_rules);
    rope_rules.player_count = 1U;
    rope_rules.bot_mask = 0U;
    rope_rules.score_limit = 0U;
    if (vox_digs_match_init(&water_match, &rope_rules) != VOX_OK) return 33;
    water_match.spawn_shield_ticks[0] = 0U;
    source_x = water_match.players[0].position_x.value_q16 >> 16;
    source_y = (water_match.players[0].position_y.value_q16 +
                water_match.players[0].half_height_q16) >> 16;
    if (source_y < 0L || source_y >= (vox_i32)VOX_WORLD_HEIGHT ||
        vox_fluid_add_at(&water_match.fluids, (vox_u16)source_x,
                         (vox_u16)source_y, 0U, VOX_FLUID_WATER,
                         VOX_FLUID_CELL_CAPACITY_Q16, 20L << 16) != VOX_OK ||
        vox_digs_match_step(&water_match) != VOX_OK ||
        !saw_event(&water_match, VOX_DIGS_EVENT_DROWN) ||
        water_match.health[0] != (vox_u16)(VOX_DIGS_MAX_HEALTH - 3U)) {
        return 34;
    }
    /* A support-frontier invalidation must become a physical debris fragment,
     * not merely a diagnostic risk counter. */
    vox_digs_rules_classic(&collapse_rules);
    collapse_rules.player_count = 1U;
    collapse_rules.bot_mask = 0U;
    collapse_rules.score_limit = 0U;
    if (vox_digs_match_init(&collapse_match, &collapse_rules) != VOX_OK ||
        vox_structure_step(&collapse_match.structure, &collapse_match.world,
                           VOX_STRUCTURE_FRONTIER_CAPACITY) != VOX_OK) {
        return 35;
    }
    for (y = 54L; y <= 68L; ++y) {
        for (x = 76L; x <= 88L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&collapse_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 36;
                }
            }
        }
    }
    for (x = 80L; x <= 95L; ++x) {
        if (vox_world_set(&collapse_match.world, (vox_u32)x, 60U, 0U,
                          VOX_MAT_STONE, 0L) != VOX_OK) return 37;
    }
    if (vox_structure_invalidate_with_cause(&collapse_match.structure,
                                            87U, 60U, 6U, 0U,
                                            VOX_DIGS_TOOL_FIRECRACKER) !=
        VOX_OK) return 38;
    for (i = 0U; i < 12U &&
         !saw_event(&collapse_match, VOX_DIGS_EVENT_CAVE_IN); ++i) {
        if (vox_digs_match_step(&collapse_match) != VOX_OK) return 39;
    }
    if (!saw_event(&collapse_match, VOX_DIGS_EVENT_CAVE_IN) ||
        (collapse_match.awards[0] &
         (1U << VOX_DIGS_AWARD_CAVE_IN_ARTIST)) == 0U) return 40;
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        if ((collapse_match.ragdolls.bodies[i].flags &
             VOX_RIGID_BODY_DEBRIS) != 0U) {
            break;
        }
    }
    if (i == VOX_RIGID_MAX_BODIES) return 41;
    /* Fifteen genuine terrain cells, even when touching a fixture, do not
     * qualify as a cave-in.  The fixture must not pad the cluster count or
     * turn an ordinary unsupported fragment into debris, attribution, or an
     * award. */
    if (vox_digs_match_init(&collapse_match, &collapse_rules) != VOX_OK ||
        vox_structure_step(&collapse_match.structure, &collapse_match.world,
                           VOX_STRUCTURE_FRONTIER_CAPACITY) != VOX_OK) {
        return 121;
    }
    for (y = 54L; y <= 68L; ++y) {
        for (x = 76L; x <= 96L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&collapse_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 122;
                }
            }
        }
    }
    for (x = 80L; x <= 94L; ++x) {
        if (vox_world_set(&collapse_match.world, (vox_u32)x, 60U, 0U,
                          VOX_MAT_STONE, 0L) != VOX_OK) return 123;
    }
    if (vox_world_set(&collapse_match.world, 95U, 60U, 0U,
                      VOX_MAT_METAL, 20L << 16) != VOX_OK ||
        vox_world_set_fixture(&collapse_match.world, 95U, 60U, 0U, 1U) !=
            VOX_OK ||
        vox_structure_invalidate_with_cause(&collapse_match.structure,
                                            87U, 60U, 6U, 0U,
                                            VOX_DIGS_TOOL_FIRECRACKER) !=
            VOX_OK) {
        return 124;
    }
    for (i = 0U; i < 32U; ++i) {
        if (vox_digs_match_step(&collapse_match) != VOX_OK) return 125;
    }
    debris_bodies = 0U;
    for (i = 0U; i < VOX_RIGID_MAX_BODIES; ++i) {
        if ((collapse_match.ragdolls.bodies[i].flags &
             VOX_RIGID_BODY_DEBRIS) != 0U) {
            debris_bodies++;
        }
    }
    if (saw_event(&collapse_match, VOX_DIGS_EVENT_CAVE_IN) ||
        debris_bodies != 0U ||
        (collapse_match.awards[0] &
         (1U << VOX_DIGS_AWARD_CAVE_IN_ARTIST)) != 0U ||
        !vox_world_is_fixture(&collapse_match.world, 95U, 60U, 0U)) {
        return 126;
    }
    /* A roof wider than one detached-body capacity must travel through the
     * bounded support frontier as multiple fragments.  This is deliberately
     * cross-chunk: a local invalidation may not leave the far half floating
     * forever just because the first physical fragment was capped at 128
     * cells. */
    vox_digs_rules_classic(&wide_collapse_rules);
    wide_collapse_rules.player_count = 1U;
    wide_collapse_rules.bot_mask = 0U;
    wide_collapse_rules.score_limit = 0U;
    if (vox_digs_match_init(&wide_collapse_match,
                            &wide_collapse_rules) != VOX_OK ||
        vox_structure_step(&wide_collapse_match.structure,
                           &wide_collapse_match.world,
                           VOX_STRUCTURE_FRONTIER_CAPACITY) != VOX_OK) {
        return 74;
    }
    for (y = 44L; y <= 76L; ++y) {
        for (x = 48L; x <= 336L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&wide_collapse_match.world,
                                    (vox_u32)x, (vox_u32)y, z) != VOX_OK) {
                    return 75;
                }
            }
        }
    }
    for (x = 64L; x < 320L; ++x) {
        if (vox_world_set(&wide_collapse_match.world, (vox_u32)x, 60U,
                          0U, VOX_MAT_STONE, 0L) != VOX_OK) return 76;
    }
    if (vox_structure_invalidate_with_cause(
            &wide_collapse_match.structure, 64U, 60U, 6U, 0U,
            VOX_DIGS_TOOL_FIRECRACKER) != VOX_OK) {
        return 77;
    }
    for (i = 0U; i < 96U; ++i) {
        if (vox_digs_match_step(&wide_collapse_match) != VOX_OK) return 78;
    }
    cave_in_events = 0U;
    for (i = 0U; i < wide_collapse_match.event_count; ++i) {
        const vox_digs_event *event = &wide_collapse_match.events[
            (wide_collapse_match.event_head + i) % VOX_DIGS_MAX_EVENTS];
        if (event->type == VOX_DIGS_EVENT_CAVE_IN &&
            event->source == 0U && event->magnitude != 0U) {
            cave_in_events++;
        }
    }
    remaining_roof_cells = 0U;
    for (x = 64L; x < 320L; ++x) {
        const vox_cell *cell = vox_world_cell(&wide_collapse_match.world,
                                              (vox_u32)x, 60U, 0U);
        if (cell != 0 && cell->material == VOX_MAT_STONE) {
            remaining_roof_cells++;
        }
    }
    if (cave_in_events < 2U || remaining_roof_cells != 0U) return 79;
    /* A physical cave-in body must use normal hit attribution rather than
     * becoming a harmless visual after terrain extraction. */
    vox_digs_rules_classic(&impact_rules);
    impact_rules.player_count = 2U;
    impact_rules.bot_mask = 0U;
    impact_rules.score_limit = 0U;
    if (vox_digs_match_init(&impact_match, &impact_rules) != VOX_OK) {
        return 42;
    }
    impact_match.spawn_shield_ticks[0] = 0U;
    impact_match.spawn_shield_ticks[1] = 0U;
    impact_match.players[1].position_x.value_q16 = 200L << 16;
    impact_match.players[1].position_y.value_q16 = 80L << 16;
    impact_match.players[1].velocity_x.value_q16 = 0L;
    impact_match.players[1].velocity_y.value_q16 = 0L;
    for (y = 70L; y <= 90L; ++y) {
        for (x = 190L; x <= 210L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&impact_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 43;
                }
            }
        }
    }
    if (vox_rigid_spawn(&impact_match.ragdolls, &body_index,
                        (199L << 16) + 16384L, 80L << 16,
                        32768L, 32768L, 65536L,
                        VOX_RIGID_BODY_DEBRIS) != VOX_OK) return 44;
    impact_match.ragdolls.bodies[body_index].velocity_x_q16 = 49152L;
    impact_match.rigid_source[body_index] = 0U;
    impact_match.rigid_weapon[body_index] = VOX_DIGS_TOOL_FIRECRACKER;
    if (vox_digs_match_step(&impact_match) != VOX_OK ||
        impact_match.health[1] >= VOX_DIGS_MAX_HEALTH ||
        impact_match.last_attacker[1] != 0U ||
        impact_match.rigid_impact_cooldown[body_index] == 0U ||
        !saw_event(&impact_match, VOX_DIGS_EVENT_DEBRIS_IMPACT)) return 45;
    /* Scrap is not ordinary cave-in terrain: only a body travelling at least
     * one cell per tick may hurt, it deals minor direct health damage, and
     * two scraps cannot stack a second hit onto the same miner in one tick. */
    if (vox_digs_match_init(&impact_match, &impact_rules) != VOX_OK) {
        return 127;
    }
    impact_match.spawn_shield_ticks[0] = 0U;
    impact_match.spawn_shield_ticks[1] = 0U;
    impact_match.players[1].position_x.value_q16 = 200L << 16;
    impact_match.players[1].position_y.value_q16 = 80L << 16;
    impact_match.players[1].velocity_x.value_q16 = 0L;
    impact_match.players[1].velocity_y.value_q16 = 0L;
    for (y = 70L; y <= 90L; ++y) {
        for (x = 190L; x <= 210L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&impact_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 128;
                }
            }
        }
    }
    anatomy_before = impact_match.anatomy[1][VOX_DIGS_PART_TORSO].health;
    if (vox_rigid_spawn(&impact_match.ragdolls, &body_index,
                        (199L << 16) + 16384L, 80L << 16,
                        32768L, 32768L, 16384L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK ||
        vox_rigid_spawn(&impact_match.ragdolls, &body_index_second,
                        (199L << 16) + 16384L, 80L << 16,
                        32768L, 32768L, 16384L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK) {
        return 129;
    }
    impact_match.ragdolls.bodies[body_index].velocity_x_q16 = 65536L;
    impact_match.ragdolls.bodies[body_index_second].velocity_x_q16 = 65536L;
    impact_match.rigid_source[body_index] = 0U;
    impact_match.rigid_source[body_index_second] = 0U;
    impact_match.rigid_weapon[body_index] = VOX_DIGS_TOOL_FIRECRACKER;
    impact_match.rigid_weapon[body_index_second] = VOX_DIGS_TOOL_FIRECRACKER;
    impact_match.rigid_material[body_index] = VOX_MAT_METAL;
    impact_match.rigid_material[body_index_second] = VOX_MAT_METAL;
    if (vox_digs_match_step(&impact_match) != VOX_OK ||
        impact_match.health[1] != VOX_DIGS_MAX_HEALTH - 1U ||
        impact_match.alive[1] == 0U ||
        impact_match.anatomy[1][VOX_DIGS_PART_TORSO].health != anatomy_before ||
        impact_match.rigid_impact_cooldown[body_index] == 0U ||
        impact_match.rigid_impact_cooldown[body_index_second] != 0U ||
        count_events(&impact_match, VOX_DIGS_EVENT_DEBRIS_IMPACT) != 1U ||
        saw_event(&impact_match, VOX_DIGS_EVENT_KILL)) return 130;
    /* Every additional half-cell per tick rises by one damage, bounded at
     * ten; even the cap cannot finish a miner already at one health. */
    if (vox_digs_match_init(&impact_match, &impact_rules) != VOX_OK) {
        return 131;
    }
    impact_match.spawn_shield_ticks[0] = 0U;
    impact_match.spawn_shield_ticks[1] = 0U;
    impact_match.players[1].position_x.value_q16 = 200L << 16;
    impact_match.players[1].position_y.value_q16 = 80L << 16;
    impact_match.players[1].velocity_x.value_q16 = 0L;
    impact_match.players[1].velocity_y.value_q16 = 0L;
    for (y = 70L; y <= 90L; ++y) {
        for (x = 190L; x <= 210L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&impact_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 132;
                }
            }
        }
    }
    anatomy_before = impact_match.anatomy[1][VOX_DIGS_PART_TORSO].health;
    if (vox_rigid_spawn(&impact_match.ragdolls, &body_index,
                        (194L << 16) + 16384L, 80L << 16,
                        32768L, 32768L, 16384L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK) return 133;
    impact_match.ragdolls.bodies[body_index].velocity_x_q16 = 360448L;
    impact_match.rigid_source[body_index] = 0U;
    impact_match.rigid_weapon[body_index] = VOX_DIGS_TOOL_FIRECRACKER;
    impact_match.rigid_material[body_index] = VOX_MAT_METAL;
    if (vox_digs_match_step(&impact_match) != VOX_OK ||
        impact_match.health[1] != VOX_DIGS_MAX_HEALTH - 10U ||
        impact_match.alive[1] == 0U ||
        impact_match.anatomy[1][VOX_DIGS_PART_TORSO].health != anatomy_before ||
        !saw_event(&impact_match, VOX_DIGS_EVENT_DEBRIS_IMPACT) ||
        saw_event(&impact_match, VOX_DIGS_EVENT_KILL)) return 134;
    if (vox_digs_match_init(&impact_match, &impact_rules) != VOX_OK) {
        return 135;
    }
    impact_match.spawn_shield_ticks[0] = 0U;
    impact_match.spawn_shield_ticks[1] = 0U;
    impact_match.health[1] = 1U;
    impact_match.players[1].position_x.value_q16 = 200L << 16;
    impact_match.players[1].position_y.value_q16 = 80L << 16;
    impact_match.players[1].velocity_x.value_q16 = 0L;
    impact_match.players[1].velocity_y.value_q16 = 0L;
    for (y = 70L; y <= 90L; ++y) {
        for (x = 190L; x <= 210L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&impact_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 136;
                }
            }
        }
    }
    if (vox_rigid_spawn(&impact_match.ragdolls, &body_index,
                        (194L << 16) + 16384L, 80L << 16,
                        32768L, 32768L, 16384L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK) return 137;
    impact_match.ragdolls.bodies[body_index].velocity_x_q16 = 360448L;
    impact_match.rigid_source[body_index] = 0U;
    impact_match.rigid_weapon[body_index] = VOX_DIGS_TOOL_FIRECRACKER;
    impact_match.rigid_material[body_index] = VOX_MAT_METAL;
    if (vox_digs_match_step(&impact_match) != VOX_OK ||
        impact_match.health[1] != 1U || impact_match.alive[1] == 0U ||
        saw_event(&impact_match, VOX_DIGS_EVENT_KILL) ||
        saw_event(&impact_match, VOX_DIGS_EVENT_DAMAGE)) return 138;
    /* A stationary scrap is pass-through: walking into it cannot deal a
     * contact hit merely because a rigid body is overlapping the miner. */
    if (vox_digs_match_init(&impact_match, &impact_rules) != VOX_OK) {
        return 139;
    }
    impact_match.spawn_shield_ticks[0] = 0U;
    impact_match.spawn_shield_ticks[1] = 0U;
    impact_match.players[1].position_x.value_q16 = 200L << 16;
    impact_match.players[1].position_y.value_q16 = 80L << 16;
    impact_match.players[1].velocity_x.value_q16 = 0L;
    impact_match.players[1].velocity_y.value_q16 = 0L;
    for (y = 70L; y <= 90L; ++y) {
        for (x = 190L; x <= 210L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&impact_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 140;
                }
            }
        }
    }
    if (vox_rigid_spawn(&impact_match.ragdolls, &body_index,
                        200L << 16, 80L << 16,
                        32768L, 32768L, 16384L,
                        VOX_RIGID_BODY_SCRAP) != VOX_OK) return 141;
    impact_match.rigid_source[body_index] = 0U;
    impact_match.rigid_weapon[body_index] = VOX_DIGS_TOOL_FIRECRACKER;
    impact_match.rigid_material[body_index] = VOX_MAT_METAL;
    if (vox_digs_match_step(&impact_match) != VOX_OK ||
        impact_match.health[1] != VOX_DIGS_MAX_HEALTH ||
        impact_match.rigid_impact_cooldown[body_index] != 0U ||
        saw_event(&impact_match, VOX_DIGS_EVENT_DEBRIS_IMPACT)) return 142;
    /* A settled terrain fragment returns a bounded compact set of loose
     * material, then records every unmerged cell rather than silently losing
     * it.  This runs after the rigid step, so the new loose cells remain in
     * their deterministic insertion order for this inspection tick. */
    impact_rules.player_count = 1U;
    impact_rules.bot_mask = 0U;
    impact_rules.score_limit = 0U;
    if (vox_digs_match_init(&impact_match, &impact_rules) != VOX_OK) {
        return 67;
    }
    for (y = 96L; y <= 104L; ++y) {
        for (x = 246L; x <= 254L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&impact_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 68;
                }
            }
        }
    }
    if (vox_rigid_spawn(&impact_match.ragdolls, &body_index,
                        250L << 16, 100L << 16,
                        32768L, 32768L, 8L << 16,
                        VOX_RIGID_BODY_DEBRIS) != VOX_OK) return 69;
    impact_match.ragdolls.bodies[body_index].flags = (vox_u16)(
        impact_match.ragdolls.bodies[body_index].flags |
        VOX_RIGID_BODY_SLEEPING);
    impact_match.ragdolls.bodies[body_index].sleep_ticks = 180U;
    impact_match.rigid_material[body_index] = VOX_MAT_COAL;
    impact_match.rigid_loose_cells[body_index] = 20U;
    if (vox_digs_match_step(&impact_match) != VOX_OK) return 70;
    settled_cells = 0U;
    for (y = 96L; y <= 104L; ++y) {
        for (x = 246L; x <= 254L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(
                    &impact_match.world, (vox_u32)x, (vox_u32)y, z);
                if (cell != 0 && cell->material == VOX_MAT_COAL &&
                    (cell->flags & VOX_CELL_LOOSE) != 0U) {
                    settled_cells++;
                }
            }
        }
    }
    saw_settle = 0U;
    for (i = 0U; i < impact_match.event_count; ++i) {
        const vox_digs_event *event = &impact_match.events[
            (impact_match.event_head + i) % VOX_DIGS_MAX_EVENTS];
        if (event->type == VOX_DIGS_EVENT_DEBRIS_IMPACT &&
            event->material == VOX_MAT_COAL && event->magnitude == 16U &&
            event->variant == 4U) {
            saw_settle = 1U;
        }
    }
    if ((impact_match.ragdolls.bodies[body_index].flags &
         VOX_RIGID_BODY_ACTIVE) != 0U || settled_cells != 16U ||
        impact_match.rigid_settle_discarded != 4U ||
        impact_match.rigid_material[body_index] != VOX_MAT_AIR ||
        impact_match.rigid_loose_cells[body_index] != 0U ||
        saw_settle == 0U) return 71;
    /* Replay choice reads bounded authoritative history, but its ledger
     * remains render-only.  A headshot beside a recent explosion after a
     * cave-in and prior kill must retain every scoring context. */
    impact_rules.player_count = 2U;
    impact_rules.bot_mask = 0U;
    impact_rules.score_limit = 0U;
    if (vox_digs_match_init(&water_match, &impact_rules) != VOX_OK) {
        return 72;
    }
    water_match.tick = 100U;
    water_match.spawn_shield_ticks[0] = 0U;
    water_match.spawn_shield_ticks[1] = 0U;
    water_match.players[0].position_x.value_q16 = 96L << 16;
    water_match.players[0].position_y.value_q16 = 100L << 16;
    water_match.players[1].position_x.value_q16 = 100L << 16;
    water_match.players[1].position_y.value_q16 = 100L << 16;
    water_match.last_damage_part[1] = VOX_DIGS_PART_HEAD;
    water_match.last_damage_weapon[1] = VOX_DIGS_TOOL_FIRECRACKER;
    water_match.award_value[0] = 30U;
    water_match.event_head = 0U;
    water_match.event_count = 3U;
    water_match.event_sequence = 3U;
    water_match.events[0].sequence = 1U;
    water_match.events[0].tick = 80U;
    water_match.events[0].position_x_q16 = 100L << 16;
    water_match.events[0].position_y_q16 = 100L << 16;
    water_match.events[0].type = VOX_DIGS_EVENT_CAVE_IN;
    water_match.events[0].source = 0U;
    water_match.events[0].target = VOX_DIGS_NO_PLAYER;
    water_match.events[0].weapon = VOX_DIGS_TOOL_FIRECRACKER;
    water_match.events[0].material = VOX_MAT_STONE;
    water_match.events[0].magnitude = 42U;
    water_match.events[0].variant = 0U;
    water_match.events[0].reserved = 0U;
    water_match.events[1].sequence = 2U;
    water_match.events[1].tick = 95U;
    water_match.events[1].position_x_q16 = 101L << 16;
    water_match.events[1].position_y_q16 = 100L << 16;
    water_match.events[1].type = VOX_DIGS_EVENT_EXPLOSION;
    water_match.events[1].source = 0U;
    water_match.events[1].target = 1U;
    water_match.events[1].weapon = VOX_DIGS_TOOL_FIRECRACKER;
    water_match.events[1].material = VOX_MAT_STONE;
    water_match.events[1].magnitude = 3U;
    water_match.events[1].variant = 0U;
    water_match.events[1].reserved = 0U;
    water_match.events[2].sequence = 3U;
    water_match.events[2].tick = 99U;
    water_match.events[2].position_x_q16 = 100L << 16;
    water_match.events[2].position_y_q16 = 100L << 16;
    water_match.events[2].type = VOX_DIGS_EVENT_KILL;
    water_match.events[2].source = 0U;
    water_match.events[2].target = 1U;
    water_match.events[2].weapon = VOX_DIGS_TOOL_PICK;
    water_match.events[2].material = VOX_MAT_BLOOD;
    water_match.events[2].magnitude = 1U;
    water_match.events[2].variant = 0U;
    water_match.events[2].reserved = 0U;
    if (vox_digs_record_kill(&water_match, 0U, 1U) != VOX_OK ||
        water_match.replay.active == 0U ||
        water_match.replay.headshot == 0U ||
        water_match.replay.kill_streak != 2U ||
        water_match.replay.multi_kill != 2U ||
        water_match.replay.cave_in_scale != 42U ||
        water_match.replay.blast_distance != 1U ||
        water_match.replay.award_value == 0U ||
        !saw_event(&water_match, VOX_DIGS_EVENT_REPLAY_SELECT)) return 73;
    /* Player bark contexts are selected from already-authoritative actions
     * and events.  They must be specific without adding a new persona or a
     * persistent-memory schema. */
    vox_digs_rules_classic(&bark_rules);
    bark_rules.player_count = 1U;
    bark_rules.bot_mask = 0U;
    bark_rules.score_limit = 0U;
    if (vox_digs_match_init(&bark_match, &bark_rules) != VOX_OK) return 46;
    bark_input.abi_version = VOX_ABI_VERSION;
    bark_input.struct_size = (vox_u32)sizeof(bark_input);
    bark_input.player = 0U;
    bark_input.actions = VOX_DIGS_ACTION_BARK;
    bark_input.aim_x = bark_match.aim_x[0];
    bark_input.aim_y = bark_match.aim_y[0];
    bark_input.move_x_q15 = 0;
    bark_input.move_y_q15 = 0;
    bark_input.selected_weapon = VOX_DIGS_TOOL_PICK;
    bark_input.reserved = 0U;
    bark_match.health[0] = 25U;
    if (vox_digs_submit_input(&bark_match, &bark_input) != VOX_OK ||
        vox_digs_match_step(&bark_match) != VOX_OK ||
        last_bark_stimulus(&bark_match, 0U) !=
        VOX_DIGS_STIMULUS_NEAR_DEATH) return 47;
    if (vox_digs_match_init(&bark_match, &bark_rules) != VOX_OK) return 48;
    bark_input.actions = (vox_u16)(VOX_DIGS_ACTION_BARK |
                                   VOX_DIGS_ACTION_RIGHT);
    bark_input.aim_x = bark_match.aim_x[0];
    bark_input.aim_y = bark_match.aim_y[0];
    bark_input.move_x_q15 = 32767;
    if (vox_digs_submit_input(&bark_match, &bark_input) != VOX_OK ||
        vox_digs_match_step(&bark_match) != VOX_OK ||
        last_bark_stimulus(&bark_match, 0U) != VOX_DIGS_STIMULUS_MOVE) {
        return 49;
    }
    if (vox_digs_match_init(&bark_match, &bark_rules) != VOX_OK) return 50;
    bark_input.actions = (vox_u16)(VOX_DIGS_ACTION_BARK |
                                   VOX_DIGS_ACTION_ROPE);
    bark_input.aim_x = bark_match.aim_x[0];
    bark_input.aim_y = bark_match.aim_y[0];
    bark_input.move_x_q15 = 0;
    if (vox_digs_submit_input(&bark_match, &bark_input) != VOX_OK ||
        vox_digs_match_step(&bark_match) != VOX_OK ||
        last_bark_stimulus(&bark_match, 0U) != VOX_DIGS_STIMULUS_GRAPPLE) {
        return 51;
    }
    if (vox_digs_match_init(&bark_match, &bark_rules) != VOX_OK) return 52;
    bark_input.actions = VOX_DIGS_ACTION_FIRE;
    bark_input.aim_x = bark_match.aim_x[0];
    bark_input.aim_y = bark_match.aim_y[0];
    bark_input.move_x_q15 = 0;
    bark_input.selected_weapon = VOX_DIGS_TOOL_PRESSURE_HOSE;
    if (vox_digs_submit_input(&bark_match, &bark_input) != VOX_OK ||
        vox_digs_match_step(&bark_match) != VOX_OK) return 53;
    bark_input.actions = VOX_DIGS_ACTION_BARK;
    if (vox_digs_submit_input(&bark_match, &bark_input) != VOX_OK ||
        vox_digs_match_step(&bark_match) != VOX_OK ||
        last_bark_stimulus(&bark_match, 0U) != VOX_DIGS_STIMULUS_MISS) {
        return 54;
    }
    bark_rules.player_count = 2U;
    if (vox_digs_match_init(&bark_match, &bark_rules) != VOX_OK) return 55;
    bark_match.spawn_shield_ticks[0] = 0U;
    bark_match.spawn_shield_ticks[1] = 0U;
    if (vox_digs_apply_hit(&bark_match, 0U, 1U,
                           VOX_DIGS_TOOL_NAIL_GUN,
                           VOX_DIGS_PART_HEAD, 1U,
                           VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK) return 56;
    bark_input.actions = VOX_DIGS_ACTION_BARK;
    bark_input.aim_x = bark_match.aim_x[0];
    bark_input.aim_y = bark_match.aim_y[0];
    bark_input.selected_weapon = VOX_DIGS_TOOL_PICK;
    if (vox_digs_submit_input(&bark_match, &bark_input) != VOX_OK ||
        vox_digs_match_step(&bark_match) != VOX_OK ||
        last_bark_stimulus(&bark_match, 0U) !=
        VOX_DIGS_STIMULUS_HUMILIATION) return 57;
    /* The three fixed bots must report distinct, target-aware tunnel choices:
     * RIVET plans a blocked route, CINDER commits to a direct breach with the
     * same fire action a player uses, and FLAMEY switches between ambush and
     * trap when sight changes. */
    vox_digs_rules_classic(&ai_rules);
    ai_rules.player_count = 4U;
    ai_rules.bot_mask = 14U;
    ai_rules.score_limit = 0U;
    if (vox_digs_match_init(&ai_match, &ai_rules) != VOX_OK) return 58;
    for (y = 44L; y <= 76L; ++y) {
        for (x = 100L; x <= 140L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&ai_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 59;
                }
            }
        }
    }
    ai_match.players[0].position_x.value_q16 = 112L << 16;
    ai_match.players[0].position_y.value_q16 = 60L << 16;
    ai_match.players[1].position_x.value_q16 = 128L << 16;
    ai_match.players[1].position_y.value_q16 = 60L << 16;
    ai_match.players[1].flags = 0U;
    ai_match.alive[2] = 0U;
    ai_match.alive[3] = 0U;
    for (y = 50L; y <= 68L; ++y) {
        for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
            if (vox_world_set(&ai_match.world, 120U, (vox_u32)y, z,
                              VOX_MAT_STONE, 0L) != VOX_OK) return 60;
        }
    }
    ai_match.bots[1].target = 0U;
    ai_match.bots[1].memory_ticks = 16U;
    ai_match.bots[1].last_seen_x_q16 = ai_match.players[0].position_x.value_q16;
    ai_match.bots[1].last_seen_y_q16 = ai_match.players[0].position_y.value_q16;
    ai_match.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_archetype(&ai_match, 1U) !=
        VOX_DIGS_ARCHETYPE_ENGINEER ||
        vox_digs_bot_think(&ai_match, 1U) != VOX_OK ||
        ai_match.bots[1].tunnel_state != VOX_DIGS_TUNNEL_PLANNING) return 61;
    ai_match.alive[1] = 0U;
    ai_match.alive[2] = 1U;
    ai_match.players[2].position_x.value_q16 = 126L << 16;
    ai_match.players[2].position_y.value_q16 = 60L << 16;
    ai_match.players[2].flags = 0U;
    ai_match.bots[2].target = 0U;
    ai_match.bots[2].memory_ticks = 16U;
    ai_match.bots[2].last_seen_x_q16 = ai_match.players[0].position_x.value_q16;
    ai_match.bots[2].last_seen_y_q16 = ai_match.players[0].position_y.value_q16;
    ai_match.bots[2].stuck_ticks = 255U;
    ai_match.bots[2].breach_ticks = 1U;
    ai_match.bots[2].decision_ticks = 0U;
    if (vox_digs_bot_archetype(&ai_match, 2U) !=
        VOX_DIGS_ARCHETYPE_BERSERKER ||
        vox_digs_bot_think(&ai_match, 2U) != VOX_OK ||
        ai_match.bots[2].tunnel_state != VOX_DIGS_TUNNEL_EXCAVATING ||
        ai_match.selected_weapon[2] != VOX_DIGS_TOOL_PULASKI ||
        (ai_match.player_actions[2] & VOX_DIGS_ACTION_FIRE) == 0U) return 62;
    ai_match.alive[2] = 0U;
    ai_match.alive[3] = 1U;
    ai_match.players[3].position_x.value_q16 = 126L << 16;
    ai_match.players[3].position_y.value_q16 = 60L << 16;
    ai_match.players[3].flags = 0U;
    for (y = 50L; y <= 68L; ++y) {
        for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
            if (clear_test_cell(&ai_match.world, 120U, (vox_u32)y, z) !=
                VOX_OK) return 63;
        }
    }
    ai_match.bots[3].target = VOX_DIGS_NO_PLAYER;
    ai_match.bots[3].memory_ticks = 0U;
    ai_match.bots[3].decision_ticks = 0U;
    if (vox_digs_bot_archetype(&ai_match, 3U) !=
        VOX_DIGS_ARCHETYPE_TRICKSTER ||
        vox_digs_bot_think(&ai_match, 3U) != VOX_OK ||
        ai_match.bots[3].tunnel_state != VOX_DIGS_TUNNEL_AMBUSH) return 64;
    for (y = 50L; y <= 68L; ++y) {
        for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
            if (vox_world_set(&ai_match.world, 120U, (vox_u32)y, z,
                              VOX_MAT_STONE, 0L) != VOX_OK) return 65;
        }
    }
    ai_match.bots[3].target = 0U;
    ai_match.bots[3].memory_ticks = 16U;
    ai_match.bots[3].last_seen_x_q16 = ai_match.players[0].position_x.value_q16;
    ai_match.bots[3].last_seen_y_q16 = ai_match.players[0].position_y.value_q16;
    ai_match.bots[3].decision_ticks = 0U;
    if (vox_digs_bot_think(&ai_match, 3U) != VOX_OK ||
        ai_match.bots[3].tunnel_state != VOX_DIGS_TUNNEL_TRAP) return 66;
    /* Auto-aim must find arbitrary terrain, remain attached during another
     * action, retarget on a second edge press, and cancel on jump. */
    vox_digs_rules_classic(&rope_rules);
    rope_rules.player_count = 1U;
    rope_rules.bot_mask = 0U;
    rope_rules.score_limit = 0U;
    if (vox_digs_match_init(&rope_match, &rope_rules) != VOX_OK) return 6;
    source_x = rope_match.players[0].position_x.value_q16 >> 16;
    source_y = rope_match.players[0].position_y.value_q16 >> 16;
    target_x = source_x + 8L;
    target_y = source_y - 8L;
    second_x = source_x + 10L;
    second_y = source_y - 3L;
    if (target_x < 4L || second_x >= (vox_i32)VOX_WORLD_WIDTH ||
        target_y < 4L || second_y < 4L) return 7;
    for (y = source_y - V005_ROPE_CLEAR;
         y <= source_y + V005_ROPE_CLEAR; ++y) {
        for (x = source_x - V005_ROPE_CLEAR;
             x <= source_x + V005_ROPE_CLEAR; ++x) {
            if (x < 0L || y < 0L ||
                x >= (vox_i32)VOX_WORLD_WIDTH ||
                y >= (vox_i32)VOX_WORLD_HEIGHT) continue;
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                if (clear_test_cell(&rope_match.world, (vox_u32)x,
                                    (vox_u32)y, z) != VOX_OK) {
                    return 8;
                }
            }
        }
    }
    if (vox_world_set(&rope_match.world, (vox_u32)target_x,
                      (vox_u32)target_y, 0U, VOX_MAT_STONE, 20L << 16) !=
            VOX_OK ||
        vox_world_set(&rope_match.world, (vox_u32)second_x,
                      (vox_u32)second_y, 0U, VOX_MAT_METAL, 20L << 16) !=
            VOX_OK ||
        /* The fixture shares its collision column with durable terrain.  A
         * later fixture-only blast must sever a fixture rope rather than
         * silently downgrading it into a terrain rope. */
        vox_world_set(&rope_match.world, (vox_u32)second_x,
                      (vox_u32)second_y, 1U, VOX_MAT_BEDROCK, 0L) !=
            VOX_OK ||
        vox_world_set_fixture(&rope_match.world, (vox_u32)second_x,
                              (vox_u32)second_y, 0U, 1U) != VOX_OK ||
        vox_world_sleep_all(&rope_match.world) != VOX_OK) return 9;
    rope_input.abi_version = VOX_ABI_VERSION;
    rope_input.struct_size = (vox_u32)sizeof(rope_input);
    rope_input.player = 0U;
    rope_input.actions = VOX_DIGS_ACTION_ROPE;
    rope_input.aim_x = (vox_u16)target_x;
    rope_input.aim_y = (vox_u16)target_y;
    rope_input.move_x_q15 = 0;
    rope_input.move_y_q15 = 0;
    rope_input.selected_weapon = VOX_DIGS_TOOL_PICK;
    rope_input.reserved = 0U;
    if (vox_digs_submit_input(&rope_match, &rope_input) != VOX_OK) return 10;
    for (i = 0U; i < 24U &&
         rope_match.ropes[0].state != VOX_DIGS_ROPE_ATTACHED; ++i) {
        if (vox_digs_match_step(&rope_match) != VOX_OK) return 11;
    }
    if (rope_match.ropes[0].state != VOX_DIGS_ROPE_ATTACHED ||
        rope_match.ropes[0].flags != VOX_DIGS_ROPE_TARGET_TERRAIN ||
        rope_match.ropes[0].target_x != (vox_u16)target_x) return 12;
    rope_input.actions = VOX_DIGS_ACTION_FIRE;
    if (vox_digs_submit_input(&rope_match, &rope_input) != VOX_OK ||
        vox_digs_match_step(&rope_match) != VOX_OK ||
        rope_match.ropes[0].state != VOX_DIGS_ROPE_ATTACHED) return 13;
    rope_input.actions = 0U;
    if (vox_digs_submit_input(&rope_match, &rope_input) != VOX_OK ||
        vox_digs_match_step(&rope_match) != VOX_OK) return 14;
    rope_input.actions = VOX_DIGS_ACTION_ROPE;
    rope_input.aim_x = (vox_u16)second_x;
    rope_input.aim_y = (vox_u16)second_y;
    if (vox_digs_submit_input(&rope_match, &rope_input) != VOX_OK ||
        vox_digs_match_step(&rope_match) != VOX_OK) return 15;
    for (i = 0U; i < 24U &&
         rope_match.ropes[0].state != VOX_DIGS_ROPE_ATTACHED; ++i) {
        if (vox_digs_match_step(&rope_match) != VOX_OK) return 16;
    }
    if (rope_match.ropes[0].state != VOX_DIGS_ROPE_ATTACHED ||
        rope_match.ropes[0].target_x != (vox_u16)second_x ||
        (rope_match.ropes[0].flags & VOX_DIGS_ROPE_TARGET_FIXTURE) == 0U) return 17;
    if (vox_digs_use_tool(&rope_match, 0U, VOX_DIGS_TOOL_FIRECRACKER,
                          (vox_u32)second_x, (vox_u32)second_y, 0U) !=
            VOX_OK ||
        vox_world_is_fixture(&rope_match.world, (vox_u32)second_x,
                             (vox_u32)second_y, 0U) ||
        vox_world_cell(&rope_match.world, (vox_u32)second_x,
                       (vox_u32)second_y, 1U)->material != VOX_MAT_BEDROCK) {
        return 143;
    }
    rope_input.actions = 0U;
    if (vox_digs_submit_input(&rope_match, &rope_input) != VOX_OK ||
        vox_digs_match_step(&rope_match) != VOX_OK ||
        rope_match.ropes[0].state != VOX_DIGS_ROPE_IDLE ||
        !saw_event(&rope_match, VOX_DIGS_EVENT_ROPE_BREAK)) return 144;
    rope_input.actions = VOX_DIGS_ACTION_JUMP;
    if (vox_digs_submit_input(&rope_match, &rope_input) != VOX_OK ||
        vox_digs_match_step(&rope_match) != VOX_OK ||
        rope_match.ropes[0].state != VOX_DIGS_ROPE_IDLE) return 18;
    /* This fixture changes ship position after the general gameplay steps,
     * so explicitly activate its virtual hull instead of relying on a
     * match_init side effect. */
    match.dropship.phase = VOX_DIGS_DROPSHIP_PHASE_LAUNCH;
    match.dropship.route_ticks = 0U;
    match.dropship.launched_mask = 0U;
    match.dropship.extracted_mask = 0U;
    match.dropship.position_x_q16 = 32L << 16;
    match.dropship.previous_position_x_q16 = 32L << 16;
    match.dropship.position_y_q16 = 8L << 16;
    match.players[0].position_x.value_q16 = 32L << 16;
    match.players[0].position_y.value_q16 =
        match.dropship.position_y_q16 - VOX_DIGS_DROPSHIP_HALF_HEIGHT_Q16 -
        match.players[0].half_height_q16;
    if (vox_digs_dropship_launch(&match, 0U) != VOX_OK ||
        vox_digs_dropship_board(&match, 0U) != VOX_OK ||
        (match.dropship.extracted_mask & 1U) == 0U ||
        (match.awards[0] & (1U << VOX_DIGS_AWARD_EXTRACTIONIST)) == 0U ||
        match.replay.active == 0U ||
        !saw_event(&match, VOX_DIGS_EVENT_SHIP_LAUNCH) ||
        !saw_event(&match, VOX_DIGS_EVENT_SHIP_EXTRACT)) return 19;
    match.phase = VOX_DIGS_RESULTS;
    if (vox_digs_replay_step(&match, &replay_frame) != VOX_OK ||
        match.replay.active != 0U ||
        replay_frame.terrain_width != VOX_DIGS_REPLAY_WINDOW_DIAMETER ||
        replay_frame.terrain_height != VOX_DIGS_REPLAY_WINDOW_DIAMETER ||
        replay_frame.camera_zoom_q16 != 65536L ||
        replay_frame.event_count == 0U) return 20;
    vox_digs_rules_classic(&ship_rules);
    ship_rules.player_count = 1U;
    ship_rules.bot_mask = 0U;
    ship_rules.score_limit = 0U;
    if (vox_digs_match_init(&ship_match, &ship_rules) != VOX_OK) return 21;
    ship_match.dropship.phase = VOX_DIGS_DROPSHIP_PHASE_LAUNCH;
    source_x = ship_match.players[0].position_x.value_q16 >> 16;
    source_y = ship_match.players[0].position_y.value_q16 >> 16;
    for (x = source_x - 8L; x <= source_x + 8L; ++x) {
        if (x < 0L || x >= (vox_i32)VOX_WORLD_WIDTH) continue;
        for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
            if (clear_test_cell(&ship_match.world, (vox_u32)x,
                                (vox_u32)source_y, z) != VOX_OK) return 22;
        }
    }
    ship_match.dropship.position_x_q16 = (source_x + 8L) << 16;
    ship_match.dropship.previous_position_x_q16 =
        ship_match.dropship.position_x_q16;
    ship_match.dropship.position_y_q16 = source_y << 16;
    ship_match.dropship.velocity_x_q16 = 0L;
    if (vox_digs_dropship_launch(&ship_match, 0U) != VOX_OK) return 90;
    if (vox_digs_dropship_grapple(&ship_match, 0U) != VOX_OK) return 91;
    if (ship_match.ropes[0].flags != VOX_DIGS_ROPE_TARGET_SHIP ||
        !saw_event(&ship_match, VOX_DIGS_EVENT_SHIP_GRAPPLE)) return 23;
    if (vox_digs_dropship_board(&ship_match, 0U) != VOX_OK ||
        (ship_match.dropship.extracted_mask & 1U) == 0U) return 24;
    vox_digs_rules_classic(&ship_rules);
    ship_rules.player_count = 1U;
    ship_rules.bot_mask = 0U;
    ship_rules.score_limit = 0U;
    if (vox_digs_match_init(&collision_match, &ship_rules) != VOX_OK) return 25;
    collision_match.dropship.phase = VOX_DIGS_DROPSHIP_PHASE_LAUNCH;
    collision_match.spawn_shield_ticks[0] = 0U;
    collision_match.dropship.position_x_q16 =
        collision_match.players[0].position_x.value_q16;
    collision_match.dropship.previous_position_x_q16 =
        collision_match.dropship.position_x_q16;
    collision_match.dropship.position_y_q16 =
        collision_match.players[0].position_y.value_q16;
    collision_match.dropship.velocity_x_q16 = 0L;
    if (vox_digs_dropship_launch(&collision_match, 0U) != VOX_OK) return 25;
    /* A genuine post-launch strike stays lethal.  The normal launch path
     * ejects a deck rider through the hold, so deliberately put this miner
     * back in the hull to exercise the dangerous collision contract. */
    collision_match.players[0].position_x.value_q16 =
        collision_match.dropship.position_x_q16;
    collision_match.players[0].position_y.value_q16 =
        collision_match.dropship.position_y_q16;
    if (vox_digs_dropship_step(&collision_match) != VOX_OK ||
        collision_match.alive[0] != 0U ||
        !saw_event(&collision_match, VOX_DIGS_EVENT_SHIP_COLLISION) ||
        !saw_event(&collision_match, VOX_DIGS_EVENT_SHIP_SPLATTER)) return 26;
    /* A normal match begins aboard the moving ship, not below a cosmetic
     * flyover.  The first authoritative tick keeps a miner on the deck;
     * Fire then releases that same body with a ship-launch event. */
    vox_digs_rules_classic(&launch_rules);
    launch_rules.player_count = 1U;
    launch_rules.bot_mask = 0U;
    launch_rules.score_limit = 0U;
    if (vox_digs_match_init(&launch_match, &launch_rules) != VOX_OK) return 27;
    launch_input.abi_version = VOX_ABI_VERSION;
    launch_input.struct_size = (vox_u32)sizeof(launch_input);
    launch_input.player = 0U;
    launch_input.actions = 0U;
    launch_input.aim_x = launch_match.aim_x[0];
    launch_input.aim_y = launch_match.aim_y[0];
    launch_input.move_x_q15 = 0;
    launch_input.move_y_q15 = 0;
    launch_input.selected_weapon = VOX_DIGS_TOOL_PICK;
    launch_input.reserved = 0U;
    if (vox_digs_dropship_begin(&launch_match) != VOX_OK ||
        vox_digs_submit_input(&launch_match, &launch_input) != VOX_OK ||
        vox_digs_match_step(&launch_match) != VOX_OK ||
        (launch_match.dropship.launched_mask & 1U) != 0U ||
        launch_match.players[0].position_x.value_q16 <
        launch_match.dropship.position_x_q16 -
        VOX_DIGS_DROPSHIP_BOARD_RADIUS_Q16 ||
        launch_match.players[0].position_x.value_q16 >
        launch_match.dropship.position_x_q16 +
        VOX_DIGS_DROPSHIP_BOARD_RADIUS_Q16) return 28;
    launch_input.actions = VOX_DIGS_ACTION_FIRE;
    if (vox_digs_submit_input(&launch_match, &launch_input) != VOX_OK ||
        vox_digs_match_step(&launch_match) != VOX_OK ||
        (launch_match.dropship.launched_mask & 1U) == 0U ||
        launch_match.alive[0] == 0U ||
        launch_match.players[0].position_y.value_q16 -
        launch_match.players[0].half_height_q16 <=
        launch_match.dropship.position_y_q16 +
        VOX_DIGS_DROPSHIP_HALF_HEIGHT_Q16 ||
        !saw_event(&launch_match, VOX_DIGS_EVENT_SHIP_LAUNCH)) return 29;
    /* Reproduce the first tick after a player presses FIRE.  This used to
     * immediately splatter the newly launched miner against an unseen hull. */
    launch_input.actions = 0U;
    if (vox_digs_submit_input(&launch_match, &launch_input) != VOX_OK ||
        vox_digs_match_step(&launch_match) != VOX_OK ||
        launch_match.alive[0] == 0U ||
        saw_event(&launch_match, VOX_DIGS_EVENT_SHIP_COLLISION) ||
        saw_event(&launch_match, VOX_DIGS_EVENT_SHIP_SPLATTER)) return 89;
    if (vox_digs_match_init(&phase_match, &ship_rules) != VOX_OK) return 30;
    phase_match.dropship.phase = VOX_DIGS_DROPSHIP_PHASE_LAUNCH;
    phase_match.players[0].position_x.value_q16 = 0L;
    phase_match.players[0].position_y.value_q16 = 0L;
    phase_match.dropship.position_x_q16 = -(32L << 16);
    phase_match.dropship.previous_position_x_q16 =
        phase_match.dropship.position_x_q16;
    phase_match.dropship.position_y_q16 = 8L << 16;
    phase_match.dropship.route_ticks =
        VOX_DIGS_DROP_SHIP_ROUTE_TICKS - 1U;
    if (vox_digs_dropship_step(&phase_match) != VOX_OK ||
        phase_match.dropship.phase != VOX_DIGS_DROPSHIP_PHASE_WAITING ||
        (phase_match.dropship.launched_mask & 1U) == 0U ||
        phase_match.dropship.position_x_q16 !=
        VOX_DIGS_DROPSHIP_LAUNCH_END_X_Q16 ||
        !saw_event(&phase_match, VOX_DIGS_EVENT_SHIP_LAUNCH)) return 31;
    phase_match.tick = phase_match.rules.lava_start_tick;
    if (vox_digs_dropship_step(&phase_match) != VOX_OK ||
        phase_match.dropship.phase != VOX_DIGS_DROPSHIP_PHASE_EXTRACTION ||
        phase_match.dropship.alarmed == 0U ||
        !saw_event(&phase_match, VOX_DIGS_EVENT_SHIP_ALARM)) return 32;
    phase_match.dropship.route_ticks =
        VOX_DIGS_DROP_SHIP_ROUTE_TICKS - 1U;
    if (vox_digs_dropship_step(&phase_match) != VOX_OK ||
        phase_match.dropship.phase != VOX_DIGS_DROPSHIP_PHASE_DEPARTED ||
        phase_match.dropship.position_x_q16 !=
        VOX_DIGS_DROPSHIP_LAUNCH_START_X_Q16) return 33;
    printf("dash, headshot, corpse, awards, replay and dropship passed\n");
    return 0;
}

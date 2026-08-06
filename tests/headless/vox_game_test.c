/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_game.h"
#include "digs_lines.h"
#include <string.h>

#define TEST_MAP_JUMP_ENVELOPE 28U
#define TEST_MAP_RAIL_MIN_CLEARANCE 36U
#define TEST_MAP_ROPE_REACH 48U
#define TEST_SPAWN_HEADROOM_CELLS 4U
#define TEST_SPAWN_SUPPORT_CELLS 4U
/* Comfortably past DIGS_BURIED_LETHAL_TICKS and the health budget. */
#define DIGS_TEST_BURIED_GUARD_TICKS 240U
/* Mirrors DIGS_SMOKER_DIRECT_DAMAGE; below the head's 45 health. */
#define DIGS_TEST_SMOKER_DAMAGE 40U
/* Bolt Action is the gated one: below its charge it will not fire. */
#define DIGS_TEST_MIN_GATED_CHARGE 30U
/* Past DIGS_AI_RETREAT_MAX_TICKS and every mode dwell. */
#define DIGS_TEST_RETREAT_EXPIRED_TICKS 200U

/*
 * A match owns the complete fixed-size voxel world, so keeping one fixture per
 * test used hundreds of MiB of BSS.  Tests in this binary execute serially;
 * only the determinism comparisons need two live matches at once.
 */
static vox_digs_match match;
static vox_digs_match match_peer;

static void init_test_input(vox_digs_input *input, vox_u16 player,
                            vox_u16 aim_x, vox_u16 aim_y)
{
    input->abi_version = VOX_ABI_VERSION;
    input->struct_size = (vox_u32)sizeof(*input);
    input->player = player;
    input->actions = 0U;
    input->aim_x = aim_x;
    input->aim_y = aim_y;
    input->move_x_q15 = 0;
    input->move_y_q15 = 0;
    input->selected_weapon = VOX_DIGS_TOOL_PICK;
    input->reserved = 0U;
}

static int event_type_seen(const vox_digs_match *match, vox_u16 type)
{
    vox_u16 ordinal;
    for (ordinal = 0U; ordinal < match->event_count; ++ordinal) {
        const vox_digs_event *event = vox_digs_event_get(match, ordinal);
        if (event != 0 && event->type == type) {
            return 1;
        }
    }
    return 0;
}

static int event_mode_seen(const vox_digs_match *match, vox_u16 mode)
{
    vox_u16 ordinal;
    for (ordinal = 0U; ordinal < match->event_count; ++ordinal) {
        const vox_digs_event *event = vox_digs_event_get(match, ordinal);
        if (event != 0 && event->type == VOX_DIGS_EVENT_AI_STATE &&
            event->magnitude == mode) {
            return 1;
        }
    }
    return 0;
}

static int set_test_column(vox_world *world, vox_u32 x, vox_u32 y,
                           vox_u16 material)
{
    vox_u32 z;
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        if (vox_world_set(world, x, y, z, material, 20L << 16) != VOX_OK) {
            return 0;
        }
    }
    return 1;
}

static vox_u16 test_map_material_at(const vox_world *world, vox_u32 x,
                                    vox_u32 y)
{
    const vox_cell *cell = vox_world_cell(world, x, y, 0U);
    return cell == 0 ? VOX_MAT_COUNT : cell->material;
}

static int test_map_cell_is_solid(const vox_world *world, vox_u32 x,
                                  vox_u32 y)
{
    vox_u16 material = test_map_material_at(world, x, y);
    const vox_material_properties *properties = vox_material_get(material);
    return properties != 0 &&
           (properties->flags & VOX_MATERIAL_SOLID) != 0U;
}

static vox_u32 test_map_walk_surface(const vox_world *world, vox_u32 x)
{
    vox_u32 y;
    for (y = VOX_WORLD_HEIGHT / 4U;
         y + 24U < VOX_WORLD_HEIGHT; ++y) {
        vox_u16 top = test_map_material_at(world, x, y);
        if (top != VOX_MAT_LAVA && top != VOX_MAT_BEDROCK &&
            top != VOX_MAT_METAL &&
            test_map_cell_is_solid(world, x, y)) {
            return y <= VOX_WORLD_HEIGHT / 2U + 32U ?
                   y : VOX_WORLD_HEIGHT;
        }
        if (top == VOX_MAT_METAL) {
            vox_u32 depth;
            for (depth = 1U; depth <= 24U; ++depth) {
                vox_u16 below = test_map_material_at(world, x, y + depth);
                if (below != VOX_MAT_METAL && below != VOX_MAT_LAVA &&
                    below != VOX_MAT_BEDROCK &&
                    test_map_cell_is_solid(world, x, y + depth)) {
                    return y <= VOX_WORLD_HEIGHT / 2U + 32U ?
                           y : VOX_WORLD_HEIGHT;
                }
            }
        }
    }
    return VOX_WORLD_HEIGHT;
}

static vox_u32 test_map_terrain_surface(const vox_world *world, vox_u32 x)
{
    return test_map_walk_surface(world, x);
}

static int test_map_floor_has_support(const vox_world *world, vox_u32 x,
                                      vox_u32 floor_y)
{
    vox_i32 offset_x;
    vox_u32 depth;
    if (x == 0U || x + 1U >= VOX_WORLD_WIDTH ||
        floor_y + TEST_SPAWN_SUPPORT_CELLS >= VOX_WORLD_HEIGHT) {
        return 0;
    }
    for (offset_x = -1; offset_x <= 1; ++offset_x) {
        vox_u32 sample_x = (vox_u32)((vox_i32)x + offset_x);
        for (depth = 0U; depth < TEST_SPAWN_SUPPORT_CELLS; ++depth) {
            if (!test_map_cell_is_solid(world, sample_x,
                                        floor_y + depth)) {
                return 0;
            }
        }
    }
    return 1;
}

static vox_u32 test_map_player_surface(const vox_world *world, vox_u32 x)
{
    vox_u32 y;
    for (y = VOX_WORLD_HEIGHT / 4U; y + 1U < VOX_WORLD_HEIGHT; ++y) {
        vox_u16 material = test_map_material_at(world, x, y);
        if (material != VOX_MAT_LAVA && material != VOX_MAT_BEDROCK &&
            test_map_cell_is_solid(world, x, y) &&
            (material != VOX_MAT_METAL ||
             test_map_floor_has_support(world, x, y) ||
             (y + 12U < VOX_WORLD_HEIGHT &&
              test_map_material_at(world, x, y + 12U) != VOX_MAT_LAVA &&
              test_map_material_at(world, x, y + 12U) != VOX_MAT_BEDROCK &&
              test_map_cell_is_solid(world, x, y + 12U)))) {
            return y;
        }
    }
    return VOX_WORLD_HEIGHT;
}

static vox_u32 test_map_outdoor_surface(const vox_world *world, vox_u32 x)
{
    vox_u32 surface = test_map_player_surface(world, x);
    if (surface > VOX_WORLD_HEIGHT / 2U + 32U) {
        return VOX_WORLD_HEIGHT;
    }
    return surface;
}

static vox_u32 test_map_count_material(const vox_world *world,
                                       vox_u16 material,
                                       vox_u32 minimum_y)
{
    vox_u32 count = 0U;
    vox_u32 x;
    vox_u32 y;
    for (y = minimum_y; y < VOX_WORLD_HEIGHT; ++y) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            if (test_map_material_at(world, x, y) == material) {
                count++;
            }
        }
    }
    return count;
}

static int test_map_walk_lane(const vox_world *world)
{
    vox_u32 x;
    vox_u32 previous_surface = VOX_WORLD_HEIGHT;
    vox_u32 gap = 0U;
    int found_land = 0;
    for (x = 2U; x + 2U < VOX_WORLD_WIDTH; ++x) {
        vox_u32 surface = test_map_terrain_surface(world, x);
        vox_u32 y;
        if (surface == VOX_WORLD_HEIGHT) {
            if (found_land) {
                gap++;
            }
            continue;
        }
        if (found_land && gap > 5U) {
            return 0;
        }
        if (gap == 0U && previous_surface != VOX_WORLD_HEIGHT &&
            (surface > previous_surface + 2U ||
             previous_surface > surface + 2U)) {
            return 0;
        }
        gap = 0U;
        found_land = 1;
        previous_surface = surface;
        if (surface < 8U) {
            return 0;
        }
        for (y = surface - 8U; y < surface; ++y) {
            if (test_map_cell_is_solid(world, x, y)) {
                return 0;
            }
        }
    }
    return found_land;
}

static int test_map_has_anchor_near(const vox_world *world, vox_u32 player_x)
{
    vox_u32 surface = test_map_outdoor_surface(world, player_x);
    vox_u32 min_x = player_x > TEST_MAP_ROPE_REACH ?
                    player_x - TEST_MAP_ROPE_REACH : 0U;
    vox_u32 max_x = player_x + TEST_MAP_ROPE_REACH < VOX_WORLD_WIDTH ?
                    player_x + TEST_MAP_ROPE_REACH : VOX_WORLD_WIDTH - 1U;
    vox_u32 player_y;
    vox_u32 x;
    vox_u32 y;
    if (surface == VOX_WORLD_HEIGHT || surface < 3U) {
        return 0;
    }
    player_y = surface - 1U;
    for (x = min_x; x <= max_x; ++x) {
        vox_u32 delta_x = x > player_x ? x - player_x : player_x - x;
        for (y = 1U; y + TEST_MAP_JUMP_ENVELOPE < surface; ++y) {
            vox_u16 material = test_map_material_at(world, x, y);
            vox_u32 delta_y = player_y > y ? player_y - y : y - player_y;
            vox_u32 largest = delta_x > delta_y ? delta_x : delta_y;
            vox_u32 smallest = delta_x > delta_y ? delta_y : delta_x;
            if (material == VOX_MAT_METAL &&
                largest + smallest / 2U <= TEST_MAP_ROPE_REACH) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_map_suspended_fixtures(const vox_world *world)
{
    vox_u32 overhead_metal = 0U;
    vox_u32 anchor_samples = 0U;
    vox_u32 land_columns = 0U;
    vox_u32 x;
    for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
        vox_u32 surface = test_map_outdoor_surface(world, x);
        vox_u32 vertical_run = 0U;
        vox_u32 y;
        if (surface == VOX_WORLD_HEIGHT) {
            continue;
        }
        land_columns++;
        if (surface <= TEST_MAP_JUMP_ENVELOPE) {
            return 0;
        }
        for (y = 1U; y + TEST_MAP_JUMP_ENVELOPE < surface; ++y) {
            if (test_map_material_at(world, x, y) == VOX_MAT_METAL) {
                if (y + TEST_MAP_RAIL_MIN_CLEARANCE > surface) {
                    return 0;
                }
                vertical_run++;
                overhead_metal++;
                if (vertical_run > 10U) {
                    return 0;
                }
            } else {
                vertical_run = 0U;
            }
        }
        for (y = surface - TEST_MAP_JUMP_ENVELOPE;
             y + 2U < surface; ++y) {
            if (test_map_cell_is_solid(world, x, y)) {
                return 0;
            }
        }
    }
    if (land_columns == 0U || overhead_metal < land_columns / 5U) {
        return 0;
    }
    for (x = 0U; x < VOX_WORLD_WIDTH; x += 8U) {
        if (test_map_outdoor_surface(world, x) != VOX_WORLD_HEIGHT) {
            anchor_samples++;
            if (!test_map_has_anchor_near(world, x)) {
                return 0;
            }
        }
    }
    return anchor_samples >= VOX_WORLD_WIDTH / 16U;
}

static int test_map_deepworks_connected(const vox_world *world)
{
    static unsigned char visited[VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT];
    static vox_u16 queue_x[VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT];
    static vox_u16 queue_y[VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT];
    vox_u32 minimum_y = VOX_WORLD_HEIGHT / 2U + 8U;
    vox_u32 head = 0U;
    vox_u32 tail = 0U;
    vox_u32 index;
    vox_u32 x;
    vox_u32 y;
    int reached_right = 0;
    int reached_shaft = 0;
    for (index = 0U; index < VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT; ++index) {
        visited[index] = 0U;
    }
    for (x = 7U; x < 16U && tail == 0U; ++x) {
        for (y = minimum_y; y + 4U < VOX_WORLD_HEIGHT; ++y) {
            vox_u16 material = test_map_material_at(world, x, y);
            if (material == VOX_MAT_AIR || material == VOX_MAT_FIREDAMP) {
                queue_x[tail] = (vox_u16)x;
                queue_y[tail] = (vox_u16)y;
                visited[y * VOX_WORLD_WIDTH + x] = 1U;
                tail++;
                break;
            }
        }
    }
    while (head < tail) {
        vox_u32 current_x = queue_x[head];
        vox_u32 current_y = queue_y[head];
        vox_i32 direction;
        head++;
        if (current_x >= VOX_WORLD_WIDTH - 8U) {
            reached_right = 1;
        }
        if (current_y == minimum_y) {
            reached_shaft = 1;
        }
        for (direction = 0; direction < 4; ++direction) {
            vox_i32 next_x = (vox_i32)current_x;
            vox_i32 next_y = (vox_i32)current_y;
            vox_u32 next_index;
            vox_u16 material;
            if (direction == 0) {
                next_x--;
            } else if (direction == 1) {
                next_x++;
            } else if (direction == 2) {
                next_y--;
            } else {
                next_y++;
            }
            if (next_x < 0 || next_y < (vox_i32)minimum_y ||
                next_x >= (vox_i32)VOX_WORLD_WIDTH ||
                next_y + 4L >= (vox_i32)VOX_WORLD_HEIGHT) {
                continue;
            }
            next_index = (vox_u32)next_y * VOX_WORLD_WIDTH +
                         (vox_u32)next_x;
            material = test_map_material_at(world, (vox_u32)next_x,
                                            (vox_u32)next_y);
            if (visited[next_index] == 0U &&
                (material == VOX_MAT_AIR ||
                 material == VOX_MAT_FIREDAMP)) {
                visited[next_index] = 1U;
                queue_x[tail] = (vox_u16)next_x;
                queue_y[tail] = (vox_u16)next_y;
                tail++;
            }
        }
    }
    return reached_right && reached_shaft;
}

static int test_map_macro_landform(const vox_world *world,
                                   vox_u16 map_style, vox_u32 seed)
{
    vox_u16 landform = vox_digs_map_landform(map_style, seed);
    vox_u32 x;
    vox_u32 land_columns = 0U;
    vox_u32 broad_gaps = 0U;
    vox_u32 gap = 0U;
    vox_u32 undercut_columns = 0U;
    vox_u32 left_peak = VOX_WORLD_HEIGHT;
    vox_u32 right_peak = VOX_WORLD_HEIGHT;
    vox_u32 saddle = VOX_WORLD_HEIGHT;
    int found_land = 0;
    if (landform >= VOX_DIGS_LANDFORM_COUNT ||
        test_map_terrain_surface(world, 0U) != VOX_WORLD_HEIGHT ||
        test_map_terrain_surface(world, VOX_WORLD_WIDTH - 1U) !=
        VOX_WORLD_HEIGHT) {
        return 0;
    }
    for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
        vox_u32 surface = test_map_terrain_surface(world, x);
        if (test_map_material_at(world, x, VOX_WORLD_HEIGHT - 5U) !=
            VOX_MAT_LAVA) {
            return 0;
        }
        if (surface == VOX_WORLD_HEIGHT) {
            if (found_land) {
                gap++;
            }
            continue;
        }
        if (found_land && gap >= 10U) {
            broad_gaps++;
        }
        gap = 0U;
        found_land = 1;
        land_columns++;
        if (surface + 72U < VOX_WORLD_HEIGHT - 5U &&
            test_map_material_at(world, x, surface + 72U) == VOX_MAT_AIR) {
            undercut_columns++;
        }
        if (x >= VOX_WORLD_WIDTH / 8U &&
            x < VOX_WORLD_WIDTH / 2U - 24U && surface < left_peak) {
            left_peak = surface;
        }
        if (x > VOX_WORLD_WIDTH / 2U + 24U &&
            x <= (VOX_WORLD_WIDTH * 7U) / 8U && surface < right_peak) {
            right_peak = surface;
        }
        if (x + 16U >= VOX_WORLD_WIDTH / 2U &&
            x <= VOX_WORLD_WIDTH / 2U + 16U && surface < saddle) {
            saddle = surface;
        }
    }
    if (land_columns < VOX_WORLD_WIDTH / 2U) {
        return 0;
    }
    if (landform == VOX_DIGS_LANDFORM_ARCHIPELAGO) {
        return broad_gaps == 3U &&
               undercut_columns >= VOX_WORLD_WIDTH / 2U;
    }
    if (broad_gaps != 0U || undercut_columns != 0U) {
        return 0;
    }
    if (landform == VOX_DIGS_LANDFORM_TWIN_HILLS &&
        (left_peak == VOX_WORLD_HEIGHT ||
         right_peak == VOX_WORLD_HEIGHT || saddle == VOX_WORLD_HEIGHT ||
         left_peak + 6U >= saddle || right_peak + 6U >= saddle)) {
        return 0;
    }
    return 1;
}

static int test_map_topology(void)
{
    vox_world *world = &match.world;
    static const vox_u32 seeds[3] = {
        0U, 1U, 2U
    };
    vox_u32 hashes[VOX_DIGS_MAP_COUNT];
    vox_u16 map_style;
    vox_u32 seed_index;
    for (map_style = VOX_DIGS_MAP_COAL_RIDGE;
         map_style < VOX_DIGS_MAP_COUNT; ++map_style) {
        for (seed_index = 0U; seed_index < 3U; ++seed_index) {
            vox_u32 first_hash;
            if (vox_digs_generate_map(world, map_style,
                                      seeds[seed_index]) != VOX_OK ||
                world->awake_cells != 0U) {
                return 1;
            }
            first_hash = vox_world_hash(world);
            if (vox_digs_generate_map(world, map_style,
                                      seeds[seed_index]) != VOX_OK ||
                world->awake_cells != 0U ||
                vox_world_hash(world) != first_hash) {
                return 2;
            }
            if (vox_digs_map_landform(map_style, seeds[seed_index]) !=
                seed_index ||
                !test_map_macro_landform(world, map_style,
                                         seeds[seed_index])) {
                return 14 + (int)(map_style * 3U + seed_index);
            }
        }
        if (vox_digs_generate_map(world, map_style, seeds[0]) != VOX_OK) {
            return 3;
        }
        hashes[map_style] = vox_world_hash(world);
        if (!test_map_suspended_fixtures(world)) {
            return 4 + (int)map_style;
        }
        if (map_style == VOX_DIGS_MAP_COAL_RIDGE) {
            if (test_map_count_material(world, VOX_MAT_COAL, 0U) < 500U ||
                test_map_count_material(world, VOX_MAT_SAND, 0U) < 50U ||
                test_map_count_material(world, VOX_MAT_FIREDAMP, 0U) != 0U ||
                test_map_count_material(world, VOX_MAT_LAVA, 0U) <
                VOX_WORLD_WIDTH * 8U) {
                return 10;
            }
        } else if (map_style == VOX_DIGS_MAP_DEEPWORKS) {
            if (test_map_count_material(world, VOX_MAT_FIREDAMP, 0U) < 40U ||
                test_map_count_material(world, VOX_MAT_AIR,
                                        VOX_WORLD_HEIGHT / 2U + 16U) < 900U ||
                test_map_count_material(world, VOX_MAT_LAVA, 0U) <
                VOX_WORLD_WIDTH * 8U) {
                return 11;
            }
        } else {
            vox_u32 lava = test_map_count_material(world, VOX_MAT_LAVA, 0U);
            if (lava <= VOX_WORLD_WIDTH * 20U ||
                test_map_count_material(world, VOX_MAT_METAL, 0U) < 500U ||
                test_map_count_material(world, VOX_MAT_FIREDAMP, 0U) != 0U) {
                return 12;
            }
        }
        if (vox_digs_generate_map(world, map_style, seeds[1]) != VOX_OK ||
            !test_map_walk_lane(world) ||
            (map_style == VOX_DIGS_MAP_DEEPWORKS &&
             !test_map_deepworks_connected(world))) {
            return 23 + (int)map_style;
        }
    }
    if (hashes[0] == hashes[1] || hashes[0] == hashes[2] ||
        hashes[1] == hashes[2]) {
        return 13;
    }
    return 0;
}

static int test_map_generation(void)
{
#define first match
#define second match_peer
#define variant match_peer
    vox_digs_rules rules;
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    int found_sand = 0;
    int found_firedamp = 0;
    int found_lava = 0;
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
                                  cell->material != VOX_MAT_SAND &&
                                  cell->material != VOX_MAT_LAVA &&
                                  cell->material != VOX_MAT_METAL)) {
                    return 3;
                }
                if (cell->material == VOX_MAT_SAND) {
                    found_sand = 1;
                }
                if (cell->material == VOX_MAT_LAVA) {
                    found_lava = 1;
                }
            }
        }
    }
    if (!found_sand || !found_lava) {
        return 4;
    }
    rules.map_style = VOX_DIGS_MAP_DEEPWORKS;
    if (vox_digs_match_init(&variant, &rules) != VOX_OK ||
        variant.terrain_hash == first.terrain_hash) {
        return 5;
    }
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (y = 0U; y < VOX_WORLD_HEIGHT; ++y) {
            for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
                const vox_cell *cell = vox_world_cell(&variant.world, x, y, z);
                if (cell != 0 && cell->material == VOX_MAT_FIREDAMP) {
                    found_firedamp = 1;
                }
            }
        }
    }
    if (!found_firedamp) {
        return 6;
    }
    rules.map_style = VOX_DIGS_MAP_COAL_RIDGE;
    rules.seed ^= 0x11111111U;
    if (vox_digs_match_init(&variant, &rules) != VOX_OK ||
        variant.terrain_hash == first.terrain_hash) {
        return 7;
    }
    return 0;
#undef variant
#undef second
#undef first
}

static int test_player_spawn_has_supported_floor(
    const vox_digs_match *match, vox_u16 player)
{
    const vox_physics_body *body = &match->players[player];
    vox_i32 bottom_q16 = body->position_y.value_q16 +
                         body->half_height_q16;
    vox_i32 center_x = body->position_x.value_q16 / 65536L;
    vox_i32 floor_y = bottom_q16 / 65536L;
    vox_i32 offset_x;
    vox_u32 distance;
    if (center_x <= 0 || center_x + 1L >= (vox_i32)VOX_WORLD_WIDTH ||
        floor_y < (vox_i32)TEST_SPAWN_HEADROOM_CELLS ||
        floor_y + (vox_i32)TEST_SPAWN_SUPPORT_CELLS >=
        (vox_i32)VOX_WORLD_HEIGHT) {
        return 0;
    }
    for (offset_x = -1; offset_x <= 1; ++offset_x) {
        vox_u32 sample_x = (vox_u32)(center_x + offset_x);
        for (distance = 1U; distance <= TEST_SPAWN_HEADROOM_CELLS;
             ++distance) {
            if (test_map_cell_is_solid(&match->world, sample_x,
                                       (vox_u32)floor_y - distance)) {
                return 0;
            }
        }
        for (distance = 0U; distance < TEST_SPAWN_SUPPORT_CELLS;
             ++distance) {
            if (!test_map_cell_is_solid(&match->world, sample_x,
                                        (vox_u32)floor_y + distance)) {
                return 0;
            }
        }
    }
    return 1;
}

static int test_player_layout_and_input_authority(void)
{
    vox_digs_rules rules;
    vox_digs_input first;
    vox_digs_input second;
    vox_u16 first_x;
    vox_u16 first_y;
    vox_u16 second_x;
    vox_u16 second_y;
    vox_u16 player;
    vox_digs_rules_classic(&rules);
    rules.player_count = 4U;
    rules.bot_mask = 0x000cU;
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        !vox_digs_player_is_active(&match, 0U) ||
        !vox_digs_player_is_active(&match, 3U) ||
        vox_digs_player_is_active(&match, 4U) ||
        vox_digs_player_is_bot(&match, 0U) ||
        vox_digs_player_is_bot(&match, 1U) ||
        !vox_digs_player_is_bot(&match, 2U) ||
        !vox_digs_player_is_bot(&match, 3U)) {
        return 1;
    }
    for (player = 0U; player < rules.player_count; ++player) {
        if (!test_player_spawn_has_supported_floor(&match, player)) {
            return 7;
        }
    }
    first_x = (vox_u16)(match.players[0].position_x.value_q16 / 65536L);
    first_y = (vox_u16)(match.players[0].position_y.value_q16 / 65536L);
    second_x = (vox_u16)(match.players[1].position_x.value_q16 / 65536L);
    second_y = (vox_u16)(match.players[1].position_y.value_q16 / 65536L);
    init_test_input(&first, 0U, first_x, first_y);
    init_test_input(&second, 1U, second_x, second_y);
    first.actions = VOX_DIGS_ACTION_RIGHT;
    first.move_x_q15 = 32767;
    second.actions = (vox_u16)(VOX_DIGS_ACTION_LEFT |
                                VOX_DIGS_ACTION_STEAM);
    second.move_x_q15 = -24576;
    second.move_y_q15 = 8192;
    if (vox_digs_submit_input(&match, &first) != VOX_OK ||
        vox_digs_submit_input(&match, &second) != VOX_OK ||
        match.player_actions[0] != VOX_DIGS_ACTION_RIGHT ||
        match.player_actions[1] != second.actions ||
        match.move_x_q15[0] != 32767 ||
        match.move_x_q15[1] != -24576 ||
        match.move_y_q15[0] != 0 || match.move_y_q15[1] != 8192) {
        return 2;
    }
    first.player = 2U;
    if (vox_digs_submit_input(&match, &first) != VOX_ERR_INVALID) {
        return 3;
    }
    rules.player_count = 3U;
    rules.bot_mask = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_ERR_INVALID) {
        return 4;
    }
    rules.player_count = 2U;
    rules.bot_mask = 0x0004U;
    if (vox_digs_match_init(&match, &rules) != VOX_ERR_INVALID) {
        return 5;
    }
    /*
     * One human against three bots is the headline single-player match, so
     * this must now succeed.  It was rejected while VOX_DIGS_MAX_BOTS was 2.
     */
    rules.player_count = 4U;
    rules.bot_mask = 0x000eU;
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_player_is_bot(&match, 0U) ||
        !vox_digs_player_is_bot(&match, 1U) ||
        !vox_digs_player_is_bot(&match, 2U) ||
        !vox_digs_player_is_bot(&match, 3U)) {
        return 6;
    }
    /* Every slot still needs somewhere real to stand. */
    {
        vox_u16 slot;
        for (slot = 0U; slot < 4U; ++slot) {
            if (!test_player_spawn_has_supported_floor(&match, slot)) {
                return 7;
            }
        }
    }
    /* A match with no human at all remains invalid. */
    rules.bot_mask = 0x000fU;
    if (vox_digs_match_init(&match, &rules) != VOX_ERR_INVALID) {
        return 8;
    }
    return 0;
}

static int test_spawn_shield_timing_and_attack_cancel(void)
{
    vox_digs_rules rules;
    vox_u32 tick;
    vox_u32 target_x;
    vox_u32 target_y;
    vox_i32 player_x;
    vox_i32 player_y;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        match.spawn_shield_ticks[0] != VOX_DIGS_SPAWN_SHIELD_TICKS ||
        vox_digs_consume_events(&match, match.event_count) != VOX_OK) {
        return 1;
    }
    if (vox_digs_apply_hit(&match, 1U, 0U, VOX_DIGS_TOOL_NAIL_GUN,
                           VOX_DIGS_PART_TORSO, 20U,
                           VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK ||
        match.health[0] != VOX_DIGS_MAX_HEALTH ||
        !event_type_seen(&match, VOX_DIGS_EVENT_SHIELD_BLOCK) ||
        vox_digs_consume_events(&match, match.event_count) != VOX_OK) {
        return 2;
    }
    for (tick = 0U; tick + 1U < VOX_DIGS_SPAWN_SHIELD_TICKS; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK ||
            match.spawn_shield_ticks[0] !=
                (vox_u16)(VOX_DIGS_SPAWN_SHIELD_TICKS - tick - 1U) ||
            event_type_seen(&match, VOX_DIGS_EVENT_SHIELD_END)) {
            return 3;
        }
    }
    if (match.spawn_shield_ticks[0] != 1U ||
        vox_digs_match_step(&match) != VOX_OK ||
        match.spawn_shield_ticks[0] != 0U ||
        !event_type_seen(&match, VOX_DIGS_EVENT_SHIELD_END)) {
        return 4;
    }
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_consume_events(&match, match.event_count) != VOX_OK) {
        return 5;
    }
    player_x = match.players[0].position_x.value_q16 / 65536L;
    player_y = match.players[0].position_y.value_q16 / 65536L;
    target_x = (vox_u32)(player_x + 8L < (vox_i32)VOX_WORLD_WIDTH ?
                         player_x + 8L : player_x - 8L);
    target_y = (vox_u32)(player_y > 4L ? player_y - 4L : player_y);
    if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_NAIL_GUN,
                             target_x, target_y) != VOX_OK ||
        match.spawn_shield_ticks[0] != 0U ||
        !event_type_seen(&match, VOX_DIGS_EVENT_SHIELD_END) ||
        !event_type_seen(&match, VOX_DIGS_EVENT_WEAPON_FIRE)) {
        return 6;
    }
    return 0;
}

static int test_rope_reel_anchor_and_events(void)
{
    vox_digs_rules rules;
    vox_digs_input input;
    vox_i32 player_x;
    vox_i32 player_y;
    vox_u32 anchor_x;
    vox_u32 anchor_y;
    vox_u32 y;
    vox_i32 attached_length;
    vox_i32 reeled_length;
    vox_u16 event_count;
    vox_u16 cast_tick;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_consume_events(&match, match.event_count) != VOX_OK) {
        return 1;
    }
    player_x = match.players[0].position_x.value_q16 / 65536L;
    player_y = match.players[0].position_y.value_q16 / 65536L;
    anchor_x = (vox_u32)player_x;
    anchor_y = (vox_u32)(player_y > 44L ? player_y - 40L : 1L);
    for (y = anchor_y + 1U; y < (vox_u32)player_y; ++y) {
        if (!set_test_column(&match.world, anchor_x, y, VOX_MAT_AIR)) {
            return 2;
        }
    }
    if (!set_test_column(&match.world, anchor_x, anchor_y, VOX_MAT_METAL) ||
        vox_world_sleep_all(&match.world) != VOX_OK) {
        return 3;
    }
    init_test_input(&input, 0U, (vox_u16)anchor_x, (vox_u16)anchor_y);
    input.actions = VOX_DIGS_ACTION_ROPE;
    if (vox_digs_submit_input(&match, &input) != VOX_OK) {
        return 4;
    }
    for (cast_tick = 0U; cast_tick < 24U &&
         match.ropes[0].state != VOX_DIGS_ROPE_ATTACHED; ++cast_tick) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 4;
        }
    }
    if (!match.ropes[0].active ||
        match.ropes[0].state != VOX_DIGS_ROPE_ATTACHED ||
        !event_type_seen(&match, VOX_DIGS_EVENT_ROPE_ATTACH)) {
        return 4;
    }
    attached_length = match.ropes[0].length_q16;
    input.move_y_q15 = -32767;
    if (vox_digs_submit_input(&match, &input) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK || !match.ropes[0].active ||
        match.ropes[0].length_q16 >= attached_length) {
        return 5;
    }
    reeled_length = match.ropes[0].length_q16;
    input.move_y_q15 = 32767;
    if (vox_digs_submit_input(&match, &input) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK || !match.ropes[0].active ||
        match.ropes[0].length_q16 <= reeled_length) {
        return 6;
    }
    if (!set_test_column(&match.world, anchor_x, anchor_y, VOX_MAT_AIR) ||
        !set_test_column(&match.world, anchor_x, anchor_y - 1U,
                         VOX_MAT_AIR) ||
        vox_world_sleep_all(&match.world) != VOX_OK) {
        return 7;
    }
    input.move_y_q15 = 0;
    if (vox_digs_submit_input(&match, &input) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK || match.ropes[0].active ||
        !event_type_seen(&match, VOX_DIGS_EVENT_ROPE_BREAK)) {
        return 8;
    }
    event_count = match.event_count;
    if (event_count == 0U ||
        vox_digs_consume_events(&match, event_count) != VOX_OK ||
        match.event_count != 0U || vox_digs_event_get(&match, 0U) != 0 ||
        vox_digs_consume_events(&match, 1U) != VOX_ERR_INVALID) {
        return 9;
    }
    return 0;
}

static int test_anatomy_bleed_cautery_and_sever(void)
{
    vox_digs_rules rules;
    vox_u16 health_after_hit;
    vox_digs_anatomy_part *part;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    match.spawn_shield_ticks[1] = 0U;
    if (vox_digs_consume_events(&match, match.event_count) != VOX_OK ||
        vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_NAIL_GUN,
                           VOX_DIGS_PART_LEFT_THIGH, 20U,
                           VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK) {
        return 2;
    }
    part = &match.anatomy[1][VOX_DIGS_PART_LEFT_THIGH];
    if (part->health != part->max_health - 20U ||
        !(part->flags & VOX_DIGS_PART_BLEEDING) ||
        (part->flags & VOX_DIGS_PART_CAUTERIZED) ||
        part->bleed_rate_q8 == 0U ||
        !event_type_seen(&match, VOX_DIGS_EVENT_DAMAGE)) {
        return 3;
    }
    health_after_hit = match.health[1];
    match.bleed_accumulator_q8[1] = 255U;
    if (vox_digs_match_step(&match) != VOX_OK ||
        match.health[1] >= health_after_hit ||
        !event_type_seen(&match, VOX_DIGS_EVENT_BLEED)) {
        return 4;
    }
    if (vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_CINDER_FLASK,
                           VOX_DIGS_PART_LEFT_THIGH, 1U,
                           VOX_DIGS_DAMAGE_HEAT) != VOX_OK ||
        !(part->flags & VOX_DIGS_PART_CAUTERIZED) ||
        (part->flags & VOX_DIGS_PART_BLEEDING) ||
        part->bleed_rate_q8 != 0U) {
        return 5;
    }
    part = &match.anatomy[1][VOX_DIGS_PART_RIGHT_HAND];
    if (vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_NAIL_GUN,
                           VOX_DIGS_PART_RIGHT_HAND, part->max_health,
                           VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK ||
        part->health != 0U || !(part->flags & VOX_DIGS_PART_SEVERED) ||
        !event_type_seen(&match, VOX_DIGS_EVENT_LIMB_SEVER) ||
        !match.alive[1]) {
        return 6;
    }
    return 0;
}

static int test_ai_state_machine(void)
{
    vox_digs_rules rules;
    vox_i32 bot_x;
    vox_i32 bot_y;
    vox_u32 player_x;
    vox_u32 wall_x;
    vox_u32 x;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0x0002U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_consume_events(&match, match.event_count) != VOX_OK) {
        return 1;
    }
    bot_x = match.players[1].position_x.value_q16 / 65536L;
    bot_y = match.players[1].position_y.value_q16 / 65536L;
    player_x = (vox_u32)(bot_x - 8L);
    match.players[0].position_x.value_q16 = (vox_i32)(player_x << 16);
    match.players[0].position_y.value_q16 = bot_y << 16;
    for (x = player_x; x <= (vox_u32)bot_x; ++x) {
        if (!set_test_column(&match.world, x, (vox_u32)bot_y,
                             VOX_MAT_AIR)) {
            return 2;
        }
    }
    match.weapon_cooldown[1] = 1U;
    match.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_think(&match, 1U) != VOX_OK ||
        match.bots[1].mode != VOX_DIGS_AI_ATTACKING ||
        !event_mode_seen(&match, VOX_DIGS_AI_ATTACKING)) {
        return 3;
    }
    match.health[1] = 20U;
    match.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_think(&match, 1U) != VOX_OK ||
        match.bots[1].mode != VOX_DIGS_AI_RETREATING ||
        !event_mode_seen(&match, VOX_DIGS_AI_RETREATING)) {
        return 4;
    }
    /*
     * A retreat is time-boxed rather than health-boxed, because nothing in the
     * simulation heals a living miner -- so restoring health here must NOT end
     * the withdrawal. The bot stays committed until the retreat window expires.
     */
    match.health[1] = VOX_DIGS_MAX_HEALTH;
    match.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_think(&match, 1U) != VOX_OK ||
        match.bots[1].mode != VOX_DIGS_AI_RETREATING) {
        return 6;
    }
    wall_x = (player_x + (vox_u32)bot_x) / 2U;
    if (!set_test_column(&match.world, wall_x, (vox_u32)bot_y,
                         VOX_MAT_METAL)) {
        return 7;
    }
    /* Expire the retreat window; only then may it downgrade. */
    match.bots[1].state_ticks = DIGS_TEST_RETREAT_EXPIRED_TICKS;
    match.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_think(&match, 1U) != VOX_OK ||
        match.bots[1].mode != VOX_DIGS_AI_SEARCHING ||
        !event_mode_seen(&match, VOX_DIGS_AI_SEARCHING)) {
        return 8;
    }
    /* Leaving a retreat arms a refractory period against retreating again. */
    if (match.bots[1].retreat_lock_ticks == 0U) {
        return 9;
    }
    /* Even at one health, the lock keeps it fighting rather than fleeing. */
    match.health[1] = 1U;
    match.bots[1].state_ticks = DIGS_TEST_RETREAT_EXPIRED_TICKS;
    match.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_think(&match, 1U) != VOX_OK ||
        match.bots[1].mode == VOX_DIGS_AI_RETREATING) {
        return 10;
    }
    match.health[1] = VOX_DIGS_MAX_HEALTH;
    match.bots[1].memory_ticks = 0U;
    match.bots[1].state_ticks = DIGS_TEST_RETREAT_EXPIRED_TICKS;
    match.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_think(&match, 1U) != VOX_OK ||
        match.bots[1].mode != VOX_DIGS_AI_ROAMING ||
        !event_mode_seen(&match, VOX_DIGS_AI_ROAMING)) {
        return 11;
    }
    return 0;
}

static int test_movement_acceleration_and_step_assist(void)
{
    vox_digs_rules rules;
    vox_digs_input input;
    vox_u32 x;
    vox_u32 z;
    vox_u32 tick;
    vox_i32 start_x;
    vox_i32 ground_y;
    vox_i32 min_y;
    vox_i32 peak_velocity;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    if (match.physics_config.max_step_q16 != (2L << 16)) {
        return 2;
    }
    vox_world_init(&match.world);
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            if (vox_world_set(&match.world, x, 100U, z, VOX_MAT_STONE,
                              20L << 16) != VOX_OK) {
                return 3;
            }
        }
        if (vox_world_set(&match.world, 49U, 98U, z, VOX_MAT_STONE,
                          20L << 16) != VOX_OK ||
            vox_world_set(&match.world, 49U, 99U, z, VOX_MAT_STONE,
                          20L << 16) != VOX_OK) {
            return 4;
        }
    }
    if (vox_world_sleep_all(&match.world) != VOX_OK) {
        return 5;
    }
    match.players[0].position_x.value_q16 = 46L << 16;
    ground_y = (100L << 16) - match.players[0].half_height_q16;
    match.players[0].position_y.value_q16 = ground_y;
    match.players[0].velocity_x.value_q16 = 0L;
    match.players[0].velocity_y.value_q16 = 0L;
    match.players[0].flags = VOX_PHYSICS_BODY_GROUNDED;
    init_test_input(&input, 0U, 60U, 90U);
    input.move_x_q15 = 32767;
    start_x = match.players[0].position_x.value_q16;
    min_y = ground_y;
    peak_velocity = 0L;
    for (tick = 0U; tick < 10U; ++tick) {
        if (vox_digs_submit_input(&match, &input) != VOX_OK ||
            vox_digs_match_step(&match) != VOX_OK) {
            return 6;
        }
        if (match.players[0].position_y.value_q16 < min_y) {
            min_y = match.players[0].position_y.value_q16;
        }
        if (match.players[0].velocity_x.value_q16 > peak_velocity) {
            peak_velocity = match.players[0].velocity_x.value_q16;
        }
    }
    if (match.players[0].position_x.value_q16 <= start_x + (3L << 16) ||
        min_y >= ground_y || peak_velocity <= 0L ||
        (match.players[0].flags & VOX_PHYSICS_BODY_BLOCKED_X)) {
        return 7;
    }
    input.move_x_q15 = 0;
    for (tick = 0U; tick < 8U; ++tick) {
        if (vox_digs_submit_input(&match, &input) != VOX_OK ||
            vox_digs_match_step(&match) != VOX_OK) {
            return 8;
        }
    }
    if (match.players[0].velocity_x.value_q16 != 0L) {
        return 9;
    }
    return 0;
}

static int run_match(vox_u32 *hash_out)
{
    vox_digs_rules rules;
    vox_u32 i;
    vox_digs_rules_classic(&rules);
    rules.seed = 0xA11CE001U;
    rules.player_count = 4U;
    rules.bot_mask = 0x000cU;
    rules.match_ticks = 600U;
    rules.lava_start_tick = 420U;
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
    vox_digs_rules rules;
    vox_u16 map_style;
    vox_u16 player;
    vox_u32 tick;
    vox_u32 occupied_before;
    vox_u32 terrain_hash;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
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
        for (player = 0U; player < rules.player_count; ++player) {
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
    vox_digs_rules rules;
    const vox_cell *cell;
    vox_u32 x;
    vox_u32 y;
    vox_u32 initial_hash;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
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
    vox_digs_rules rules;
    vox_digs_input input;
    vox_i32 start_x;
    vox_i32 start_y;
    vox_u16 steam_before;
    vox_u32 tick;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
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
    input.aim_x = (vox_u16)(match.players[0].position_x.value_q16 / 65536L);
    input.aim_y = (vox_u16)(match.players[0].position_y.value_q16 / 65536L);
    input.move_x_q15 = 0;
    input.move_y_q15 = 0;
    input.selected_weapon = VOX_DIGS_TOOL_PICK;
    input.reserved = 0U;
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
    /*
     * Speaking is a real action now, so the old literal 64 is valid.  Derive
     * the first bit above the mask instead of hardcoding another one, so the
     * next action added does not silently turn this back into a no-op.
     */
    input.actions = VOX_DIGS_ACTION_BARK;
    if (vox_digs_submit_input(&match, &input) != VOX_OK) {
        return 12;
    }
    input.actions = (vox_u16)(VOX_DIGS_ACTION_MASK + 1U);
    if (vox_digs_submit_input(&match, &input) != VOX_ERR_INVALID) {
        return 13;
    }
    return 0;
}

static int test_combat_and_respawn(void)
{
    vox_digs_rules rules;
    vox_u32 tick;
    vox_u16 effect;
    int found_flesh = 0;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0x0002U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        match.health[0] != VOX_DIGS_MAX_HEALTH ||
        match.health[1] != VOX_DIGS_MAX_HEALTH ||
        match.spawn_shield_ticks[1] != VOX_DIGS_SPAWN_SHIELD_TICKS) {
        return 1;
    }
    match.spawn_shield_ticks[1] = 0U;
    if (vox_digs_apply_damage(&match, 0U, 1U, 25U) != VOX_OK ||
        match.health[1] != 75U || match.last_attacker[1] != 0U ||
        match.effect_count == 0U) {
        return 2;
    }
    if (vox_digs_apply_damage(&match, 0U, 1U, 100U) != VOX_OK ||
        match.alive[1] || match.health[1] != 0U ||
        match.deaths[1] != 1U || match.scores[0] != 1U ||
        match.respawn_ticks[1] != VOX_DIGS_RESPAWN_TICKS) {
        return 3;
    }
    for (effect = 0U; effect < VOX_DIGS_MAX_EFFECTS; ++effect) {
        if (match.effects[effect].active &&
            match.effects[effect].material == VOX_MAT_FLESH) {
            found_flesh = 1;
        }
    }
    if (!found_flesh) {
        return 4;
    }
    for (tick = 0U; tick < VOX_DIGS_RESPAWN_TICKS; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 5;
        }
    }
    if (!match.alive[1] || match.health[1] != VOX_DIGS_MAX_HEALTH ||
        match.respawn_ticks[1] != 0U) {
        return 6;
    }
    match.spawn_shield_ticks[1] = 0U;
    if (vox_digs_apply_damage(&match, 0U, 1U, 1U) != VOX_OK ||
        vox_digs_apply_damage(&match, VOX_DIGS_NO_PLAYER, 1U, 200U) !=
        VOX_OK || match.alive[1] || match.scores[0] != 2U ||
        match.deaths[1] != 2U) {
        return 7;
    }
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 8;
    }
    match.spawn_shield_ticks[1] = 0U;
    if (
        vox_digs_apply_damage(&match, VOX_DIGS_NO_PLAYER, 1U, 200U) !=
            VOX_OK || match.alive[1] || match.scores[0] != 0U) {
        return 8;
    }
    return 0;
}

static int test_weapon_table_and_pool(void)
{
    vox_digs_rules rules;
    vox_u16 weapon;
    vox_i32 player_x;
    vox_i32 player_y;
    vox_u32 target_x;
    vox_u32 target_y;
    vox_u32 hash_before;
    vox_u16 shot;
    for (weapon = 0U; weapon < VOX_DIGS_TOOL_COUNT; ++weapon) {
        const vox_digs_weapon_properties *properties =
            vox_digs_weapon_get(weapon);
        if (properties == 0 || properties->name == 0 ||
            properties->name[0] == '\0' || properties->cooldown_ticks == 0U) {
            return 1;
        }
    }
    if (vox_digs_weapon_get(VOX_DIGS_TOOL_COUNT) != 0) {
        return 2;
    }
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    if (rules.weapon_mask != 0x07ffU) {
        return 3;
    }
    for (weapon = 0U; weapon < VOX_DIGS_TOOL_COUNT; ++weapon) {
        const vox_digs_weapon_properties *properties =
            vox_digs_weapon_get(weapon);
        if (vox_digs_match_init(&match, &rules) != VOX_OK) {
            return 4;
        }
        player_x = match.players[0].position_x.value_q16 / 65536L;
        player_y = match.players[0].position_y.value_q16 / 65536L;
        target_x = (vox_u32)(player_x + 8L < (vox_i32)VOX_WORLD_WIDTH ?
                             player_x + 8L : player_x - 8L);
        target_y = (vox_u32)(player_y > 6L ? player_y - 6L : player_y);
        if (properties->flags & VOX_DIGS_WEAPON_MELEE) {
            target_x = (vox_u32)(player_x + 2L);
            target_y = (vox_u32)player_y;
        }
        if (vox_digs_fire_weapon(&match, 0U, weapon,
                                 target_x, target_y) != VOX_OK ||
            match.selected_weapon[0] != weapon ||
            match.weapon_cooldown[0] == 0U) {
            return 5;
        }
        if ((properties->flags & VOX_DIGS_WEAPON_MELEE) &&
            match.projectile_count != 0U) {
            return 6;
        }
        if ((properties->flags & VOX_DIGS_WEAPON_PROJECTILE) &&
            match.projectile_count == 0U) {
            return 7;
        }
    }
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 8;
    }
    player_x = match.players[0].position_x.value_q16 / 65536L;
    player_y = match.players[0].position_y.value_q16 / 65536L;
    target_x = (vox_u32)(player_x + 8L < (vox_i32)VOX_WORLD_WIDTH ?
                         player_x + 8L : player_x - 8L);
    target_y = (vox_u32)(player_y > 6L ? player_y - 6L : player_y);
    if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_PRESSURE_HOSE,
                             target_x, target_y) != VOX_OK ||
        match.projectile_count != 1U ||
        match.selected_weapon[0] != VOX_DIGS_TOOL_PRESSURE_HOSE ||
        match.weapon_cooldown[0] == 0U) {
        return 9;
    }
    if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_PRESSURE_HOSE,
                             target_x, target_y) != VOX_ERR_INVALID) {
        return 10;
    }
    for (shot = 1U; shot < VOX_DIGS_MAX_PROJECTILES; ++shot) {
        match.weapon_cooldown[0] = 0U;
        if (vox_digs_fire_weapon(&match, 0U,
                                 VOX_DIGS_TOOL_PRESSURE_HOSE,
                                 target_x, target_y) != VOX_OK) {
            return 11;
        }
    }
    if (match.projectile_count != VOX_DIGS_MAX_PROJECTILES) {
        return 12;
    }
    match.weapon_cooldown[0] = 0U;
    hash_before = vox_digs_hash(&match);
    if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_PRESSURE_HOSE,
                             target_x, target_y) != VOX_ERR_CAPACITY ||
        match.projectile_count != VOX_DIGS_MAX_PROJECTILES ||
        vox_digs_hash(&match) != hash_before) {
        return 13;
    }
    rules.weapon_mask = (vox_u16)(1U << VOX_DIGS_TOOL_PICK);
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_PRESSURE_HOSE,
                             target_x, target_y) != VOX_ERR_INVALID) {
        return 14;
    }
    rules.weapon_mask = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_ERR_INVALID) {
        return 15;
    }
    return 0;
}

static int test_bot_authority(void)
{
    vox_digs_rules rules;
    vox_i32 bot_x;
    vox_i32 bot_y;
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    vox_u32 tick;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0x0002U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    bot_x = match.players[1].position_x.value_q16 / 65536L;
    bot_y = match.players[1].position_y.value_q16 / 65536L;
    match.players[0].position_x.value_q16 = (bot_x - 2L) << 16;
    match.players[0].position_y.value_q16 =
        match.players[1].position_y.value_q16;
    match.players[0].velocity_x.value_q16 = 0L;
    match.players[0].velocity_y.value_q16 = 0L;
    match.health[0] = 10U;
    match.spawn_shield_ticks[0] = 0U;
    for (y = (vox_u32)(bot_y - 2L); y <= (vox_u32)(bot_y + 1L); ++y) {
        for (x = (vox_u32)(bot_x - 4L); x <= (vox_u32)(bot_x + 1L); ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                (void)vox_world_set(&match.world, x, y, z,
                                    VOX_MAT_AIR, 20L << 16);
            }
        }
    }
    match.tick = 7U;
    match.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_think(&match, 1U) != VOX_OK ||
        match.move_x_q15[1] == 0 ||
        (match.player_actions[1] & VOX_DIGS_ACTION_FIRE) == 0U) {
        return 2;
    }
    for (tick = 0U; tick < 6U && match.scores[1] == 0U; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 3;
        }
    }
    if (match.scores[1] == 0U || match.alive[0]) {
        return 4;
    }
    if (vox_digs_bot_think(&match, 0U) != VOX_ERR_INVALID) {
        return 5;
    }
    return 0;
}

static int test_rising_lava(void)
{
    vox_digs_rules rules;
    const vox_cell *cell;
    vox_u32 tick;
    vox_u32 carve_x;
    int lava_found = 0;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    rules.match_ticks = 180U;
    rules.lava_start_tick = 60U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    for (tick = 0U; tick < 62U; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 2;
        }
    }
    if (match.lava_level_q16 == 0U ||
        match.lava_surface_y >= VOX_WORLD_HEIGHT - 4U) {
        return 3;
    }
    for (carve_x = 0U; carve_x < VOX_WORLD_WIDTH; ++carve_x) {
        cell = vox_world_cell(&match.world, carve_x,
                              match.lava_surface_y,
                              VOX_WORLD_DEPTH - 1U);
        if (cell != 0 && cell->material == VOX_MAT_LAVA) {
            lava_found = 1;
            break;
        }
    }
    if (!lava_found) {
        return 4;
    }
    for (carve_x = VOX_WORLD_WIDTH / 2U - 1U;
         carve_x <= VOX_WORLD_WIDTH / 2U; ++carve_x) {
        for (tick = 0U; tick < VOX_WORLD_DEPTH; ++tick) {
            (void)vox_world_set(&match.world, carve_x,
                                match.lava_surface_y - 1U, tick,
                                VOX_MAT_AIR, 20L << 16);
            (void)vox_world_set(&match.world, carve_x,
                                match.lava_surface_y - 2U, tick,
                                VOX_MAT_AIR, 20L << 16);
        }
    }
    match.players[0].position_x.value_q16 =
        (vox_i32)(VOX_WORLD_WIDTH / 2U) << 16;
    match.players[0].position_y.value_q16 =
        (vox_i32)(match.lava_surface_y - 1U) << 16;
    match.players[0].velocity_x.value_q16 = 0L;
    match.players[0].velocity_y.value_q16 = 0L;
    match.health[0] = VOX_DIGS_MAX_HEALTH;
    match.spawn_shield_ticks[0] = 0U;
    if (vox_digs_match_step(&match) != VOX_OK ||
        match.health[0] >= VOX_DIGS_MAX_HEALTH) {
        return 5;
    }
    return 0;
}

static int test_rule_bounds_and_long_lava(void)
{
    vox_digs_rules rules;
    vox_u32 first_level;
    vox_digs_rules_classic(&rules);
    rules.score_limit = 65536U;
    if (vox_digs_match_init(&match, &rules) != VOX_ERR_INVALID) {
        return 1;
    }
    rules.score_limit = 0U;
    rules.respawn_mode = 2U;
    if (vox_digs_match_init(&match, &rules) != VOX_ERR_INVALID) {
        return 2;
    }
    rules.respawn_mode = VOX_DIGS_RESPAWN_AUTO;
    rules.respawn_delay_ticks = 3601U;
    if (vox_digs_match_init(&match, &rules) != VOX_ERR_INVALID) {
        return 3;
    }
    rules.respawn_delay_ticks = VOX_DIGS_RESPAWN_TICKS;
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    rules.match_ticks = 100000U;
    rules.lava_start_tick = 1U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 4;
    }
    match.tick = 65537U;
    if (vox_digs_match_step(&match) != VOX_OK ||
        match.lava_level_q16 < 40000U) {
        return 5;
    }
    first_level = match.lava_level_q16;
    if (vox_digs_match_step(&match) != VOX_OK ||
        match.lava_level_q16 < first_level) {
        return 6;
    }
    return 0;
}

static int test_bot_score_limit_step(void)
{
    vox_digs_rules rules;
    vox_i32 bot_x;
    vox_i32 bot_y;
    vox_digs_rules_classic(&rules);
    rules.player_count = 3U;
    rules.bot_mask = 0x0006U;
    rules.score_limit = 1U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    bot_x = match.players[1].position_x.value_q16;
    bot_y = match.players[1].position_y.value_q16;
    match.players[0].position_x.value_q16 = bot_x - (2L << 16);
    match.players[0].position_y.value_q16 = bot_y;
    match.players[0].velocity_x.value_q16 = 0L;
    match.players[0].velocity_y.value_q16 = 0L;
    match.health[0] = 1U;
    match.spawn_shield_ticks[0] = 0U;
    match.bots[1].decision_ticks = 0U;
    match.bots[2].decision_ticks = 0U;
    match.tick = 847U;
    if (vox_digs_match_step(&match) != VOX_OK ||
        match.phase != VOX_DIGS_RESULTS || match.scores[1] != 1U) {
        return 2;
    }
    return 0;
}

static int test_respawn_modes_and_requests(void)
{
    vox_digs_rules rules;
    vox_u32 tick;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    rules.respawn_delay_ticks = 2U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    match.spawn_shield_ticks[1] = 0U;
    if (vox_digs_record_kill(&match, 0U, 1U) != VOX_OK ||
        match.respawn_ticks[1] != 2U || match.respawn_ready[1] ||
        match.respawn_target_x_q16[1] <= 0L ||
        vox_digs_request_respawn(&match, 1U) != VOX_ERR_INVALID) {
        return 2;
    }
    for (tick = 0U; tick < 2U; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 3;
        }
    }
    if (!match.alive[1] || match.respawn_ticks[1] != 0U ||
        match.respawn_ready[1] || match.respawn_requested[1]) {
        return 4;
    }

    rules.respawn_mode = VOX_DIGS_RESPAWN_ON_FIRE;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 5;
    }
    match.spawn_shield_ticks[1] = 0U;
    if (vox_digs_record_kill(&match, 0U, 1U) != VOX_OK) {
        return 6;
    }
    for (tick = 0U; tick < 2U; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 7;
        }
    }
    if (match.alive[1] || !match.respawn_ready[1] ||
        !event_type_seen(&match, VOX_DIGS_EVENT_RESPAWN_READY) ||
        vox_digs_request_respawn(&match, 1U) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK || !match.alive[1]) {
        return 8;
    }

    rules.respawn_mode = VOX_DIGS_RESPAWN_AUTO;
    rules.respawn_delay_ticks = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 9;
    }
    match.spawn_shield_ticks[1] = 0U;
    if (vox_digs_record_kill(&match, 0U, 1U) != VOX_OK ||
        match.alive[1] || match.respawn_ready[1] ||
        vox_digs_match_step(&match) != VOX_OK || !match.alive[1]) {
        return 10;
    }

    rules.respawn_mode = VOX_DIGS_RESPAWN_ON_FIRE;
    rules.respawn_delay_ticks = 1U;
    rules.bot_mask = 0x0002U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 11;
    }
    match.spawn_shield_ticks[1] = 0U;
    if (vox_digs_record_kill(&match, 0U, 1U) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK || !match.alive[1]) {
        return 12;
    }
    return 0;
}

static int test_match_results_and_final_batch(void)
{
    vox_digs_rules rules;
    vox_u16 ordinal;
    vox_u16 end_events;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    rules.match_ticks = 1U;
    rules.lava_start_tick = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    match.scores[0] = 3U;
    match.scores[1] = 2U;
    if (vox_digs_match_step(&match) != VOX_OK ||
        match.phase != VOX_DIGS_RESULTS ||
        match.result_reason != VOX_DIGS_END_TIME || match.result_draw ||
        match.winner_player != 0U) {
        return 2;
    }
    end_events = 0U;
    for (ordinal = 0U; ordinal < match.event_count; ++ordinal) {
        const vox_digs_event *event = vox_digs_event_get(&match, ordinal);
        if (event != 0 && event->type == VOX_DIGS_EVENT_MATCH_END) {
            end_events++;
        }
    }
    if (end_events != 1U || vox_digs_match_step(&match) != VOX_ERR_INVALID) {
        return 3;
    }

    rules.player_count = 4U;
    rules.bot_mask = 0x000cU;
    rules.score_limit = 1U;
    rules.match_ticks = 600U;
    rules.lava_start_tick = 500U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_record_kill(&match, 0U, 2U) != VOX_OK ||
        match.phase != VOX_DIGS_RUNNING ||
        vox_digs_record_kill(&match, 1U, 3U) != VOX_OK ||
        match.phase != VOX_DIGS_RUNNING ||
        vox_digs_match_step(&match) != VOX_OK ||
        match.phase != VOX_DIGS_RESULTS ||
        match.result_reason != VOX_DIGS_END_SCORE || !match.result_draw ||
        match.winner_player != VOX_DIGS_NO_PLAYER) {
        return 4;
    }

    rules.player_count = 2U;
    rules.bot_mask = 0x0002U;
    return 0;
}

static int test_last_attacker_expiry(void)
{
    vox_digs_rules rules;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    match.spawn_shield_ticks[1] = 0U;
    if (vox_digs_apply_damage(&match, 0U, 1U, 1U) != VOX_OK) {
        return 2;
    }
    match.tick = VOX_DIGS_LAST_ATTACKER_TICKS;
    if (vox_digs_apply_damage(&match, VOX_DIGS_NO_PLAYER, 1U, 200U) !=
            VOX_OK || match.scores[0] != 1U) {
        return 3;
    }
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 4;
    }
    match.spawn_shield_ticks[1] = 0U;
    if (vox_digs_apply_damage(&match, 0U, 1U, 1U) != VOX_OK) {
        return 5;
    }
    match.tick = VOX_DIGS_LAST_ATTACKER_TICKS + 1U;
    if (vox_digs_apply_damage(&match, VOX_DIGS_NO_PLAYER, 1U, 200U) !=
            VOX_OK || match.scores[0] != 0U || match.alive[1]) {
        return 6;
    }
    return 0;
}

static int test_projectile_owner_clearance(void)
{
    vox_digs_rules rules;
    vox_i32 player_x;
    vox_i32 player_y;
    vox_u32 target_x;
    vox_u32 target_y;
    vox_u16 slot;
    vox_u16 found;
    vox_u16 health_before;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    player_x = match.players[0].position_x.value_q16 / 65536L;
    player_y = match.players[0].position_y.value_q16 / 65536L;
    target_x = (vox_u32)(player_x + 12L < (vox_i32)VOX_WORLD_WIDTH ?
                         player_x + 12L : player_x - 12L);
    target_y = (vox_u32)(player_y > 10L ? player_y - 10L : 0L);
    if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_CONCUSSION_GRENADE,
                             target_x, target_y) != VOX_OK) {
        return 2;
    }
    found = 0U;
    slot = 0U;
    while (slot < VOX_DIGS_MAX_PROJECTILES) {
        if (match.projectiles[slot].active) {
            found = 1U;
            break;
        }
        slot++;
    }
    if (!found || !match.projectiles[slot].owner_clear ||
        match.projectiles[slot].arming_ticks !=
            VOX_DIGS_PROJECTILE_OWNER_CLEAR_TICKS ||
        (match.projectiles[slot].position_x_q16 ==
             match.players[0].position_x.value_q16 &&
         match.projectiles[slot].position_y_q16 ==
             match.players[0].position_y.value_q16) ||
        match.projectiles[slot].position_x_q16 <
            match.projectiles[slot].launch_min_x_q16 ||
        match.projectiles[slot].position_x_q16 >
            match.projectiles[slot].launch_max_x_q16 ||
        match.projectiles[slot].position_y_q16 <
            match.projectiles[slot].launch_min_y_q16 ||
        match.projectiles[slot].position_y_q16 >
            match.projectiles[slot].launch_max_y_q16) {
        return 3;
    }
    health_before = match.health[0];
    if (vox_digs_match_step(&match) != VOX_OK ||
        match.health[0] != health_before || !match.projectiles[slot].active ||
        match.projectiles[slot].arming_ticks !=
            VOX_DIGS_PROJECTILE_OWNER_CLEAR_TICKS - 1U) {
        return 4;
    }
    if (match.projectiles[slot].active) {
        match.projectiles[slot].position_x_q16 =
            match.players[0].position_x.value_q16;
        match.projectiles[slot].position_y_q16 =
            match.players[0].position_y.value_q16;
        match.projectiles[slot].velocity_x_q16 = 0L;
        match.projectiles[slot].velocity_y_q16 = 0L;
        match.projectiles[slot].owner_clear = 0U;
        match.projectiles[slot].fuse_ticks = 1U;
        if (vox_digs_match_step(&match) != VOX_OK ||
            match.health[0] != health_before) {
            return 5;
        }
    } else {
        return 6;
    }
    match.weapon_cooldown[0] = 0U;
    if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_CONCUSSION_GRENADE,
                             target_x, target_y) != VOX_OK) {
        return 7;
    }
    found = 0U;
    for (slot = 0U; slot < VOX_DIGS_MAX_PROJECTILES; ++slot) {
        if (match.projectiles[slot].active) {
            found = 1U;
            break;
        }
    }
    if (!found) {
        return 8;
    }
    match.projectiles[slot].position_x_q16 =
        match.players[0].position_x.value_q16;
    match.projectiles[slot].position_y_q16 =
        match.players[0].position_y.value_q16;
    match.projectiles[slot].velocity_x_q16 = 0L;
    match.projectiles[slot].velocity_y_q16 = 0L;
    match.projectiles[slot].owner_clear = 0U;
    match.projectiles[slot].arming_ticks = 0U;
    match.projectiles[slot].fuse_ticks = 1U;
    if (vox_digs_match_step(&match) != VOX_OK ||
        match.health[0] >= health_before) {
        return 9;
    }
    return 0;
}

#define v003_match_a match
#define v003_match_b match_peer

static int v003_clear_box(vox_world *world, vox_u32 left, vox_u32 top,
                          vox_u32 right, vox_u32 bottom)
{
    vox_u32 x;
    vox_u32 y;
    for (y = top; y <= bottom; ++y) {
        for (x = left; x <= right; ++x) {
            if (!set_test_column(world, x, y, VOX_MAT_AIR)) {
                return 0;
            }
        }
    }
    return 1;
}

static void v003_place_player(vox_digs_match *match, vox_u16 player,
                              vox_u32 x, vox_u32 y)
{
    match->players[player].position_x.value_q16 =
        (vox_i32)(x << 16) + 32768L;
    match->players[player].position_y.value_q16 =
        (vox_i32)(y << 16) + 32768L;
    match->players[player].velocity_x.value_q16 = 0L;
    match->players[player].velocity_y.value_q16 = 0L;
    match->players[player].flags = 0U;
}

static int test_v003_event_drain_ai_invariance(void)
{
    vox_digs_rules rules;
    vox_u32 y;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0x0002U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK ||
        !v003_clear_box(&v003_match_a.world, 70U, 84U, 130U, 112U) ||
        vox_digs_consume_events(&v003_match_a,
                                v003_match_a.event_count) != VOX_OK) {
        return 1;
    }
    v003_place_player(&v003_match_a, 0U, 80U, 100U);
    v003_place_player(&v003_match_a, 1U, 120U, 100U);
    for (y = 88U; y <= 108U; ++y) {
        if (!set_test_column(&v003_match_a.world, 100U, y,
                             VOX_MAT_BEDROCK)) {
            return 2;
        }
    }
    if (vox_world_sleep_all(&v003_match_a.world) != VOX_OK ||
        vox_digs_fire_weapon(&v003_match_a, 0U, VOX_DIGS_TOOL_NAIL_GUN,
                             90U, 100U) != VOX_OK) {
        return 3;
    }
    v003_match_b = v003_match_a;
    if (vox_digs_consume_events(&v003_match_b,
                                v003_match_b.event_count) != VOX_OK) {
        return 4;
    }
    v003_match_a.bots[1].decision_ticks = 0U;
    v003_match_b.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_think(&v003_match_a, 1U) != VOX_OK ||
        vox_digs_bot_think(&v003_match_b, 1U) != VOX_OK ||
        v003_match_a.bots[1].mode != VOX_DIGS_AI_SEARCHING ||
        v003_match_b.bots[1].mode != VOX_DIGS_AI_SEARCHING ||
        v003_match_a.bots[1].target != 0U ||
        v003_match_b.bots[1].target != 0U ||
        v003_match_a.state_hash != v003_match_b.state_hash) {
        return 5;
    }
    /* A retained sound older than two seconds must not refresh memory. */
    v003_match_a.tick += 121U;
    v003_match_a.bots[1].memory_ticks = 0U;
    v003_match_a.bots[1].target = VOX_DIGS_NO_PLAYER;
    v003_match_a.bots[1].decision_ticks = 0U;
    /* Standing down is a drop in alertness, so it waits out the dwell. */
    v003_match_a.bots[1].state_ticks = DIGS_TEST_RETREAT_EXPIRED_TICKS;
    if (vox_digs_bot_think(&v003_match_a, 1U) != VOX_OK ||
        v003_match_a.bots[1].mode != VOX_DIGS_AI_ROAMING) {
        return 6;
    }
    return 0;
}

static int test_v003_rope_wrap_unwrap_replay(void)
{
    vox_digs_rules rules;
    vox_digs_input input;
    vox_u16 tick;
    vox_u32 x;
    vox_u32 y;
    vox_i32 wrap_x;
    vox_i32 wrap_y;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK ||
        !v003_clear_box(&v003_match_a.world, 80U, 60U, 150U, 116U)) {
        return 1;
    }
    v003_place_player(&v003_match_a, 0U, 100U, 104U);
    for (x = 80U; x <= 150U; ++x) {
        if (!set_test_column(&v003_match_a.world, x, 106U,
                             VOX_MAT_BEDROCK)) {
            return 2;
        }
    }
    if (!set_test_column(&v003_match_a.world, 136U, 84U,
                         VOX_MAT_BEDROCK) ||
        vox_world_sleep_all(&v003_match_a.world) != VOX_OK) {
        return 3;
    }
    init_test_input(&input, 0U, 136U, 84U);
    input.actions = VOX_DIGS_ACTION_ROPE;
    if (vox_digs_submit_input(&v003_match_a, &input) != VOX_OK) {
        return 4;
    }
    for (tick = 0U; tick < 20U &&
         v003_match_a.ropes[0].state != VOX_DIGS_ROPE_ATTACHED; ++tick) {
        if (vox_digs_match_step(&v003_match_a) != VOX_OK) {
            return 5;
        }
    }
    if (v003_match_a.ropes[0].state != VOX_DIGS_ROPE_ATTACHED ||
        v003_match_a.ropes[0].point_count != 1U) {
        return 6;
    }
    v003_match_b = v003_match_a;
    for (y = 89U; y <= 99U; ++y) {
        if (!set_test_column(&v003_match_a.world, 118U, y,
                             VOX_MAT_BEDROCK) ||
            !set_test_column(&v003_match_b.world, 118U, y,
                             VOX_MAT_BEDROCK)) {
            return 7;
        }
    }
    if (vox_world_sleep_all(&v003_match_a.world) != VOX_OK ||
        vox_world_sleep_all(&v003_match_b.world) != VOX_OK ||
        vox_digs_match_step(&v003_match_a) != VOX_OK ||
        vox_digs_match_step(&v003_match_b) != VOX_OK ||
        v003_match_a.state_hash != v003_match_b.state_hash ||
        v003_match_a.ropes[0].point_count != 2U ||
        v003_match_b.ropes[0].point_count != 2U) {
        fprintf(stderr, "rope wrap debug state=%u points=%u hash=%08x/%08x "
                "pos=%ld,%ld anchor=%ld,%ld\n",
                (unsigned int)v003_match_a.ropes[0].state,
                (unsigned int)v003_match_a.ropes[0].point_count,
                (unsigned int)v003_match_a.state_hash,
                (unsigned int)v003_match_b.state_hash,
                (long)v003_match_a.players[0].position_x.value_q16,
                (long)v003_match_a.players[0].position_y.value_q16,
                (long)v003_match_a.ropes[0].anchor_x_q16,
                (long)v003_match_a.ropes[0].anchor_y_q16);
        return 8;
    }
    wrap_x = v003_match_a.ropes[0].points[1].position_x_q16 >> 16;
    wrap_y = v003_match_a.ropes[0].points[1].position_y_q16 >> 16;
    if (wrap_x < 0L || wrap_y < 0L ||
        vox_world_collision_classify(&v003_match_a.world,
                                     (vox_u32)wrap_x,
                                     (vox_u32)wrap_y) ==
            VOX_WORLD_COLLISION_SOLID) {
        return 9;
    }
    for (y = 89U; y <= 99U; ++y) {
        if (!set_test_column(&v003_match_a.world, 118U, y, VOX_MAT_AIR) ||
            !set_test_column(&v003_match_b.world, 118U, y, VOX_MAT_AIR)) {
            return 10;
        }
    }
    if (vox_world_sleep_all(&v003_match_a.world) != VOX_OK ||
        vox_world_sleep_all(&v003_match_b.world) != VOX_OK ||
        vox_digs_match_step(&v003_match_a) != VOX_OK ||
        vox_digs_match_step(&v003_match_b) != VOX_OK ||
        v003_match_a.state_hash != v003_match_b.state_hash ||
        v003_match_a.ropes[0].state != VOX_DIGS_ROPE_ATTACHED ||
        v003_match_a.ropes[0].point_count != 1U) {
        fprintf(stderr, "rope unwrap debug state=%u points=%u integrity=%u "
                "tension=%ld hash=%08x/%08x pos=%ld,%ld\n",
                (unsigned int)v003_match_a.ropes[0].state,
                (unsigned int)v003_match_a.ropes[0].point_count,
                (unsigned int)v003_match_a.ropes[0].integrity,
                (long)v003_match_a.ropes[0].tension_q16,
                (unsigned int)v003_match_a.state_hash,
                (unsigned int)v003_match_b.state_hash,
                (long)v003_match_a.players[0].position_x.value_q16,
                (long)v003_match_a.players[0].position_y.value_q16);
        return 11;
    }
    return 0;
}

static int test_v003_rail_strata_and_shield(void)
{
    vox_digs_rules rules;
    vox_u32 x;
    vox_u32 z;
    const vox_cell *cell;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK ||
        !v003_clear_box(&v003_match_a.world, 70U, 82U, 145U, 112U)) {
        return 1;
    }
    v003_place_player(&v003_match_a, 0U, 80U, 100U);
    v003_place_player(&v003_match_a, 1U, 120U, 100U);
    v003_match_a.spawn_shield_ticks[1] = 0U;
    for (x = 95U; x <= 97U; ++x) {
        if (!set_test_column(&v003_match_a.world, x, 100U,
                             VOX_MAT_STONE)) {
            return 2;
        }
    }
    if (vox_world_sleep_all(&v003_match_a.world) != VOX_OK ||
        vox_digs_fire_weapon(&v003_match_a, 0U, VOX_DIGS_TOOL_RAIL_GUN,
                             140U, 100U) != VOX_OK) {
        return 3;
    }
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        cell = vox_world_cell(&v003_match_a.world, 95U, 100U, z);
        if (cell == 0 || cell->material != VOX_MAT_AIR) {
            return 4;
        }
        cell = vox_world_cell(&v003_match_a.world, 96U, 100U, z);
        if (cell == 0 || cell->material != VOX_MAT_AIR) {
            return 5;
        }
        cell = vox_world_cell(&v003_match_a.world, 97U, 100U, z);
        if (cell == 0 || cell->material != VOX_MAT_STONE) {
            return 6;
        }
    }
    if (v003_match_a.health[1] != VOX_DIGS_MAX_HEALTH ||
        !v003_match_a.alive[1]) {
        return 7;
    }
    if (!set_test_column(&v003_match_a.world, 97U, 100U, VOX_MAT_AIR)) {
        return 8;
    }
    v003_match_a.weapon_cooldown[0] = 0U;
    if (vox_digs_fire_weapon(&v003_match_a, 0U, VOX_DIGS_TOOL_RAIL_GUN,
                             140U, 100U) != VOX_OK ||
        v003_match_a.alive[1] ||
        v003_match_a.last_damage_part[1] == VOX_DIGS_NO_PART ||
        !event_type_seen(&v003_match_a, VOX_DIGS_EVENT_RAIL_TRACE)) {
        return 9;
    }
    if (vox_digs_match_init(&v003_match_b, &rules) != VOX_OK ||
        !v003_clear_box(&v003_match_b.world, 70U, 82U, 145U, 112U)) {
        return 10;
    }
    v003_place_player(&v003_match_b, 0U, 80U, 100U);
    v003_place_player(&v003_match_b, 1U, 120U, 100U);
    if (vox_digs_fire_weapon(&v003_match_b, 0U,
                             VOX_DIGS_TOOL_RAIL_GUN,
                             140U, 100U) != VOX_OK ||
        !v003_match_b.alive[1] ||
        v003_match_b.health[1] != VOX_DIGS_MAX_HEALTH ||
        !event_type_seen(&v003_match_b, VOX_DIGS_EVENT_SHIELD_BLOCK)) {
        return 11;
    }
    return 0;
}

static int test_v003_overlap_recovery_and_crush(void)
{
    vox_digs_rules rules;
    vox_i32 origin_x;
    vox_i32 origin_y;
    vox_i32 center_x;
    vox_i32 center_y;
    vox_i32 x;
    vox_i32 y;
    vox_u16 deaths;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK) return 1;
    origin_x = v003_match_a.players[0].position_x.value_q16;
    origin_y = v003_match_a.players[0].position_y.value_q16;
    center_x = origin_x >> 16;
    center_y = origin_y >> 16;
    if (!set_test_column(&v003_match_a.world, (vox_u32)center_x,
                         (vox_u32)center_y, VOX_MAT_STONE) ||
        vox_world_sleep_all(&v003_match_a.world) != VOX_OK ||
        vox_digs_match_step(&v003_match_a) != VOX_OK ||
        !v003_match_a.alive[0] || v003_match_a.deaths[0] != 0U ||
        (v003_match_a.players[0].flags &
         VOX_PHYSICS_BODY_RECOVERED) == 0U ||
        (v003_match_a.players[0].position_x.value_q16 - origin_x >
             (2L << 16)) ||
        (origin_x - v003_match_a.players[0].position_x.value_q16 >
             (2L << 16)) ||
        (v003_match_a.players[0].position_y.value_q16 - origin_y >
             (2L << 16)) ||
        (origin_y - v003_match_a.players[0].position_y.value_q16 >
             (2L << 16))) {
        return 2;
    }
    center_x = v003_match_a.players[0].position_x.value_q16 >> 16;
    center_y = v003_match_a.players[0].position_y.value_q16 >> 16;
    for (y = center_y - 5L; y <= center_y + 5L; ++y) {
        for (x = center_x - 5L; x <= center_x + 5L; ++x) {
            if (x >= 0L && y >= 0L &&
                !set_test_column(&v003_match_a.world, (vox_u32)x,
                                 (vox_u32)y, VOX_MAT_STONE)) {
                return 3;
            }
        }
    }
    deaths = v003_match_a.deaths[0];
    v003_match_a.spawn_shield_ticks[0] = 0U;
    /*
     * Full burial is survivable for a bounded window rather than instantly
     * fatal.  The first entombed tick announces itself and starts hurting;
     * the miner keeps their controls and can dig free.
     */
    if (vox_world_sleep_all(&v003_match_a.world) != VOX_OK ||
        vox_digs_match_step(&v003_match_a) != VOX_OK ||
        !v003_match_a.alive[0] ||
        v003_match_a.deaths[0] != deaths ||
        v003_match_a.buried_ticks[0] != 1U ||
        v003_match_a.health[0] >= VOX_DIGS_MAX_HEALTH ||
        !event_type_seen(&v003_match_a, VOX_DIGS_EVENT_CRUSH)) {
        return 4;
    }
    /* Crush pressure kills if the miner cannot escape it. */
    {
        vox_u16 guard;
        for (guard = 0U; guard < DIGS_TEST_BURIED_GUARD_TICKS &&
             v003_match_a.alive[0]; ++guard) {
            if (vox_digs_match_step(&v003_match_a) != VOX_OK) {
                return 5;
            }
        }
        if (v003_match_a.alive[0] ||
            v003_match_a.deaths[0] != (vox_u16)(deaths + 1U)) {
            return 6;
        }
    }
    deaths = v003_match_a.deaths[0];
    if (vox_digs_match_step(&v003_match_a) != VOX_OK ||
        v003_match_a.deaths[0] != deaths) {
        return 7;
    }
    return 0;
}

/*
 * Bots are three distinct opponents, and they can use charge weapons.
 *
 * Identity comes from the bot ordinal, so the first bot is always RIVET
 * whichever slot it sits in. Weapon choice is scored from an archetype
 * preference table rather than flat distance bands, so the three must not
 * converge on the same tool at the same range.
 *
 * The charge half matters because bots previously could not fire the Bolt
 * Action at all: they held fire for one eight-tick decision window, reaching
 * a charge of eight, and digs_release_charged_weapon needs thirty. It was
 * their selected weapon for mid range, so they simply never shot.
 */
static int test_bot_archetypes_and_charge_weapons(void)
{
    vox_digs_rules rules;
    vox_u32 tick;
    vox_u32 charge_weapon_shots = 0U;
    vox_u16 slot;
    vox_u16 favourite[VOX_DIGS_MAX_SLOTS];
    vox_u32 use[VOX_DIGS_MAX_SLOTS][VOX_DIGS_TOOL_COUNT];
    vox_digs_rules_classic(&rules);
    rules.player_count = 4U;
    rules.bot_mask = 0x000EU;
    rules.weapon_mask = 0x07FFU;
    rules.match_ticks = 3600U;
    rules.lava_start_tick = 3500U;
    rules.score_limit = 0U;
    rules.seed = 0xC0A1C0DEU;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;

    /* Identity is stable and derived from the bot ordinal. */
    if (vox_digs_bot_archetype(&match, 0U) != VOX_DIGS_ARCHETYPE_COUNT ||
        vox_digs_bot_archetype(&match, 1U) != VOX_DIGS_ARCHETYPE_ENGINEER ||
        vox_digs_bot_archetype(&match, 2U) != VOX_DIGS_ARCHETYPE_BERSERKER ||
        vox_digs_bot_archetype(&match, 3U) != VOX_DIGS_ARCHETYPE_TRICKSTER) {
        return 2;
    }
    if (vox_digs_personality_get(VOX_DIGS_ARCHETYPE_BERSERKER) == 0 ||
        vox_digs_personality_get(VOX_DIGS_ARCHETYPE_COUNT) != 0 ||
        vox_digs_archetype_name(VOX_DIGS_ARCHETYPE_ENGINEER) == 0) {
        return 3;
    }
    /* The berserker must want to fight closer than the engineer. */
    if (vox_digs_personality_get(VOX_DIGS_ARCHETYPE_BERSERKER)->aggression <=
        vox_digs_personality_get(VOX_DIGS_ARCHETYPE_ENGINEER)->aggression) {
        return 4;
    }

    for (slot = 0U; slot < VOX_DIGS_MAX_SLOTS; ++slot) {
        vox_u16 weapon;
        favourite[slot] = VOX_DIGS_TOOL_COUNT;
        for (weapon = 0U; weapon < VOX_DIGS_TOOL_COUNT; ++weapon) {
            use[slot][weapon] = 0U;
        }
    }
    for (tick = 0U; tick < 3600U && match.phase == VOX_DIGS_RUNNING; ++tick) {
        vox_u16 ordinal;
        if (vox_digs_match_step(&match) != VOX_OK) return 5;
        for (ordinal = 0U; ordinal < match.event_count; ++ordinal) {
            const vox_digs_event *event = vox_digs_event_get(&match, ordinal);
            if (event == 0 ||
                event->type != VOX_DIGS_EVENT_WEAPON_FIRE) {
                continue;
            }
            if (event->weapon >= VOX_DIGS_TOOL_COUNT ||
                event->source >= VOX_DIGS_MAX_SLOTS) {
                continue;
            }
            use[event->source][event->weapon]++;
            /* Any shot from a weapon that must be charged proves the hold. */
            if (vox_digs_weapon_get(event->weapon)->charge_ticks >=
                DIGS_TEST_MIN_GATED_CHARGE) {
                charge_weapon_shots++;
            }
        }
        if (match.event_count > 0U &&
            vox_digs_consume_events(&match, match.event_count) != VOX_OK) {
            return 6;
        }
    }
    if (charge_weapon_shots == 0U) {
        return 7;
    }

    /*
     * Each bot must have shot at all, and the three must not converge on one
     * favourite tool -- that is the whole point of the preference table.
     */
    for (slot = 1U; slot < 4U; ++slot) {
        vox_u16 weapon;
        vox_u32 best = 0U;
        for (weapon = 0U; weapon < VOX_DIGS_TOOL_COUNT; ++weapon) {
            if (use[slot][weapon] > best) {
                best = use[slot][weapon];
                favourite[slot] = weapon;
            }
        }
        if (favourite[slot] == VOX_DIGS_TOOL_COUNT) {
            return 8;
        }
    }
    if (favourite[1] == favourite[2] && favourite[2] == favourite[3]) {
        return 9;
    }
    return 0;
}

/*
 * Undermined terrain must actually come down.
 *
 * The collapse machinery -- structural support, UNSTABLE, bottom-up falling --
 * existed since v0.0.3 but was completely inert in play: clearing a cell put
 * it to sleep without waking anything, so the structural pass never visited
 * the roof above a fresh tunnel and a fully undermined slab hung in mid-air
 * with the world reporting zero awake cells. Nothing tested it, so nothing
 * caught it.
 *
 * A wide excavation must cave in, and a narrow tunnel must stay usable --
 * otherwise the digging tools destroy the tunnels they exist to make.
 */
static int test_bark_pacing_and_variance(void)
{
    vox_digs_rules rules;
    vox_digs_input input;
    vox_u32 tick;
    vox_u32 lines = 0U;
    vox_u32 repeats = 0U;
    vox_u32 bot_lines[VOX_DIGS_MAX_SLOTS];
    vox_u16 window[12];
    vox_u16 window_count = 0U;
    vox_u16 slot;
    vox_u32 presses = 0U;
    vox_u32 answered = 0U;
    vox_u32 since = 0xFFFFFFFFUL;

    for (slot = 0U; slot < VOX_DIGS_MAX_SLOTS; ++slot) bot_lines[slot] = 0U;
    for (slot = 0U; slot < 12U; ++slot) window[slot] = 0U;

    vox_digs_rules_classic(&rules);
    rules.player_count = 4U;
    rules.bot_mask = 0x000EU;          /* one human, RIVET, CINDER, FLAMEY */
    rules.match_ticks = 11400U;
    rules.lava_start_tick = 10800U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;

    /*
     * A full match with the player never pressing bark.  This is the case
     * the pacing target is written against: enough voices that the mine is
     * inhabited, few enough that it is not a commentary.
     */
    for (tick = 0U; tick < 10800U && match.phase == VOX_DIGS_RUNNING;
         ++tick) {
        vox_u16 index;
        vox_u32 spoke_this_tick = 0U;
        if (vox_digs_match_step(&match) != VOX_OK) return 2;
        for (index = 0U; index < match.event_count; ++index) {
            const vox_digs_event *event =
                &match.events[(match.event_head + index) %
                              VOX_DIGS_MAX_EVENTS];
            vox_u16 look;
            if (event->type != VOX_DIGS_EVENT_AI_BARK) continue;
            spoke_this_tick++;
            lines++;
            if (event->source < VOX_DIGS_MAX_SLOTS) {
                bot_lines[event->source]++;
            }
            if (event->source == 0U) return 3;   /* the human never speaks */
            for (look = 0U; look < window_count; ++look) {
                if (window[look] == event->variant) repeats++;
            }
            window[window_count % 12U] = event->variant;
            if (window_count < 12U) window_count++;
        }
        if (spoke_this_tick > 1U) return 4;      /* never two at once */
        if (match.event_count > 0U) {
            (void)vox_digs_consume_events(&match, match.event_count);
        }
    }
    /*
     * The band is deliberately wide.  It is here to catch a return to the
     * old behaviour -- seventy-two lines a match, a quarter of them repeats
     * -- not to pin a tuning decision that taste may revisit.
     */
    if (lines < 4U) return 5;            /* a silent mine is also wrong */
    if (lines > 30U) return 6;
    /* Repetition was the other half of the complaint. */
    if (repeats > 2U) return 7;
    /* And the quiet one must be quieter than the loud one. */
    if (bot_lines[1] > bot_lines[2] + bot_lines[3]) return 8;

    /*
     * Now the same match with the player speaking.  Pressing bark must make
     * an answer much more likely -- that is what makes the bots feel like
     * they are reacting to you rather than reciting.
     */
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 9;
    for (tick = 0U; tick < 10800U && match.phase == VOX_DIGS_RUNNING;
         ++tick) {
        vox_u16 index;
        if ((tick % 600U) == 300U && match.alive[0]) {
            input.abi_version = VOX_ABI_VERSION;
            input.struct_size = (vox_u32)sizeof(input);
            input.player = 0U;
            input.actions = VOX_DIGS_ACTION_BARK;
            input.move_x_q15 = 0;
            input.move_y_q15 = 0;
            input.aim_x = match.aim_x[0];
            input.aim_y = match.aim_y[0];
            input.selected_weapon = match.selected_weapon[0];
            input.reserved = 0U;
            (void)vox_digs_submit_input(&match, &input);
        }
        if (vox_digs_match_step(&match) != VOX_OK) return 10;
        for (index = 0U; index < match.event_count; ++index) {
            const vox_digs_event *event =
                &match.events[(match.event_head + index) %
                              VOX_DIGS_MAX_EVENTS];
            if (event->type != VOX_DIGS_EVENT_AI_BARK) continue;
            if (event->source == 0U) {
                presses++;
                since = 0U;
            } else if (since < 240U) {
                answered++;
            }
        }
        if (since != 0xFFFFFFFFUL) since++;
        if (match.event_count > 0U) {
            (void)vox_digs_consume_events(&match, match.event_count);
        }
    }
    if (presses < 8U) return 11;         /* the button must work */
    /* Most of what the player says should get an answer within four seconds. */
    if (answered * 2U < presses) return 12;
    return 0;
}

static int test_memory_carries_between_matches(void)
{
    vox_digs_rules rules;
    vox_digs_bot_memory memory;
    vox_digs_bot_memory second;
    vox_digs_bot_memory foreign;
    const vox_digs_contract *contract;
    vox_u16 slot;
    vox_u32 tick;

    /* Identity pairs must be symmetric and cover every combination once. */
    {
        vox_u16 a;
        vox_u16 b;
        vox_u16 seen[VOX_DIGS_MAX_PAIRS];
        for (a = 0U; a < VOX_DIGS_MAX_PAIRS; ++a) seen[a] = 0U;
        for (a = 0U; a < VOX_DIGS_IDENTITY_COUNT; ++a) {
            for (b = 0U; b < VOX_DIGS_IDENTITY_COUNT; ++b) {
                vox_u16 index = vox_digs_regard_index(a, b);
                if (a == b) {
                    if (index != VOX_DIGS_MAX_PAIRS) return 1;
                    continue;
                }
                if (index >= VOX_DIGS_MAX_PAIRS) return 2;
                if (index != vox_digs_regard_index(b, a)) return 3;
                seen[index]++;
            }
        }
        for (a = 0U; a < VOX_DIGS_MAX_PAIRS; ++a) {
            if (seen[a] != 2U) return 4;
        }
    }

    vox_digs_memory_init(&memory);
    if (memory.memory_hash == 0U) return 5;
    if (memory.regard[0].tone != VOX_DIGS_TONE_NEUTRAL) return 6;

    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0x0002U;
    rules.match_ticks = 900U;
    rules.lava_start_tick = 850U;
    if (vox_digs_match_init_ex(&match, &rules, &memory) != VOX_OK) return 7;
    for (slot = 0U; slot < 2U; ++slot) match.spawn_shield_ticks[slot] = 0U;

    /* Make them hate each other, then close the match. */
    for (tick = 0U; tick < 500U && match.phase == VOX_DIGS_RUNNING; ++tick) {
        if (match.alive[1]) {
            (void)vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_POPPER,
                                     VOX_DIGS_NO_PART, 20U,
                                     VOX_DIGS_DAMAGE_BALLISTIC);
        }
        if (vox_digs_match_step(&match) != VOX_OK) return 8;
        if (match.event_count > 0U) {
            (void)vox_digs_consume_events(&match, match.event_count);
        }
    }
    contract = vox_digs_contract_get(&match, 0U, 1U);
    if (contract == 0 || contract->tone >= VOX_DIGS_TONE_NEUTRAL) return 9;

    if (vox_digs_match_export_memory(&match, &second) != VOX_OK) return 10;
    if (second.memory_hash == memory.memory_hash) return 11;
    {
        vox_u16 pair = vox_digs_regard_index(VOX_DIGS_IDENTITY_PLAYER,
                                             VOX_DIGS_IDENTITY_RIVET);
        if (pair >= VOX_DIGS_MAX_PAIRS) return 12;
        if (second.regard[pair].tone >= VOX_DIGS_TONE_NEUTRAL) return 13;
        if (second.regard[pair].matches_met == 0U) return 14;
        /* Both of them should have played a match now. */
        if (second.identities[VOX_DIGS_IDENTITY_RIVET].matches_played == 0U) {
            return 15;
        }
    }

    /*
     * The next match must open where the last one ended.  This is the whole
     * feature: they walk in already knowing what happened.
     */
    if (vox_digs_match_init_ex(&match, &rules, &second) != VOX_OK) return 16;
    contract = vox_digs_contract_get(&match, 0U, 1U);
    if (contract == 0) return 17;
    if (contract->tone >= VOX_DIGS_TONE_NEUTRAL) return 18;
    if (!contract->met) return 19;

    /*
     * A snapshot from another build is a first meeting, not a reinterpreted
     * one.  Reading foreign bytes the wrong way round would produce
     * plausible traits and a wrong match, which is worse than no memory.
     */
    foreign = second;
    foreign.memory_version = VOX_DIGS_MEMORY_VERSION + 99U;
    if (vox_digs_match_init_ex(&match, &rules, &foreign) != VOX_OK) return 20;
    contract = vox_digs_contract_get(&match, 0U, 1U);
    if (contract == 0 || contract->tone != VOX_DIGS_TONE_NEUTRAL) return 21;
    if (contract->met) return 22;

    /* And the plain init must still behave exactly like a blank snapshot. */
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 23;
    contract = vox_digs_contract_get(&match, 0U, 1U);
    if (contract == 0 || contract->tone != VOX_DIGS_TONE_NEUTRAL) return 24;
    return 0;
}

static int test_contracts_steer_targeting(void)
{
    vox_digs_rules rules;
    vox_digs_contract *near_pair;
    vox_digs_contract *far_pair;
    vox_u32 tick;
    vox_i32 bot_x;
    vox_i32 bot_y;
    vox_u16 picked_far = 0U;
    vox_u16 picked_near = 0U;

    vox_digs_rules_classic(&rules);
    rules.player_count = 3U;
    rules.bot_mask = 0x0001U;        /* slot 0 is the bot */
    rules.match_ticks = 4000U;
    rules.lava_start_tick = 3900U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;
    for (tick = 0U; tick < 3U; ++tick) match.spawn_shield_ticks[tick] = 0U;

    bot_x = match.players[0].position_x.value_q16 >> 16;
    bot_y = match.players[0].position_y.value_q16 >> 16;
    /* Clear air around the three of them so sight lines are not the story. */
    {
        vox_u32 x;
        vox_u32 y;
        for (x = (vox_u32)(bot_x - 6); x <= (vox_u32)(bot_x + 60); ++x) {
            for (y = (vox_u32)(bot_y - 8); y <= (vox_u32)(bot_y + 1); ++y) {
                if (!set_test_column(&match.world, x, y, VOX_MAT_AIR)) {
                    return 2;
                }
            }
            for (y = (vox_u32)(bot_y + 2); y <= (vox_u32)(bot_y + 5); ++y) {
                if (!set_test_column(&match.world, x, y, VOX_MAT_SOIL)) {
                    return 3;
                }
            }
        }
    }
    if (vox_world_sleep_all(&match.world) != VOX_OK) return 4;
    /* Slot 1 stands close.  Slot 2 stands a long way off. */
    match.players[1].position_x.value_q16 = (vox_i32)((bot_x + 12) << 16);
    match.players[1].position_y.value_q16 = match.players[0].position_y.value_q16;
    match.players[2].position_x.value_q16 = (vox_i32)((bot_x + 50) << 16);
    match.players[2].position_y.value_q16 = match.players[0].position_y.value_q16;

    near_pair = (vox_digs_contract *)vox_digs_contract_get(&match, 0U, 1U);
    far_pair = (vox_digs_contract *)vox_digs_contract_get(&match, 0U, 2U);
    if (near_pair == 0 || far_pair == 0) return 5;

    /*
     * An arrangement with the near one and a blood feud with the far one.
     * Geometry says shoot the near miner; history must say otherwise.
     */
    near_pair->tone = (vox_u16)VOX_DIGS_TONE_TRUCE;
    near_pair->valence = 400;
    near_pair->met = 1U;
    far_pair->tone = (vox_u16)VOX_DIGS_TONE_FEUD;
    far_pair->valence = -800;
    far_pair->met = 1U;

    for (tick = 0U; tick < 240U && match.phase == VOX_DIGS_RUNNING; ++tick) {
        /*
         * Hold the contracts, the positions and both lives.  Targeting is
         * what is on trial, and once the feuded miner dies, falling back to
         * the one you have an arrangement with is correct behaviour rather
         * than a failure -- counting those ticks measured the wrong thing.
         */
        near_pair->tone = (vox_u16)VOX_DIGS_TONE_TRUCE;
        far_pair->tone = (vox_u16)VOX_DIGS_TONE_FEUD;
        match.alive[1] = 1U;
        match.alive[2] = 1U;
        match.health[1] = VOX_DIGS_MAX_HEALTH;
        match.health[2] = VOX_DIGS_MAX_HEALTH;
        match.players[1].position_x.value_q16 = (vox_i32)((bot_x + 12) << 16);
        match.players[2].position_x.value_q16 = (vox_i32)((bot_x + 50) << 16);
        match.players[1].position_y.value_q16 =
            match.players[0].position_y.value_q16;
        match.players[2].position_y.value_q16 =
            match.players[0].position_y.value_q16;
        if (vox_digs_match_step(&match) != VOX_OK) return 6;
        if (match.event_count > 0U) {
            (void)vox_digs_consume_events(&match, match.event_count);
        }
        if (match.bots[0].target == 2U) picked_far++;
        else if (match.bots[0].target == 1U) picked_near++;
    }
    /* The feud must win over the shorter walk. */
    if (picked_far == 0U) return 7;
    if (picked_near > picked_far) return 8;

    /* Now make them both ordinary, and geometry should decide again. */
    near_pair->tone = (vox_u16)VOX_DIGS_TONE_NEUTRAL;
    near_pair->valence = 0;
    far_pair->tone = (vox_u16)VOX_DIGS_TONE_NEUTRAL;
    far_pair->valence = 0;
    picked_near = 0U;
    picked_far = 0U;
    for (tick = 0U; tick < 240U && match.phase == VOX_DIGS_RUNNING; ++tick) {
        near_pair->tone = (vox_u16)VOX_DIGS_TONE_NEUTRAL;
        far_pair->tone = (vox_u16)VOX_DIGS_TONE_NEUTRAL;
        match.alive[1] = 1U;
        match.alive[2] = 1U;
        match.health[1] = VOX_DIGS_MAX_HEALTH;
        match.health[2] = VOX_DIGS_MAX_HEALTH;
        match.players[1].position_x.value_q16 = (vox_i32)((bot_x + 12) << 16);
        match.players[2].position_x.value_q16 = (vox_i32)((bot_x + 50) << 16);
        match.players[1].position_y.value_q16 =
            match.players[0].position_y.value_q16;
        match.players[2].position_y.value_q16 =
            match.players[0].position_y.value_q16;
        if (vox_digs_match_step(&match) != VOX_OK) return 9;
        if (match.event_count > 0U) {
            (void)vox_digs_consume_events(&match, match.event_count);
        }
        if (match.bots[0].target == 2U) picked_far++;
        else if (match.bots[0].target == 1U) picked_near++;
    }
    if (picked_near == 0U) return 10;
    if (picked_far > picked_near) return 11;
    return 0;
}

static int test_speech_is_paced_and_answered(void)
{
    vox_digs_rules rules;
    vox_digs_input input;
    vox_u32 tick;
    vox_u32 spoke_bot = 0U;
    vox_u32 spoke_human = 0U;
    vox_u32 last_spoke_tick = 0U;
    vox_u32 min_gap = 0xFFFFFFFFUL;
    vox_u16 seen[16];
    vox_u16 seen_count = 0U;
    vox_u16 repeats = 0U;

    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0x0002U;          /* slot 0 human, slot 1 RIVET */
    rules.match_ticks = 4000U;
    rules.lava_start_tick = 3900U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;
    match.spawn_shield_ticks[0] = 0U;
    match.spawn_shield_ticks[1] = 0U;

    /*
     * Nobody speaks the instant something happens -- there is a pause.
     *
     * An ordinary hit is rolled for now and usually passes without comment,
     * so this uses the one thing nobody stays quiet about: shooting a miner
     * you had an arrangement with.  That is loud enough to bypass the roll,
     * which is what makes it a reliable probe for the pause.
     */
    {
        vox_digs_contract *pair =
            (vox_digs_contract *)vox_digs_contract_get(&match, 0U, 1U);
        if (pair == 0) return 2;
        pair->tone = (vox_u16)VOX_DIGS_TONE_TRUCE;
        pair->valence = 400;
        pair->met = 1U;
    }
    if (vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_POPPER,
                           VOX_DIGS_NO_PART, 15U,
                           VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK) {
        return 3;
    }
    if (match.speech_stimulus[0] != VOX_DIGS_STIMULUS_TRUCE_BROKEN) return 4;
    if (match.speech_stimulus[1] != VOX_DIGS_STIMULUS_BETRAYED) return 5;
    if (match.speech_delay[1] == 0U) return 6;

    for (tick = 0U; tick < 1500U && match.phase == VOX_DIGS_RUNNING; ++tick) {
        vox_u16 index;
        vox_u32 spoke_this_tick = 0U;
        if (vox_digs_match_step(&match) != VOX_OK) return 5;
        for (index = 0U; index < match.event_count; ++index) {
            const vox_digs_event *event =
                &match.events[(match.event_head + index) %
                              VOX_DIGS_MAX_EVENTS];
            if (event->type != VOX_DIGS_EVENT_AI_BARK) continue;
            spoke_this_tick++;
            if (event->source == 1U) spoke_bot++; else spoke_human++;
            if (digs_lines_text(event->variant)[0] == '\0') return 6;
            if (event->magnitude >= VOX_DIGS_STIMULUS_COUNT) return 7;
            if (event->reserved >= VOX_DIGS_TONE_COUNT) return 8;
            if (last_spoke_tick != 0U &&
                match.tick - last_spoke_tick < min_gap) {
                min_gap = match.tick - last_spoke_tick;
            }
            last_spoke_tick = match.tick;
            if (seen_count < 16U) {
                vox_u16 look;
                for (look = 0U; look < seen_count; ++look) {
                    if (seen[look] == event->variant) repeats++;
                }
                seen[seen_count++] = event->variant;
            }
        }
        /* Two miners must never talk over each other. */
        if (spoke_this_tick > 1U) return 9;
        if (match.event_count > 0U) {
            (void)vox_digs_consume_events(&match, match.event_count);
        }
        if (match.alive[0] && match.alive[1] && (tick % 200U) == 0U) {
            (void)vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_POPPER,
                                     VOX_DIGS_NO_PART, 12U,
                                     VOX_DIGS_DAMAGE_BALLISTIC);
        }
    }
    if (spoke_bot == 0U) return 10;
    /*
     * The miner the player is driving must never speak on its own.  The
     * simulation works out what it would say and holds it; the button is
     * what says it.
     */
    if (spoke_human != 0U) return 11;
    /* And the floor is real: lines are spaced, never stacked. */
    if (min_gap != 0xFFFFFFFFUL && min_gap < 2U) return 12;
    if (seen_count >= 8U && repeats > seen_count / 2U) return 13;

    /* Now press bark, and the held line comes out. */
    if (!match.alive[0]) {
        match.alive[0] = 1U;
        match.health[0] = VOX_DIGS_MAX_HEALTH;
    }
    match.speech_floor_ticks = 0U;
    match.speech_cooldown[0] = 0U;
    for (tick = 0U; tick < 200U && spoke_human == 0U &&
         match.phase == VOX_DIGS_RUNNING; ++tick) {
        vox_u16 index;
        input.abi_version = VOX_ABI_VERSION;
        input.struct_size = (vox_u32)sizeof(input);
        input.player = 0U;
        input.actions = VOX_DIGS_ACTION_BARK;
        input.move_x_q15 = 0;
        input.move_y_q15 = 0;
        input.aim_x = match.aim_x[0];
        input.aim_y = match.aim_y[0];
        input.selected_weapon = match.selected_weapon[0];
        input.reserved = 0U;
        if (match.alive[0] &&
            vox_digs_submit_input(&match, &input) != VOX_OK) {
            return 14;
        }
        if (vox_digs_match_step(&match) != VOX_OK) return 15;
        for (index = 0U; index < match.event_count; ++index) {
            const vox_digs_event *event =
                &match.events[(match.event_head + index) %
                              VOX_DIGS_MAX_EVENTS];
            if (event->type == VOX_DIGS_EVENT_AI_BARK &&
                event->source == 0U) {
                spoke_human++;
                if (digs_lines_text(event->variant)[0] == '\0') return 16;
            }
        }
        if (match.event_count > 0U) {
            (void)vox_digs_consume_events(&match, match.event_count);
        }
    }
    if (spoke_human == 0U) return 17;
    return 0;
}

static int test_every_line_cell_resolves(void)
{
    vox_u16 voice;
    vox_u16 tone;
    vox_u16 stimulus;
    vox_u16 total;

    /*
     * The index is the quality filter.  Every combination a speaker can
     * actually find itself in must resolve to something written, or the game
     * shows an empty speech bubble at exactly the moment it had something to
     * say.  NONE is the one stimulus that is allowed to be silent.
     */
    for (voice = 0U; voice < DIGS_VOICE_COUNT; ++voice) {
        for (tone = 0U; tone < VOX_DIGS_TONE_COUNT; ++tone) {
            for (stimulus = 1U; stimulus < VOX_DIGS_STIMULUS_COUNT;
                 ++stimulus) {
                digs_line_pool pool = digs_lines_pool(voice, tone, stimulus);
                vox_u16 index;
                if (pool.count == 0U) return 1;
                for (index = 0U; index < pool.count; ++index) {
                    const char *line =
                        digs_lines_text((vox_u16)(pool.first + index));
                    if (line == 0 || line[0] == '\0') return 2;
                }
            }
        }
    }
    /* NONE stays silent rather than saying something generic. */
    if (digs_lines_pool(DIGS_VOICE_RIVET, VOX_DIGS_TONE_NEUTRAL,
                        VOX_DIGS_STIMULUS_NONE).count != 0U) {
        return 3;
    }
    /* Out of range must not read past the tables. */
    if (digs_lines_pool(99U, 99U, VOX_DIGS_STIMULUS_COUNT).count != 0U) {
        return 4;
    }
    if (digs_lines_text(65535U) == 0) return 5;

    /*
     * The three opponents must not share a voice.  Falling back to the
     * generic pool for everything would satisfy the coverage check above
     * while leaving all three sounding identical, which is the failure this
     * whole system exists to prevent.
     */
    {
        digs_line_pool rivet = digs_lines_pool(DIGS_VOICE_RIVET,
            VOX_DIGS_TONE_NEUTRAL, VOX_DIGS_STIMULUS_KILLED_THEM);
        digs_line_pool cinder = digs_lines_pool(DIGS_VOICE_CINDER,
            VOX_DIGS_TONE_NEUTRAL, VOX_DIGS_STIMULUS_KILLED_THEM);
        digs_line_pool flamey = digs_lines_pool(DIGS_VOICE_FLAMEY,
            VOX_DIGS_TONE_NEUTRAL, VOX_DIGS_STIMULUS_KILLED_THEM);
        if (rivet.first == cinder.first || cinder.first == flamey.first ||
            rivet.first == flamey.first) {
            return 6;
        }
    }
    /*
     * The tone layer must actually be reached, and reached correctly.  An
     * off-by-one in the index table would still resolve to real lines -- just
     * somebody else's -- so this checks a known cell against known words.
     */
    {
        digs_line_pool feud = digs_lines_pool(DIGS_VOICE_RIVET,
            VOX_DIGS_TONE_FEUD, VOX_DIGS_STIMULUS_KILLED_THEM);
        digs_line_pool bonded = digs_lines_pool(DIGS_VOICE_RIVET,
            VOX_DIGS_TONE_BONDED, VOX_DIGS_STIMULUS_KILLED_THEM);
        digs_line_pool plain = digs_lines_pool(DIGS_VOICE_RIVET,
            VOX_DIGS_TONE_NEUTRAL, VOX_DIGS_STIMULUS_KILLED_THEM);
        vox_u16 look;
        int found_feud = 0;
        int found_bonded = 0;
        if (feud.first == bonded.first || feud.first == plain.first) return 8;
        for (look = 0U; look < feud.count; ++look) {
            if (strstr(digs_lines_text((vox_u16)(feud.first + look)),
                       "LEDGER") != 0) {
                found_feud = 1;
            }
        }
        for (look = 0U; look < bonded.count; ++look) {
            if (strstr(digs_lines_text((vox_u16)(bonded.first + look)),
                       "SORRY") != 0) {
                found_bonded = 1;
            }
        }
        /* A feud keeps a ledger.  Being bonded means apologising. */
        if (!found_feud) return 9;
        if (!found_bonded) return 10;
    }
    /* Cinder in a feud and Cinder at a truce must not share words. */
    if (digs_lines_pool(DIGS_VOICE_CINDER, VOX_DIGS_TONE_FEUD,
                        VOX_DIGS_STIMULUS_HURT_BY).first ==
        digs_lines_pool(DIGS_VOICE_CINDER, VOX_DIGS_TONE_TRUCE,
                        VOX_DIGS_STIMULUS_HURT_BY).first) {
        return 11;
    }
    /*
     * Line ids pack a set number and a position into one vox_u16.  Overflow
     * there is silent and vicious: sets past 255 wrapped onto low ones, so a
     * feud line resolved to the generic idle pool and read one entry past the
     * end of it.  This is the guard for that.
     */
    if (!digs_lines_stride_is_sound()) return 12;
    total = digs_lines_total();
    if (total < 1200U) return 13;    /* the corpus is meant to be large */
    return 0;
}

static int test_stimuli_reach_the_contract(void)
{
    vox_digs_rules rules;
    vox_u16 stimulus;
    const vox_digs_contract *contract;
    vox_i16 after_hit;

    /* Every stimulus must name itself, and out of range must not read past. */
    for (stimulus = 0U; stimulus < VOX_DIGS_STIMULUS_COUNT; ++stimulus) {
        const char *name = vox_digs_stimulus_name(stimulus);
        if (name == 0 || name[0] == '\0') return 1;
    }
    if (vox_digs_stimulus_name(VOX_DIGS_STIMULUS_COUNT) == 0) return 2;

    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 3;
    match.spawn_shield_ticks[0] = 0U;
    match.spawn_shield_ticks[1] = 0U;
    contract = vox_digs_contract_get(&match, 0U, 1U);
    if (contract == 0) return 4;
    if (contract->last_stimulus != VOX_DIGS_STIMULUS_NONE) return 5;
    if (contract->last_actor != VOX_DIGS_NO_PLAYER) return 6;

    /* A hit records who did it and which way round it was. */
    if (vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_POPPER,
                           VOX_DIGS_NO_PART, 18U,
                           VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK) {
        return 7;
    }
    if (contract->last_stimulus != VOX_DIGS_STIMULUS_HURT_THEM) return 8;
    if (contract->last_actor != 0U) return 9;
    after_hit = contract->valence;
    if (after_hit >= 0) return 10;

    /*
     * A kill is worth far more than a hit, and killing the miner who last
     * killed you reads as settling a score rather than starting one.
     */
    if (vox_digs_record_kill(&match, 0U, 1U) != VOX_OK) return 11;
    if (contract->last_stimulus != VOX_DIGS_STIMULUS_KILLED_THEM) return 12;
    if (contract->valence >= after_hit) return 13;

    /*
     * Measure the two kinds of kill against each other through the real path
     * rather than reaching for the weight table -- that proves the wiring,
     * not a constant.
     */
    {
        vox_i16 plain;
        vox_i16 revenge;
        if (vox_digs_match_init(&match, &rules) != VOX_OK) return 14;
        match.spawn_shield_ticks[0] = 0U;
        match.spawn_shield_ticks[1] = 0U;
        contract = vox_digs_contract_get(&match, 0U, 1U);
        if (vox_digs_record_kill(&match, 0U, 1U) != VOX_OK) return 15;
        if (contract->last_stimulus != VOX_DIGS_STIMULUS_KILLED_THEM) {
            return 16;
        }
        plain = contract->valence;

        if (vox_digs_match_init(&match, &rules) != VOX_OK) return 17;
        match.spawn_shield_ticks[0] = 0U;
        match.spawn_shield_ticks[1] = 0U;
        contract = vox_digs_contract_get(&match, 0U, 1U);
        match.last_attacker[0] = 1U;
        if (vox_digs_record_kill(&match, 0U, 1U) != VOX_OK) return 18;
        if (contract->last_stimulus != VOX_DIGS_STIMULUS_REVENGE) return 19;
        revenge = contract->valence;

        /* Settling a score must cost the account less than starting one. */
        if (revenge <= plain) return 20;
    }
    return 0;
}

static int test_contracts_pair_index_and_tone(void)
{
    vox_digs_rules rules;
    vox_u16 a;
    vox_u16 b;
    vox_u16 seen[VOX_DIGS_MAX_PAIRS];
    vox_u32 tick;
    const vox_digs_contract *contract;
    vox_u16 first_tone;

    /* Every unordered pair maps to its own slot, in either order. */
    for (a = 0U; a < VOX_DIGS_MAX_PAIRS; ++a) seen[a] = 0U;
    for (a = 0U; a < VOX_DIGS_MAX_SLOTS; ++a) {
        for (b = 0U; b < VOX_DIGS_MAX_SLOTS; ++b) {
            vox_u16 index = vox_digs_pair_index(a, b);
            if (a == b) {
                if (index != VOX_DIGS_MAX_PAIRS) return 1;
                continue;
            }
            if (index >= VOX_DIGS_MAX_PAIRS) return 2;
            if (index != vox_digs_pair_index(b, a)) return 3;
            seen[index]++;
        }
    }
    for (a = 0U; a < VOX_DIGS_MAX_PAIRS; ++a) {
        if (seen[a] != 2U) return 4;      /* each pair hit once per order */
    }
    if (vox_digs_pair_index(0U, VOX_DIGS_MAX_SLOTS) != VOX_DIGS_MAX_PAIRS) {
        return 5;
    }

    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 6;
    match.spawn_shield_ticks[0] = 0U;
    match.spawn_shield_ticks[1] = 0U;
    contract = vox_digs_contract_get(&match, 0U, 1U);
    if (contract == 0) return 7;
    if (contract->tone != VOX_DIGS_TONE_NEUTRAL || contract->valence != 0) {
        return 8;
    }
    if (contract->last_speaker != VOX_DIGS_NO_PLAYER || contract->met) {
        return 9;
    }

    /*
     * Shooting somebody should sour things -- but not instantly, and not
     * before the dwell has elapsed.  This is the check that AI modes failed
     * for two releases: a condition recomputed every tick with no dwell
     * flips as fast as the condition wobbles.
     */
    if (vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_POPPER,
                           VOX_DIGS_NO_PART, 40U,
                           VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK) {
        return 10;
    }
    if (!contract->met || contract->valence >= 0) return 11;
    first_tone = contract->tone;
    if (first_tone != VOX_DIGS_TONE_NEUTRAL) {
        return 12;       /* one popper hit is not yet a quarrel */
    }
    for (tick = 0U; tick < 170U && match.phase == VOX_DIGS_RUNNING; ++tick) {
        if (match.alive[1] &&
            vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_POPPER,
                               VOX_DIGS_NO_PART, 20U,
                               VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK) {
            return 13;
        }
        if (vox_digs_match_step(&match) != VOX_OK) return 14;
        if (match.event_count > 0U) {
            (void)vox_digs_consume_events(&match, match.event_count);
        }
    }
    /*
     * The balance is already deep in the red, but the dwell has not elapsed,
     * so the tone must not have moved yet.  This is the assertion that would
     * have caught the AI mode oscillation before it shipped.
     */
    if (contract->valence > VOX_DIGS_TONE_NEUTRAL) return 15;
    if (contract->tone != VOX_DIGS_TONE_NEUTRAL) return 16;
    for (tick = 0U; tick < 260U && match.phase == VOX_DIGS_RUNNING; ++tick) {
        if (match.alive[1] &&
            vox_digs_apply_hit(&match, 0U, 1U, VOX_DIGS_TOOL_POPPER,
                               VOX_DIGS_NO_PART, 20U,
                               VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK) {
            return 17;
        }
        if (vox_digs_match_step(&match) != VOX_OK) return 18;
        if (match.event_count > 0U) {
            (void)vox_digs_consume_events(&match, match.event_count);
        }
    }
    /* Past the dwell, sustained violence must show in the tone. */
    if (contract->tone >= VOX_DIGS_TONE_NEUTRAL) return 19;
    if (contract->valence > 0) return 20;

    /* Every tone must name itself, and out-of-range must not read past. */
    for (a = 0U; a < VOX_DIGS_TONE_COUNT; ++a) {
        if (vox_digs_tone_name(a) == 0 || vox_digs_tone_name(a)[0] == '\0') {
            return 21;
        }
    }
    if (vox_digs_tone_name(VOX_DIGS_TONE_COUNT) == 0) return 22;
    return 0;
}

static int test_kill_heals_the_killer(void)
{
    vox_digs_rules rules;
    vox_u16 before;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;
    match.spawn_shield_ticks[0] = 0U;
    match.spawn_shield_ticks[1] = 0U;
    /* Hurt, but not so hurt that a full heal would be capped. */
    match.health[0] = 20U;
    before = match.health[0];
    if (vox_digs_record_kill(&match, 0U, 1U) != VOX_OK) return 2;
    if (match.health[0] <= before) return 3;
    if (match.health[0] > VOX_DIGS_MAX_HEALTH) return 4;

    /* A kill at near-full health must cap rather than overflow. */
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 5;
    match.spawn_shield_ticks[0] = 0U;
    match.spawn_shield_ticks[1] = 0U;
    match.health[0] = (vox_u16)(VOX_DIGS_MAX_HEALTH - 1U);
    if (vox_digs_record_kill(&match, 0U, 1U) != VOX_OK) return 6;
    if (match.health[0] != VOX_DIGS_MAX_HEALTH) return 7;

    /* The victim stays dead at zero -- healing is the killer's alone. */
    if (match.health[1] != 0U || match.alive[1]) return 8;
    return 0;
}

static int test_bot_bores_through_a_wall(void)
{
    vox_digs_rules rules;
    vox_u32 x;
    vox_u32 y;
    vox_u32 tick;
    vox_u32 opened = 0U;
    vox_i32 bot_x;
    vox_i32 bot_y;
    vox_u32 wall_x;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0x0002U;      /* slot 1 is the only bot: RIVET */
    rules.weapon_mask = 0x07FFU;
    rules.match_ticks = 3000U;
    rules.lava_start_tick = 2900U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;
    match.spawn_shield_ticks[0] = 0U;
    match.spawn_shield_ticks[1] = 0U;
    bot_x = match.players[1].position_x.value_q16 >> 16;
    bot_y = match.players[1].position_y.value_q16 >> 16;
    /*
     * Seal the bot into a pocket with its goal on the far side of a wall it
     * cannot jump, steam over, or walk around.  Before bots could dig, this
     * was a life sentence.
     */
    for (x = (vox_u32)(bot_x - 20); x <= (vox_u32)(bot_x + 30); ++x) {
        for (y = (vox_u32)(bot_y - 20); y <= (vox_u32)(bot_y + 6); ++y) {
            if (!set_test_column(&match.world, x, y, VOX_MAT_SOIL)) return 2;
        }
    }
    for (x = (vox_u32)(bot_x - 6); x <= (vox_u32)(bot_x + 5); ++x) {
        for (y = (vox_u32)(bot_y - 4); y <= (vox_u32)(bot_y + 1); ++y) {
            if (!set_test_column(&match.world, x, y, VOX_MAT_AIR)) return 3;
        }
    }
    for (x = (vox_u32)(bot_x + 10); x <= (vox_u32)(bot_x + 30); ++x) {
        for (y = (vox_u32)(bot_y - 4); y <= (vox_u32)(bot_y + 1); ++y) {
            if (!set_test_column(&match.world, x, y, VOX_MAT_AIR)) return 4;
        }
    }
    wall_x = (vox_u32)(bot_x + 6);
    if (vox_world_sleep_all(&match.world) != VOX_OK) return 5;
    match.players[0].position_x.value_q16 = (vox_i32)((bot_x + 24) << 16);
    match.players[0].position_y.value_q16 =
        match.players[1].position_y.value_q16;
    for (tick = 0U; tick < 1200U && match.phase == VOX_DIGS_RUNNING; ++tick) {
        /*
         * Pin the intent rather than the behaviour: the bot wants to be on
         * the far side.  How it gets there is what is under test.
         */
        match.bots[1].mode = VOX_DIGS_AI_ROAMING;
        match.bots[1].roam_goal_x = (vox_u16)(bot_x + 24);
        match.bots[1].roam_goal_ticks = 900U;
        if (vox_digs_match_step(&match) != VOX_OK) return 6;
        if (match.event_count > 0U) {
            (void)vox_digs_consume_events(&match, match.event_count);
        }
    }
    for (x = wall_x; x < wall_x + 4U; ++x) {
        for (y = (vox_u32)(bot_y - 4); y <= (vox_u32)(bot_y + 1); ++y) {
            if (vox_world_collision_classify(&match.world, x, y) !=
                VOX_WORLD_COLLISION_SOLID) {
                opened++;
            }
        }
    }
    /* The wall must be substantially gone, not merely scratched. */
    if (opened < 8U) return 7;
    /* And the miner must actually be on the other side of it. */
    if ((match.players[1].position_x.value_q16 >> 16) <=
        (vox_i32)(wall_x + 3U)) {
        return 8;
    }
    return 0;
}

static int test_wide_excavation_caves_in(void)
{
    vox_digs_rules rules;
    vox_u32 x;
    vox_u32 y;
    vox_u32 tick;
    vox_u32 wide_open = 0U;
    vox_u32 narrow_open = 0U;
    const vox_u32 base_x = 200U;
    const vox_u32 ground = 210U;
    const vox_u32 roof = 180U;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;
    /* Solid overburden, with clear air above it. */
    for (y = roof; y <= ground; ++y) {
        for (x = base_x - 10U; x <= base_x + 60U; ++x) {
            if (!set_test_column(&match.world, x, y, VOX_MAT_SOIL)) return 2;
        }
    }
    for (y = roof - 12U; y < roof; ++y) {
        for (x = base_x - 10U; x <= base_x + 60U; ++x) {
            if (!set_test_column(&match.world, x, y, VOX_MAT_AIR)) return 3;
        }
    }
    if (vox_world_sleep_all(&match.world) != VOX_OK) return 4;
    /* A 40-wide chamber: far past the cohesion span, so it must slump. */
    for (y = ground - 3U; y <= ground; ++y) {
        for (x = base_x; x < base_x + 40U; ++x) {
            if (!set_test_column(&match.world, x, y, VOX_MAT_AIR)) return 5;
        }
    }
    for (tick = 0U; tick < 300U; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) return 6;
    }
    for (y = ground - 3U; y <= ground; ++y) {
        for (x = base_x; x < base_x + 40U; ++x) {
            if (vox_world_collision_classify(&match.world, x, y) !=
                VOX_WORLD_COLLISION_SOLID) {
                wide_open++;
            }
        }
    }
    /* At least a third of the void must have filled with fallen material. */
    if (wide_open * 3U > 40U * 4U * 2U) {
        return 7;
    }

    /* Now a narrow tunnel in fresh ground: it must survive intact. */
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 8;
    for (y = roof; y <= ground; ++y) {
        for (x = base_x - 10U; x <= base_x + 60U; ++x) {
            if (!set_test_column(&match.world, x, y, VOX_MAT_SOIL)) return 9;
        }
    }
    for (y = roof - 12U; y < roof; ++y) {
        for (x = base_x - 10U; x <= base_x + 60U; ++x) {
            if (!set_test_column(&match.world, x, y, VOX_MAT_AIR)) return 10;
        }
    }
    if (vox_world_sleep_all(&match.world) != VOX_OK) return 11;
    for (y = ground - 3U; y <= ground; ++y) {
        for (x = base_x; x < base_x + 5U; ++x) {
            if (!set_test_column(&match.world, x, y, VOX_MAT_AIR)) return 12;
        }
    }
    for (tick = 0U; tick < 300U; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) return 13;
    }
    for (y = ground - 3U; y <= ground; ++y) {
        for (x = base_x; x < base_x + 5U; ++x) {
            if (vox_world_collision_classify(&match.world, x, y) !=
                VOX_WORLD_COLLISION_SOLID) {
                narrow_open++;
            }
        }
    }
    if (narrow_open != 5U * 4U) {
        return 14;
    }
    return 0;
}

/*
 * A smoker canister used to rewind onto its victim and re-strike every tick
 * for the whole fuse, pinning itself in place and spraying damage events.
 * A direct hit must land exactly once, hurt hard, never kill from full
 * health, and then roll away spent.
 */
static int test_smoker_direct_hit_lands_once(void)
{
    vox_digs_rules rules;
    vox_digs_input input;
    vox_u16 tick;
    vox_u16 damage_events = 0U;
    vox_i32 origin_x;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;
    match.spawn_shield_ticks[0] = 0U;
    match.spawn_shield_ticks[1] = 0U;
    origin_x = match.players[0].position_x.value_q16;
    match.players[1].position_x.value_q16 = origin_x + (3L << 16);
    match.players[1].position_y.value_q16 =
        match.players[0].position_y.value_q16;
    init_test_input(&input, 0U,
                    (vox_u16)(match.players[1].position_x.value_q16 >> 16),
                    (vox_u16)(match.players[1].position_y.value_q16 >> 16));
    input.selected_weapon = VOX_DIGS_TOOL_SMOKER;
    for (tick = 0U; tick < 120U; ++tick) {
        vox_u16 ordinal;
        /* Hold briefly so the charged throw releases, then let it fly. */
        input.actions = tick < 2U ? VOX_DIGS_ACTION_FIRE : 0U;
        if (vox_digs_submit_input(&match, &input) != VOX_OK) return 2;
        if (vox_digs_match_step(&match) != VOX_OK) return 3;
        for (ordinal = 0U; ordinal < match.event_count; ++ordinal) {
            const vox_digs_event *event = vox_digs_event_get(&match, ordinal);
            if (event != 0 && event->type == VOX_DIGS_EVENT_DAMAGE &&
                event->target == 1U &&
                event->weapon == VOX_DIGS_TOOL_SMOKER) {
                damage_events++;
            }
        }
        if (match.event_count > 0U &&
            vox_digs_consume_events(&match, match.event_count) != VOX_OK) {
            return 4;
        }
    }
    /* Exactly one strike, and a full-health miner is hurt but standing. */
    if (damage_events != 1U || !match.alive[1] || match.deaths[1] != 0U) {
        return 5;
    }
    if (match.health[1] > VOX_DIGS_MAX_HEALTH - DIGS_TEST_SMOKER_DAMAGE) {
        return 6;
    }
    return 0;
}

/*
 * The hot rail bores by heating terrain, and used to convert coal and
 * biomass straight to lava.  A miner tunnelling down through a seam
 * liquefied their own floor, fell into the pool, and died to lava contact
 * credited to the hot rail.  Heating flammable strata past ignition is the
 * intended behaviour; leaving molten rock in the bore is not.
 */
static int test_hot_rail_bore_leaves_no_lava(void)
{
    vox_digs_rules rules;
    vox_digs_input input;
    vox_i32 center_x;
    vox_i32 center_y;
    vox_i32 x;
    vox_i32 y;
    vox_u32 z;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;
    match.spawn_shield_ticks[0] = 0U;
    center_x = match.players[0].position_x.value_q16 >> 16;
    center_y = match.players[0].position_y.value_q16 >> 16;
    /* Lay a coal seam to the miner's right, well inside hot rail range. */
    for (y = center_y - 2L; y <= center_y + 2L; ++y) {
        for (x = center_x + 2L; x <= center_x + 8L; ++x) {
            if (x >= 0L && y >= 0L &&
                !set_test_column(&match.world, (vox_u32)x, (vox_u32)y,
                                 VOX_MAT_COAL)) {
                return 2;
            }
        }
    }
    init_test_input(&input, 0U, (vox_u16)(center_x + 6L), (vox_u16)center_y);
    input.selected_weapon = VOX_DIGS_TOOL_HOT_RAIL;
    input.actions = VOX_DIGS_ACTION_FIRE;
    if (vox_digs_submit_input(&match, &input) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK) {
        return 3;
    }
    /* The seam must be scorched, never molten. */
    for (y = center_y - 2L; y <= center_y + 2L; ++y) {
        for (x = center_x + 2L; x <= center_x + 8L; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(&match.world,
                    (vox_u32)x, (vox_u32)y, z);
                if (cell != 0 && cell->material == VOX_MAT_LAVA) {
                    return 4;
                }
            }
        }
    }
    return 0;
}

/*
 * Partial burial -- the case the lead reported as "random dying from digging
 * through the landscape".  Settling debris leaves a miner overlapping solid
 * terrain without fully entombing them.  That must never be instantly fatal,
 * and clearing the obstruction must end the emergency cleanly.
 */
static int test_partial_burial_is_survivable(void)
{
    vox_digs_rules rules;
    vox_i32 center_x;
    vox_i32 center_y;
    vox_i32 x;
    vox_i32 y;
    vox_u16 tick;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) return 1;
    center_x = match.players[0].position_x.value_q16 >> 16;
    center_y = match.players[0].position_y.value_q16 >> 16;
    for (y = center_y - 3L; y <= center_y + 3L; ++y) {
        for (x = center_x - 3L; x <= center_x + 3L; ++x) {
            if (x >= 0L && y >= 0L &&
                !set_test_column(&match.world, (vox_u32)x, (vox_u32)y,
                                 VOX_MAT_SOIL)) {
                return 2;
            }
        }
    }
    /* Burial cannot hurt an invulnerable miner, so retire the spawn shield. */
    match.spawn_shield_ticks[0] = 0U;
    if (vox_world_sleep_all(&match.world) != VOX_OK) return 3;
    for (tick = 0U; tick < 20U; ++tick) {
        if (vox_digs_match_step(&match) != VOX_OK) return 4;
    }
    /* Hurt and clearly flagged, but alive and still holding their slot. */
    if (!match.alive[0] || match.deaths[0] != 0U ||
        match.buried_ticks[0] == 0U ||
        match.health[0] >= VOX_DIGS_MAX_HEALTH) {
        return 5;
    }
    /* Digging free ends the emergency and resets the struggle timer. */
    for (y = center_y - 4L; y <= center_y + 4L; ++y) {
        for (x = center_x - 4L; x <= center_x + 4L; ++x) {
            if (x >= 0L && y >= 0L &&
                !set_test_column(&match.world, (vox_u32)x, (vox_u32)y,
                                 VOX_MAT_AIR)) {
                return 6;
            }
        }
    }
    if (vox_world_sleep_all(&match.world) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK ||
        !match.alive[0] || match.buried_ticks[0] != 0U) {
        return 7;
    }
    return 0;
}

static int test_v003_swept_hit_and_effect_deposition(void)
{
    vox_digs_rules rules;
    vox_u32 x;
    vox_u32 y;
    vox_u16 tick;
    vox_u16 deposited = 0U;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK ||
        !v003_clear_box(&v003_match_a.world, 48U, 72U, 132U, 116U)) {
        return 1;
    }
    v003_place_player(&v003_match_a, 0U, 60U, 100U);
    v003_place_player(&v003_match_a, 1U, 91U, 100U);
    v003_match_a.spawn_shield_ticks[0] = 0U;
    v003_match_a.spawn_shield_ticks[1] = 0U;
    v003_match_a.projectiles[0].active = 1U;
    v003_match_a.projectiles[0].owner = 0U;
    v003_match_a.projectiles[0].weapon = VOX_DIGS_TOOL_NAIL_GUN;
    v003_match_a.projectiles[0].material = VOX_MAT_METAL;
    v003_match_a.projectiles[0].position_x_q16 =
        (70L << 16) + 32768L;
    v003_match_a.projectiles[0].position_y_q16 =
        (98L << 16) + 32768L;
    v003_match_a.projectiles[0].velocity_x_q16 = 8L << 16;
    v003_match_a.projectiles[0].velocity_y_q16 = 0L;
    v003_match_a.projectiles[0].fuse_ticks = 40U;
    v003_match_a.projectiles[0].age_ticks = 0U;
    v003_match_a.projectiles[0].damage = 18U;
    v003_match_a.projectiles[0].blast_radius = 0U;
    v003_match_a.projectiles[0].owner_clear = 0U;
    v003_match_a.projectiles[0].arming_ticks = 0U;
    v003_match_a.projectile_count = 1U;
    if (vox_world_sleep_all(&v003_match_a.world) != VOX_OK) {
        return 2;
    }
    for (tick = 0U; tick < 8U && v003_match_a.health[1] ==
         VOX_DIGS_MAX_HEALTH; ++tick) {
        if (vox_digs_match_step(&v003_match_a) != VOX_OK) return 3;
    }
    if (v003_match_a.health[1] >= VOX_DIGS_MAX_HEALTH) return 4;
    if (v003_match_a.last_damage_part[1] == VOX_DIGS_NO_PART) return 9;
    if (v003_match_a.last_damage_weapon[1] != VOX_DIGS_TOOL_NAIL_GUN)
        return 10;
    if (!v003_clear_box(&v003_match_a.world, 140U, 30U, 150U, 110U) ||
        !set_test_column(&v003_match_a.world, 145U, 100U,
                         VOX_MAT_STONE)) {
        return 5;
    }
    v003_match_a.effects[0].active = 1U;
    v003_match_a.effects[0].material = VOX_MAT_STONE;
    v003_match_a.effects[0].position_x_q16 = (145L << 16) + 32768L;
    v003_match_a.effects[0].position_y_q16 = (70L << 16) + 32768L;
    v003_match_a.effects[0].velocity_x_q16 = 0L;
    v003_match_a.effects[0].velocity_y_q16 = 36L << 16;
    v003_match_a.effects[0].ttl_ticks = 20U;
    v003_match_a.effects[0].variant = 0U;
    v003_match_a.effects[0].source = 0U;
    v003_match_a.effects[0].depth = 0U;
    v003_match_a.effects[0].flags = 0U;
    v003_match_a.effects[1] = v003_match_a.effects[0];
    v003_match_a.effects[1].position_x_q16 = (148L << 16) + 32768L;
    v003_match_a.effects[1].position_y_q16 = (50L << 16) + 32768L;
    v003_match_a.effects[1].velocity_y_q16 = 0L;
    v003_match_a.effects[1].ttl_ticks = 1U;
    v003_match_a.effect_count = 2U;
    if (vox_world_sleep_all(&v003_match_a.world) != VOX_OK ||
        vox_digs_match_step(&v003_match_a) != VOX_OK ||
        v003_match_a.effects[0].active ||
        v003_match_a.effects[1].active) {
        return 6;
    }
    for (y = 30U; y < 100U; ++y) {
        const vox_cell *cell = vox_world_cell(&v003_match_a.world,
                                               145U, y, 0U);
        if (cell != 0 && cell->material == VOX_MAT_STONE) deposited++;
    }
    for (x = 147U; x <= 149U; ++x) {
        for (y = 49U; y <= 51U; ++y) {
            const vox_cell *cell = vox_world_cell(&v003_match_a.world,
                                                   x, y, 0U);
            if (cell != 0 && cell->material == VOX_MAT_STONE) return 7;
        }
    }
    if (deposited > 1U) return 8;
    return 0;
}

static int test_v003_rope_strike_cover_and_shield(void)
{
    vox_digs_rules rules;
    vox_digs_input input;
    vox_u16 tick;
    vox_u16 torso_health;
    vox_u32 y;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK ||
        !v003_clear_box(&v003_match_a.world, 88U, 82U, 132U, 112U)) {
        return 1;
    }
    v003_place_player(&v003_match_a, 0U, 96U, 100U);
    v003_place_player(&v003_match_a, 1U, 120U, 100U);
    v003_match_a.spawn_shield_ticks[1] = 0U;
    torso_health = v003_match_a.anatomy[1][VOX_DIGS_PART_TORSO].health;
    init_test_input(&input, 0U, 120U, 98U);
    input.actions = VOX_DIGS_ACTION_ROPE;
    if (vox_digs_submit_input(&v003_match_a, &input) != VOX_OK) return 2;
    for (tick = 0U; tick < 16U && v003_match_a.health[1] != 1U; ++tick) {
        if (vox_digs_match_step(&v003_match_a) != VOX_OK) return 3;
    }
    if (v003_match_a.health[1] != 1U || !v003_match_a.alive[1] ||
        v003_match_a.anatomy[1][VOX_DIGS_PART_TORSO].health != torso_health ||
        v003_match_a.last_attacker[1] != 0U ||
        v003_match_a.ropes[0].state != VOX_DIGS_ROPE_IDLE ||
        !event_type_seen(&v003_match_a, VOX_DIGS_EVENT_ROPE_HIT)) {
        return 4;
    }
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK ||
        !v003_clear_box(&v003_match_a.world, 88U, 82U, 132U, 112U)) {
        return 5;
    }
    v003_place_player(&v003_match_a, 0U, 96U, 100U);
    v003_place_player(&v003_match_a, 1U, 120U, 100U);
    for (y = 92U; y <= 104U; ++y) {
        if (!set_test_column(&v003_match_a.world, 108U, y,
                             VOX_MAT_STONE)) return 6;
    }
    init_test_input(&input, 0U, 120U, 98U);
    input.actions = VOX_DIGS_ACTION_ROPE;
    if (vox_digs_submit_input(&v003_match_a, &input) != VOX_OK) return 7;
    for (tick = 0U; tick < 16U &&
         !event_type_seen(&v003_match_a, VOX_DIGS_EVENT_ROPE_ATTACH) &&
         !event_type_seen(&v003_match_a, VOX_DIGS_EVENT_ROPE_BREAK);
         ++tick) {
        if (vox_digs_match_step(&v003_match_a) != VOX_OK) return 8;
    }
    if (v003_match_a.health[1] != VOX_DIGS_MAX_HEALTH ||
        event_type_seen(&v003_match_a, VOX_DIGS_EVENT_ROPE_HIT) ||
        v003_match_a.ropes[0].point_count > VOX_DIGS_ROPE_MAX_POINTS) {
        return 9;
    }
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK ||
        !v003_clear_box(&v003_match_a.world, 88U, 82U, 132U, 112U)) {
        return 10;
    }
    v003_place_player(&v003_match_a, 0U, 96U, 100U);
    v003_place_player(&v003_match_a, 1U, 120U, 100U);
    init_test_input(&input, 0U, 120U, 98U);
    input.actions = VOX_DIGS_ACTION_ROPE;
    if (vox_digs_submit_input(&v003_match_a, &input) != VOX_OK) return 11;
    for (tick = 0U; tick < 16U &&
         !event_type_seen(&v003_match_a, VOX_DIGS_EVENT_SHIELD_BLOCK);
         ++tick) {
        if (vox_digs_match_step(&v003_match_a) != VOX_OK) return 12;
    }
    if (v003_match_a.health[1] != VOX_DIGS_MAX_HEALTH ||
        v003_match_a.last_attacker[1] != VOX_DIGS_NO_PLAYER ||
        !event_type_seen(&v003_match_a, VOX_DIGS_EVENT_SHIELD_BLOCK)) {
        return 13;
    }
    return 0;
}

static int test_v003_rail_steam_and_replay(void)
{
    vox_digs_rules rules;
    vox_digs_input input_a;
    vox_digs_input input_b;
    vox_u16 tick;
    vox_u16 shield_before;
    vox_i32 release_velocity_x;
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    rules.weapon_mask = (vox_u16)(rules.weapon_mask &
        (vox_u16)~(vox_u16)(1U << VOX_DIGS_TOOL_RAIL_GUN));
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK) {
        return 19;
    }
    shield_before = v003_match_a.spawn_shield_ticks[0];
    init_test_input(&input_a, 0U, 180U, 98U);
    input_a.selected_weapon = VOX_DIGS_TOOL_RAIL_GUN;
    input_a.actions = VOX_DIGS_ACTION_FIRE;
    if (vox_digs_submit_input(&v003_match_a, &input_a) != VOX_OK ||
        vox_digs_match_step(&v003_match_a) != VOX_OK ||
        v003_match_a.rail_charging[0] != 0U ||
        v003_match_a.rail_charge_ticks[0] != 0U ||
        v003_match_a.spawn_shield_ticks[0] !=
            (vox_u16)(shield_before - 1U) ||
        event_type_seen(&v003_match_a, VOX_DIGS_EVENT_RAIL_CHARGE)) {
        return 20;
    }
    vox_digs_rules_classic(&rules);
    rules.player_count = 3U;
    rules.bot_mask = 0x0004U;
    rules.score_limit = 0U;
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK ||
        !v003_clear_box(&v003_match_a.world, 72U, 70U, 188U, 112U)) {
        return 1;
    }
    v003_place_player(&v003_match_a, 0U, 80U, 100U);
    v003_place_player(&v003_match_a, 1U, 120U, 100U);
    v003_place_player(&v003_match_a, 2U, 160U, 100U);
    v003_match_a.spawn_shield_ticks[1] = 0U;
    v003_match_a.spawn_shield_ticks[2] = 0U;
    v003_match_a.bots[2].decision_ticks = 600U;
    init_test_input(&input_a, 0U, 180U, 98U);
    input_a.selected_weapon = VOX_DIGS_TOOL_RAIL_GUN;
    input_a.actions = VOX_DIGS_ACTION_FIRE;
    if (vox_digs_submit_input(&v003_match_a, &input_a) != VOX_OK ||
        vox_digs_match_step(&v003_match_a) != VOX_OK ||
        !event_type_seen(&v003_match_a, VOX_DIGS_EVENT_RAIL_CHARGE)) {
        return 2;
    }
    v003_match_a.rail_charge_ticks[0] = 71U;
    if (vox_digs_match_step(&v003_match_a) != VOX_OK ||
        v003_match_a.rail_charge_ticks[0] != 72U) return 3;
    input_a.actions = 0U;
    if (vox_digs_submit_input(&v003_match_a, &input_a) != VOX_OK)
        return 4;
    if (vox_digs_match_step(&v003_match_a) != VOX_OK) return 13;
    if (v003_match_a.rail_charging[0]) return 14;
    if (v003_match_a.weapon_cooldown[0] != 75U) return 15;
    if (!event_type_seen(&v003_match_a, VOX_DIGS_EVENT_RAIL_TRACE))
        return 16;
    if (v003_match_a.alive[1]) {
        fprintf(stderr, "rail debug hp=%u torso=%u part=%u shield=%u p2=%u\n",
                (unsigned int)v003_match_a.health[1],
                (unsigned int)v003_match_a.anatomy[1][VOX_DIGS_PART_TORSO].health,
                (unsigned int)v003_match_a.last_damage_part[1],
                (unsigned int)v003_match_a.spawn_shield_ticks[1],
                (unsigned int)v003_match_a.health[2]);
        return 17;
    }
    if (v003_match_a.health[2] >= VOX_DIGS_MAX_HEALTH) return 18;
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK) return 5;
    v003_match_a.spawn_shield_ticks[1] = 0U;
    if (vox_digs_apply_hit(&v003_match_a, 0U, 1U,
                           VOX_DIGS_TOOL_RAIL_GUN,
                           VOX_DIGS_PART_LEFT_THIGH, 100U,
                           VOX_DIGS_DAMAGE_BALLISTIC) != VOX_OK ||
        !v003_match_a.alive[1] || v003_match_a.health[1] != 50U ||
        (v003_match_a.anatomy[1][VOX_DIGS_PART_LEFT_THIGH].flags &
         VOX_DIGS_PART_SEVERED) == 0U) {
        return 6;
    }
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK ||
        !v003_clear_box(&v003_match_a.world, 220U, 80U, 280U, 250U)) {
        return 7;
    }
    v003_place_player(&v003_match_a, 0U, 250U, 220U);
    init_test_input(&input_a, 0U, 270U, 180U);
    input_a.actions = VOX_DIGS_ACTION_STEAM;
    input_a.move_x_q15 = 24575;
    for (tick = 0U; tick < 30U; ++tick) {
        if (vox_digs_submit_input(&v003_match_a, &input_a) != VOX_OK ||
            vox_digs_match_step(&v003_match_a) != VOX_OK) return 8;
    }
    if (v003_match_a.steam_q16[0] >= 45000U ||
        v003_match_a.players[0].velocity_y.value_q16 < -98304L ||
        v003_match_a.players[0].velocity_x.value_q16 <= 0L) return 9;
    release_velocity_x = v003_match_a.players[0].velocity_x.value_q16;
    input_a.actions = 0U;
    input_a.move_x_q15 = 0;
    if (vox_digs_submit_input(&v003_match_a, &input_a) != VOX_OK ||
        vox_digs_match_step(&v003_match_a) != VOX_OK ||
        v003_match_a.players[0].velocity_x.value_q16 <= 0L ||
        v003_match_a.players[0].velocity_x.value_q16 >= release_velocity_x) {
        return 10;
    }
    if (vox_digs_match_init(&v003_match_a, &rules) != VOX_OK ||
        vox_digs_match_init(&v003_match_b, &rules) != VOX_OK) return 11;
    init_test_input(&input_a, 0U, 300U, 100U);
    init_test_input(&input_b, 0U, 300U, 100U);
    for (tick = 0U; tick < 24U; ++tick) {
        vox_u16 actions = tick < 8U ?
            (vox_u16)(VOX_DIGS_ACTION_RIGHT | VOX_DIGS_ACTION_STEAM) :
            (tick < 18U ? VOX_DIGS_ACTION_FIRE : 0U);
        input_a.actions = actions;
        input_b.actions = actions;
        input_a.selected_weapon = tick >= 8U ?
            VOX_DIGS_TOOL_RAIL_GUN : VOX_DIGS_TOOL_PICK;
        input_b.selected_weapon = input_a.selected_weapon;
        if (vox_digs_submit_input(&v003_match_a, &input_a) != VOX_OK ||
            vox_digs_submit_input(&v003_match_b, &input_b) != VOX_OK ||
            vox_digs_match_step(&v003_match_a) != VOX_OK ||
            vox_digs_match_step(&v003_match_b) != VOX_OK ||
            v003_match_a.state_hash != v003_match_b.state_hash) return 12;
    }
    return 0;
}

int main(void)
{
    vox_digs_rules rules;
    vox_u32 first;
    vox_u32 second;
    vox_digs_rules_classic(&rules);
    if (rules.match_ticks != 7200U || rules.lava_start_tick != 5400U ||
        rules.respawn_mode != VOX_DIGS_RESPAWN_AUTO ||
        rules.respawn_delay_ticks != VOX_DIGS_RESPAWN_TICKS) {
        fprintf(stderr, "classic timing mismatch\n");
        return 1;
    }
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_record_kill(&match, 0U, 1U) != VOX_OK ||
        match.scores[0] != 1U) {
        fprintf(stderr, "kill attribution mismatch\n");
        return 2;
    }
    {
        int result = test_respawn_modes_and_requests();
        if (result != 0) {
            fprintf(stderr, "DIGS respawn-mode mismatch (%d)\n", result);
            return 21;
        }
    }
    {
        int result = test_match_results_and_final_batch();
        if (result != 0) {
            fprintf(stderr, "DIGS match-result mismatch (%d)\n", result);
            return 22;
        }
    }
    {
        int result = test_last_attacker_expiry();
        if (result != 0) {
            fprintf(stderr, "DIGS attacker-expiry mismatch (%d)\n", result);
            return 23;
        }
    }
    {
        int result = test_projectile_owner_clearance();
        if (result != 0) {
            fprintf(stderr, "DIGS projectile-clearance mismatch (%d)\n",
                    result);
            return 24;
        }
    }
    {
        int map_generation_result = test_map_generation();
        if (map_generation_result != 0) {
            fprintf(stderr, "DIGS map generation mismatch (%d)\n",
                    map_generation_result);
            return 4;
        }
    }
    {
        int map_topology_result = test_map_topology();
        if (map_topology_result != 0) {
            fprintf(stderr, "DIGS map topology mismatch (%d)\n",
                    map_topology_result);
            return 20;
        }
    }
    if (test_player_layout_and_input_authority() != 0) {
        fprintf(stderr, "DIGS player layout/input authority mismatch\n");
        return 14;
    }
    {
        int shield_result = test_spawn_shield_timing_and_attack_cancel();
        if (shield_result != 0) {
            fprintf(stderr, "DIGS spawn shield mismatch (%d)\n",
                    shield_result);
            return 15;
        }
    }
    {
        int rope_result = test_rope_reel_anchor_and_events();
        if (rope_result != 0) {
            fprintf(stderr, "DIGS rope/event mismatch (%d)\n", rope_result);
            return 16;
        }
    }
    {
        int anatomy_result = test_anatomy_bleed_cautery_and_sever();
        if (anatomy_result != 0) {
            fprintf(stderr, "DIGS anatomy mismatch (%d)\n", anatomy_result);
            return 17;
        }
    }
    {
        int ai_result = test_ai_state_machine();
        if (ai_result != 0) {
            fprintf(stderr, "DIGS AI state mismatch (%d)\n", ai_result);
            return 18;
        }
    }
    {
        int movement_result = test_movement_acceleration_and_step_assist();
        if (movement_result != 0) {
            fprintf(stderr, "DIGS movement mismatch (%d)\n",
                    movement_result);
            return 19;
        }
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
    if (test_combat_and_respawn() != 0) {
        fprintf(stderr, "DIGS combat/respawn mismatch\n");
        return 8;
    }
    if (test_weapon_table_and_pool() != 0) {
        fprintf(stderr, "DIGS weapon/pool mismatch\n");
        return 9;
    }
    {
        int bot_result = test_bot_authority();
        if (bot_result != 0) {
            fprintf(stderr, "DIGS bot authority mismatch (%d)\n",
                    bot_result);
            return 10;
        }
    }
    {
        int lava_result = test_rising_lava();
        if (lava_result != 0) {
            fprintf(stderr, "DIGS rising lava mismatch (%d)\n", lava_result);
            return 11;
        }
    }
    if (test_rule_bounds_and_long_lava() != 0) {
        fprintf(stderr, "DIGS rule/lava range mismatch\n");
        return 12;
    }
    if (test_bot_score_limit_step() != 0) {
        fprintf(stderr, "DIGS bot score-limit step mismatch\n");
        return 13;
    }
    {
        int result = test_v003_event_drain_ai_invariance();
        if (result != 0) {
            fprintf(stderr,
                    "DIGS v0.0.3 AI event-drain mismatch (%d)\n", result);
            return 25;
        }
    }
    {
        int result = test_bot_archetypes_and_charge_weapons();
        if (result != 0) {
            fprintf(stderr, "DIGS archetype mismatch (%d)\n", result);
            return 65;
        }
    }
    {
        int result = test_bark_pacing_and_variance();
        if (result != 0) {
            fprintf(stderr, "DIGS bark pacing mismatch (%d)\n", result);
            return 74;
        }
    }
    {
        int result = test_memory_carries_between_matches();
        if (result != 0) {
            fprintf(stderr, "DIGS memory mismatch (%d)\n", result);
            return 73;
        }
    }
    {
        int result = test_contracts_steer_targeting();
        if (result != 0) {
            fprintf(stderr, "DIGS contract targeting mismatch (%d)\n", result);
            return 72;
        }
    }
    {
        int result = test_speech_is_paced_and_answered();
        if (result != 0) {
            fprintf(stderr, "DIGS speech mismatch (%d)\n", result);
            return 71;
        }
    }
    {
        int result = test_every_line_cell_resolves();
        if (result != 0) {
            fprintf(stderr, "DIGS line index mismatch (%d)\n", result);
            return 70;
        }
    }
    {
        int result = test_stimuli_reach_the_contract();
        if (result != 0) {
            fprintf(stderr, "DIGS stimulus mismatch (%d)\n", result);
            return 69;
        }
    }
    {
        int result = test_contracts_pair_index_and_tone();
        if (result != 0) {
            fprintf(stderr, "DIGS contract mismatch (%d)\n", result);
            return 68;
        }
    }
    {
        int result = test_kill_heals_the_killer();
        if (result != 0) {
            fprintf(stderr, "DIGS kill-heal mismatch (%d)\n", result);
            return 67;
        }
    }
    {
        int result = test_bot_bores_through_a_wall();
        if (result != 0) {
            fprintf(stderr, "DIGS bot breach mismatch (%d)\n", result);
            return 66;
        }
    }
    {
        int result = test_wide_excavation_caves_in();
        if (result != 0) {
            fprintf(stderr, "DIGS cave-in mismatch (%d)\n", result);
            return 64;
        }
    }
    {
        int result = test_smoker_direct_hit_lands_once();
        if (result != 0) {
            fprintf(stderr, "DIGS smoker direct-hit mismatch (%d)\n", result);
            return 63;
        }
    }
    {
        int result = test_hot_rail_bore_leaves_no_lava();
        if (result != 0) {
            fprintf(stderr, "DIGS hot-rail bore mismatch (%d)\n", result);
            return 62;
        }
    }
    {
        int result = test_partial_burial_is_survivable();
        if (result != 0) {
            fprintf(stderr, "DIGS partial-burial mismatch (%d)\n", result);
            return 61;
        }
    }
    {
        int result = test_v003_overlap_recovery_and_crush();
        if (result != 0) {
            fprintf(stderr, "DIGS v0.0.3 overlap/crush mismatch (%d)\n",
                    result);
            return 26;
        }
    }
    {
        int result = test_v003_swept_hit_and_effect_deposition();
        if (result != 0) {
            fprintf(stderr, "DIGS v0.0.3 sweep/deposition mismatch (%d)\n",
                    result);
            return 27;
        }
    }
    {
        int result = test_v003_rope_strike_cover_and_shield();
        if (result != 0) {
            fprintf(stderr, "DIGS v0.0.3 rope mismatch (%d)\n", result);
            return 28;
        }
    }
    {
        int result = test_v003_rope_wrap_unwrap_replay();
        if (result != 0) {
            fprintf(stderr, "DIGS v0.0.3 rope wrap mismatch (%d)\n",
                    result);
            return 29;
        }
    }
    {
        int result = test_v003_rail_strata_and_shield();
        if (result != 0) {
            fprintf(stderr, "DIGS v0.0.3 rail strata mismatch (%d)\n",
                    result);
            return 30;
        }
    }
    {
        int result = test_v003_rail_steam_and_replay();
        if (result != 0) {
            fprintf(stderr, "DIGS v0.0.3 rail/steam/replay mismatch (%d)\n",
                    result);
            return 31;
        }
    }
    if (run_match(&first) != 0 || run_match(&second) != 0 || first != second) {
        fprintf(stderr, "DIGS determinism mismatch\n");
        return 3;
    }
    printf("DIGS deterministic hash=%08x\n", (unsigned int)first);
    return 0;
}

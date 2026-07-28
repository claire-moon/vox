/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_game.h"

#define TEST_MAP_JUMP_ENVELOPE 28U
#define TEST_MAP_RAIL_MIN_CLEARANCE 36U
#define TEST_MAP_ROPE_REACH 48U
#define TEST_SPAWN_HEADROOM_CELLS 4U
#define TEST_SPAWN_SUPPORT_CELLS 4U

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
    rules.player_count = 4U;
    rules.bot_mask = 0x000eU;
    if (vox_digs_match_init(&match, &rules) != VOX_ERR_INVALID) {
        return 6;
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
    wall_x = (player_x + (vox_u32)bot_x) / 2U;
    if (!set_test_column(&match.world, wall_x, (vox_u32)bot_y,
                         VOX_MAT_METAL)) {
        return 5;
    }
    match.health[1] = VOX_DIGS_MAX_HEALTH;
    match.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_think(&match, 1U) != VOX_OK ||
        match.bots[1].mode != VOX_DIGS_AI_SEARCHING ||
        !event_mode_seen(&match, VOX_DIGS_AI_SEARCHING)) {
        return 6;
    }
    match.bots[1].memory_ticks = 0U;
    match.bots[1].decision_ticks = 0U;
    if (vox_digs_bot_think(&match, 1U) != VOX_OK ||
        match.bots[1].mode != VOX_DIGS_AI_ROAMING ||
        !event_mode_seen(&match, VOX_DIGS_AI_ROAMING)) {
        return 7;
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
    input.actions = (vox_u16)(VOX_DIGS_ACTION_MASK | 64U);
    if (vox_digs_submit_input(&match, &input) != VOX_ERR_INVALID) {
        return 12;
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
    if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_NAIL_GUN,
                             target_x, target_y) != VOX_OK ||
        match.projectile_count != 1U ||
        match.selected_weapon[0] != VOX_DIGS_TOOL_NAIL_GUN ||
        match.weapon_cooldown[0] == 0U) {
        return 9;
    }
    if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_NAIL_GUN,
                             target_x, target_y) != VOX_ERR_INVALID) {
        return 10;
    }
    for (shot = 1U; shot < VOX_DIGS_MAX_PROJECTILES; ++shot) {
        match.weapon_cooldown[0] = 0U;
        if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_NAIL_GUN,
                                 target_x, target_y) != VOX_OK) {
            return 11;
        }
    }
    if (match.projectile_count != VOX_DIGS_MAX_PROJECTILES) {
        return 12;
    }
    match.weapon_cooldown[0] = 0U;
    hash_before = vox_digs_hash(&match);
    if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_NAIL_GUN,
                             target_x, target_y) != VOX_ERR_CAPACITY ||
        match.projectile_count != VOX_DIGS_MAX_PROJECTILES ||
        vox_digs_hash(&match) != hash_before) {
        return 13;
    }
    rules.weapon_mask = (vox_u16)(1U << VOX_DIGS_TOOL_PICK);
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_NAIL_GUN,
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
        match.winner_player != 0U ||
        match.winner_team != VOX_DIGS_NO_TEAM) {
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
    rules.team_mode = VOX_DIGS_MODE_MINERS_VS_MACHINES;
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_record_kill(&match, 0U, 1U) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK ||
        match.result_reason != VOX_DIGS_END_SCORE || match.result_draw ||
        match.winner_team != VOX_DIGS_TEAM_MINERS ||
        match.winner_player != VOX_DIGS_NO_PLAYER) {
        return 5;
    }
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
    if (vox_digs_fire_weapon(&match, 0U, VOX_DIGS_TOOL_BLAST_CHARGE,
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
        match.health[0] != health_before) {
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
        match.projectiles[slot].arming_ticks = 0U;
        match.projectiles[slot].fuse_ticks = 1U;
        if (vox_digs_match_step(&match) != VOX_OK ||
            match.health[0] >= health_before) {
            return 5;
        }
    } else {
        return 6;
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
    if (vox_world_sleep_all(&v003_match_a.world) != VOX_OK ||
        vox_digs_match_step(&v003_match_a) != VOX_OK ||
        v003_match_a.alive[0] ||
        v003_match_a.deaths[0] != (vox_u16)(deaths + 1U) ||
        !event_type_seen(&v003_match_a, VOX_DIGS_EVENT_CRUSH)) {
        return 4;
    }
    deaths = v003_match_a.deaths[0];
    if (vox_digs_match_step(&v003_match_a) != VOX_OK ||
        v003_match_a.deaths[0] != deaths) {
        return 5;
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

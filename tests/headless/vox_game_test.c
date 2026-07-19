/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_game.h"

#define TEST_MAP_JUMP_ENVELOPE 28U
#define TEST_MAP_RAIL_MIN_CLEARANCE 36U
#define TEST_MAP_ROPE_REACH 48U
#define TEST_SPAWN_HEADROOM_CELLS 4U
#define TEST_SPAWN_SUPPORT_CELLS 4U

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
    static vox_world world;
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
            if (vox_digs_generate_map(&world, map_style,
                                      seeds[seed_index]) != VOX_OK ||
                world.awake_cells != 0U) {
                return 1;
            }
            first_hash = vox_world_hash(&world);
            if (vox_digs_generate_map(&world, map_style,
                                      seeds[seed_index]) != VOX_OK ||
                world.awake_cells != 0U ||
                vox_world_hash(&world) != first_hash) {
                return 2;
            }
            if (vox_digs_map_landform(map_style, seeds[seed_index]) !=
                seed_index ||
                !test_map_macro_landform(&world, map_style,
                                         seeds[seed_index])) {
                return 14 + (int)(map_style * 3U + seed_index);
            }
        }
        if (vox_digs_generate_map(&world, map_style, seeds[0]) != VOX_OK) {
            return 3;
        }
        hashes[map_style] = vox_world_hash(&world);
        if (!test_map_suspended_fixtures(&world)) {
            return 4 + (int)map_style;
        }
        if (map_style == VOX_DIGS_MAP_COAL_RIDGE) {
            if (test_map_count_material(&world, VOX_MAT_COAL, 0U) < 500U ||
                test_map_count_material(&world, VOX_MAT_SAND, 0U) < 50U ||
                test_map_count_material(&world, VOX_MAT_FIREDAMP, 0U) != 0U ||
                test_map_count_material(&world, VOX_MAT_LAVA, 0U) <
                VOX_WORLD_WIDTH * 8U) {
                return 10;
            }
        } else if (map_style == VOX_DIGS_MAP_DEEPWORKS) {
            if (test_map_count_material(&world, VOX_MAT_FIREDAMP, 0U) < 40U ||
                test_map_count_material(&world, VOX_MAT_AIR,
                                        VOX_WORLD_HEIGHT / 2U + 16U) < 900U ||
                test_map_count_material(&world, VOX_MAT_LAVA, 0U) <
                VOX_WORLD_WIDTH * 8U) {
                return 11;
            }
        } else {
            vox_u32 lava = test_map_count_material(&world, VOX_MAT_LAVA, 0U);
            if (lava <= VOX_WORLD_WIDTH * 20U ||
                test_map_count_material(&world, VOX_MAT_METAL, 0U) < 500U ||
                test_map_count_material(&world, VOX_MAT_FIREDAMP, 0U) != 0U) {
                return 12;
            }
        }
        if (vox_digs_generate_map(&world, map_style, seeds[1]) != VOX_OK ||
            !test_map_walk_lane(&world) ||
            (map_style == VOX_DIGS_MAP_DEEPWORKS &&
             !test_map_deepworks_connected(&world))) {
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
    static vox_digs_match first;
    static vox_digs_match second;
    static vox_digs_match variant;
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
    static vox_digs_match match;
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
    static vox_digs_match match;
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
        !event_type_seen(&match, VOX_DIGS_EVENT_DAMAGE) ||
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
    static vox_digs_match match;
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
    if (vox_digs_submit_input(&match, &input) != VOX_OK ||
        vox_digs_match_step(&match) != VOX_OK || !match.ropes[0].active ||
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
    static vox_digs_match match;
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
    static vox_digs_match match;
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
    static vox_digs_match match;
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
    static vox_digs_match match;
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
    static vox_digs_match match;
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
    static vox_digs_match match;
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
    static vox_digs_match match;
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
    input.actions = (vox_u16)(VOX_DIGS_ACTION_MASK | 32U);
    if (vox_digs_submit_input(&match, &input) != VOX_ERR_INVALID) {
        return 12;
    }
    return 0;
}

static int test_combat_and_respawn(void)
{
    static vox_digs_match match;
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
    static vox_digs_match match;
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
    if (rules.weapon_mask != 0x03ffU) {
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
    static vox_digs_match match;
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
        (match.projectile_count == 0U && match.health[0] == 10U)) {
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
    static vox_digs_match match;
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
    static vox_digs_match match;
    vox_digs_rules rules;
    vox_u32 first_level;
    vox_digs_rules_classic(&rules);
    rules.score_limit = 65536U;
    if (vox_digs_match_init(&match, &rules) != VOX_ERR_INVALID) {
        return 1;
    }
    rules.score_limit = 10U;
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    rules.match_ticks = 100000U;
    rules.lava_start_tick = 1U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 2;
    }
    match.tick = 65537U;
    if (vox_digs_match_step(&match) != VOX_OK ||
        match.lava_level_q16 < 40000U) {
        return 3;
    }
    first_level = match.lava_level_q16;
    if (vox_digs_match_step(&match) != VOX_OK ||
        match.lava_level_q16 < first_level) {
        return 4;
    }
    return 0;
}

static int test_bot_score_limit_step(void)
{
    static vox_digs_match match;
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
    if (test_spawn_shield_timing_and_attack_cancel() != 0) {
        fprintf(stderr, "DIGS spawn shield mismatch\n");
        return 15;
    }
    if (test_rope_reel_anchor_and_events() != 0) {
        fprintf(stderr, "DIGS rope/event mismatch\n");
        return 16;
    }
    if (test_anatomy_bleed_cautery_and_sever() != 0) {
        fprintf(stderr, "DIGS anatomy mismatch\n");
        return 17;
    }
    if (test_ai_state_machine() != 0) {
        fprintf(stderr, "DIGS AI state mismatch\n");
        return 18;
    }
    if (test_movement_acceleration_and_step_assist() != 0) {
        fprintf(stderr, "DIGS movement/step mismatch\n");
        return 19;
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
    if (run_match(&first) != 0 || run_match(&second) != 0 || first != second) {
        fprintf(stderr, "DIGS determinism mismatch\n");
        return 3;
    }
    printf("DIGS deterministic hash=%08x\n", (unsigned int)first);
    return 0;
}

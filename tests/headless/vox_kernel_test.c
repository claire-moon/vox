/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_kernel.h"

#define TEST_AMBIENT_Q16 (20L << 16)

static vox_u32 test_hash_mix(vox_u32 hash, vox_u32 value)
{
    hash ^= value;
    hash *= 16777619U;
    return hash;
}

static vox_u32 test_cell_signature(vox_u32 index, const vox_cell *cell)
{
    vox_u32 hash;
    if (cell->material == VOX_MAT_AIR && cell->flags == 0U &&
        cell->temperature_q16 == TEST_AMBIENT_Q16 && cell->damage_q16 == 0L) {
        return 0U;
    }
    hash = 2166136261U;
    hash = test_hash_mix(hash, index);
    hash = test_hash_mix(hash, (vox_u32)cell->material);
    hash = test_hash_mix(hash, (vox_u32)cell->flags);
    hash = test_hash_mix(hash, (vox_u32)cell->temperature_q16);
    hash = test_hash_mix(hash, (vox_u32)cell->damage_q16);
    return hash;
}

static int test_validate_world(const vox_world *world)
{
    vox_u32 occupied[VOX_WORLD_CHUNK_COUNT];
    vox_u32 awake[VOX_WORLD_CHUNK_COUNT];
    vox_u32 cell_hash[VOX_WORLD_CHUNK_COUNT];
    vox_u32 total_occupied = 0U;
    vox_u32 total_awake = 0U;
    vox_u32 hash = 2166136261U;
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    vox_u32 i;
    if (world == 0) {
        return 1;
    }
    for (i = 0U; i < VOX_WORLD_CHUNK_COUNT; ++i) {
        occupied[i] = 0U;
        awake[i] = 0U;
        cell_hash[i] = 0U;
    }
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (y = 0U; y < VOX_WORLD_HEIGHT; ++y) {
            for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
                const vox_cell *cell = vox_world_cell(world, x, y, z);
                vox_u32 index = z * VOX_WORLD_HEIGHT * VOX_WORLD_WIDTH +
                                y * VOX_WORLD_WIDTH + x;
                vox_u32 chunk_index = (y / VOX_CHUNK_HEIGHT) *
                                      VOX_WORLD_CHUNKS_X +
                                      (x / VOX_CHUNK_WIDTH);
                if (cell == 0) {
                    return 2;
                }
                if (cell->material == VOX_MAT_AIR) {
                    if (cell->flags != 0U ||
                        cell->temperature_q16 != TEST_AMBIENT_Q16 ||
                        cell->damage_q16 != 0L) {
                        return 3;
                    }
                } else {
                    if (!(cell->flags & VOX_CELL_OCCUPIED)) {
                        return 4;
                    }
                    occupied[chunk_index]++;
                    total_occupied++;
                }
                if (cell->flags & VOX_CELL_AWAKE) {
                    if (cell->material == VOX_MAT_AIR) {
                        return 5;
                    }
                    awake[chunk_index]++;
                    total_awake++;
                }
                cell_hash[chunk_index] ^= test_cell_signature(index, cell);
            }
        }
    }
    if (world->occupied_cells != total_occupied ||
        world->awake_cells != total_awake) {
        return 6;
    }
    hash = test_hash_mix(hash, world->tick);
    hash = test_hash_mix(hash, world->occupied_cells);
    hash = test_hash_mix(hash, world->awake_cells);
    for (i = 0U; i < VOX_WORLD_CHUNK_COUNT; ++i) {
        const vox_chunk *chunk = vox_world_chunk(world,
                                                  i % VOX_WORLD_CHUNKS_X,
                                                  i / VOX_WORLD_CHUNKS_X);
        if (chunk == 0 || chunk->occupied_cells != occupied[i] ||
            chunk->awake_cells != awake[i] || chunk->cell_hash != cell_hash[i] ||
            (((chunk->flags & VOX_CHUNK_ACTIVE) != 0U) != (awake[i] != 0U))) {
            return 7;
        }
        hash = test_hash_mix(hash, chunk->cell_hash);
        hash = test_hash_mix(hash, chunk->occupied_cells);
        hash = test_hash_mix(hash, chunk->awake_cells);
        hash = test_hash_mix(hash, (vox_u32)(chunk->flags & VOX_CHUNK_ACTIVE));
    }
    return hash == vox_world_hash(world) ? 0 : 8;
}

static int run_scenario(vox_u32 *hash_out)
{
    static vox_world world;
    vox_step_command command;
    vox_u32 i;
    vox_world_init(&world);
    command.abi_version = VOX_ABI_VERSION;
    command.struct_size = (vox_u32)sizeof(command);
    command.x = 5U;
    command.y = 7U;
    command.z = 0U;
    command.material = VOX_MAT_STONE;
    command.temperature_delta_q8 = 0;
    for (i = 0U; i < 20U; ++i) {
        if (vox_world_step(&world, &command) != VOX_OK) {
            return 1;
        }
        command.x = (command.x + 3U) % VOX_WORLD_WIDTH;
        command.y = (command.y + 5U) % VOX_WORLD_HEIGHT;
        command.z = (command.z + 1U) % VOX_WORLD_DEPTH;
        command.material = (i % 2U) == 0U ? VOX_MAT_WATER : VOX_MAT_LAVA;
        command.temperature_delta_q8 = (vox_i16)(i * 4U);
    }
    if (test_validate_world(&world) != 0) {
        return 2;
    }
    *hash_out = vox_world_hash(&world);
    return 0;
}

static int test_materials_and_sleep(void)
{
    static vox_world world;
    const vox_material_properties *lava;
    const vox_material_properties *bedrock;
    const vox_cell *water;
    vox_world_init(&world);
    lava = vox_material_get(VOX_MAT_LAVA);
    bedrock = vox_material_get(VOX_MAT_BEDROCK);
    if (lava == 0 || bedrock == 0 ||
        !(lava->flags & VOX_MATERIAL_EMISSIVE) || bedrock->strength == 0U) {
        return 1;
    }
    if (vox_world_set(&world, 1U, 1U, 1U, VOX_MAT_STONE, 20L << 16) != VOX_OK) {
        return 2;
    }
    if (world.occupied_cells != 1U || world.awake_cells != 1U) {
        return 3;
    }
    if (vox_world_step(&world, 0) != VOX_OK ||
        vox_world_cell(&world, 1U, 2U, 1U)->material != VOX_MAT_STONE ||
        !(vox_world_cell(&world, 1U, 2U, 1U)->flags & VOX_CELL_UNSTABLE)) {
        return 4;
    }
    if (vox_world_set(&world, 1U, 3U, 2U, VOX_MAT_BEDROCK, 20L << 16) != VOX_OK ||
        vox_world_set(&world, 2U, 3U, 2U, VOX_MAT_BEDROCK, 20L << 16) != VOX_OK ||
        vox_world_set(&world, 3U, 3U, 2U, VOX_MAT_BEDROCK, 20L << 16) != VOX_OK) {
        return 5;
    }
    if (vox_world_set(&world, 2U, 2U, 2U, VOX_MAT_WATER, 120L << 16) != VOX_OK) {
        return 6;
    }
    if (vox_world_step(&world, 0) != VOX_OK) {
        return 7;
    }
    water = vox_world_cell(&world, 2U, 2U, 2U);
    if (water == 0 || !(water->flags & VOX_CELL_PHASE_GAS)) {
        return 8;
    }
    if (test_validate_world(&world) != 0) {
        return 9;
    }
    return 0;
}

static int test_chunk_metadata(void)
{
    static vox_world world;
    const vox_chunk *first_chunk;
    const vox_chunk *second_chunk;
    vox_u32 hash_before;
    vox_u32 first_generation;
    vox_u32 second_generation;
    vox_world_init(&world);
    if (vox_world_set(&world, VOX_CHUNK_WIDTH - 1U, VOX_CHUNK_HEIGHT - 1U,
                      0U, VOX_MAT_STONE, TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_set(&world, VOX_CHUNK_WIDTH, VOX_CHUNK_HEIGHT,
                      VOX_WORLD_DEPTH - 1U, VOX_MAT_COAL,
                      TEST_AMBIENT_Q16) != VOX_OK) {
        return 1;
    }
    first_chunk = vox_world_chunk(&world, 0U, 0U);
    second_chunk = vox_world_chunk(&world, 1U, 1U);
    if (first_chunk == 0 || second_chunk == 0 ||
        first_chunk->occupied_cells != 1U || first_chunk->awake_cells != 1U ||
        second_chunk->occupied_cells != 1U || second_chunk->awake_cells != 1U ||
        !(first_chunk->flags & VOX_CHUNK_DIRTY) ||
        !(second_chunk->flags & VOX_CHUNK_DIRTY)) {
        return 2;
    }
    if (test_validate_world(&world) != 0) {
        return 3;
    }
    hash_before = vox_world_hash(&world);
    first_generation = first_chunk->generation;
    second_generation = second_chunk->generation;
    if (vox_world_clear_dirty(&world) != VOX_OK ||
        vox_world_hash(&world) != hash_before) {
        return 4;
    }
    first_chunk = vox_world_chunk(&world, 0U, 0U);
    second_chunk = vox_world_chunk(&world, 1U, 1U);
    if (first_chunk == 0 || second_chunk == 0 ||
        (first_chunk->flags & VOX_CHUNK_DIRTY) ||
        (second_chunk->flags & VOX_CHUNK_DIRTY) ||
        first_chunk->generation != first_generation ||
        second_chunk->generation != second_generation) {
        return 5;
    }
    if (vox_world_step(&world, 0) != VOX_OK ||
        vox_world_sleep_all(&world) != VOX_OK ||
        vox_world_sleep_all(&world) != VOX_OK ||
        test_validate_world(&world) != 0) {
        return 6;
    }
    return 0;
}

static int test_blast(void)
{
    static vox_world world;
    const vox_cell *bedrock;
    const vox_cell *smoke;
    vox_u32 before;
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    vox_world_init(&world);
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            if (vox_world_set(&world, x, 20U, z, VOX_MAT_BEDROCK,
                              TEST_AMBIENT_Q16) != VOX_OK) {
                return 1;
            }
        }
        for (y = 15U; y < 20U; ++y) {
            for (x = 7U; x < 14U; ++x) {
                if (vox_world_set(&world, x, y, z, VOX_MAT_STONE,
                                  TEST_AMBIENT_Q16) != VOX_OK) {
                    return 2;
                }
            }
        }
    }
    if (vox_world_sleep_all(&world) != VOX_OK) {
        return 3;
    }
    before = world.occupied_cells;
    if (vox_world_blast(&world, 10U, 17U, 0U, 2U,
                        700L << 16) != VOX_OK ||
        world.occupied_cells >= before) {
        return 4;
    }
    bedrock = vox_world_cell(&world, 10U, 20U, 0U);
    smoke = vox_world_cell(&world, 10U, 17U, 0U);
    if (bedrock == 0 || bedrock->material != VOX_MAT_BEDROCK ||
        smoke == 0 || smoke->material != VOX_MAT_SMOKE ||
        test_validate_world(&world) != 0) {
        return 5;
    }
    if (vox_world_step(&world, 0) != VOX_OK) {
        return 6;
    }
    smoke = vox_world_cell(&world, 10U, 16U, 0U);
    if (smoke == 0 || smoke->material != VOX_MAT_SMOKE ||
        test_validate_world(&world) != 0) {
        return 7;
    }
    return 0;
}

static int test_cellular_motion(void)
{
    static vox_world world;
    const vox_cell *sand;
    const vox_cell *smoke;
    vox_u32 x;
    vox_u32 i;
    vox_world_init(&world);
    for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
        if (vox_world_set(&world, x, VOX_WORLD_HEIGHT - 1U, 0U,
                          VOX_MAT_BEDROCK, 20L << 16) != VOX_OK) {
            return 1;
        }
    }
    if (vox_world_set(&world, VOX_WORLD_WIDTH / 2U, 1U, 0U,
                      VOX_MAT_SAND, 20L << 16) != VOX_OK) {
        return 2;
    }
    for (i = 0U; i < VOX_WORLD_HEIGHT + 2U; ++i) {
        if (vox_world_step(&world, 0) != VOX_OK) {
            return 3;
        }
    }
    sand = vox_world_cell(&world, VOX_WORLD_WIDTH / 2U,
                          VOX_WORLD_HEIGHT - 2U, 0U);
    if (sand == 0 || sand->material != VOX_MAT_SAND) {
        return 4;
    }
    if (vox_world_set(&world, VOX_WORLD_WIDTH / 2U, VOX_WORLD_HEIGHT - 3U,
                      1U, VOX_MAT_SMOKE, 20L << 16) != VOX_OK) {
        return 5;
    }
    for (i = 0U; i < 4U; ++i) {
        if (vox_world_step(&world, 0) != VOX_OK) {
            return 6;
        }
    }
    smoke = vox_world_cell(&world, VOX_WORLD_WIDTH / 2U,
                           VOX_WORLD_HEIGHT - 7U, 1U);
    if (smoke == 0 || smoke->material != VOX_MAT_SMOKE) {
        return 7;
    }
    if (test_validate_world(&world) != 0) {
        return 8;
    }
    return 0;
}

static int test_loose_debris(void)
{
    static vox_world world;
    const vox_cell *flesh;
    const vox_cell *soil;
    const vox_chunk *chunk;
    vox_u32 generation;
    vox_u32 x = VOX_WORLD_WIDTH / 2U;
    vox_u32 i;
    vox_world_init(&world);
    if (vox_world_set(&world, x, 2U, 0U, VOX_MAT_FLESH,
                      TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_set(&world, x + 2U, 2U, 0U, VOX_MAT_SOIL,
                      TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_set_loose(&world, x, 2U, 0U, 1U) != VOX_OK ||
        vox_world_set_loose(&world, x + 2U, 2U, 0U, 1U) != VOX_OK ||
        vox_world_collision_classify(&world, x, 2U) !=
            VOX_WORLD_COLLISION_LOOSE ||
        vox_world_set_loose(&world, x, 3U, 0U, 1U) != VOX_ERR_INVALID ||
        vox_world_set_loose(&world, x, 2U, 0U, 2U) != VOX_ERR_INVALID) {
        return 1;
    }
    if (vox_world_set(&world, x, 2U, 1U, VOX_MAT_METAL,
                      TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_collision_classify(&world, x, 2U) !=
            VOX_WORLD_COLLISION_SOLID ||
        vox_world_set(&world, x, 2U, 1U, VOX_MAT_AIR,
                      TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_collision_classify(&world, x, 2U) !=
            VOX_WORLD_COLLISION_LOOSE) {
        return 2;
    }
    for (i = 0U; i < 4U; ++i) {
        if (vox_world_step(&world, 0) != VOX_OK) {
            return 3;
        }
    }
    flesh = vox_world_cell(&world, x, 6U, 0U);
    soil = vox_world_cell(&world, x + 2U, 6U, 0U);
    if (flesh == 0 || flesh->material != VOX_MAT_FLESH ||
        !(flesh->flags & VOX_CELL_LOOSE) || soil == 0 ||
        soil->material != VOX_MAT_SOIL ||
        !(soil->flags & VOX_CELL_LOOSE) ||
        vox_world_collision_classify(&world, x, 6U) !=
            VOX_WORLD_COLLISION_LOOSE) {
        return 4;
    }
    if (vox_world_set_loose(&world, x, 6U, 0U, 0U) != VOX_OK ||
        vox_world_collision_classify(&world, x, 6U) !=
            VOX_WORLD_COLLISION_SOLID) {
        return 5;
    }
    chunk = vox_world_chunk(&world, x / VOX_CHUNK_WIDTH,
                            6U / VOX_CHUNK_HEIGHT);
    if (chunk == 0) {
        return 6;
    }
    generation = chunk->generation;
    if (vox_world_set_loose(&world, x, 6U, 0U, 0U) != VOX_OK ||
        chunk->generation != generation || test_validate_world(&world) != 0) {
        return 7;
    }
    return 0;
}

static int test_reaction_frontier(void)
{
    static vox_world world;
    const vox_cell *cell;
    vox_u32 x;
    vox_world_init(&world);
    for (x = 0U; x < 32U; ++x) {
        if (vox_world_set(&world, x, 20U, 0U, VOX_MAT_BEDROCK,
                          TEST_AMBIENT_Q16) != VOX_OK) {
            return 1;
        }
    }
    if (vox_world_set(&world, 5U, 19U, 0U, VOX_MAT_BIOMASS,
                      500L << 16) != VOX_OK ||
        vox_world_set(&world, 6U, 19U, 0U, VOX_MAT_STONE,
                      TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_sleep_all(&world) != VOX_OK ||
        vox_world_wake(&world, 6U, 19U, 0U) != VOX_OK ||
        vox_world_step(&world, 0) != VOX_OK) {
        return 2;
    }
    cell = vox_world_cell(&world, 5U, 19U, 0U);
    if (cell == 0 || cell->damage_q16 != 0L) {
        return 3;
    }

    vox_world_init(&world);
    for (x = 0U; x < 32U; ++x) {
        if (vox_world_set(&world, x, 20U, 0U, VOX_MAT_BEDROCK,
                          TEST_AMBIENT_Q16) != VOX_OK) {
            return 4;
        }
    }
    if (vox_world_set(&world, 15U, 19U, 0U, VOX_MAT_LAVA,
                      700L << 16) != VOX_OK ||
        vox_world_set(&world, 16U, 19U, 0U, VOX_MAT_BIOMASS,
                      TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_sleep_all(&world) != VOX_OK ||
        vox_world_wake(&world, 15U, 19U, 0U) != VOX_OK ||
        vox_world_step(&world, 0) != VOX_OK) {
        return 5;
    }
    cell = vox_world_cell(&world, 16U, 19U, 0U);
    if (cell == 0 || cell->damage_q16 != 0L ||
        !(cell->flags & VOX_CELL_AWAKE)) {
        return 6;
    }
    if (vox_world_step(&world, 0) != VOX_OK) {
        return 7;
    }
    cell = vox_world_cell(&world, 16U, 19U, 0U);
    if (cell == 0 || cell->damage_q16 == 0L) {
        return 8;
    }

    vox_world_init(&world);
    for (x = 0U; x < 32U; ++x) {
        if (vox_world_set(&world, x, 20U, 0U, VOX_MAT_BEDROCK,
                          TEST_AMBIENT_Q16) != VOX_OK) {
            return 9;
        }
    }
    if (vox_world_set(&world, 15U, 19U, 0U, VOX_MAT_LAVA,
                      700L << 16) != VOX_OK ||
        vox_world_set(&world, 16U, 19U, 0U, VOX_MAT_STONE,
                      TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_sleep_all(&world) != VOX_OK ||
        vox_world_wake(&world, 15U, 19U, 0U) != VOX_OK ||
        vox_world_step(&world, 0) != VOX_OK) {
        return 10;
    }
    cell = vox_world_cell(&world, 16U, 19U, 0U);
    if (cell == 0 || (cell->flags & VOX_CELL_AWAKE) ||
        test_validate_world(&world) != 0) {
        return 11;
    }
    return 0;
}

static int test_material_reactions(void)
{
    static vox_world world;
    const vox_cell *cell;
    vox_u32 x;
    vox_u32 i;
    vox_world_init(&world);
    for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
        if (vox_world_set(&world, x, 40U, 0U, VOX_MAT_BEDROCK,
                          TEST_AMBIENT_Q16) != VOX_OK) {
            return 1;
        }
    }
    if (vox_world_set(&world, 20U, 39U, 0U, VOX_MAT_LAVA,
                      700L << 16) != VOX_OK ||
        vox_world_set(&world, 19U, 39U, 0U, VOX_MAT_WATER,
                      TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_step(&world, 0) != VOX_OK) {
        return 2;
    }
    cell = vox_world_cell(&world, 20U, 39U, 0U);
    if (cell == 0 || cell->material != VOX_MAT_STONE) {
        return 3;
    }
    cell = vox_world_cell(&world, 19U, 38U, 0U);
    if (cell == 0 || cell->material != VOX_MAT_SMOKE) {
        return 4;
    }
    if (vox_world_set(&world, 50U, 39U, 0U, VOX_MAT_BIOMASS,
                      TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_set(&world, 51U, 39U, 0U, VOX_MAT_LAVA,
                      700L << 16) != VOX_OK) {
        return 5;
    }
    for (i = 0U; i < 3U; ++i) {
        if (vox_world_step(&world, 0) != VOX_OK) {
            return 6;
        }
    }
    cell = vox_world_cell(&world, 50U, 39U, 0U);
    if (cell == 0 || cell->temperature_q16 < (300L << 16) ||
        cell->damage_q16 == 0L) {
        return 7;
    }
    if (vox_world_set(&world, 80U, 39U, 0U, VOX_MAT_FIREDAMP,
                      300L << 16) != VOX_OK ||
        vox_world_set(&world, 81U, 39U, 0U, VOX_MAT_COAL,
                      TEST_AMBIENT_Q16) != VOX_OK ||
        vox_world_step(&world, 0) != VOX_OK) {
        return 8;
    }
    cell = vox_world_cell(&world, 81U, 39U, 0U);
    if (cell == 0 || cell->material != VOX_MAT_AIR ||
        test_validate_world(&world) != 0) {
        return 9;
    }
    return 0;
}

int main(void)
{
    vox_u32 first;
    vox_u32 second;
    int result;
    if (test_materials_and_sleep() != 0) {
        fprintf(stderr, "material/sleep scenario failed\n");
        return 3;
    }
    if (test_cellular_motion() != 0) {
        fprintf(stderr, "cellular motion scenario failed\n");
        return 4;
    }
    result = test_loose_debris();
    if (result != 0) {
        fprintf(stderr, "loose debris scenario failed: %d\n", result);
        return 8;
    }
    result = test_reaction_frontier();
    if (result != 0) {
        fprintf(stderr, "reaction frontier scenario failed: %d\n", result);
        return 9;
    }
    if (test_material_reactions() != 0) {
        fprintf(stderr, "material reaction scenario failed\n");
        return 7;
    }
    if (test_chunk_metadata() != 0) {
        fprintf(stderr, "chunk metadata scenario failed\n");
        return 5;
    }
    if (test_blast() != 0) {
        fprintf(stderr, "blast scenario failed\n");
        return 6;
    }
    if (run_scenario(&first) != 0 || run_scenario(&second) != 0) {
        return 1;
    }
    if (first != second) {
        fprintf(stderr, "determinism failure: %08x != %08x\n",
                (unsigned int)first, (unsigned int)second);
        return 2;
    }
    printf("deterministic hash=%08x\n", (unsigned int)first);
    return 0;
}

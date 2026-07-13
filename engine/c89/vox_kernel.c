/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_kernel.h"

#define VOX_AMBIENT_Q16 (20L << 16)
#define VOX_STEAM_Q16 (140L << 16)
#define VOX_REACTION_HOT_Q16 (320L << 16)

static const vox_material_properties vox_materials[VOX_MAT_COUNT] = {
    {0U, 0U, 0U, 0U, 0L, 0L},
    {65535U, 65535U, 65535U, VOX_MATERIAL_SOLID, 0L, 0L},
    {2400U, 500U, 400U, VOX_MATERIAL_SOLID, 0L, 1000L << 16},
    {1400U, 80U, 120U, VOX_MATERIAL_SOLID, 0L, 500L << 16},
    {1300U, 120U, 180U, VOX_MATERIAL_FLAMMABLE | VOX_MATERIAL_SOLID,
     450L << 16, 600L << 16},
    {900U, 60U, 100U, VOX_MATERIAL_FLAMMABLE | VOX_MATERIAL_SOLID,
     300L << 16, 450L << 16},
    {1600U, 20U, 80U, VOX_MATERIAL_FLUID | VOX_MATERIAL_SOLID,
     0L, 1000L << 16},
    {1000U, 0U, 600U, VOX_MATERIAL_FLUID, 0L, 100L << 16},
    {3000U, 0U, 900U, VOX_MATERIAL_FLUID | VOX_MATERIAL_EMISSIVE,
     0L, 650L << 16},
    {7800U, 500U, 800U, VOX_MATERIAL_SOLID, 0L, 1500L << 16},
    {1050U, 40U, 80U, VOX_MATERIAL_FLAMMABLE | VOX_MATERIAL_SOLID,
     400L << 16, 500L << 16},
    {1050U, 10U, 120U, VOX_MATERIAL_FLUID, 0L, 100L << 16},
    {10U, 0U, 20U, VOX_MATERIAL_GAS, 0L, 0L},
    {2U, 0U, 10U, VOX_MATERIAL_GAS | VOX_MATERIAL_FLAMMABLE,
     250L << 16, 0L}
};

static vox_u32 vox_index(vox_u32 x, vox_u32 y, vox_u32 z)
{
    return (z * VOX_WORLD_HEIGHT * VOX_WORLD_WIDTH) +
           (y * VOX_WORLD_WIDTH) + x;
}

static vox_u32 vox_hash_mix(vox_u32 hash, vox_u32 value)
{
    hash ^= value;
    hash *= 16777619U;
    return hash;
}

static vox_u32 vox_cell_signature(vox_u32 index, const vox_cell *cell)
{
    vox_u32 hash;
    if (cell->material == VOX_MAT_AIR && cell->flags == 0U &&
        cell->temperature_q16 == VOX_AMBIENT_Q16 && cell->damage_q16 == 0L) {
        return 0U;
    }
    hash = 2166136261U;
    hash = vox_hash_mix(hash, index);
    hash = vox_hash_mix(hash, (vox_u32)cell->material);
    hash = vox_hash_mix(hash, (vox_u32)cell->flags);
    hash = vox_hash_mix(hash, (vox_u32)cell->temperature_q16);
    hash = vox_hash_mix(hash, (vox_u32)cell->damage_q16);
    return hash;
}

static void vox_toggle_cell_signature(vox_chunk *chunk, vox_u32 index,
                                      const vox_cell *cell)
{
    chunk->cell_hash ^= vox_cell_signature(index, cell);
}

static int vox_in_bounds(vox_u32 x, vox_u32 y, vox_u32 z)
{
    return x < VOX_WORLD_WIDTH && y < VOX_WORLD_HEIGHT && z < VOX_WORLD_DEPTH;
}

static vox_u32 vox_chunk_index(vox_u32 x, vox_u32 y)
{
    return (y / VOX_CHUNK_HEIGHT) * VOX_WORLD_CHUNKS_X +
           (x / VOX_CHUNK_WIDTH);
}

static void vox_mark_dirty(vox_chunk *chunk)
{
    chunk->flags = (vox_u16)(chunk->flags | VOX_CHUNK_DIRTY);
    chunk->generation++;
}

static void vox_update_active(vox_world *world, vox_chunk *chunk,
                              vox_u32 cell_index, vox_cell *cell)
{
    int occupied = cell->material != VOX_MAT_AIR;
    vox_toggle_cell_signature(chunk, cell_index, cell);
    if (occupied && !(cell->flags & VOX_CELL_OCCUPIED)) {
        world->occupied_cells++;
        chunk->occupied_cells++;
    } else if (!occupied && (cell->flags & VOX_CELL_OCCUPIED)) {
        world->occupied_cells--;
        chunk->occupied_cells--;
    }
    if (occupied) {
        cell->flags = (vox_u16)(cell->flags | VOX_CELL_OCCUPIED);
    } else {
        cell->flags = (vox_u16)(cell->flags & (vox_u16)~VOX_CELL_OCCUPIED);
    }
    vox_toggle_cell_signature(chunk, cell_index, cell);
}

static void vox_wake_cell(vox_world *world, vox_chunk *chunk,
                          vox_u32 cell_index, vox_cell *cell)
{
    if (!(cell->flags & VOX_CELL_AWAKE)) {
        vox_toggle_cell_signature(chunk, cell_index, cell);
        cell->flags = (vox_u16)(cell->flags | VOX_CELL_AWAKE);
        world->awake_cells++;
        chunk->awake_cells++;
        chunk->flags = (vox_u16)(chunk->flags | VOX_CHUNK_ACTIVE);
        vox_toggle_cell_signature(chunk, cell_index, cell);
    }
}

static void vox_sleep_cell(vox_world *world, vox_chunk *chunk,
                           vox_u32 cell_index, vox_cell *cell)
{
    if (cell->flags & VOX_CELL_AWAKE) {
        vox_toggle_cell_signature(chunk, cell_index, cell);
        cell->flags = (vox_u16)(cell->flags & (vox_u16)~VOX_CELL_AWAKE);
        world->awake_cells--;
        chunk->awake_cells--;
        if (chunk->awake_cells == 0U) {
            chunk->flags = (vox_u16)(chunk->flags & (vox_u16)~VOX_CHUNK_ACTIVE);
        }
        vox_toggle_cell_signature(chunk, cell_index, cell);
    }
}

static void vox_set_cell_state(vox_world *world, vox_u32 x, vox_u32 y,
                               vox_u32 z, vox_i32 temperature_q16,
                               vox_i32 damage_q16)
{
    vox_u32 cell_index = vox_index(x, y, z);
    vox_cell *cell = &world->cells[cell_index];
    vox_chunk *chunk = &world->chunks[vox_chunk_index(x, y)];
    vox_toggle_cell_signature(chunk, cell_index, cell);
    cell->temperature_q16 = temperature_q16;
    cell->damage_q16 = damage_q16;
    vox_toggle_cell_signature(chunk, cell_index, cell);
    vox_wake_cell(world, chunk, cell_index, cell);
    vox_mark_dirty(chunk);
}

static int vox_neighbor_is_hot(const vox_world *world, vox_u32 x,
                               vox_u32 y, vox_u32 z)
{
    static const int delta_x[4] = {-1, 1, 0, 0};
    static const int delta_y[4] = {0, 0, -1, 1};
    vox_u32 neighbor;
    for (neighbor = 0U; neighbor < 4U; ++neighbor) {
        long sample_x = (long)x + (long)delta_x[neighbor];
        long sample_y = (long)y + (long)delta_y[neighbor];
        const vox_cell *cell;
        if (sample_x < 0L || sample_y < 0L ||
            sample_x >= (long)VOX_WORLD_WIDTH ||
            sample_y >= (long)VOX_WORLD_HEIGHT) {
            continue;
        }
        cell = &world->cells[vox_index((vox_u32)sample_x,
                                       (vox_u32)sample_y, z)];
        if (cell->material == VOX_MAT_LAVA ||
            cell->temperature_q16 >= VOX_REACTION_HOT_Q16) {
            return 1;
        }
    }
    return 0;
}

static int vox_react_water_lava(vox_world *world, vox_u32 x, vox_u32 y,
                                vox_u32 z)
{
    static const int delta_x[4] = {-1, 1, 0, 0};
    static const int delta_y[4] = {0, 0, -1, 1};
    vox_u32 neighbor;
    for (neighbor = 0U; neighbor < 4U; ++neighbor) {
        long sample_x = (long)x + (long)delta_x[neighbor];
        long sample_y = (long)y + (long)delta_y[neighbor];
        vox_cell *cell;
        if (sample_x < 0L || sample_y < 0L ||
            sample_x >= (long)VOX_WORLD_WIDTH ||
            sample_y >= (long)VOX_WORLD_HEIGHT) {
            continue;
        }
        cell = &world->cells[vox_index((vox_u32)sample_x,
                                       (vox_u32)sample_y, z)];
        if (cell->material == VOX_MAT_LAVA) {
            (void)vox_world_set(world, (vox_u32)sample_x,
                                (vox_u32)sample_y, z, VOX_MAT_STONE,
                                300L << 16);
            (void)vox_world_set(world, x, y, z, VOX_MAT_SMOKE,
                                VOX_STEAM_Q16);
            return 1;
        }
    }
    return 0;
}

static void vox_wake_reaction_neighbors(vox_world *world, vox_u32 x,
                                        vox_u32 y, vox_u32 z)
{
    static const int delta_x[4] = {-1, 1, 0, 0};
    static const int delta_y[4] = {0, 0, -1, 1};
    vox_u32 neighbor;
    for (neighbor = 0U; neighbor < 4U; ++neighbor) {
        long sample_x = (long)x + (long)delta_x[neighbor];
        long sample_y = (long)y + (long)delta_y[neighbor];
        if (sample_x >= 0L && sample_y >= 0L &&
            sample_x < (long)VOX_WORLD_WIDTH &&
            sample_y < (long)VOX_WORLD_HEIGHT) {
            (void)vox_world_wake(world, (vox_u32)sample_x,
                                 (vox_u32)sample_y, z);
        }
    }
}

static void vox_step_reactions(vox_world *world)
{
    vox_u32 chunk_y;
    vox_u32 chunk_x;
    vox_u32 depth;
    vox_u32 local_y;
    vox_u32 local_x;
    int firedamp_ignited = 0;
    vox_u32 blast_x = 0U;
    vox_u32 blast_y = 0U;
    vox_u32 blast_z = 0U;
    for (chunk_y = 0U; chunk_y < VOX_WORLD_CHUNKS_Y; ++chunk_y) {
        for (chunk_x = 0U; chunk_x < VOX_WORLD_CHUNKS_X; ++chunk_x) {
            vox_chunk *chunk = &world->chunks[chunk_y * VOX_WORLD_CHUNKS_X +
                                               chunk_x];
            if (!(chunk->flags & VOX_CHUNK_ACTIVE)) {
                continue;
            }
            for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
                for (local_y = 0U; local_y < VOX_CHUNK_HEIGHT; ++local_y) {
                    vox_u32 y = chunk_y * VOX_CHUNK_HEIGHT + local_y;
                    for (local_x = 0U; local_x < VOX_CHUNK_WIDTH; ++local_x) {
                        vox_u32 x = chunk_x * VOX_CHUNK_WIDTH + local_x;
                        vox_cell *cell = &world->cells[vox_index(x, y, depth)];
                        const vox_material_properties *properties;
                        int hot;
                        if (cell->material == VOX_MAT_AIR) {
                            continue;
                        }
                        if (cell->material == VOX_MAT_WATER &&
                            vox_react_water_lava(world, x, y, depth)) {
                            continue;
                        }
                        properties = vox_material_get(cell->material);
                        if (properties == 0 ||
                            !(properties->flags & VOX_MATERIAL_FLAMMABLE)) {
                            if (cell->material == VOX_MAT_LAVA ||
                                cell->temperature_q16 >= VOX_REACTION_HOT_Q16) {
                                vox_wake_reaction_neighbors(world, x, y, depth);
                            }
                            continue;
                        }
                        hot = cell->temperature_q16 >= properties->ignition_q16 ||
                              vox_neighbor_is_hot(world, x, y, depth);
                        if (!hot) {
                            continue;
                        }
                        if (cell->material == VOX_MAT_FIREDAMP) {
                            if (!firedamp_ignited) {
                                firedamp_ignited = 1;
                                blast_x = x;
                                blast_y = y;
                                blast_z = depth;
                            }
                            continue;
                        }
                        if (cell->damage_q16 >=
                            ((vox_i32)properties->strength << 16)) {
                            (void)vox_world_set(world, x, y, depth,
                                                VOX_MAT_SMOKE,
                                                VOX_STEAM_Q16);
                            continue;
                        }
                        vox_set_cell_state(world, x, y, depth,
                                           VOX_REACTION_HOT_Q16,
                                           cell->damage_q16 + (1L << 16));
                        if (y > 0U &&
                            world->cells[vox_index(x, y - 1U, depth)].material ==
                                VOX_MAT_AIR) {
                            (void)vox_world_set(world, x, y - 1U, depth,
                                                VOX_MAT_SMOKE,
                                                VOX_STEAM_Q16);
                        }
                        vox_wake_reaction_neighbors(world, x, y, depth);
                    }
                }
            }
        }
    }
    if (firedamp_ignited) {
        (void)vox_world_blast(world, blast_x, blast_y, blast_z, 3U,
                              700L << 16);
    }
}

static void vox_clear_cell(vox_world *world, vox_chunk *chunk,
                           vox_u32 cell_index, vox_cell *cell)
{
    vox_sleep_cell(world, chunk, cell_index, cell);
    vox_toggle_cell_signature(chunk, cell_index, cell);
    if (cell->flags & VOX_CELL_OCCUPIED) {
        world->occupied_cells--;
        chunk->occupied_cells--;
    }
    cell->material = VOX_MAT_AIR;
    cell->flags = 0U;
    cell->temperature_q16 = VOX_AMBIENT_Q16;
    cell->damage_q16 = 0L;
    vox_toggle_cell_signature(chunk, cell_index, cell);
    vox_mark_dirty(chunk);
}

static int vox_is_falling_material(vox_u16 material)
{
    return material == VOX_MAT_SAND || material == VOX_MAT_WATER ||
           material == VOX_MAT_LAVA || material == VOX_MAT_BLOOD;
}

static int vox_is_structural_material(vox_u16 material)
{
    return material == VOX_MAT_SOIL || material == VOX_MAT_STONE ||
           material == VOX_MAT_COAL || material == VOX_MAT_BIOMASS ||
           material == VOX_MAT_METAL;
}

static int vox_cell_has_support(const vox_world *world, vox_u32 x,
                                vox_u32 y, vox_u32 z)
{
    const vox_cell *below;
    if (y + 1U >= VOX_WORLD_HEIGHT) {
        return 1;
    }
    below = &world->cells[vox_index(x, y + 1U, z)];
    if (below->material != VOX_MAT_AIR &&
        !(below->flags & VOX_CELL_PHASE_GAS)) {
        return 1;
    }
    if (x > 0U) {
        below = &world->cells[vox_index(x - 1U, y + 1U, z)];
        if (below->material != VOX_MAT_AIR &&
            !(below->flags & VOX_CELL_PHASE_GAS)) {
            return 1;
        }
    }
    if (x + 1U < VOX_WORLD_WIDTH) {
        below = &world->cells[vox_index(x + 1U, y + 1U, z)];
        if (below->material != VOX_MAT_AIR &&
            !(below->flags & VOX_CELL_PHASE_GAS)) {
            return 1;
        }
    }
    return 0;
}

static void vox_step_structures(vox_world *world)
{
    vox_u32 z;
    vox_u32 y;
    vox_u32 x;
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (y = 0U; y < VOX_WORLD_HEIGHT; ++y) {
            for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
                vox_u32 index = vox_index(x, y, z);
                vox_cell *cell = &world->cells[index];
                vox_chunk *chunk;
                int supported;
                if (!(cell->flags & VOX_CELL_AWAKE) ||
                    !vox_is_structural_material(cell->material)) {
                    continue;
                }
                supported = vox_cell_has_support(world, x, y, z);
                if (!supported && !(cell->flags & VOX_CELL_UNSTABLE)) {
                    chunk = &world->chunks[vox_chunk_index(x, y)];
                    vox_toggle_cell_signature(chunk, index, cell);
                    cell->flags = (vox_u16)(cell->flags | VOX_CELL_UNSTABLE);
                    vox_toggle_cell_signature(chunk, index, cell);
                    vox_mark_dirty(chunk);
                } else if (supported && (cell->flags & VOX_CELL_UNSTABLE)) {
                    chunk = &world->chunks[vox_chunk_index(x, y)];
                    vox_toggle_cell_signature(chunk, index, cell);
                    cell->flags = (vox_u16)(cell->flags &
                                             (vox_u16)~VOX_CELL_UNSTABLE);
                    vox_toggle_cell_signature(chunk, index, cell);
                    vox_mark_dirty(chunk);
                }
            }
        }
    }
}

static int vox_is_gas_cell(const vox_cell *cell)
{
    return cell->material == VOX_MAT_SMOKE || cell->material == VOX_MAT_FIREDAMP ||
           (cell->flags & VOX_CELL_PHASE_GAS);
}

static int vox_try_move(vox_world *world, vox_u32 source_x, vox_u32 source_y,
                        vox_u32 source_z, int delta_x, int delta_y)
{
    long destination_x = (long)source_x + (long)delta_x;
    long destination_y = (long)source_y + (long)delta_y;
    vox_cell *source;
    vox_cell *destination;
    vox_cell original;
    vox_cell *above;
    vox_chunk *source_chunk;
    vox_chunk *destination_chunk;
    vox_chunk *above_chunk;
    if (destination_x < 0L || destination_y < 0L ||
        destination_x >= (long)VOX_WORLD_WIDTH ||
        destination_y >= (long)VOX_WORLD_HEIGHT) {
        return 0;
    }
    source = &world->cells[vox_index(source_x, source_y, source_z)];
    destination = &world->cells[vox_index((vox_u32)destination_x,
                                          (vox_u32)destination_y, source_z)];
    source_chunk = &world->chunks[vox_chunk_index(source_x, source_y)];
    destination_chunk = &world->chunks[vox_chunk_index((vox_u32)destination_x,
                                                        (vox_u32)destination_y)];
    if (source->material == VOX_MAT_AIR || destination->material != VOX_MAT_AIR) {
        return 0;
    }
    original = *source;
    vox_clear_cell(world, source_chunk,
                   vox_index(source_x, source_y, source_z), source);
    vox_toggle_cell_signature(destination_chunk,
                              vox_index((vox_u32)destination_x,
                                        (vox_u32)destination_y, source_z),
                              destination);
    destination->material = original.material;
    destination->flags = (vox_u16)(original.flags &
                                    (VOX_CELL_PHASE_GAS | VOX_CELL_UNSTABLE));
    destination->temperature_q16 = original.temperature_q16;
    destination->damage_q16 = original.damage_q16;
    vox_toggle_cell_signature(destination_chunk,
                              vox_index((vox_u32)destination_x,
                                        (vox_u32)destination_y, source_z),
                              destination);
    vox_update_active(world, destination_chunk,
                      vox_index((vox_u32)destination_x,
                                (vox_u32)destination_y, source_z), destination);
    vox_wake_cell(world, destination_chunk,
                  vox_index((vox_u32)destination_x, (vox_u32)destination_y,
                            source_z), destination);
    vox_toggle_cell_signature(destination_chunk,
                              vox_index((vox_u32)destination_x,
                                        (vox_u32)destination_y, source_z),
                              destination);
    destination->flags = (vox_u16)(destination->flags | VOX_CELL_MOVED);
    vox_toggle_cell_signature(destination_chunk,
                              vox_index((vox_u32)destination_x,
                                        (vox_u32)destination_y, source_z),
                              destination);
    vox_mark_dirty(destination_chunk);
    if (source_y > 0U) {
        above = &world->cells[vox_index(source_x, source_y - 1U, source_z)];
        above_chunk = &world->chunks[vox_chunk_index(source_x, source_y - 1U)];
        if (above->material != VOX_MAT_AIR) {
            vox_wake_cell(world, above_chunk,
                          vox_index(source_x, source_y - 1U, source_z), above);
        }
    }
    return 1;
}

static void vox_step_falling(vox_world *world)
{
    vox_u32 depth;
    vox_u32 chunk_y_loop;
    vox_u32 chunk_x;
    vox_u32 local_y_loop;
    vox_u32 local_x;
    for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
        for (chunk_y_loop = VOX_WORLD_CHUNKS_Y; chunk_y_loop > 0U;
             --chunk_y_loop) {
            vox_u32 chunk_y = chunk_y_loop - 1U;
            for (local_y_loop = VOX_CHUNK_HEIGHT; local_y_loop > 0U;
                 --local_y_loop) {
                vox_u32 source_y = chunk_y * VOX_CHUNK_HEIGHT +
                                   local_y_loop - 1U;
                for (chunk_x = 0U; chunk_x < VOX_WORLD_CHUNKS_X; ++chunk_x) {
                    vox_chunk *chunk = &world->chunks[chunk_y * VOX_WORLD_CHUNKS_X +
                                                       chunk_x];
                    if (!(chunk->flags & VOX_CHUNK_ACTIVE)) {
                        continue;
                    }
                    for (local_x = 0U; local_x < VOX_CHUNK_WIDTH; ++local_x) {
                        vox_u32 x = chunk_x * VOX_CHUNK_WIDTH + local_x;
                        vox_cell *cell = &world->cells[vox_index(x, source_y, depth)];
                        int moved = 0;
                        int prefer_left;
                        if (!(cell->flags & VOX_CELL_AWAKE) ||
                            (cell->flags & VOX_CELL_PHASE_GAS) ||
                            (!vox_is_falling_material(cell->material) &&
                             !(cell->flags & VOX_CELL_UNSTABLE))) {
                            continue;
                        }
                        moved = vox_try_move(world, x, source_y, depth, 0, 1);
                        if (!moved) {
                            prefer_left = (int)((world->tick + x + source_y + depth) & 1U);
                            if (prefer_left) {
                                moved = vox_try_move(world, x, source_y, depth, -1, 1);
                                if (!moved) {
                                    (void)vox_try_move(world, x, source_y, depth, 1, 1);
                                }
                            } else {
                                moved = vox_try_move(world, x, source_y, depth, 1, 1);
                                if (!moved) {
                                    (void)vox_try_move(world, x, source_y, depth, -1, 1);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static void vox_step_gases(vox_world *world)
{
    vox_u32 depth;
    vox_u32 chunk_y;
    vox_u32 chunk_x;
    vox_u32 local_y;
    vox_u32 local_x;
    for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
        for (chunk_y = 0U; chunk_y < VOX_WORLD_CHUNKS_Y; ++chunk_y) {
            for (local_y = 0U; local_y < VOX_CHUNK_HEIGHT; ++local_y) {
                vox_u32 source_y = chunk_y * VOX_CHUNK_HEIGHT + local_y;
                if (source_y == 0U) {
                    continue;
                }
                for (chunk_x = 0U; chunk_x < VOX_WORLD_CHUNKS_X; ++chunk_x) {
                    vox_chunk *chunk = &world->chunks[chunk_y * VOX_WORLD_CHUNKS_X +
                                                       chunk_x];
                    if (!(chunk->flags & VOX_CHUNK_ACTIVE)) {
                        continue;
                    }
                    for (local_x = 0U; local_x < VOX_CHUNK_WIDTH; ++local_x) {
                        vox_u32 x = chunk_x * VOX_CHUNK_WIDTH + local_x;
                        vox_cell *cell = &world->cells[vox_index(x, source_y, depth)];
                        int moved = 0;
                        int prefer_left;
                        if (!(cell->flags & VOX_CELL_AWAKE) || !vox_is_gas_cell(cell)) {
                            continue;
                        }
                        moved = vox_try_move(world, x, source_y, depth, 0, -1);
                        if (!moved) {
                            prefer_left = (int)((world->tick + x + source_y + depth) & 1U);
                            if (prefer_left) {
                                moved = vox_try_move(world, x, source_y, depth, -1, -1);
                                if (!moved) {
                                    (void)vox_try_move(world, x, source_y, depth, 1, -1);
                                }
                            } else {
                                moved = vox_try_move(world, x, source_y, depth, 1, -1);
                                if (!moved) {
                                    (void)vox_try_move(world, x, source_y, depth, -1, -1);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static void vox_step_materials(vox_world *world)
{
    vox_u32 chunk_y;
    vox_u32 chunk_x;
    vox_u32 depth;
    vox_u32 local_y;
    vox_u32 local_x;
    for (chunk_y = 0U; chunk_y < VOX_WORLD_CHUNKS_Y; ++chunk_y) {
        for (chunk_x = 0U; chunk_x < VOX_WORLD_CHUNKS_X; ++chunk_x) {
            vox_chunk *chunk = &world->chunks[chunk_y * VOX_WORLD_CHUNKS_X +
                                               chunk_x];
            if (!(chunk->flags & VOX_CHUNK_ACTIVE)) {
                continue;
            }
            for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
                for (local_y = 0U; local_y < VOX_CHUNK_HEIGHT; ++local_y) {
                    vox_u32 y = chunk_y * VOX_CHUNK_HEIGHT + local_y;
                    for (local_x = 0U; local_x < VOX_CHUNK_WIDTH; ++local_x) {
                        vox_u32 x = chunk_x * VOX_CHUNK_WIDTH + local_x;
                        vox_cell *cell = &world->cells[vox_index(x, y, depth)];
                        if (!(cell->flags & VOX_CELL_AWAKE)) {
                            continue;
                        }
                        if (cell->flags & VOX_CELL_MOVED) {
                            vox_toggle_cell_signature(chunk, vox_index(x, y, depth),
                                                      cell);
                            cell->flags = (vox_u16)(cell->flags &
                                                    (vox_u16)~VOX_CELL_MOVED);
                            vox_toggle_cell_signature(chunk, vox_index(x, y, depth),
                                                      cell);
                        } else if (cell->material == VOX_MAT_WATER &&
                                   cell->temperature_q16 > (100L << 16)) {
                            vox_toggle_cell_signature(chunk, vox_index(x, y, depth),
                                                      cell);
                            cell->flags = (vox_u16)(cell->flags |
                                                    VOX_CELL_PHASE_GAS);
                            cell->temperature_q16 = 80L << 16;
                            vox_toggle_cell_signature(chunk, vox_index(x, y, depth),
                                                      cell);
                            vox_mark_dirty(chunk);
                        } else if (cell->material == VOX_MAT_LAVA &&
                                   cell->temperature_q16 < (650L << 16)) {
                            vox_toggle_cell_signature(chunk, vox_index(x, y, depth),
                                                      cell);
                            cell->temperature_q16 += 1L << 12;
                            vox_toggle_cell_signature(chunk, vox_index(x, y, depth),
                                                      cell);
                            vox_mark_dirty(chunk);
                        } else {
                            vox_sleep_cell(world, chunk, vox_index(x, y, depth),
                                           cell);
                        }
                    }
                }
            }
        }
    }
}

void vox_world_init(vox_world *world)
{
    vox_u32 i;
    world->abi_version = VOX_ABI_VERSION;
    world->struct_size = (vox_u32)sizeof(*world);
    world->tick = 0U;
    world->occupied_cells = 0U;
    world->awake_cells = 0U;
    for (i = 0U; i < VOX_WORLD_CHUNK_COUNT; ++i) {
        world->chunks[i].occupied_cells = 0U;
        world->chunks[i].awake_cells = 0U;
        world->chunks[i].generation = 0U;
        world->chunks[i].cell_hash = 0U;
        world->chunks[i].flags = 0U;
        world->chunks[i].reserved = 0U;
    }
    for (i = 0U; i < VOX_WORLD_CELLS; ++i) {
        world->cells[i].material = VOX_MAT_AIR;
        world->cells[i].flags = 0U;
        world->cells[i].temperature_q16 = VOX_AMBIENT_Q16;
        world->cells[i].damage_q16 = 0L;
    }
}

const vox_material_properties *vox_material_get(vox_u16 material)
{
    if (material >= VOX_MAT_COUNT) {
        return 0;
    }
    return &vox_materials[material];
}

vox_result vox_world_set(vox_world *world, vox_u32 x, vox_u32 y, vox_u32 z,
                         vox_u16 material, vox_i32 temperature_q16)
{
    vox_cell *cell;
    vox_chunk *chunk;
    if (world == 0 || !vox_in_bounds(x, y, z)) {
        return VOX_ERR_INVALID;
    }
    if (material >= VOX_MAT_COUNT) {
        return VOX_ERR_INVALID;
    }
    cell = &world->cells[vox_index(x, y, z)];
    chunk = &world->chunks[vox_chunk_index(x, y)];
    if (material == VOX_MAT_AIR) {
        vox_clear_cell(world, chunk, vox_index(x, y, z), cell);
        return VOX_OK;
    }
    vox_toggle_cell_signature(chunk, vox_index(x, y, z), cell);
    cell->material = material;
    cell->flags = (vox_u16)(cell->flags &
                            (vox_u16)~(VOX_CELL_PHASE_GAS | VOX_CELL_MOVED));
    cell->temperature_q16 = temperature_q16;
    vox_toggle_cell_signature(chunk, vox_index(x, y, z), cell);
    vox_update_active(world, chunk, vox_index(x, y, z), cell);
    vox_wake_cell(world, chunk, vox_index(x, y, z), cell);
    vox_mark_dirty(chunk);
    return VOX_OK;
}

vox_result vox_world_wake(vox_world *world, vox_u32 x, vox_u32 y, vox_u32 z)
{
    vox_cell *cell;
    vox_chunk *chunk;
    if (world == 0 || !vox_in_bounds(x, y, z)) {
        return VOX_ERR_INVALID;
    }
    cell = &world->cells[vox_index(x, y, z)];
    if (cell->material == VOX_MAT_AIR) {
        return VOX_OK;
    }
    chunk = &world->chunks[vox_chunk_index(x, y)];
    vox_wake_cell(world, chunk, vox_index(x, y, z), cell);
    return VOX_OK;
}

vox_result vox_world_sleep_all(vox_world *world)
{
    vox_u32 i;
    if (world == 0) {
        return VOX_ERR_INVALID;
    }
    for (i = 0U; i < VOX_WORLD_CELLS; ++i) {
        vox_u32 x = i % VOX_WORLD_WIDTH;
        vox_u32 y = (i / VOX_WORLD_WIDTH) % VOX_WORLD_HEIGHT;
        vox_chunk *chunk = &world->chunks[vox_chunk_index(x, y)];
        vox_toggle_cell_signature(chunk, i, &world->cells[i]);
        world->cells[i].flags = (vox_u16)(world->cells[i].flags &
                                          (vox_u16)~(VOX_CELL_AWAKE |
                                                    VOX_CELL_MOVED));
        vox_toggle_cell_signature(chunk, i, &world->cells[i]);
    }
    world->awake_cells = 0U;
    for (i = 0U; i < VOX_WORLD_CHUNK_COUNT; ++i) {
        world->chunks[i].awake_cells = 0U;
        world->chunks[i].flags = (vox_u16)(world->chunks[i].flags &
                                           (vox_u16)~VOX_CHUNK_ACTIVE);
    }
    return VOX_OK;
}

vox_result vox_world_clear_dirty(vox_world *world)
{
    vox_u32 i;
    if (world == 0) {
        return VOX_ERR_INVALID;
    }
    for (i = 0U; i < VOX_WORLD_CHUNK_COUNT; ++i) {
        world->chunks[i].flags = (vox_u16)(world->chunks[i].flags &
                                           (vox_u16)~VOX_CHUNK_DIRTY);
    }
    return VOX_OK;
}

vox_result vox_world_blast(vox_world *world, vox_u32 x, vox_u32 y,
                           vox_u32 z, vox_u32 radius, vox_i32 heat_q16)
{
    long min_x;
    long max_x;
    long min_y;
    long max_y;
    long sample_x;
    long sample_y;
    long core_squared;
    long fracture_radius;
    long fracture_squared;
    vox_u32 depth;
    if (world == 0 || !vox_in_bounds(x, y, z) ||
        radius == 0U || radius > VOX_BLAST_MAX_RADIUS) {
        return VOX_ERR_INVALID;
    }
    fracture_radius = (long)radius + 2L;
    min_x = (long)x - fracture_radius;
    max_x = (long)x + fracture_radius;
    min_y = (long)y - fracture_radius;
    max_y = (long)y + fracture_radius;
    if (min_x < 0L) {
        min_x = 0L;
    }
    if (min_y < 0L) {
        min_y = 0L;
    }
    if (max_x >= (long)VOX_WORLD_WIDTH) {
        max_x = (long)VOX_WORLD_WIDTH - 1L;
    }
    if (max_y >= (long)VOX_WORLD_HEIGHT) {
        max_y = (long)VOX_WORLD_HEIGHT - 1L;
    }
    core_squared = (long)radius * (long)radius;
    fracture_squared = fracture_radius * fracture_radius;
    for (sample_y = min_y; sample_y <= max_y; ++sample_y) {
        for (sample_x = min_x; sample_x <= max_x; ++sample_x) {
            long delta_x = sample_x - (long)x;
            long delta_y = sample_y - (long)y;
            long distance_squared = delta_x * delta_x + delta_y * delta_y;
            vox_u32 fracture = ((vox_u32)sample_x * 1103515245U) ^
                               ((vox_u32)sample_y * 2654435761U) ^
                               (world->tick * 2246822519U) ^
                               (x * 3266489917U) ^ (y * 668265263U);
            if (distance_squared > fracture_squared ||
                (distance_squared > core_squared && (fracture & 3U) == 0U)) {
                continue;
            }
            for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
                vox_cell *cell = &world->cells[vox_index((vox_u32)sample_x,
                                                          (vox_u32)sample_y,
                                                          depth)];
                vox_chunk *chunk = &world->chunks[vox_chunk_index(
                    (vox_u32)sample_x, (vox_u32)sample_y)];
                if (cell->material != VOX_MAT_AIR &&
                    cell->material != VOX_MAT_BEDROCK &&
                    (distance_squared <= core_squared ||
                     ((fracture >> (depth & 7U)) & 1U) != 0U)) {
                    vox_clear_cell(world, chunk, vox_index((vox_u32)sample_x,
                                                           (vox_u32)sample_y,
                                                           depth), cell);
                }
            }
        }
    }
    for (sample_y = min_y > 0L ? min_y - 1L : 0L;
         sample_y <= max_y + 1L && sample_y < (long)VOX_WORLD_HEIGHT;
         ++sample_y) {
        for (sample_x = min_x > 0L ? min_x - 1L : 0L;
             sample_x <= max_x + 1L && sample_x < (long)VOX_WORLD_WIDTH;
             ++sample_x) {
            for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
                if (world->cells[vox_index((vox_u32)sample_x,
                                            (vox_u32)sample_y,
                                            depth)].material != VOX_MAT_AIR) {
                    (void)vox_world_wake(world, (vox_u32)sample_x,
                                         (vox_u32)sample_y, depth);
                }
            }
        }
    }
    for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
        vox_cell *cell = &world->cells[vox_index(x, y, depth)];
        if (cell->material == VOX_MAT_AIR && heat_q16 > VOX_AMBIENT_Q16 &&
            vox_world_set(world, x, y, depth, VOX_MAT_SMOKE,
                          heat_q16) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    }
    return VOX_OK;
}

vox_result vox_world_step(vox_world *world, const vox_step_command *command)
{
    vox_result result = VOX_OK;
    if (world == 0) {
        return VOX_ERR_INVALID;
    }
    if (command != 0) {
        if (command->abi_version != VOX_ABI_VERSION ||
            command->struct_size < (vox_u32)sizeof(*command)) {
            return VOX_ERR_INVALID;
        }
        result = vox_world_set(world, command->x, command->y, command->z,
                               command->material,
                               VOX_AMBIENT_Q16 + ((vox_i32)command->temperature_delta_q8 << 8));
        if (result != VOX_OK) {
            return result;
        }
    }
    if (world->awake_cells != 0U) {
        vox_step_reactions(world);
        vox_step_structures(world);
        vox_step_falling(world);
        vox_step_gases(world);
        vox_step_materials(world);
    }
    world->tick++;
    return VOX_OK;
}

vox_u32 vox_world_hash(const vox_world *world)
{
    vox_u32 hash = 2166136261U;
    vox_u32 i;
    if (world == 0) {
        return 0U;
    }
    hash ^= world->tick;
    hash *= 16777619U;
    hash ^= world->occupied_cells;
    hash *= 16777619U;
    hash ^= world->awake_cells;
    hash *= 16777619U;
    for (i = 0U; i < VOX_WORLD_CHUNK_COUNT; ++i) {
        const vox_chunk *chunk = &world->chunks[i];
        hash = vox_hash_mix(hash, chunk->cell_hash);
        hash = vox_hash_mix(hash, chunk->occupied_cells);
        hash = vox_hash_mix(hash, chunk->awake_cells);
        hash = vox_hash_mix(hash, (vox_u32)(chunk->flags & VOX_CHUNK_ACTIVE));
    }
    return hash;
}

const vox_cell *vox_world_cell(const vox_world *world, vox_u32 x, vox_u32 y,
                               vox_u32 z)
{
    if (world == 0 || !vox_in_bounds(x, y, z)) {
        return 0;
    }
    return &world->cells[vox_index(x, y, z)];
}

const vox_chunk *vox_world_chunk(const vox_world *world, vox_u32 chunk_x,
                                 vox_u32 chunk_y)
{
    if (world == 0 || chunk_x >= VOX_WORLD_CHUNKS_X ||
        chunk_y >= VOX_WORLD_CHUNKS_Y) {
        return 0;
    }
    return &world->chunks[chunk_y * VOX_WORLD_CHUNKS_X + chunk_x];
}

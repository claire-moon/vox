/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_kernel.h"

#define VOX_AMBIENT_Q16 (20L << 16)

static const vox_material_properties vox_materials[VOX_MAT_COUNT] = {
    {0U, 0U, 0U, 0U, 0L, 0L},
    {65535U, 65535U, 65535U, 0U, 0L, 0L},
    {2400U, 500U, 400U, 0U, 0L, 1000L << 16},
    {1400U, 80U, 120U, 0U, 0L, 500L << 16},
    {1300U, 120U, 180U, VOX_MATERIAL_FLAMMABLE, 450L << 16, 600L << 16},
    {900U, 60U, 100U, VOX_MATERIAL_FLAMMABLE, 300L << 16, 450L << 16},
    {1600U, 20U, 80U, VOX_MATERIAL_FLUID, 0L, 1000L << 16},
    {1000U, 0U, 600U, VOX_MATERIAL_FLUID, 0L, 100L << 16},
    {3000U, 0U, 900U, VOX_MATERIAL_FLUID | VOX_MATERIAL_EMISSIVE,
     0L, 650L << 16},
    {7800U, 500U, 800U, 0U, 0L, 1500L << 16},
    {1050U, 40U, 80U, VOX_MATERIAL_FLAMMABLE, 400L << 16, 500L << 16},
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
    destination->flags = (vox_u16)(original.flags & VOX_CELL_PHASE_GAS);
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
                            !vox_is_falling_material(cell->material)) {
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

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

static int vox_in_bounds(vox_u32 x, vox_u32 y, vox_u32 z)
{
    return x < VOX_WORLD_WIDTH && y < VOX_WORLD_HEIGHT && z < VOX_WORLD_DEPTH;
}

static void vox_update_active(vox_world *world, vox_cell *cell)
{
    int occupied = cell->material != VOX_MAT_AIR;
    if (occupied && !(cell->flags & VOX_CELL_OCCUPIED)) {
        world->occupied_cells++;
    } else if (!occupied && (cell->flags & VOX_CELL_OCCUPIED)) {
        world->occupied_cells--;
    }
    if (occupied) {
        cell->flags = (vox_u16)(cell->flags | VOX_CELL_OCCUPIED);
    } else {
        cell->flags = (vox_u16)(cell->flags & (vox_u16)~VOX_CELL_OCCUPIED);
    }
}

static void vox_wake_cell(vox_world *world, vox_cell *cell)
{
    if (!(cell->flags & VOX_CELL_AWAKE)) {
        cell->flags = (vox_u16)(cell->flags | VOX_CELL_AWAKE);
        world->awake_cells++;
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
    if (world == 0 || !vox_in_bounds(x, y, z)) {
        return VOX_ERR_INVALID;
    }
    if (material >= VOX_MAT_COUNT) {
        return VOX_ERR_INVALID;
    }
    cell = &world->cells[vox_index(x, y, z)];
    cell->material = material;
    cell->flags = (vox_u16)(cell->flags & (vox_u16)~VOX_CELL_PHASE_GAS);
    cell->temperature_q16 = temperature_q16;
    vox_update_active(world, cell);
    vox_wake_cell(world, cell);
    return VOX_OK;
}

vox_result vox_world_wake(vox_world *world, vox_u32 x, vox_u32 y, vox_u32 z)
{
    vox_cell *cell;
    if (world == 0 || !vox_in_bounds(x, y, z)) {
        return VOX_ERR_INVALID;
    }
    cell = &world->cells[vox_index(x, y, z)];
    vox_wake_cell(world, cell);
    return VOX_OK;
}

vox_result vox_world_step(vox_world *world, const vox_step_command *command)
{
    vox_result result = VOX_OK;
    vox_u32 i;
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
    for (i = 0U; i < VOX_WORLD_CELLS; ++i) {
        vox_cell *cell = &world->cells[i];
        if (!(cell->flags & VOX_CELL_AWAKE)) {
            continue;
        }
        if (cell->material == VOX_MAT_WATER && cell->temperature_q16 > (100L << 16)) {
            cell->flags = (vox_u16)(cell->flags | VOX_CELL_PHASE_GAS);
            cell->temperature_q16 = 80L << 16;
        } else if (cell->material == VOX_MAT_LAVA && cell->temperature_q16 < (650L << 16)) {
            cell->temperature_q16 += 1L << 12;
        } else {
            cell->flags = (vox_u16)(cell->flags & (vox_u16)~VOX_CELL_AWAKE);
            world->awake_cells--;
        }
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
    for (i = 0U; i < VOX_WORLD_CELLS; ++i) {
        const vox_cell *cell = &world->cells[i];
        hash ^= (vox_u32)cell->material;
        hash *= 16777619U;
        hash ^= (vox_u32)cell->flags;
        hash *= 16777619U;
        hash ^= (vox_u32)cell->temperature_q16;
        hash *= 16777619U;
        hash ^= (vox_u32)cell->damage_q16;
        hash *= 16777619U;
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

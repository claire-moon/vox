/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_kernel.h"

#define VOX_FLAG_ACTIVE 1U
#define VOX_AMBIENT_Q16 (20L << 16)

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
    int active = cell->material != VOX_MAT_AIR &&
                 cell->material != VOX_MAT_BEDROCK;
    if (active && !(cell->flags & VOX_FLAG_ACTIVE)) {
        world->active_cells++;
    } else if (!active && (cell->flags & VOX_FLAG_ACTIVE)) {
        world->active_cells--;
    }
    if (active) {
        cell->flags = (vox_u16)(cell->flags | VOX_FLAG_ACTIVE);
    } else {
        cell->flags = (vox_u16)(cell->flags & (vox_u16)~VOX_FLAG_ACTIVE);
    }
}

void vox_world_init(vox_world *world)
{
    vox_u32 i;
    world->abi_version = VOX_ABI_VERSION;
    world->struct_size = (vox_u32)sizeof(*world);
    world->tick = 0U;
    world->active_cells = 0U;
    for (i = 0U; i < VOX_WORLD_CELLS; ++i) {
        world->cells[i].material = VOX_MAT_AIR;
        world->cells[i].flags = 0U;
        world->cells[i].temperature_q16 = VOX_AMBIENT_Q16;
        world->cells[i].damage_q16 = 0L;
    }
}

vox_result vox_world_set(vox_world *world, vox_u32 x, vox_u32 y, vox_u32 z,
                         vox_u16 material, vox_i32 temperature_q16)
{
    vox_cell *cell;
    if (world == 0 || !vox_in_bounds(x, y, z)) {
        return VOX_ERR_INVALID;
    }
    if (material > VOX_MAT_LAVA) {
        return VOX_ERR_INVALID;
    }
    cell = &world->cells[vox_index(x, y, z)];
    cell->material = material;
    cell->temperature_q16 = temperature_q16;
    vox_update_active(world, cell);
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
        if (cell->material == VOX_MAT_WATER && cell->temperature_q16 > (100L << 16)) {
            cell->material = VOX_MAT_AIR;
            cell->temperature_q16 = 80L << 16;
            vox_update_active(world, cell);
        } else if (cell->material == VOX_MAT_LAVA && cell->temperature_q16 < (650L << 16)) {
            cell->temperature_q16 += 1L << 12;
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
    hash ^= world->active_cells;
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

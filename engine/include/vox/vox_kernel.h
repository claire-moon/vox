/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_KERNEL_H
#define VOX_KERNEL_H

#include "vox_types.h"

#define VOX_WORLD_WIDTH 32U
#define VOX_WORLD_HEIGHT 24U
#define VOX_WORLD_DEPTH 4U
#define VOX_WORLD_CELLS (VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT * VOX_WORLD_DEPTH)

typedef enum vox_material_id {
    VOX_MAT_AIR = 0,
    VOX_MAT_BEDROCK = 1,
    VOX_MAT_STONE = 2,
    VOX_MAT_SOIL = 3,
    VOX_MAT_WATER = 4,
    VOX_MAT_LAVA = 5
} vox_material_id;

typedef struct vox_cell {
    vox_u16 material;
    vox_u16 flags;
    vox_i32 temperature_q16;
    vox_i32 damage_q16;
} vox_cell;

typedef struct vox_world {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 tick;
    vox_u32 active_cells;
    vox_cell cells[VOX_WORLD_CELLS];
} vox_world;

typedef struct vox_step_command {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    vox_u16 material;
    vox_i16 temperature_delta_q8;
} vox_step_command;

void vox_world_init(vox_world *world);
vox_result vox_world_set(vox_world *world, vox_u32 x, vox_u32 y, vox_u32 z,
                         vox_u16 material, vox_i32 temperature_q16);
vox_result vox_world_step(vox_world *world, const vox_step_command *command);
vox_u32 vox_world_hash(const vox_world *world);
const vox_cell *vox_world_cell(const vox_world *world, vox_u32 x, vox_u32 y,
                               vox_u32 z);

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_KERNEL_H
#define VOX_KERNEL_H

#include "vox_types.h"

#ifndef VOX_WORLD_WIDTH
#define VOX_WORLD_WIDTH 32U
#endif
#ifndef VOX_WORLD_HEIGHT
#define VOX_WORLD_HEIGHT 24U
#endif
#ifndef VOX_WORLD_DEPTH
#define VOX_WORLD_DEPTH 4U
#endif
#define VOX_WORLD_CELLS (VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT * VOX_WORLD_DEPTH)

typedef enum vox_material_id {
    VOX_MAT_AIR = 0,
    VOX_MAT_BEDROCK = 1,
    VOX_MAT_STONE = 2,
    VOX_MAT_SOIL = 3,
    VOX_MAT_COAL = 4,
    VOX_MAT_BIOMASS = 5,
    VOX_MAT_SAND = 6,
    VOX_MAT_WATER = 7,
    VOX_MAT_LAVA = 8,
    VOX_MAT_METAL = 9,
    VOX_MAT_FLESH = 10,
    VOX_MAT_BLOOD = 11,
    VOX_MAT_SMOKE = 12,
    VOX_MAT_FIREDAMP = 13,
    VOX_MAT_COUNT = 14
} vox_material_id;

typedef struct vox_material_properties {
    vox_u16 density;
    vox_u16 strength;
    vox_u16 conductivity;
    vox_u16 flags;
    vox_i32 ignition_q16;
    vox_i32 melt_q16;
} vox_material_properties;

#define VOX_MATERIAL_FLAMMABLE 1U
#define VOX_MATERIAL_FLUID 2U
#define VOX_MATERIAL_GAS 4U
#define VOX_MATERIAL_EMISSIVE 8U

#define VOX_CELL_OCCUPIED 1U
#define VOX_CELL_AWAKE 2U
#define VOX_CELL_PHASE_GAS 4U
#define VOX_CELL_MOVED 8U

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
    vox_u32 occupied_cells;
    vox_u32 awake_cells;
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
const vox_material_properties *vox_material_get(vox_u16 material);
vox_result vox_world_set(vox_world *world, vox_u32 x, vox_u32 y, vox_u32 z,
                         vox_u16 material, vox_i32 temperature_q16);
vox_result vox_world_wake(vox_world *world, vox_u32 x, vox_u32 y, vox_u32 z);
vox_result vox_world_step(vox_world *world, const vox_step_command *command);
vox_u32 vox_world_hash(const vox_world *world);
const vox_cell *vox_world_cell(const vox_world *world, vox_u32 x, vox_u32 y,
                               vox_u32 z);

#endif

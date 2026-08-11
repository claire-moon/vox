/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_FLUID_H
#define VOX_FLUID_H

#include "vox_kernel.h"

/*
 * Fluids are sparse because a full 512x320x10 scalar volume would consume
 * more state than the fixed match budget permits.  The pool is authoritative:
 * each occupied entry carries its world coordinate and is never addressed by
 * a modulo projection.  Overflow is explicit and deterministic.
 */
#define VOX_FLUID_MAX_CELLS 2048U
#define VOX_FLUID_GRID_WIDTH VOX_WORLD_WIDTH
#define VOX_FLUID_GRID_HEIGHT VOX_WORLD_HEIGHT
#define VOX_FLUID_GRID_DEPTH VOX_WORLD_DEPTH
#define VOX_FLUID_CELL_CAPACITY_Q16 65536L
#define VOX_FLUID_LOOKUP_CAPACITY 4096U

typedef enum vox_fluid_material {
    VOX_FLUID_NONE = 0,
    VOX_FLUID_WATER = 1,
    VOX_FLUID_LAVA = 2,
    VOX_FLUID_BLOOD = 3
} vox_fluid_material;

typedef struct vox_fluid_cell {
    vox_u16 x;
    vox_u16 y;
    vox_u16 z;
    vox_u16 material;
    vox_u16 active;
    vox_i32 volume_q16;
    vox_i32 pressure_q16;
    vox_i32 temperature_q16;
    vox_i32 flow_q16;
} vox_fluid_cell;

typedef struct vox_fluid_world {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 tick;
    vox_u32 active_cells;
    vox_u16 frontier_cursor;
    vox_u16 sorted_cells;
    vox_i32 total_volume_q16;
    vox_i32 reaction_loss_q16;
    vox_fluid_cell cells[VOX_FLUID_MAX_CELLS];
    /* Fixed scratch for deterministic incremental canonical ordering. */
    vox_fluid_cell sort_scratch[VOX_FLUID_MAX_CELLS];
    /* Derived coordinate index; zero means empty, otherwise index + 1. */
    vox_u16 lookup[VOX_FLUID_LOOKUP_CAPACITY];
} vox_fluid_world;

void vox_fluid_init(vox_fluid_world *world);

/* z=0 compatibility wrappers for 2-D callers. */
vox_result vox_fluid_set(vox_fluid_world *world, vox_u16 x, vox_u16 y,
                         vox_u16 material, vox_i32 volume_q16,
                         vox_i32 temperature_q16);
vox_result vox_fluid_add(vox_fluid_world *world, vox_u16 x, vox_u16 y,
                         vox_u16 material, vox_i32 volume_q16,
                         vox_i32 temperature_q16);

/* Authoritative 3-D entry points used by DIGS tools and corpse deposits. */
vox_result vox_fluid_set_at(vox_fluid_world *world, vox_u16 x, vox_u16 y,
                            vox_u16 z, vox_u16 material, vox_i32 volume_q16,
                            vox_i32 temperature_q16);
vox_result vox_fluid_add_at(vox_fluid_world *world, vox_u16 x, vox_u16 y,
                            vox_u16 z, vox_u16 material, vox_i32 volume_q16,
                            vox_i32 temperature_q16);

/* The no-terrain form is useful for isolated solver tests. */
vox_result vox_fluid_step(vox_fluid_world *world, vox_u16 max_cells);
/* The terrain form blocks transfers into solid world cells. */
vox_result vox_fluid_step_terrain(vox_fluid_world *world,
                                  const vox_world *terrain,
                                  vox_u16 max_cells);

vox_i32 vox_fluid_conserved_volume(const vox_fluid_world *world);
vox_i32 vox_fluid_reaction_loss(const vox_fluid_world *world);
const vox_fluid_cell *vox_fluid_cell_get(const vox_fluid_world *world,
                                         vox_u16 x, vox_u16 y);
const vox_fluid_cell *vox_fluid_cell_get_at(const vox_fluid_world *world,
                                            vox_u16 x, vox_u16 y,
                                            vox_u16 z);
/* Aggregate one world column for rigid-body buoyancy and drag. */
vox_result vox_fluid_sample_column(const vox_fluid_world *world,
                                   vox_u16 x, vox_u16 y,
                                   vox_i32 *volume_q16,
                                   vox_u16 *material);
vox_u32 vox_fluid_hash(const vox_fluid_world *world);

#endif

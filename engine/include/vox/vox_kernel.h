/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_KERNEL_H
#define VOX_KERNEL_H

#include "vox_types.h"

/*
 * The desktop showcase uses a four-screen 16:10 field.  Memory-constrained
 * ports can retain the same engine API with smaller compile-time dimensions.
 */
#ifndef VOX_WORLD_WIDTH
#define VOX_WORLD_WIDTH 512U
#endif
#ifndef VOX_WORLD_HEIGHT
#define VOX_WORLD_HEIGHT 320U
#endif
#ifndef VOX_WORLD_DEPTH
#define VOX_WORLD_DEPTH 10U
#endif
#ifndef VOX_CHUNK_WIDTH
#define VOX_CHUNK_WIDTH 16U
#endif
#ifndef VOX_CHUNK_HEIGHT
#define VOX_CHUNK_HEIGHT 16U
#endif

#if VOX_WORLD_WIDTH == 0U || VOX_WORLD_HEIGHT == 0U || \
    VOX_WORLD_DEPTH == 0U
#error "VOX world dimensions must be nonzero"
#else
#if VOX_WORLD_WIDTH > \
    (4294967295UL / VOX_WORLD_HEIGHT / VOX_WORLD_DEPTH)
#error "VOX world cell count must fit vox_u32"
#endif
#endif
#if VOX_CHUNK_WIDTH == 0U || VOX_CHUNK_HEIGHT == 0U
#error "VOX chunk dimensions must be nonzero"
#else
#if (VOX_WORLD_WIDTH % VOX_CHUNK_WIDTH) != 0
#error "VOX_WORLD_WIDTH must be an exact multiple of VOX_CHUNK_WIDTH"
#endif
#if (VOX_WORLD_HEIGHT % VOX_CHUNK_HEIGHT) != 0
#error "VOX_WORLD_HEIGHT must be an exact multiple of VOX_CHUNK_HEIGHT"
#endif
#endif
#if VOX_WORLD_WIDTH > 32767U || VOX_WORLD_HEIGHT > 32767U
#error "VOX world dimensions must fit signed Q16.16 coordinates"
#endif

#define VOX_WORLD_CHUNKS_X (VOX_WORLD_WIDTH / VOX_CHUNK_WIDTH)
#define VOX_WORLD_CHUNKS_Y (VOX_WORLD_HEIGHT / VOX_CHUNK_HEIGHT)
#define VOX_WORLD_CHUNK_COUNT (VOX_WORLD_CHUNKS_X * VOX_WORLD_CHUNKS_Y)
#define VOX_WORLD_CHUNK_CELLS (VOX_CHUNK_WIDTH * VOX_CHUNK_HEIGHT * \
                               VOX_WORLD_DEPTH)
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
#define VOX_MATERIAL_SOLID 16U

#define VOX_CELL_OCCUPIED 1U
#define VOX_CELL_AWAKE 2U
#define VOX_CELL_PHASE_GAS 4U
#define VOX_CELL_MOVED 8U
#define VOX_CELL_UNSTABLE 16U
/* Loose solid cells simulate as debris but do not block character bodies. */
#define VOX_CELL_LOOSE 32U
/* Authored metal rope target; never participates in terrain structure. */
#define VOX_CELL_FIXTURE 64U
/* Presentation-only blood stain on an existing terrain voxel. */
#define VOX_CELL_BLOODY 128U

typedef enum vox_world_collision_class {
    VOX_WORLD_COLLISION_EMPTY = 0,
    VOX_WORLD_COLLISION_LOOSE = 1,
    VOX_WORLD_COLLISION_SOLID = 2
} vox_world_collision_class;

#define VOX_CHUNK_ACTIVE 1U
#define VOX_CHUNK_DIRTY 2U
#define VOX_BLAST_MAX_RADIUS 16U
/* Bounded record of cells actually cleared by one fracture pass. */
#define VOX_BLAST_CAPTURE_MAX 160U

/*
 * Horizontal reach, in cells, over which intact ground holds up a ceiling.
 *
 * Without this, structural support is cohesionless: only the cell directly
 * below and its two diagonals count, so any span wider than about two cells
 * loses its middle the moment it is undermined, and ordinary tunnelling
 * destroys its own tunnel.  With it, spans up to roughly
 * 2 * VOX_STRUCTURE_COHESION_CELLS + 1 stay standing and wider excavations
 * cave in.  This is the tuning knob for how brave a miner can be with a drill.
 */
#define VOX_STRUCTURE_COHESION_CELLS 4U

/*
 * Smoke lives for a bounded number of ticks and then clears, and cools toward
 * ambient while it does.  Without a lifetime it rose to the nearest ceiling
 * and accumulated there permanently.  Three hundred ticks is five seconds at
 * 60 Hz -- long enough for a smoke pot to still hide a miner.
 */
#define VOX_SMOKE_LIFETIME_Q16 (300L << 16)
#define VOX_SMOKE_COOLING_SHIFT 6U

typedef struct vox_cell {
    vox_u16 material;
    vox_u16 flags;
    vox_i32 temperature_q16;
    vox_i32 damage_q16;
} vox_cell;

typedef struct vox_chunk {
    vox_u32 occupied_cells;
    vox_u32 awake_cells;
    vox_u32 generation;
    vox_u32 cell_hash;
    vox_u16 flags;
    vox_u16 reserved;
} vox_chunk;

typedef struct vox_world {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 tick;
    vox_u32 occupied_cells;
    vox_u32 awake_cells;
    vox_chunk chunks[VOX_WORLD_CHUNK_COUNT];
    vox_cell cells[VOX_WORLD_CELLS];
} vox_world;

typedef struct vox_blast_cell {
    vox_u16 x;
    vox_u16 y;
    vox_u16 z;
    vox_u16 material;
    vox_u16 flags;
} vox_blast_cell;

typedef struct vox_blast_capture {
    vox_u16 count;
    vox_u16 truncated;
    /* All non-fixture support-bearing cells cleared, even past record cap. */
    vox_u16 structural_count;
    vox_blast_cell cells[VOX_BLAST_CAPTURE_MAX];
} vox_blast_capture;

typedef struct vox_step_command {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    vox_u16 material;
    vox_i16 temperature_delta_q8;
} vox_step_command;

#ifdef __cplusplus
extern "C" {
#endif

void vox_world_init(vox_world *world);
/* Rebuild occupied/chunk hash indexes after deterministic bulk population. */
vox_result vox_world_rebuild(vox_world *world);
const vox_material_properties *vox_material_get(vox_u16 material);
vox_result vox_world_set(vox_world *world, vox_u32 x, vox_u32 y, vox_u32 z,
                         vox_u16 material, vox_i32 temperature_q16);
/* Mark authored metal as a static rope target, distinct from terrain. */
vox_result vox_world_set_fixture(vox_world *world, vox_u32 x, vox_u32 y,
                                 vox_u32 z, vox_u16 fixture);
int vox_world_is_fixture(const vox_world *world, vox_u32 x, vox_u32 y,
                         vox_u32 z);
/* Toggle a nonphysical blood stain without changing material or collision. */
vox_result vox_world_set_bloody(vox_world *world, vox_u32 x, vox_u32 y,
                                vox_u32 z, vox_u16 bloody);
/* Set one complete x/z layer, retaining a material such as bedrock. */
vox_result vox_world_set_layer_except(vox_world *world, vox_u32 y,
                                      vox_u16 material,
                                      vox_i32 temperature_q16,
                                      vox_u16 skip_material);
/* Bulk layer fill for persistent hazards; does not wake sleeping cells. */
vox_result vox_world_set_layer_quiet_except(vox_world *world, vox_u32 y,
                                            vox_u16 material,
                                            vox_i32 temperature_q16,
                                            vox_u16 skip_material);
/* Setting loose is idempotent; AIR may only be cleared, never made loose. */
vox_result vox_world_set_loose(vox_world *world, vox_u32 x, vox_u32 y,
                               vox_u32 z, vox_u16 loose);
vox_result vox_world_wake(vox_world *world, vox_u32 x, vox_u32 y, vox_u32 z);
vox_result vox_world_sleep_all(vox_world *world);
vox_result vox_world_clear_dirty(vox_world *world);
vox_result vox_world_blast(vox_world *world, vox_u32 x, vox_u32 y,
                           vox_u32 z, vox_u32 radius, vox_i32 heat_q16);
/* Initialize a bounded fracture capture record before reuse. */
void vox_blast_capture_init(vox_blast_capture *capture);
/*
 * Apply the normal blast fracture while recording the pre-clear location and
 * material of each destroyed non-bedrock, non-fixture cell.  The record is
 * ordered by the deterministic fracture traversal; truncated is set when it
 * reaches capacity. structural_count records all cleared support-bearing
 * terrain, including cells beyond the bounded record.
 */
vox_result vox_world_blast_capture(vox_world *world, vox_u32 x, vox_u32 y,
                                   vox_u32 z, vox_u32 radius,
                                   vox_i32 heat_q16,
                                   vox_blast_capture *capture);
vox_result vox_world_step(vox_world *world, const vox_step_command *command);
vox_u32 vox_world_hash(const vox_world *world);
const vox_cell *vox_world_cell(const vox_world *world, vox_u32 x, vox_u32 y,
                               vox_u32 z);
/* A stable solid at any depth makes the whole x/y column body-solid. */
vox_u16 vox_world_collision_classify(const vox_world *world, vox_u32 x,
                                     vox_u32 y);
const vox_chunk *vox_world_chunk(const vox_world *world, vox_u32 chunk_x,
                                 vox_u32 chunk_y);

#ifdef __cplusplus
}
#endif

#endif

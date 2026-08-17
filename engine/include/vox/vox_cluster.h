/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_CLUSTER_H
#define VOX_CLUSTER_H

#include "vox_rigid.h"

#define VOX_CLUSTER_MAX_CELLS 128U
#define VOX_STRUCTURE_FRONTIER_CAPACITY VOX_WORLD_CHUNK_COUNT
#define VOX_STRUCTURE_COLLAPSE_CAPACITY 128U
#define VOX_STRUCTURE_NO_SOURCE 65535U

typedef struct vox_structure_cell {
    vox_u16 x;
    vox_u16 y;
    vox_u16 z;
    vox_u16 material;
} vox_structure_cell;

typedef struct vox_structure_cluster {
    vox_u16 count;
    vox_u16 complete;
    vox_u16 min_x;
    vox_u16 max_x;
    vox_u16 min_y;
    vox_u16 max_y;
    vox_u16 min_z;
    vox_u16 max_z;
    vox_u16 anchored;
    vox_u16 support_q8;
    vox_u16 load_q8;
    vox_structure_cell cells[VOX_CLUSTER_MAX_CELLS];
} vox_structure_cluster;

typedef struct vox_structure_chunk_state {
    vox_u16 support_q8;
    vox_u16 load_q8;
    vox_u16 dirty;
    vox_u16 collapse_risk_q8;
    /* Accumulated excavation/blast strain.  A low-strain unsupported fleck
     * is a creak, not an immediate cave-in; only a critical chunk can queue
     * a physical collapse seed. */
    vox_u16 strain_q8;
    vox_u16 warning_pending;
    /* An invalidation arms this chunk for one physical collapse decision.
     * Initial map analysis deliberately leaves it disarmed, so authored
     * overhangs do not spontaneously turn into debris before any excavation
     * or blast has disturbed their support frontier. */
    vox_u16 collapse_armed;
    vox_u16 collapse_pending;
    vox_u16 source;
    vox_u16 weapon;
} vox_structure_chunk_state;

/* A queued collapse is deliberately a seed rather than a copy of terrain.
 * The world remains authoritative until the game consumes the seed and
 * extracts the corresponding bounded unsupported fragment. */
typedef struct vox_structure_collapse {
    vox_u16 x;
    vox_u16 y;
    vox_u16 z;
    vox_u16 chunk_index;
    vox_u16 source;
    vox_u16 weapon;
    vox_u16 risk_q8;
    vox_u16 reserved;
} vox_structure_collapse;

/* Match-owned support frontier.  The voxel world remains the terrain
 * authority; this bounded state records which chunk decisions are pending and
 * lets a large excavation carry its work over fixed ticks. */
typedef struct vox_structure_state {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 tick;
    vox_u32 cascade_count;
    vox_u32 discarded_frontier;
    vox_u32 discarded_collapse;
    vox_u16 frontier_head;
    vox_u16 frontier_count;
    vox_u16 frontier_cursor;
    vox_u16 collapse_head;
    vox_u16 collapse_count;
    vox_u16 reserved;
    vox_u16 frontier[VOX_STRUCTURE_FRONTIER_CAPACITY];
    vox_structure_collapse collapses[VOX_STRUCTURE_COLLAPSE_CAPACITY];
    vox_structure_chunk_state chunks[VOX_WORLD_CHUNK_COUNT];
} vox_structure_state;

void vox_cluster_init(vox_structure_cluster *cluster);
vox_result vox_cluster_extract(vox_world *world, vox_u32 x, vox_u32 y,
                               vox_u32 z, vox_structure_cluster *cluster);
/* Detach only the unsupported part of a connected component.  If its extent
 * exceeds VOX_CLUSTER_MAX_CELLS, the returned fragment is still detached and
 * complete is zero so callers can invalidate the remaining frontier on later
 * fixed ticks. */
vox_result vox_cluster_extract_unsupported(vox_world *world, vox_u32 x,
                                           vox_u32 y, vox_u32 z,
                                           vox_structure_cluster *cluster);
/* Restore a just-detached fragment when a bounded debris pool has no slot.
 * This makes the overflow rule explicit instead of silently deleting solid
 * terrain. */
vox_result vox_cluster_restore(vox_world *world,
                               const vox_structure_cluster *cluster,
                               vox_u16 loose);
vox_result vox_cluster_spawn_debris(const vox_structure_cluster *cluster,
                                    vox_rigid_world *rigid,
                                    vox_i32 impulse_x_q16,
                                    vox_i32 impulse_y_q16);
void vox_structure_init(vox_structure_state *state);
vox_result vox_structure_invalidate(vox_structure_state *state,
                                    vox_u32 x, vox_u32 y, vox_u32 radius);
/* As above, with the action that disturbed the frontier.  Source and weapon
 * travel with the queued collapse so cave-in kills/awards stay attributable. */
vox_result vox_structure_invalidate_with_cause(vox_structure_state *state,
                                               vox_u32 x, vox_u32 y,
                                               vox_u32 radius,
                                               vox_u16 source,
                                               vox_u16 weapon);
/* Like invalidate_with_cause, with an explicit deterministic structural
 * impulse.  Direct cuts are gentle; explosive fractures can cross the
 * critical threshold in one hit. */
vox_result vox_structure_invalidate_with_impulse(vox_structure_state *state,
                                                 vox_u32 x, vox_u32 y,
                                                 vox_u32 radius,
                                                 vox_u16 source,
                                                 vox_u16 weapon,
                                                 vox_u16 impulse_q8);
vox_result vox_structure_step(vox_structure_state *state,
                              const vox_world *world, vox_u16 max_chunks);
/* Returns VOX_OK and writes one stable FIFO collapse seed, or
 * VOX_ERR_CAPACITY when the queue is empty. */
vox_result vox_structure_pop_collapse(vox_structure_state *state,
                                      vox_structure_collapse *collapse);
vox_u32 vox_structure_hash(const vox_structure_state *state);

#endif

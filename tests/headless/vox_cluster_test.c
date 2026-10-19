/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_cluster.h"

static void audit_seed(vox_structure_collapse *seed, vox_u16 x,
                       vox_u16 y, vox_u16 z)
{
    seed->x = x;
    seed->y = y;
    seed->z = z;
    seed->chunk_index = (vox_u16)((y / VOX_CHUNK_HEIGHT) *
                                  VOX_WORLD_CHUNKS_X + x / VOX_CHUNK_WIDTH);
    seed->source = 7U;
    seed->weapon = 9U;
    seed->risk_q8 = 255U;
    seed->reserved = 0U;
}

static int finish_audit(vox_structure_state *state, const vox_world *world)
{
    vox_u16 tick;
    for (tick = 0U; tick < 1024U; ++tick) {
        vox_result result = vox_structure_audit_step(state, world, 1U);
        if (result == VOX_OK) return 1;
        if (result != VOX_ERR_CAPACITY) return 0;
    }
    return 0;
}

int main(void)
{
    static vox_world world;
    vox_rigid_world rigid;
    vox_structure_cluster cluster;
    vox_structure_state structure;
    vox_structure_collapse collapse;
    vox_u16 body_index;
    vox_u16 i;
    vox_u16 row;
    vox_u32 audit_hash;

    vox_world_init(&world);
    vox_rigid_init(&rigid);
    vox_structure_init(&structure);
    for (i = 4U; i < 7U; ++i) {
        if (vox_world_set(&world, i, 10U, 3U, VOX_MAT_STONE, 0L) != VOX_OK) {
            return 1;
        }
    }
    if (vox_structure_step(&structure, &world,
                           VOX_STRUCTURE_FRONTIER_CAPACITY) != VOX_OK ||
        structure.chunks[0].load_q8 != 3U ||
        structure.chunks[0].support_q8 != 0U ||
        structure.chunks[0].collapse_risk_q8 == 0U) return 10;
    if (structure.collapse_count != 0U ||
        vox_structure_invalidate_with_cause(&structure, 5U, 10U, 4U,
                                            7U, 9U) != VOX_OK ||
        structure.frontier_count == 0U ||
        vox_structure_hash(&structure) == 0U ||
        vox_structure_step(&structure, &world, 1U) != VOX_OK) return 11;
    if (vox_structure_pop_collapse(&structure, &collapse) != VOX_OK ||
        collapse.x != 4U || collapse.y != 10U || collapse.z != 3U ||
        collapse.source != 7U || collapse.weapon != 9U ||
        collapse.risk_q8 == 0U ||
        vox_structure_pop_collapse(&structure, &collapse) != VOX_ERR_CAPACITY) {
        return 12;
    }
    if (vox_cluster_extract(&world, 5U, 10U, 3U, &cluster) != VOX_OK ||
        cluster.count != 3U || cluster.complete == 0U) return 2;
    if (vox_world_cell(&world, 5U, 10U, 3U)->material != VOX_MAT_AIR) {
        return 3;
    }
    if (cluster.support_q8 != 0U || cluster.load_q8 != 3U) return 4;
    if (vox_cluster_spawn_debris(&cluster, &rigid, 2L << 16, -(3L << 16)) !=
        VOX_OK) return 5;
    if (rigid.body_count != 1U) return 6;
    body_index = 0U;
    if ((rigid.bodies[body_index].flags & VOX_RIGID_BODY_DEBRIS) == 0U ||
        rigid.bodies[body_index].velocity_x_q16 != 2L << 16 ||
        rigid.bodies[body_index].velocity_y_q16 != -(3L << 16)) return 7;
    /* Vertical connectivity is part of the same detached cluster. */
    if (vox_world_set(&world, 30U, 20U, 0U, VOX_MAT_SOIL, 0L) != VOX_OK ||
        vox_world_set(&world, 30U, 20U, 1U, VOX_MAT_SOIL, 0L) != VOX_OK ||
        vox_cluster_extract(&world, 30U, 20U, 0U, &cluster) != VOX_OK ||
        cluster.count != 2U) return 8;
    /* A wide unsupported roof is consumed in deterministic fragments rather
     * than failing all-or-nothing at the fixed cluster capacity. */
    for (i = 100U; i < 229U; ++i) {
        if (vox_world_set(&world, i, 40U, 0U, VOX_MAT_STONE, 0L) != VOX_OK) {
            return 13;
        }
    }
    if (vox_cluster_extract_unsupported(&world, 100U, 40U, 0U, &cluster) !=
            VOX_OK ||
        cluster.count != VOX_CLUSTER_MAX_CELLS || cluster.complete != 0U ||
        vox_world_cell(&world, 100U, 40U, 0U)->material != VOX_MAT_AIR ||
        vox_world_cell(&world, 227U, 40U, 0U)->material != VOX_MAT_AIR ||
        vox_world_cell(&world, 228U, 40U, 0U)->material != VOX_MAT_STONE ||
        vox_cluster_restore(&world, &cluster, 1U) != VOX_OK ||
        (vox_world_cell(&world, 100U, 40U, 0U)->flags & VOX_CELL_LOOSE) == 0U ||
        vox_cluster_spawn_debris(&cluster, &rigid, 0L, -(2L << 16)) != VOX_OK) {
        return 14;
    }
    /* A cluster rooted at bedrock is load-bearing and must remain in place. */
    if (vox_world_set(&world, 40U, VOX_WORLD_HEIGHT - 1U, 0U,
                      VOX_MAT_STONE, 0L) != VOX_OK ||
        vox_world_set(&world, 40U, VOX_WORLD_HEIGHT - 2U, 0U,
                      VOX_MAT_STONE, 0L) != VOX_OK ||
        vox_cluster_extract(&world, 40U, VOX_WORLD_HEIGHT - 2U, 0U, &cluster) !=
            VOX_ERR_COLLISION || cluster.anchored == 0U ||
        vox_world_cell(&world, 40U, VOX_WORLD_HEIGHT - 2U, 0U)->material !=
            VOX_MAT_STONE) return 9;
    if (vox_cluster_extract_unsupported(&world, 40U,
                                        VOX_WORLD_HEIGHT - 2U, 0U,
                                        &cluster) != VOX_ERR_COLLISION ||
        cluster.anchored == 0U) return 15;
    /* A floating 17x17 slab exceeds the debris capacity and contains vertical
     * cells that the local one-row support test mistakes for grounded load.
     * Complete connectivity discovers all 289 cells over bounded ticks and
     * extracts a 128-cell volume including its upper rows. */
    vox_world_init(&world);
    vox_structure_init(&structure);
    for (i = 300U; i < 317U; ++i) {
        for (row = 50U; row < 67U; ++row) {
            if (vox_world_set(&world, i, row, 2U, VOX_MAT_STONE,
                              0L) != VOX_OK) return 16;
        }
    }
    audit_seed(&collapse, 300U, 50U, 2U);
    if (vox_structure_audit_begin(&structure, &world, &collapse) != VOX_OK ||
        vox_structure_audit_step(&structure, &world, 1U) !=
            VOX_ERR_CAPACITY || structure.audit_complete != 0U ||
        structure.audit_cells == 0U || structure.audit_cells >= 289U ||
        !finish_audit(&structure, &world) ||
        structure.audit_cells != 289U || structure.audit_anchors != 0U ||
        structure.audit_load != 289U * 24U ||
        structure.audit_support != 0U || structure.audit_pending_hash != 0U) {
        return 17;
    }
    audit_hash = vox_structure_hash(&structure);
    if (vox_structure_extract_collapse(&structure, &world, 1U, &collapse,
                                       &cluster) != VOX_OK ||
        cluster.count != VOX_CLUSTER_MAX_CELLS || cluster.complete != 0U ||
        cluster.min_y != 50U || cluster.max_y == 50U ||
        structure.audit_cells != 289U ||
        vox_structure_hash(&structure) == audit_hash ||
        vox_world_cell(&world, 300U, 50U, 2U)->material != VOX_MAT_AIR) {
        return 18;
    }
    /* An anchor beyond a 128-cell fragment is real support.  Its finite
     * capacity and the entire component's load are measured before any cell
     * is removed, preventing a distant bedrock root from being missed. */
    vox_world_init(&world);
    vox_structure_init(&structure);
    for (i = 100U; i <= 300U; ++i) {
        if (vox_world_set(&world, i, 20U, 0U, VOX_MAT_STONE,
                          0L) != VOX_OK) return 19;
    }
    if (vox_world_set(&world, 301U, 20U, 0U, VOX_MAT_BEDROCK,
                      0L) != VOX_OK) return 20;
    audit_seed(&collapse, 100U, 20U, 0U);
    if (vox_structure_audit_begin(&structure, &world, &collapse) != VOX_OK ||
        !finish_audit(&structure, &world) ||
        structure.audit_cells != 201U || structure.audit_anchors != 1U ||
        structure.audit_load != 201U * 24U ||
        structure.audit_support != 16000U ||
        vox_structure_extract_collapse(&structure, &world, 1U, &collapse,
                                       &cluster) != VOX_ERR_COLLISION ||
        cluster.anchored == 0U ||
        vox_world_cell(&world, 100U, 20U, 0U)->material != VOX_MAT_STONE) {
        return 21;
    }
    /* Even a completed frontier is revalidated at extraction.  Removing the
     * root after the audit cannot reuse its earlier safe result. */
    if (vox_structure_audit_begin(&structure, &world, &collapse) != VOX_OK ||
        !finish_audit(&structure, &world) ||
        vox_world_set(&world, 301U, 20U, 0U, VOX_MAT_AIR, 0L) != VOX_OK ||
        vox_structure_extract_collapse(&structure, &world, 1U, &collapse,
                                       &cluster) != VOX_ERR_CAPACITY ||
        structure.audit_restarts != 1U ||
        !finish_audit(&structure, &world) ||
        structure.audit_cells != 201U || structure.audit_anchors != 0U ||
        vox_structure_extract_collapse(&structure, &world, 1U, &collapse,
                                       &cluster) != VOX_OK ||
        cluster.count != VOX_CLUSTER_MAX_CELLS) return 22;
    /* Loose debris never bridges otherwise separate rooted components. */
    vox_world_init(&world);
    vox_structure_init(&structure);
    for (i = 100U; i <= 300U; ++i) {
        if (vox_world_set(&world, i, 20U, 0U, VOX_MAT_STONE,
                          0L) != VOX_OK) return 23;
    }
    if (vox_world_set_loose(&world, 250U, 20U, 0U, 1U) != VOX_OK ||
        vox_world_set(&world, 301U, 20U, 0U, VOX_MAT_BEDROCK,
                      0L) != VOX_OK) return 24;
    audit_seed(&collapse, 100U, 20U, 0U);
    if (vox_structure_audit_begin(&structure, &world, &collapse) != VOX_OK ||
        !finish_audit(&structure, &world) ||
        structure.audit_cells != 150U || structure.audit_anchors != 0U) {
        return 25;
    }
    printf("cluster extraction and debris passed\n");
    return 0;
}

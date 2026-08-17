/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_cluster.h"

int main(void)
{
    static vox_world world;
    vox_rigid_world rigid;
    vox_structure_cluster cluster;
    vox_structure_state structure;
    vox_structure_collapse collapse;
    vox_u16 body_index;
    vox_u16 i;

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
    /* Ordinary excavation must accumulate visible strain before it can
     * detach an unsupported roof.  A high-energy fracture uses the explicit
     * impulse path and may collapse immediately. */
    if (structure.collapse_count != 0U ||
        vox_structure_invalidate_with_cause(&structure, 5U, 10U, 4U,
                                            7U, 9U) != VOX_OK ||
        structure.frontier_count == 0U ||
        vox_structure_hash(&structure) == 0U ||
        vox_structure_step(&structure, &world, 1U) != VOX_OK ||
        structure.collapse_count != 0U ||
        structure.chunks[0].strain_q8 == 0U ||
        vox_structure_invalidate_with_impulse(&structure, 5U, 10U, 4U,
                                              7U, 9U, 255U) != VOX_OK ||
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
    printf("cluster extraction and debris passed\n");
    return 0;
}

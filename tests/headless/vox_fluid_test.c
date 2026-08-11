/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>

#include "vox/vox_fluid.h"

static vox_i32 total(const vox_fluid_world *world)
{
    vox_u16 i;
    vox_i32 value = 0L;
    for (i = 0U; i < VOX_FLUID_MAX_CELLS; ++i) {
        value += world->cells[i].volume_q16;
    }
    return value;
}

int main(void)
{
    vox_fluid_world first;
    vox_fluid_world second;
    vox_fluid_world ordered_a;
    vox_fluid_world ordered_b;
    vox_fluid_world depth_flow;
    static vox_fluid_world pressure_flow;
    static vox_fluid_world lake_flow;
    static vox_fluid_world spill_flow;
    static vox_fluid_world lava_spill;
    static vox_fluid_world blood_pool;
    static vox_world terrain;
    static vox_world pressure_terrain;
    static vox_world basin_terrain;
    static vox_world spill_terrain;
    vox_i32 before;
    vox_i32 reaction_before;
    vox_u16 i;
    vox_u16 j;
    vox_fluid_init(&first);
    vox_fluid_init(&second);
    if (first.abi_version != VOX_ABI_VERSION ||
        vox_fluid_add(&first, 7U, 15U, VOX_FLUID_WATER,
                      50000L, 20L << 16) != VOX_OK ||
        vox_fluid_add(&first, 7U, 15U, VOX_FLUID_WATER,
                      10000L, 20L << 16) != VOX_OK) {
        fprintf(stderr, "fluid setup failed\n");
        return 1;
    }
    before = vox_fluid_conserved_volume(&first);
    if (before != total(&first)) {
        fprintf(stderr, "fluid accounting failed\n");
        return 2;
    }
    for (i = 0U; i < 32U; ++i) {
        if (vox_fluid_step(&first, 256U) != VOX_OK) {
            return 3;
        }
    }
    if (vox_fluid_conserved_volume(&first) != before ||
        vox_fluid_cell_get(&first, 7U, 47U) == 0) {
        fprintf(stderr, "fluid did not conserve or fall\n");
        return 4;
    }
    if (vox_fluid_add(&first, 7U, 47U, VOX_FLUID_LAVA, 1L,
                      600L << 16) != VOX_ERR_COLLISION) {
        fprintf(stderr, "fluid material mixing was not rejected\n");
        return 5;
    }
    if (vox_fluid_set(&first, 7U, 47U, VOX_FLUID_WATER, 40000L,
                      20L << 16) != VOX_OK) {
        return 6;
    }
    second = first;
    for (i = 0U; i < 32U; ++i) {
        (void)vox_fluid_step(&first, 256U);
        (void)vox_fluid_step(&second, 256U);
    }
    /* Same inputs, same bounded order, same result. */
    for (j = 0U; j < VOX_FLUID_MAX_CELLS; ++j) {
        if (first.cells[j].material != second.cells[j].material ||
            first.cells[j].volume_q16 != second.cells[j].volume_q16 ||
            first.cells[j].flow_q16 != second.cells[j].flow_q16) {
            return 7;
        }
    }
    if (vox_fluid_conserved_volume(&second) !=
            vox_fluid_conserved_volume(&first) || second.tick != 64U) {
        return 8;
    }
    /* Coordinates are global: distant cells must not alias through a tile. */
    if (vox_fluid_add_at(&first, 400U, 100U, 3U, VOX_FLUID_BLOOD,
                         1200L, 18L << 16) != VOX_OK ||
        vox_fluid_cell_get_at(&first, 400U, 100U, 3U) == 0) return 9;
    /* A solid terrain cell blocks downward transfer but does not destroy mass. */
    vox_world_init(&terrain);
    if (vox_world_set(&terrain, 20U, 3U, 2U, VOX_MAT_STONE,
                      20L << 16) != VOX_OK ||
        vox_fluid_set_at(&second, 20U, 2U, 2U, VOX_FLUID_WATER,
                         60000L, 20L << 16) != VOX_OK) return 10;
    before = vox_fluid_conserved_volume(&second);
    if (vox_fluid_step_terrain(&second, &terrain, 1U) != VOX_OK ||
        vox_fluid_conserved_volume(&second) != before ||
        vox_fluid_cell_get_at(&second, 20U, 3U, 2U) != 0) return 11;
    /* Canonical coordinate ordering makes insertion order irrelevant. */
    vox_fluid_init(&ordered_a);
    vox_fluid_init(&ordered_b);
    if (vox_fluid_add_at(&ordered_a, 31U, 8U, 1U, VOX_FLUID_WATER,
                         30000L, 20L << 16) != VOX_OK ||
        vox_fluid_add_at(&ordered_a, 30U, 8U, 1U, VOX_FLUID_WATER,
                         10000L, 20L << 16) != VOX_OK ||
        vox_fluid_add_at(&ordered_b, 30U, 8U, 1U, VOX_FLUID_WATER,
                         10000L, 20L << 16) != VOX_OK ||
        vox_fluid_add_at(&ordered_b, 31U, 8U, 1U, VOX_FLUID_WATER,
                         30000L, 20L << 16) != VOX_OK) return 12;
    for (i = 0U; i < 8U; ++i) {
        (void)vox_fluid_step(&ordered_a, 64U);
        (void)vox_fluid_step(&ordered_b, 64U);
    }
    if (vox_fluid_hash(&ordered_a) != vox_fluid_hash(&ordered_b)) return 13;
    /* Depth is part of the authoritative coordinate, not a display alias.
     * With the floor blocked, the stable lateral proposal must carry water
     * from z=2 to z=1 without changing total volume. */
    vox_fluid_init(&depth_flow);
    if (vox_world_set(&terrain, 200U, 101U, 2U, VOX_MAT_STONE,
                      20L << 16) != VOX_OK ||
        vox_fluid_add_at(&depth_flow, 200U, 100U, 2U, VOX_FLUID_WATER,
                         60000L, 20L << 16) != VOX_OK) return 16;
    before = vox_fluid_conserved_volume(&depth_flow);
    if (vox_fluid_step_terrain(&depth_flow, &terrain, 1U) != VOX_OK ||
        vox_fluid_conserved_volume(&depth_flow) != before ||
        vox_fluid_cell_get_at(&depth_flow, 200U, 100U, 1U) == 0) return 17;
    /* Reactions are explicit losses, never an unexplained conservation leak. */
    vox_fluid_init(&ordered_a);
    if (vox_fluid_add_at(&ordered_a, 40U, 2U, 0U, VOX_FLUID_WATER,
                         20000L, 20L << 16) != VOX_OK ||
        vox_fluid_add_at(&ordered_a, 40U, 3U, 0U, VOX_FLUID_LAVA,
                         20000L, 700L << 16) != VOX_OK) return 14;
    reaction_before = vox_fluid_conserved_volume(&ordered_a);
    (void)vox_fluid_step(&ordered_a, 64U);
    if (vox_fluid_reaction_loss(&ordered_a) <= 0L ||
        reaction_before - vox_fluid_conserved_volume(&ordered_a) !=
            vox_fluid_reaction_loss(&ordered_a)) return 15;
    /* A blocked floor with one open lateral neighbour must equalize by the
     * fixed pressure proposal, rather than retaining a single-cell puddle. */
    vox_world_init(&pressure_terrain);
    for (j = 0U; j < 2U; ++j) {
        for (i = 0U; i < 5U; ++i) {
            if (vox_world_set(&pressure_terrain, i, 10U, j,
                              VOX_MAT_AIR, 0L) != VOX_OK ||
                vox_world_set(&pressure_terrain, i, 11U, j,
                              VOX_MAT_AIR, 0L) != VOX_OK) return 18;
        }
    }
    if (vox_world_set(&pressure_terrain, 1U, 11U, 0U, VOX_MAT_STONE,
                      20L << 16) != VOX_OK ||
        vox_world_set(&pressure_terrain, 0U, 10U, 0U, VOX_MAT_STONE,
                      20L << 16) != VOX_OK ||
        vox_world_set(&pressure_terrain, 1U, 10U, 1U, VOX_MAT_STONE,
                      20L << 16) != VOX_OK) return 19;
    vox_fluid_init(&pressure_flow);
    if (vox_fluid_add_at(&pressure_flow, 1U, 10U, 0U, VOX_FLUID_WATER,
                         VOX_FLUID_CELL_CAPACITY_Q16, 20L << 16) != VOX_OK ||
        vox_fluid_step_terrain(&pressure_flow, &pressure_terrain, 1U) !=
        VOX_OK ||
        vox_fluid_cell_get_at(&pressure_flow, 1U, 10U, 0U) == 0 ||
        vox_fluid_cell_get_at(&pressure_flow, 2U, 10U, 0U) == 0 ||
        vox_fluid_cell_get_at(&pressure_flow, 1U, 10U, 0U)->volume_q16 !=
            49152L ||
        vox_fluid_cell_get_at(&pressure_flow, 2U, 10U, 0U)->volume_q16 !=
            16384L ||
        vox_fluid_conserved_volume(&pressure_flow) !=
            VOX_FLUID_CELL_CAPACITY_Q16) return 20;
    /* Fill a closed dammed basin with ten cells of water.  A lake must spread
     * across the floor while preserving every unit of authoritative volume. */
    vox_world_init(&basin_terrain);
    for (j = 0U; j < VOX_WORLD_DEPTH; ++j) {
        for (i = 100U; i <= 110U; ++i) {
            vox_u16 row;
            for (row = 68U; row <= 90U; ++row) {
                if (vox_world_set(&basin_terrain, i, row, j,
                                  VOX_MAT_AIR, 0L) != VOX_OK) return 21;
            }
            if (vox_world_set(&basin_terrain, i, 90U, j,
                              VOX_MAT_STONE, 20L << 16) != VOX_OK) return 22;
        }
        for (i = 68U; i <= 90U; ++i) {
            if (vox_world_set(&basin_terrain, 100U, i, j,
                              VOX_MAT_STONE, 20L << 16) != VOX_OK ||
                vox_world_set(&basin_terrain, 110U, i, j,
                              VOX_MAT_STONE, 20L << 16) != VOX_OK) return 23;
        }
    }
    vox_fluid_init(&lake_flow);
    for (i = 0U; i < 10U; ++i) {
        if (vox_fluid_add_at(&lake_flow, 105U, (vox_u16)(70U + i), 0U,
                             VOX_FLUID_WATER,
                             VOX_FLUID_CELL_CAPACITY_Q16,
                             20L << 16) != VOX_OK) return 24;
    }
    before = vox_fluid_conserved_volume(&lake_flow);
    for (i = 0U; i < 120U; ++i) {
        if (vox_fluid_step_terrain(&lake_flow, &basin_terrain, 256U) !=
            VOX_OK) return 25;
    }
    if (vox_fluid_conserved_volume(&lake_flow) != before ||
        vox_fluid_cell_get_at(&lake_flow, 102U, 89U, 0U) == 0 ||
        vox_fluid_cell_get_at(&lake_flow, 108U, 89U, 0U) == 0) return 26;
    /* Open a one-cell spillway at the bottom of a dam. Water and lava both
     * have to leave the basin through it; neither may vanish or rise into a
     * presentation-only ceiling path. */
    vox_world_init(&spill_terrain);
    for (j = 0U; j < VOX_WORLD_DEPTH; ++j) {
        for (i = 100U; i <= 118U; ++i) {
            vox_u16 row;
            for (row = 68U; row <= 90U; ++row) {
                if (vox_world_set(&spill_terrain, i, row, j,
                                  VOX_MAT_AIR, 0L) != VOX_OK) return 27;
            }
            if (vox_world_set(&spill_terrain, i, 90U, j,
                              VOX_MAT_STONE, 20L << 16) != VOX_OK) return 28;
        }
        for (i = 68U; i <= 90U; ++i) {
            if (vox_world_set(&spill_terrain, 100U, i, j,
                              VOX_MAT_STONE, 20L << 16) != VOX_OK) return 29;
            if (i != 89U &&
                vox_world_set(&spill_terrain, 110U, i, j,
                              VOX_MAT_STONE, 20L << 16) != VOX_OK) return 30;
        }
    }
    vox_fluid_init(&spill_flow);
    vox_fluid_init(&lava_spill);
    for (i = 0U; i < 10U; ++i) {
        if (vox_fluid_add_at(&spill_flow, 105U, (vox_u16)(70U + i), 0U,
                             VOX_FLUID_WATER,
                             VOX_FLUID_CELL_CAPACITY_Q16,
                             20L << 16) != VOX_OK ||
            vox_fluid_add_at(&lava_spill, 105U, (vox_u16)(70U + i), 0U,
                             VOX_FLUID_LAVA,
                             VOX_FLUID_CELL_CAPACITY_Q16,
                             700L << 16) != VOX_OK) return 31;
    }
    before = vox_fluid_conserved_volume(&spill_flow);
    reaction_before = vox_fluid_conserved_volume(&lava_spill);
    for (i = 0U; i < 160U; ++i) {
        if (vox_fluid_step_terrain(&spill_flow, &spill_terrain, 256U) !=
            VOX_OK ||
            vox_fluid_step_terrain(&lava_spill, &spill_terrain, 256U) !=
            VOX_OK) return 32;
    }
    if (vox_fluid_conserved_volume(&spill_flow) != before ||
        vox_fluid_conserved_volume(&lava_spill) != reaction_before ||
        vox_fluid_cell_get_at(&spill_flow, 112U, 89U, 0U) == 0 ||
        vox_fluid_cell_get_at(&lava_spill, 112U, 89U, 0U) == 0) return 33;
    /* Blood is not a particle-only special case: it obeys the same pooling
     * and mass rule in a closed basin. */
    vox_fluid_init(&blood_pool);
    for (i = 0U; i < 4U; ++i) {
        if (vox_fluid_add_at(&blood_pool, 105U, (vox_u16)(74U + i), 0U,
                             VOX_FLUID_BLOOD,
                             VOX_FLUID_CELL_CAPACITY_Q16,
                             37L << 16) != VOX_OK) return 34;
    }
    before = vox_fluid_conserved_volume(&blood_pool);
    for (i = 0U; i < 100U; ++i) {
        if (vox_fluid_step_terrain(&blood_pool, &basin_terrain, 256U) !=
            VOX_OK) return 35;
    }
    if (vox_fluid_conserved_volume(&blood_pool) != before ||
        (vox_fluid_cell_get_at(&blood_pool, 103U, 89U, 0U) == 0 &&
         vox_fluid_cell_get_at(&blood_pool, 107U, 89U, 0U) == 0)) return 36;
    printf("fluid conservation and deterministic flow passed\n");
    return 0;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_kernel.h"

static int run_scenario(vox_u32 *hash_out)
{
    vox_world world;
    vox_step_command command;
    vox_u32 i;
    vox_world_init(&world);
    command.abi_version = VOX_ABI_VERSION;
    command.struct_size = (vox_u32)sizeof(command);
    command.x = 5U;
    command.y = 7U;
    command.z = 0U;
    command.material = VOX_MAT_STONE;
    command.temperature_delta_q8 = 0;
    for (i = 0U; i < 20U; ++i) {
        if (vox_world_step(&world, &command) != VOX_OK) {
            return 1;
        }
        command.x = (command.x + 3U) % VOX_WORLD_WIDTH;
        command.y = (command.y + 5U) % VOX_WORLD_HEIGHT;
        command.z = (command.z + 1U) % VOX_WORLD_DEPTH;
        command.material = (i % 2U) == 0U ? VOX_MAT_WATER : VOX_MAT_LAVA;
        command.temperature_delta_q8 = (vox_i16)(i * 4U);
    }
    *hash_out = vox_world_hash(&world);
    return 0;
}

static int test_materials_and_sleep(void)
{
    vox_world world;
    const vox_material_properties *lava;
    const vox_material_properties *bedrock;
    const vox_cell *water;
    vox_world_init(&world);
    lava = vox_material_get(VOX_MAT_LAVA);
    bedrock = vox_material_get(VOX_MAT_BEDROCK);
    if (lava == 0 || bedrock == 0 ||
        !(lava->flags & VOX_MATERIAL_EMISSIVE) || bedrock->strength == 0U) {
        return 1;
    }
    if (vox_world_set(&world, 1U, 1U, 1U, VOX_MAT_STONE, 20L << 16) != VOX_OK) {
        return 2;
    }
    if (world.occupied_cells != 1U || world.awake_cells != 1U) {
        return 3;
    }
    if (vox_world_step(&world, 0) != VOX_OK || world.awake_cells != 0U) {
        return 4;
    }
    if (vox_world_set(&world, 2U, 2U, 2U, VOX_MAT_WATER, 120L << 16) != VOX_OK) {
        return 5;
    }
    if (vox_world_step(&world, 0) != VOX_OK) {
        return 6;
    }
    water = vox_world_cell(&world, 2U, 2U, 2U);
    if (water == 0 || !(water->flags & VOX_CELL_PHASE_GAS)) {
        return 7;
    }
    return 0;
}

int main(void)
{
    vox_u32 first;
    vox_u32 second;
    if (test_materials_and_sleep() != 0) {
        fprintf(stderr, "material/sleep scenario failed\n");
        return 3;
    }
    if (run_scenario(&first) != 0 || run_scenario(&second) != 0) {
        return 1;
    }
    if (first != second) {
        fprintf(stderr, "determinism failure: %08x != %08x\n",
                (unsigned int)first, (unsigned int)second);
        return 2;
    }
    printf("deterministic hash=%08x\n", (unsigned int)first);
    return 0;
}

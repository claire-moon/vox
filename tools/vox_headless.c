/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_kernel.h"

int main(void)
{
    static vox_world world;
    vox_step_command command;
    vox_u32 i;
    vox_world_init(&world);
    command.abi_version = VOX_ABI_VERSION;
    command.struct_size = (vox_u32)sizeof(command);
    command.x = 12U;
    command.y = 18U;
    command.z = 1U;
    command.material = VOX_MAT_STONE;
    command.temperature_delta_q8 = 0;
    for (i = 0U; i < 60U; ++i) {
        if (vox_world_step(&world, &command) != VOX_OK) {
            return 2;
        }
        command.material = VOX_MAT_AIR;
    }
    printf("VOX headless tick=%u occupied=%u awake=%u hash=%08x\n",
           (unsigned int)world.tick,
           (unsigned int)world.occupied_cells,
           (unsigned int)world.awake_cells,
           (unsigned int)vox_world_hash(&world));
    return 0;
}

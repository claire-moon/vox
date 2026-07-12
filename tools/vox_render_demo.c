/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_game.h"
#include "vox/vox_render.h"

#define VOX_DEMO_WIDTH 320U
#define VOX_DEMO_HEIGHT 240U

static vox_u8 vox_demo_pixels[VOX_DEMO_WIDTH * VOX_DEMO_HEIGHT *
                              VOX_SOFTWARE_RGB_BYTES];

static void vox_demo_world(vox_world *world)
{
    vox_u32 x;
    vox_u32 y;
    if (vox_digs_generate_map(world, VOX_DIGS_MAP_FURNACE_YARD,
                              0xD1655EEDU) != VOX_OK) {
        vox_world_init(world);
        return;
    }
    for (x = 18U; x < 43U; ++x) {
        for (y = 51U; y < 54U; ++y) {
            (void)vox_world_set(world, x, y, VOX_WORLD_DEPTH - 1U,
                                VOX_MAT_WATER, 20L << 16);
        }
    }
    for (x = 78U; x < 106U; ++x) {
        for (y = 54U; y < 59U; ++y) {
            (void)vox_world_set(world, x, y, VOX_WORLD_DEPTH - 1U,
                                VOX_MAT_LAVA, 700L << 16);
        }
    }
    for (x = 82U; x < 102U; ++x) {
        (void)vox_world_set(world, x, 50U, VOX_WORLD_DEPTH - 1U,
                            VOX_MAT_SMOKE, 40L << 16);
    }
    (void)vox_world_blast(world, 91U, 55U, 0U, 4U, 700L << 16);
    for (x = 0U; x < 4U; ++x) {
        (void)vox_world_step(world, 0);
    }
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/tmp/vox-demo.ppm";
    static vox_world world;
    vox_software_target target;
    FILE *file;
    vox_demo_world(&world);
    target.abi_version = VOX_ABI_VERSION;
    target.struct_size = (vox_u32)sizeof(target);
    target.width = VOX_DEMO_WIDTH;
    target.height = VOX_DEMO_HEIGHT;
    target.stride = VOX_DEMO_WIDTH * VOX_SOFTWARE_RGB_BYTES;
    target.pixels = vox_demo_pixels;
    if (vox_software_render(&world, &target) != VOX_OK) {
        return 2;
    }
    file = fopen(path, "wb");
    if (file == 0) {
        return 3;
    }
    if (fprintf(file, "P6\n%u %u\n255\n", (unsigned int)target.width,
                (unsigned int)target.height) < 0 ||
        fwrite(target.pixels, 1U,
               (size_t)(target.height * target.stride), file) !=
            (size_t)(target.height * target.stride)) {
        (void)fclose(file);
        return 4;
    }
    (void)fclose(file);
    printf("VOX software frame hash=%08x path=%s\n",
           (unsigned int)vox_software_hash(&target), path);
    return 0;
}

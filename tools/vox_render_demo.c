/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_render.h"

#define VOX_DEMO_WIDTH 320U
#define VOX_DEMO_HEIGHT 240U

static vox_u8 vox_demo_pixels[VOX_DEMO_WIDTH * VOX_DEMO_HEIGHT *
                              VOX_SOFTWARE_RGB_BYTES];

static void vox_demo_world(vox_world *world)
{
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    vox_world_init(world);
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            for (y = VOX_WORLD_HEIGHT - 5U; y < VOX_WORLD_HEIGHT; ++y) {
                vox_u16 material = y == VOX_WORLD_HEIGHT - 1U ?
                    VOX_MAT_BEDROCK : VOX_MAT_STONE;
                (void)vox_world_set(world, x, y, z, material, 20L << 16);
            }
        }
    }
    for (x = 2U; x < VOX_WORLD_WIDTH - 2U; ++x) {
        vox_u32 hill = VOX_WORLD_HEIGHT - 6U - ((x * 5U) % 7U);
        for (y = hill; y < VOX_WORLD_HEIGHT - 4U; ++y) {
            vox_u16 material = y == hill ? VOX_MAT_BIOMASS : VOX_MAT_SOIL;
            (void)vox_world_set(world, x, y, 2U, material, 20L << 16);
        }
    }
    for (x = 5U; x < 12U; ++x) {
        (void)vox_world_set(world, x, VOX_WORLD_HEIGHT - 6U, 3U,
                            VOX_MAT_WATER, 20L << 16);
    }
    for (x = 20U; x < 27U; ++x) {
        (void)vox_world_set(world, x, VOX_WORLD_HEIGHT - 6U, 3U,
                            VOX_MAT_LAVA, 700L << 16);
    }
    for (x = 14U; x < 19U; ++x) {
        (void)vox_world_set(world, x, VOX_WORLD_HEIGHT - 8U, 1U,
                            VOX_MAT_COAL, 20L << 16);
    }
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/tmp/vox-demo.ppm";
    vox_world world;
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

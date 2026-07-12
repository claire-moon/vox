/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_render.h"

#define TEST_WIDTH 64U
#define TEST_HEIGHT 48U

static vox_u8 pixels_a[TEST_WIDTH * TEST_HEIGHT * VOX_SOFTWARE_RGB_BYTES];
static vox_u8 pixels_b[TEST_WIDTH * TEST_HEIGHT * VOX_SOFTWARE_RGB_BYTES];

static int render_scene(vox_u8 *pixels, vox_u16 gi_quality,
                        vox_u32 *hash_out)
{
    static vox_world world;
    vox_software_target target;
    vox_software_config config;
    vox_u32 x;
    vox_world_init(&world);
    for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
        if (vox_world_set(&world, x, VOX_WORLD_HEIGHT - 1U, 0U,
                          VOX_MAT_BEDROCK, 20L << 16) != VOX_OK ||
            vox_world_set(&world, x, VOX_WORLD_HEIGHT - 2U, 2U,
                          VOX_MAT_SOIL, 20L << 16) != VOX_OK) {
            return 1;
        }
    }
    if (vox_world_set(&world, VOX_WORLD_WIDTH / 2U,
                      VOX_WORLD_HEIGHT - 3U, 3U,
                      VOX_MAT_LAVA, 700L << 16) != VOX_OK) {
        return 2;
    }
    target.abi_version = VOX_ABI_VERSION;
    target.struct_size = (vox_u32)sizeof(target);
    target.width = TEST_WIDTH;
    target.height = TEST_HEIGHT;
    target.stride = TEST_WIDTH * VOX_SOFTWARE_RGB_BYTES;
    target.pixels = pixels;
    vox_software_config_default(&config);
    config.gi_quality = gi_quality;
    if (vox_software_render_ex(&world, &target, &config) != VOX_OK) {
        return 3;
    }
    *hash_out = vox_software_hash(&target);
    return 0;
}

int main(void)
{
    vox_u32 first;
    vox_u32 second;
    vox_u32 compatibility;
    vox_u32 showcase;
    if (render_scene(pixels_a, VOX_GI_BALANCED, &first) != 0 ||
        render_scene(pixels_b, VOX_GI_BALANCED, &second) != 0 ||
        first != second) {
        fprintf(stderr, "software renderer determinism failure\n");
        return 1;
    }
    if (first == 0U) {
        fprintf(stderr, "software renderer produced an empty hash\n");
        return 2;
    }
    if (render_scene(pixels_a, VOX_GI_COMPATIBILITY, &compatibility) != 0 ||
        render_scene(pixels_b, VOX_GI_SHOWCASE, &showcase) != 0 ||
        compatibility == first || showcase == first ||
        compatibility == showcase) {
        fprintf(stderr, "software GI tiers did not produce distinct frames\n");
        return 3;
    }
    printf("software renderer hash=%08x\n", (unsigned int)first);
    return 0;
}

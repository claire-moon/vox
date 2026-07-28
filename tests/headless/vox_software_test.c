/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include "vox/vox_render.h"

#define TEST_WIDTH 256U
#define TEST_HEIGHT 160U

static vox_u8 pixels_a[TEST_WIDTH * TEST_HEIGHT * VOX_SOFTWARE_RGB_BYTES];
static vox_u8 pixels_b[TEST_WIDTH * TEST_HEIGHT * VOX_SOFTWARE_RGB_BYTES];
static vox_u8 pixels_view[TEST_WIDTH * TEST_HEIGHT *
                          VOX_SOFTWARE_RGB_BYTES];
static vox_u8 pixels_native[VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT *
                            VOX_SOFTWARE_RGB_BYTES];
static vox_world region_world;

static int render_scene(vox_u8 *pixels, vox_u16 gi_quality,
                        vox_u32 *hash_out)
{
    static vox_world world;
    vox_software_target target;
    vox_software_config config;
    vox_u32 x;
    vox_u32 y;
    vox_world_init(&world);
    for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
        if (vox_world_set(&world, x, VOX_WORLD_HEIGHT - 1U, 0U,
                          VOX_MAT_BEDROCK, 20L << 16) != VOX_OK ||
            vox_world_set(&world, x, VOX_WORLD_HEIGHT - 2U, 2U,
                          VOX_MAT_SOIL, 20L << 16) != VOX_OK) {
            return 1;
        }
    }
    for (y = VOX_WORLD_HEIGHT - 12U; y < VOX_WORLD_HEIGHT - 4U; ++y) {
        for (x = VOX_WORLD_WIDTH / 2U - 4U;
             x < VOX_WORLD_WIDTH / 2U + 4U; ++x) {
            if (vox_world_set(&world, x, y, VOX_WORLD_DEPTH - 1U,
                              VOX_MAT_LAVA, 700L << 16) != VOX_OK) {
                return 2;
            }
        }
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
    vox_software_target region_target;
    vox_software_config region_config;
    vox_software_view full_view;
    vox_software_view crop_view;
    vox_u32 full_view_hash;
    vox_u32 crop_hash;
    vox_software_target native_target;
    vox_u32 row;
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
    vox_world_init(&region_world);
    if (vox_world_set(&region_world, VOX_WORLD_WIDTH / 2U,
                      VOX_WORLD_HEIGHT / 2U, VOX_WORLD_DEPTH - 1U,
                      VOX_MAT_LAVA, 700L << 16) != VOX_OK) {
        return 4;
    }
    region_target.abi_version = VOX_ABI_VERSION;
    region_target.struct_size = (vox_u32)sizeof(region_target);
    region_target.width = TEST_WIDTH;
    region_target.height = TEST_HEIGHT;
    region_target.stride = TEST_WIDTH * VOX_SOFTWARE_RGB_BYTES;
    region_target.pixels = pixels_a;
    vox_software_config_default(&region_config);
    vox_software_view_full(&full_view);
    if (vox_software_render_ex(&region_world, &region_target,
                               &region_config) != VOX_OK) {
        return 5;
    }
    full_view_hash = vox_software_hash(&region_target);
    region_target.pixels = pixels_b;
    if (vox_software_render_view_ex(&region_world, &region_target,
                                    &region_config, &full_view) != VOX_OK ||
        vox_software_hash(&region_target) != full_view_hash) {
        fprintf(stderr, "full software view changed compatibility output\n");
        return 6;
    }
    crop_view = full_view;
    crop_view.origin_x_q16 = (vox_i32)(VOX_WORLD_WIDTH << 14);
    crop_view.origin_y_q16 = (vox_i32)(VOX_WORLD_HEIGHT << 14);
    crop_view.width_q16 = (vox_i32)(VOX_WORLD_WIDTH << 15);
    crop_view.height_q16 = (vox_i32)(VOX_WORLD_HEIGHT << 15);
    region_target.pixels = pixels_view;
    if (vox_software_render_view_ex(&region_world, &region_target,
                                    &region_config, &crop_view) != VOX_OK) {
        fprintf(stderr, "software crop view failed\n");
        return 7;
    }
    crop_hash = vox_software_hash(&region_target);
    if (crop_hash == 0U || crop_hash == full_view_hash ||
        vox_software_render_view_ex(&region_world, &region_target,
                                    &region_config, &crop_view) != VOX_OK ||
        vox_software_hash(&region_target) != crop_hash) {
        fprintf(stderr, "software crop view is not deterministic\n");
        return 8;
    }
    native_target = region_target;
    native_target.width = VOX_WORLD_WIDTH;
    native_target.height = VOX_WORLD_HEIGHT;
    native_target.stride = VOX_WORLD_WIDTH * VOX_SOFTWARE_RGB_BYTES;
    native_target.pixels = pixels_native;
    if (vox_software_render_ex(&region_world, &native_target,
                               &region_config) != VOX_OK) {
        return 9;
    }
    for (row = 0U; row < TEST_HEIGHT; ++row) {
        const vox_u8 *native_row = pixels_native +
            ((row + VOX_WORLD_HEIGHT / 4U) * VOX_WORLD_WIDTH +
             VOX_WORLD_WIDTH / 4U) * VOX_SOFTWARE_RGB_BYTES;
        const vox_u8 *view_row = pixels_view +
            row * TEST_WIDTH * VOX_SOFTWARE_RGB_BYTES;
        if (memcmp(native_row, view_row,
                   TEST_WIDTH * VOX_SOFTWARE_RGB_BYTES) != 0) {
            fprintf(stderr, "software crop differs from full GI oracle\n");
            return 10;
        }
    }
    if (vox_world_set(&region_world, 0U, 0U, VOX_WORLD_DEPTH - 1U,
                      VOX_MAT_LAVA, 700L << 16) != VOX_OK ||
        vox_software_render_view_ex(&region_world, &region_target,
                                    &region_config, &crop_view) != VOX_OK ||
        vox_software_hash(&region_target) != crop_hash) {
        fprintf(stderr, "off-camera cell leaked into software view\n");
        return 11;
    }
    crop_view.origin_x_q16 = -1L;
    if (vox_software_render_view_ex(&region_world, &region_target,
                                    &region_config, &crop_view) !=
        VOX_ERR_INVALID) {
        fprintf(stderr, "invalid software view was accepted\n");
        return 12;
    }
    printf("software renderer hash=%08x\n", (unsigned int)first);
    return 0;
}

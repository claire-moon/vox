/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_render.h"

typedef struct vox_rgb {
    vox_u8 red;
    vox_u8 green;
    vox_u8 blue;
} vox_rgb;

static const vox_rgb vox_palette[VOX_MAT_COUNT] = {
    {20U, 28U, 48U},
    {36U, 30U, 35U},
    {108U, 108U, 116U},
    {112U, 72U, 45U},
    {46U, 44U, 48U},
    {74U, 126U, 64U},
    {176U, 150U, 82U},
    {58U, 117U, 196U},
    {236U, 84U, 22U},
    {148U, 155U, 166U},
    {165U, 86U, 69U},
    {184U, 44U, 39U},
    {104U, 104U, 118U},
    {150U, 125U, 71U}
};

static vox_u8 vox_clamp_u8(vox_u32 value)
{
    if (value > 255U) {
        return 255U;
    }
    return (vox_u8)value;
}

static vox_u32 vox_lava_glow(const vox_world *world, vox_u32 center_x,
                              vox_u32 center_y)
{
    int delta_x;
    int delta_y;
    vox_u32 depth;
    vox_u32 glow = 0U;
    for (delta_y = -3; delta_y <= 3; ++delta_y) {
        long sample_y = (long)center_y + (long)delta_y;
        if (sample_y < 0L || sample_y >= (long)VOX_WORLD_HEIGHT) {
            continue;
        }
        for (delta_x = -3; delta_x <= 3; ++delta_x) {
            long sample_x = (long)center_x + (long)delta_x;
            vox_u32 distance;
            if (sample_x < 0L || sample_x >= (long)VOX_WORLD_WIDTH) {
                continue;
            }
            distance = (vox_u32)(delta_x < 0 ? -delta_x : delta_x) +
                       (vox_u32)(delta_y < 0 ? -delta_y : delta_y) + 1U;
            for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
                const vox_cell *cell = vox_world_cell(world, (vox_u32)sample_x,
                                                       (vox_u32)sample_y, depth);
                if (cell != 0 && cell->material == VOX_MAT_LAVA) {
                    glow += 96U / distance;
                }
            }
        }
    }
    return glow;
}

static const vox_cell *vox_surface_cell(const vox_world *world, vox_u32 x,
                                        vox_u32 y)
{
    vox_u32 depth;
    for (depth = VOX_WORLD_DEPTH; depth > 0U; --depth) {
        const vox_cell *cell = vox_world_cell(world, x, y, depth - 1U);
        if (cell != 0 && cell->material != VOX_MAT_AIR) {
            return cell;
        }
    }
    return 0;
}

vox_result vox_software_render(const vox_world *world,
                               vox_software_target *target)
{
    vox_u32 pixel_y;
    vox_u32 pixel_x;
    if (world == 0 || target == 0 || target->pixels == 0 ||
        target->abi_version != VOX_ABI_VERSION ||
        target->struct_size < (vox_u32)sizeof(*target) ||
        target->width == 0U || target->height == 0U ||
        target->stride < target->width * VOX_SOFTWARE_RGB_BYTES) {
        return VOX_ERR_INVALID;
    }
    for (pixel_y = 0U; pixel_y < target->height; ++pixel_y) {
        vox_u32 world_y = pixel_y * VOX_WORLD_HEIGHT / target->height;
        for (pixel_x = 0U; pixel_x < target->width; ++pixel_x) {
            vox_u32 world_x = pixel_x * VOX_WORLD_WIDTH / target->width;
            const vox_cell *cell = vox_surface_cell(world, world_x, world_y);
            vox_u8 *destination = target->pixels +
                pixel_y * target->stride + pixel_x * VOX_SOFTWARE_RGB_BYTES;
            if (cell == 0) {
                destination[0] = (vox_u8)(18U + world_y * 20U / VOX_WORLD_HEIGHT);
                destination[1] = (vox_u8)(26U + world_y * 24U / VOX_WORLD_HEIGHT);
                destination[2] = (vox_u8)(44U + world_y * 30U / VOX_WORLD_HEIGHT);
            } else {
                const vox_rgb *base = &vox_palette[cell->material];
                const vox_material_properties *properties =
                    vox_material_get(cell->material);
                vox_u32 light = 92U + world_y * 80U / VOX_WORLD_HEIGHT;
                light += vox_lava_glow(world, world_x, world_y);
                if (properties != 0 &&
                    (properties->flags & VOX_MATERIAL_EMISSIVE)) {
                    light += 96U;
                }
                if (cell->temperature_q16 > (200L << 16)) {
                    light += (vox_u32)(cell->temperature_q16 >> 18);
                }
                destination[0] = vox_clamp_u8((vox_u32)base->red * light / 255U);
                destination[1] = vox_clamp_u8((vox_u32)base->green * light / 255U);
                destination[2] = vox_clamp_u8((vox_u32)base->blue * light / 255U);
            }
        }
    }
    return VOX_OK;
}

vox_u32 vox_software_hash(const vox_software_target *target)
{
    vox_u32 hash = 2166136261U;
    vox_u32 pixel_y;
    vox_u32 pixel_x;
    if (target == 0 || target->pixels == 0) {
        return 0U;
    }
    for (pixel_y = 0U; pixel_y < target->height; ++pixel_y) {
        for (pixel_x = 0U; pixel_x < target->width * VOX_SOFTWARE_RGB_BYTES;
             ++pixel_x) {
            hash ^= (vox_u32)target->pixels[pixel_y * target->stride + pixel_x];
            hash *= 16777619U;
        }
    }
    return hash;
}

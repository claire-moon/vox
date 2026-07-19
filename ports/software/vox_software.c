/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_render.h"

typedef struct vox_rgb {
    vox_u8 red;
    vox_u8 green;
    vox_u8 blue;
} vox_rgb;

typedef struct vox_light_rgb {
    vox_u16 red;
    vox_u16 green;
    vox_u16 blue;
} vox_light_rgb;

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

static vox_light_rgb vox_light_a[VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT];
static vox_light_rgb vox_light_b[VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT];
static vox_u8 vox_light_solid[VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT];
/* Reused by the raster pass after vox_build_lightfield scans each column. */
static vox_u16 vox_surface_material[VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT];

static vox_u8 vox_clamp_u8(vox_u32 value)
{
    if (value > 255U) {
        return 255U;
    }
    return (vox_u8)value;
}

static vox_u16 vox_light_max(vox_u16 left, vox_u16 right)
{
    return left > right ? left : right;
}

static vox_u16 vox_light_decay(vox_u16 value, vox_u16 attenuation)
{
    return value > attenuation ? (vox_u16)(value - attenuation) : 0U;
}

static vox_u16 vox_scan_column(const vox_world *world, vox_u32 x, vox_u32 y,
                               vox_light_rgb *emission)
{
    vox_u32 depth;
    vox_u16 surface_material = VOX_MAT_AIR;
    emission->red = 0U;
    emission->green = 0U;
    emission->blue = 0U;
    for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
        const vox_cell *cell = vox_world_cell(world, x, y, depth);
        if (cell == 0 || cell->material == VOX_MAT_AIR) {
            continue;
        }
        surface_material = cell->material;
        if (cell->material == VOX_MAT_LAVA) {
            emission->red = 255U;
            emission->green = vox_light_max(emission->green, 150U);
            emission->blue = vox_light_max(emission->blue, 48U);
        } else if (cell->temperature_q16 > (300L << 16)) {
            vox_u16 heat = (vox_u16)(cell->temperature_q16 >> 19);
            emission->red = vox_light_max(emission->red,
                                          heat > 255U ? 255U : heat);
            emission->green = vox_light_max(emission->green,
                                            heat > 160U ? 160U : heat);
            emission->blue = vox_light_max(emission->blue, 36U);
        }
    }
    return surface_material;
}

static vox_light_rgb *vox_build_lightfield(const vox_world *world,
                                            vox_u16 gi_quality)
{
    vox_light_rgb *source = vox_light_a;
    vox_light_rgb *destination = vox_light_b;
    vox_u32 passes = gi_quality == VOX_GI_COMPATIBILITY ? 1U :
                     (gi_quality == VOX_GI_BALANCED ? 3U : 5U);
    vox_u32 x;
    vox_u32 y;
    vox_u32 pass;
    for (y = 0U; y < VOX_WORLD_HEIGHT; ++y) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            vox_u32 index = y * VOX_WORLD_WIDTH + x;
            vox_light_rgb emission;
            vox_u16 surface_material = vox_scan_column(world, x, y,
                                                        &emission);
            vox_u16 sky = (vox_u16)(118U - y * 36U / VOX_WORLD_HEIGHT);
            if (surface_material != VOX_MAT_AIR) {
                sky = (vox_u16)(sky * 3U / 5U);
            }
            vox_surface_material[index] = surface_material;
            vox_light_solid[index] = surface_material != VOX_MAT_AIR ?
                                     1U : 0U;
            source[index].red = sky;
            source[index].green = (vox_u16)(sky + 4U);
            source[index].blue = (vox_u16)(sky + 12U);
            source[index].red = vox_light_max(source[index].red,
                                              emission.red);
            source[index].green = vox_light_max(source[index].green,
                                                emission.green);
            source[index].blue = vox_light_max(source[index].blue,
                                               emission.blue);
        }
    }
    for (pass = 0U; pass < passes; ++pass) {
        for (y = 0U; y < VOX_WORLD_HEIGHT; ++y) {
            for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
                vox_u32 index = y * VOX_WORLD_WIDTH + x;
                vox_u16 attenuation = vox_light_solid[index] == 0U ?
                                      13U : 24U;
                vox_light_rgb value = source[index];
                if (x > 0U) {
                    value.red = vox_light_max(value.red,
                        vox_light_decay(source[index - 1U].red, attenuation));
                    value.green = vox_light_max(value.green,
                        vox_light_decay(source[index - 1U].green, attenuation));
                    value.blue = vox_light_max(value.blue,
                        vox_light_decay(source[index - 1U].blue, attenuation));
                }
                if (x + 1U < VOX_WORLD_WIDTH) {
                    value.red = vox_light_max(value.red,
                        vox_light_decay(source[index + 1U].red, attenuation));
                    value.green = vox_light_max(value.green,
                        vox_light_decay(source[index + 1U].green, attenuation));
                    value.blue = vox_light_max(value.blue,
                        vox_light_decay(source[index + 1U].blue, attenuation));
                }
                if (y > 0U) {
                    value.red = vox_light_max(value.red,
                        vox_light_decay(source[index - VOX_WORLD_WIDTH].red,
                                        attenuation));
                    value.green = vox_light_max(value.green,
                        vox_light_decay(source[index - VOX_WORLD_WIDTH].green,
                                        attenuation));
                    value.blue = vox_light_max(value.blue,
                        vox_light_decay(source[index - VOX_WORLD_WIDTH].blue,
                                        attenuation));
                }
                if (y + 1U < VOX_WORLD_HEIGHT) {
                    value.red = vox_light_max(value.red,
                        vox_light_decay(source[index + VOX_WORLD_WIDTH].red,
                                        attenuation));
                    value.green = vox_light_max(value.green,
                        vox_light_decay(source[index + VOX_WORLD_WIDTH].green,
                                        attenuation));
                    value.blue = vox_light_max(value.blue,
                        vox_light_decay(source[index + VOX_WORLD_WIDTH].blue,
                                        attenuation));
                }
                destination[index] = value;
            }
        }
        {
            vox_light_rgb *swap = source;
            source = destination;
            destination = swap;
        }
    }
    return source;
}

void vox_software_config_default(vox_software_config *config)
{
    if (config == 0) {
        return;
    }
    config->abi_version = VOX_ABI_VERSION;
    config->struct_size = (vox_u32)sizeof(*config);
    config->gi_quality = VOX_GI_BALANCED;
    config->reserved = 0U;
}

vox_result vox_software_render_ex(const vox_world *world,
                                  vox_software_target *target,
                                  const vox_software_config *config)
{
    vox_light_rgb *lightfield;
    vox_u32 pixel_y;
    vox_u32 pixel_x;
    if (world == 0 || target == 0 || config == 0 || target->pixels == 0 ||
        target->abi_version != VOX_ABI_VERSION ||
        target->struct_size < (vox_u32)sizeof(*target) ||
        config->abi_version != VOX_ABI_VERSION ||
        config->struct_size < (vox_u32)sizeof(*config) ||
        config->gi_quality > VOX_GI_SHOWCASE || config->reserved != 0U ||
        target->width == 0U || target->height == 0U ||
        target->stride < target->width * VOX_SOFTWARE_RGB_BYTES) {
        return VOX_ERR_INVALID;
    }
    lightfield = vox_build_lightfield(world, config->gi_quality);
    for (pixel_y = 0U; pixel_y < target->height; ++pixel_y) {
        vox_u32 world_y = pixel_y * VOX_WORLD_HEIGHT / target->height;
        for (pixel_x = 0U; pixel_x < target->width; ++pixel_x) {
            vox_u32 world_x = pixel_x * VOX_WORLD_WIDTH / target->width;
            vox_u32 light_index = world_y * VOX_WORLD_WIDTH + world_x;
            vox_u16 surface_material = vox_surface_material[light_index];
            vox_u8 *destination_pixel = target->pixels +
                pixel_y * target->stride + pixel_x * VOX_SOFTWARE_RGB_BYTES;
            if (surface_material == VOX_MAT_AIR) {
                destination_pixel[0] = (vox_u8)(16U + world_y * 18U /
                                                 VOX_WORLD_HEIGHT);
                destination_pixel[1] = (vox_u8)(24U + world_y * 22U /
                                                 VOX_WORLD_HEIGHT);
                destination_pixel[2] = (vox_u8)(42U + world_y * 28U /
                                                 VOX_WORLD_HEIGHT);
            } else {
                const vox_rgb *base = &vox_palette[surface_material];
                destination_pixel[0] = vox_clamp_u8(
                    (vox_u32)base->red * lightfield[light_index].red / 128U);
                destination_pixel[1] = vox_clamp_u8(
                    (vox_u32)base->green * lightfield[light_index].green / 128U);
                destination_pixel[2] = vox_clamp_u8(
                    (vox_u32)base->blue * lightfield[light_index].blue / 128U);
            }
        }
    }
    return VOX_OK;
}

vox_result vox_software_render(const vox_world *world,
                               vox_software_target *target)
{
    vox_software_config config;
    vox_software_config_default(&config);
    return vox_software_render_ex(world, target, &config);
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

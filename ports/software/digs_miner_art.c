/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "digs_miner_art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIGS_ICON_CROP_SIZE 16U
#define DIGS_ICON_SCALE (DIGS_MINER_ICON_SIZE / DIGS_ICON_CROP_SIZE)
#define DIGS_ICON_MAX_COLORS 256U
#define DIGS_ICON_CPP 2U

typedef struct digs_icon_color {
    vox_u8 red;
    vox_u8 green;
    vox_u8 blue;
} digs_icon_color;

static vox_i32 digs_miner_temperature(vox_u16 material)
{
    if (material == VOX_MAT_LAVA) {
        return 700L << 16;
    }
    if (material == VOX_MAT_SMOKE || material == VOX_MAT_FIREDAMP) {
        return 180L << 16;
    }
    if (material == VOX_MAT_FLESH || material == VOX_MAT_BLOOD) {
        return 37L << 16;
    }
    return 20L << 16;
}

static void digs_miner_set(vox_world *world, int x, int y, vox_u16 material)
{
    int offset_x;
    int offset_y;
    for (offset_y = 0; offset_y < 2; ++offset_y) {
        for (offset_x = 0; offset_x < 2; ++offset_x) {
            int cell_x = x + offset_x;
            int cell_y = y + offset_y;
            if (cell_x >= 0 && cell_y >= 0 &&
                cell_x < (int)VOX_WORLD_WIDTH &&
                cell_y < (int)VOX_WORLD_HEIGHT) {
                (void)vox_world_set(world, (vox_u32)cell_x,
                                    (vox_u32)cell_y,
                                    VOX_WORLD_DEPTH - 1U, material,
                                    digs_miner_temperature(material));
            }
        }
    }
}

static int digs_miner_part_present(const digs_miner_pose *pose,
                                   vox_u16 part)
{
    return (pose->severed_mask & ((vox_u32)1U << part)) == 0U;
}

void digs_miner_pose_default(digs_miner_pose *pose)
{
    if (pose == 0) {
        return;
    }
    pose->coat_material = VOX_MAT_METAL;
    pose->facing_right = 0U;
    pose->severed_mask = 0U;
}

vox_result digs_miner_voxelize(vox_world *world, int x, int y,
                               const digs_miner_pose *pose)
{
    int lamp_x;
    int row;
    int column;
    if (world == 0 || pose == 0 ||
        pose->coat_material == VOX_MAT_AIR ||
        pose->coat_material >= VOX_MAT_COUNT) {
        return VOX_ERR_INVALID;
    }
    lamp_x = pose->facing_right != 0U ? x + 4 : x - 4;
    if (digs_miner_part_present(pose, VOX_DIGS_PART_HEAD)) {
        for (column = -1; column <= 1; ++column) {
            digs_miner_set(world, x + column * 2, y - 8, VOX_MAT_METAL);
            digs_miner_set(world, x + column * 2, y - 6, VOX_MAT_FLESH);
        }
        digs_miner_set(world, lamp_x, y - 8, VOX_MAT_LAVA);
    }
    for (row = -2; row <= 0; ++row) {
        for (column = -1; column <= 1; ++column) {
            digs_miner_set(world, x + column * 2, y + row * 2,
                           pose->coat_material);
        }
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_LEFT_UPPER_ARM)) {
        digs_miner_set(world, x - 4, y - 2, pose->coat_material);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_RIGHT_UPPER_ARM)) {
        digs_miner_set(world, x + 4, y - 2, pose->coat_material);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_LEFT_THIGH)) {
        digs_miner_set(world, x - 2, y + 2, VOX_MAT_METAL);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_RIGHT_THIGH)) {
        digs_miner_set(world, x + 2, y + 2, VOX_MAT_METAL);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_LEFT_FOOT)) {
        digs_miner_set(world, x - 2, y + 4, VOX_MAT_COAL);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_RIGHT_FOOT)) {
        digs_miner_set(world, x + 2, y + 4, VOX_MAT_COAL);
    }
    return VOX_OK;
}

static int digs_icon_occupied(const vox_world *world, vox_u32 x, vox_u32 y)
{
    vox_u32 depth;
    for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
        const vox_cell *cell = vox_world_cell(world, x, y, depth);
        if (cell != 0 && cell->material != VOX_MAT_AIR) {
            return 1;
        }
    }
    return 0;
}

static vox_u16 digs_icon_find_color(digs_icon_color *colors,
                                    vox_u16 *color_count,
                                    const vox_u8 *pixel)
{
    vox_u16 index;
    for (index = 0U; index < *color_count; ++index) {
        if (colors[index].red == pixel[0] &&
            colors[index].green == pixel[1] &&
            colors[index].blue == pixel[2]) {
            return index;
        }
    }
    if (*color_count >= DIGS_ICON_MAX_COLORS) {
        return DIGS_ICON_MAX_COLORS;
    }
    colors[*color_count].red = pixel[0];
    colors[*color_count].green = pixel[1];
    colors[*color_count].blue = pixel[2];
    ++(*color_count);
    return (vox_u16)(*color_count - 1U);
}

static void digs_icon_code(vox_u16 index, char code[3])
{
    static const char digits[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz@#";
    code[0] = digits[(index / 64U) % 64U];
    code[1] = digits[index % 64U];
    code[2] = '\0';
}

static int digs_icon_write_rows(FILE *output, const vox_u16 *indices)
{
    vox_u32 logical_y;
    vox_u32 repeat_y;
    for (logical_y = 0U; logical_y < DIGS_ICON_CROP_SIZE; ++logical_y) {
        for (repeat_y = 0U; repeat_y < DIGS_ICON_SCALE; ++repeat_y) {
            vox_u32 logical_x;
            if (fputc('"', output) == EOF) {
                return 0;
            }
            for (logical_x = 0U; logical_x < DIGS_ICON_CROP_SIZE;
                 ++logical_x) {
                vox_u16 index = indices[logical_y * DIGS_ICON_CROP_SIZE +
                                        logical_x];
                vox_u32 repeat_x;
                char code[3];
                if (index == DIGS_ICON_MAX_COLORS) {
                    code[0] = '.';
                    code[1] = '.';
                    code[2] = '\0';
                } else {
                    digs_icon_code(index, code);
                }
                for (repeat_x = 0U; repeat_x < DIGS_ICON_SCALE; ++repeat_x) {
                    if (fputs(code, output) == EOF) {
                        return 0;
                    }
                }
            }
            if (logical_y + 1U == DIGS_ICON_CROP_SIZE &&
                repeat_y + 1U == DIGS_ICON_SCALE) {
                if (fputs("\"\n", output) == EOF) {
                    return 0;
                }
            } else if (fputs("\",\n", output) == EOF) {
                return 0;
            }
        }
    }
    return 1;
}

vox_result digs_miner_write_icon_xpm(const char *path)
{
    vox_world *world;
    vox_u8 *pixels;
    vox_software_target target;
    vox_software_config config;
    digs_miner_pose pose;
    digs_icon_color colors[DIGS_ICON_MAX_COLORS];
    vox_u16 indices[DIGS_ICON_CROP_SIZE * DIGS_ICON_CROP_SIZE];
    vox_u16 color_count = 0U;
    vox_u32 crop_x = VOX_WORLD_WIDTH / 2U - 8U;
    vox_u32 crop_y = VOX_WORLD_HEIGHT / 2U - 9U;
    vox_u32 x;
    vox_u32 y;
    FILE *output;
    vox_result result;
    int write_ok;
    if (path == 0 || path[0] == '\0') {
        return VOX_ERR_INVALID;
    }
    world = (vox_world *)malloc(sizeof(*world));
    pixels = (vox_u8 *)malloc((size_t)VOX_WORLD_WIDTH *
                              (size_t)VOX_WORLD_HEIGHT *
                              VOX_SOFTWARE_RGB_BYTES);
    if (world == 0 || pixels == 0) {
        free(pixels);
        free(world);
        return VOX_ERR_CAPACITY;
    }
    vox_world_init(world);
    digs_miner_pose_default(&pose);
    result = digs_miner_voxelize(world, (int)(VOX_WORLD_WIDTH / 2U),
                                 (int)(VOX_WORLD_HEIGHT / 2U), &pose);
    memset(&target, 0, sizeof(target));
    target.abi_version = VOX_ABI_VERSION;
    target.struct_size = (vox_u32)sizeof(target);
    target.width = VOX_WORLD_WIDTH;
    target.height = VOX_WORLD_HEIGHT;
    target.stride = VOX_WORLD_WIDTH * VOX_SOFTWARE_RGB_BYTES;
    target.pixels = pixels;
    vox_software_config_default(&config);
    config.gi_quality = VOX_GI_BALANCED;
    if (result == VOX_OK) {
        result = vox_software_render_ex(world, &target, &config);
    }
    if (result != VOX_OK) {
        free(pixels);
        free(world);
        return result;
    }
    for (y = 0U; y < DIGS_ICON_CROP_SIZE; ++y) {
        for (x = 0U; x < DIGS_ICON_CROP_SIZE; ++x) {
            vox_u32 world_x = crop_x + x;
            vox_u32 world_y = crop_y + y;
            vox_u16 index = DIGS_ICON_MAX_COLORS;
            if (digs_icon_occupied(world, world_x, world_y)) {
                const vox_u8 *pixel = pixels +
                    (world_y * VOX_WORLD_WIDTH + world_x) *
                    VOX_SOFTWARE_RGB_BYTES;
                index = digs_icon_find_color(colors, &color_count, pixel);
            }
            indices[y * DIGS_ICON_CROP_SIZE + x] = index;
        }
    }
    free(pixels);
    free(world);
    if (color_count == 0U || color_count > DIGS_ICON_MAX_COLORS) {
        return VOX_ERR_INVALID;
    }
    output = fopen(path, "wb");
    if (output == 0) {
        return VOX_ERR_INVALID;
    }
    if (fprintf(output,
                "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
                "/* XPM */\nstatic const char *digs_miner_xpm[] = {\n"
                "\"%u %u %u %u\",\n\".. c None\",\n",
                (unsigned int)DIGS_MINER_ICON_SIZE,
                (unsigned int)DIGS_MINER_ICON_SIZE,
                (unsigned int)(color_count + 1U),
                (unsigned int)DIGS_ICON_CPP) < 0) {
        fclose(output);
        return VOX_ERR_INVALID;
    }
    for (x = 0U; x < color_count; ++x) {
        char code[3];
        digs_icon_code((vox_u16)x, code);
        if (fprintf(output, "\"%s c #%02X%02X%02X\",\n", code,
                    (unsigned int)colors[x].red,
                    (unsigned int)colors[x].green,
                    (unsigned int)colors[x].blue) < 0) {
            fclose(output);
            return VOX_ERR_INVALID;
        }
    }
    write_ok = digs_icon_write_rows(output, indices);
    if (write_ok && fputs("};\n", output) == EOF) {
        write_ok = 0;
    }
    if (fclose(output) != 0) {
        write_ok = 0;
    }
    if (!write_ok) {
        return VOX_ERR_INVALID;
    }
    return VOX_OK;
}

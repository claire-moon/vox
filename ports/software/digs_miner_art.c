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

static void digs_miner_world_plot(void *context, int x, int y,
                                  vox_u16 material)
{
    vox_world *world = (vox_world *)context;
    if (world != 0 && x >= 0 && y >= 0 &&
        x < (int)VOX_WORLD_WIDTH && y < (int)VOX_WORLD_HEIGHT) {
        (void)vox_world_set(world, (vox_u32)x, (vox_u32)y,
                            VOX_WORLD_DEPTH - 1U, material,
                            digs_miner_temperature(material));
    }
}

static void digs_miner_set(digs_miner_plot_fn plot, void *context,
                           int x, int y, vox_u16 material)
{
    int offset_x;
    int offset_y;
    for (offset_y = 0; offset_y < 2; ++offset_y) {
        for (offset_x = 0; offset_x < 2; ++offset_x) {
            int cell_x = x + offset_x;
            int cell_y = y + offset_y;
            plot(context, cell_x, cell_y, material);
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
    pose->helmet_material = VOX_MAT_METAL;
    pose->facing_right = 0U;
    pose->severed_mask = 0U;
    pose->steam_pack = 1U;
    pose->steam_thrusting = 0U;
    pose->steam_variant = 0U;
    pose->animation = DIGS_MINER_ANIMATION_IDLE;
    pose->animation_phase = 0U;
    pose->reserved = 0U;
    pose->aim_x = -1;
    pose->aim_y = 0;
}

static int digs_miner_pose_valid(const digs_miner_pose *pose)
{
    return pose != 0 && pose->coat_material != VOX_MAT_AIR &&
           pose->coat_material < VOX_MAT_COUNT &&
           pose->helmet_material != VOX_MAT_AIR &&
           pose->helmet_material < VOX_MAT_COUNT &&
           pose->animation < DIGS_MINER_ANIMATION_COUNT &&
           pose->reserved == 0U;
}

vox_result digs_miner_skeleton_build(const digs_miner_pose *pose,
                                     digs_miner_skeleton *skeleton)
{
    static const int cycle[16] = {
        0, 1, 2, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -2, -1
    };
    int swing;
    int bob = 0;
    int lean = 0;
    int knee_l = 0;
    int knee_r = 0;
    int lift_l = 0;
    int lift_r = 0;
    int arm_l = 0;
    int arm_r = 0;
    int direction;
    int phase;
    int aim_x;
    int aim_y;
    int magnitude;
    int joint;
    if (!digs_miner_pose_valid(pose) || skeleton == 0) {
        return VOX_ERR_INVALID;
    }
    direction = pose->facing_right != 0U ? 1 : -1;
    phase = (int)(pose->animation_phase & 15U);
    swing = cycle[phase];
    if (pose->animation == DIGS_MINER_ANIMATION_WALK) {
        bob = ((phase & 7) == 3 || (phase & 7) == 4) ? -1 : 0;
        lean = direction;
        knee_l = swing / 2;
        knee_r = -swing / 2;
        lift_l = swing < 0 ? -1 : 0;
        lift_r = swing > 0 ? -1 : 0;
        arm_l = -swing;
        arm_r = swing;
    } else if (pose->animation == DIGS_MINER_ANIMATION_JUMP ||
               pose->animation == DIGS_MINER_ANIMATION_STEAM) {
        lean = direction;
        knee_l = -direction * 2;
        knee_r = direction;
        lift_l = -2;
        lift_r = -1;
        arm_l = -1;
        arm_r = 1;
    } else if (pose->animation == DIGS_MINER_ANIMATION_PAIN) {
        lean = -direction * 2;
        bob = 1;
        arm_l = -2;
        arm_r = 2;
    } else if (pose->animation == DIGS_MINER_ANIMATION_FIRE) {
        lean = -direction * ((phase & 3) == 0 ? 1 : 0);
    } else if (pose->animation == DIGS_MINER_ANIMATION_DEATH) {
        lean = direction * 2;
        bob = 2;
        lift_l = 1;
        lift_r = 1;
        arm_l = -2;
        arm_r = 2;
    }
    for (joint = 0; joint < (int)DIGS_MINER_JOINT_COUNT; ++joint) {
        skeleton->x[joint] = 0;
        skeleton->y[joint] = bob;
    }
    skeleton->x[DIGS_MINER_JOINT_NECK] = lean;
    skeleton->y[DIGS_MINER_JOINT_NECK] = bob - 5;
    skeleton->x[DIGS_MINER_JOINT_LEFT_SHOULDER] = lean - 3;
    skeleton->y[DIGS_MINER_JOINT_LEFT_SHOULDER] = bob - 3;
    skeleton->x[DIGS_MINER_JOINT_RIGHT_SHOULDER] = lean + 3;
    skeleton->y[DIGS_MINER_JOINT_RIGHT_SHOULDER] = bob - 3;
    skeleton->x[DIGS_MINER_JOINT_LEFT_ELBOW] = lean - 4 + arm_l / 2;
    skeleton->y[DIGS_MINER_JOINT_LEFT_ELBOW] = bob - 1;
    skeleton->x[DIGS_MINER_JOINT_RIGHT_ELBOW] = lean + 4 + arm_r / 2;
    skeleton->y[DIGS_MINER_JOINT_RIGHT_ELBOW] = bob - 1;
    skeleton->x[DIGS_MINER_JOINT_LEFT_HAND] = lean - 4 + arm_l;
    skeleton->y[DIGS_MINER_JOINT_LEFT_HAND] = bob;
    skeleton->x[DIGS_MINER_JOINT_RIGHT_HAND] = lean + 4 + arm_r;
    skeleton->y[DIGS_MINER_JOINT_RIGHT_HAND] = bob;
    skeleton->x[DIGS_MINER_JOINT_LEFT_KNEE] = -2 + knee_l;
    skeleton->y[DIGS_MINER_JOINT_LEFT_KNEE] = bob + 2 + lift_l;
    skeleton->x[DIGS_MINER_JOINT_RIGHT_KNEE] = 2 + knee_r;
    skeleton->y[DIGS_MINER_JOINT_RIGHT_KNEE] = bob + 2 + lift_r;
    skeleton->x[DIGS_MINER_JOINT_LEFT_FOOT] = -2 + knee_l * 2;
    skeleton->y[DIGS_MINER_JOINT_LEFT_FOOT] = bob + 4 + lift_l;
    skeleton->x[DIGS_MINER_JOINT_RIGHT_FOOT] = 2 + knee_r * 2;
    skeleton->y[DIGS_MINER_JOINT_RIGHT_FOOT] = bob + 4 + lift_r;
    skeleton->x[DIGS_MINER_JOINT_PACK] = lean - direction * 5;
    skeleton->y[DIGS_MINER_JOINT_PACK] = bob - 2;
    if (pose->animation == DIGS_MINER_ANIMATION_FIRE ||
        pose->animation == DIGS_MINER_ANIMATION_IDLE) {
        int shoulder = direction > 0 ? DIGS_MINER_JOINT_RIGHT_SHOULDER :
                                      DIGS_MINER_JOINT_LEFT_SHOULDER;
        int elbow = direction > 0 ? DIGS_MINER_JOINT_RIGHT_ELBOW :
                                   DIGS_MINER_JOINT_LEFT_ELBOW;
        int hand = direction > 0 ? DIGS_MINER_JOINT_RIGHT_HAND :
                                  DIGS_MINER_JOINT_LEFT_HAND;
        aim_x = pose->aim_x;
        aim_y = pose->aim_y;
        /* Clamp external port input before multiplying it by joint lengths. */
        if (aim_x < -1024) aim_x = -1024;
        if (aim_x > 1024) aim_x = 1024;
        if (aim_y < -1024) aim_y = -1024;
        if (aim_y > 1024) aim_y = 1024;
        magnitude = aim_x < 0 ? -aim_x : aim_x;
        if ((aim_y < 0 ? -aim_y : aim_y) > magnitude) {
            magnitude = aim_y < 0 ? -aim_y : aim_y;
        }
        if (magnitude == 0) {
            aim_x = direction;
            magnitude = 1;
        }
        skeleton->x[elbow] = skeleton->x[shoulder] + aim_x * 2 / magnitude;
        skeleton->y[elbow] = skeleton->y[shoulder] + aim_y * 2 / magnitude + 1;
        skeleton->x[hand] = skeleton->x[elbow] + aim_x * 2 / magnitude;
        skeleton->y[hand] = skeleton->y[elbow] + aim_y * 2 / magnitude;
    }
    return VOX_OK;
}

static void digs_miner_segment(digs_miner_plot_fn plot, void *context,
                               int x0, int y0, int x1, int y1,
                               vox_u16 material)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x1 > x0 ? 1 : -1;
    int sy = y1 > y0 ? 1 : -1;
    int error = dx - dy;
    for (;;) {
        int twice;
        digs_miner_set(plot, context, x0, y0, material);
        if (x0 == x1 && y0 == y1) break;
        twice = error * 2;
        if (twice > -dy) { error -= dy; x0 += sx; }
        if (twice < dx) { error += dx; y0 += sy; }
    }
}

static void digs_miner_bone(int x, int y, const digs_miner_pose *pose,
                            const digs_miner_skeleton *skeleton,
                            int parent, int child, vox_u16 part,
                            vox_u16 material, digs_miner_plot_fn plot,
                            void *context)
{
    if (!digs_miner_part_present(pose, part)) return;
    digs_miner_segment(plot, context, x + skeleton->x[parent],
                       y + skeleton->y[parent], x + skeleton->x[child],
                       y + skeleton->y[child], material);
}

static vox_result digs_miner_plot_skeleton(int x, int y,
                                           const digs_miner_pose *pose,
                                           digs_miner_plot_fn plot,
                                           void *context)
{
    digs_miner_skeleton skeleton;
    int direction = pose->facing_right != 0U ? 1 : -1;
    int column;
    int row;
    if (digs_miner_skeleton_build(pose, &skeleton) != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_HEAD)) {
        int neck_x = x + skeleton.x[DIGS_MINER_JOINT_NECK];
        int neck_y = y + skeleton.y[DIGS_MINER_JOINT_NECK];
        for (column = -1; column <= 1; ++column) {
            digs_miner_set(plot, context, neck_x + column * 2, neck_y - 3,
                           pose->helmet_material);
            digs_miner_set(plot, context, neck_x + column * 2, neck_y - 1,
                           VOX_MAT_FLESH);
        }
        digs_miner_set(plot, context, neck_x + direction * 4, neck_y - 3,
                       VOX_MAT_LAVA);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_TORSO)) {
        for (row = 0; row < 3; ++row) {
            int lean = skeleton.x[DIGS_MINER_JOINT_NECK] * (2 - row) / 2;
            for (column = -1; column <= 1; ++column) {
                digs_miner_set(plot, context, x + lean + column * 2,
                               y + skeleton.y[0] - 4 + row * 2,
                               pose->coat_material);
            }
        }
    }
    digs_miner_bone(x, y, pose, &skeleton, 2, 3,
        VOX_DIGS_PART_LEFT_UPPER_ARM, pose->coat_material, plot, context);
    digs_miner_bone(x, y, pose, &skeleton, 3, 4,
        VOX_DIGS_PART_LEFT_FOREARM, pose->coat_material, plot, context);
    digs_miner_bone(x, y, pose, &skeleton, 5, 6,
        VOX_DIGS_PART_RIGHT_UPPER_ARM, pose->coat_material, plot, context);
    digs_miner_bone(x, y, pose, &skeleton, 6, 7,
        VOX_DIGS_PART_RIGHT_FOREARM, pose->coat_material, plot, context);
    digs_miner_bone(x, y, pose, &skeleton, 0, 8,
        VOX_DIGS_PART_LEFT_THIGH, VOX_MAT_METAL, plot, context);
    digs_miner_bone(x, y, pose, &skeleton, 8, 9,
        VOX_DIGS_PART_LEFT_SHIN, VOX_MAT_METAL, plot, context);
    digs_miner_bone(x, y, pose, &skeleton, 0, 10,
        VOX_DIGS_PART_RIGHT_THIGH, VOX_MAT_METAL, plot, context);
    digs_miner_bone(x, y, pose, &skeleton, 10, 11,
        VOX_DIGS_PART_RIGHT_SHIN, VOX_MAT_METAL, plot, context);
    if (digs_miner_part_present(pose, VOX_DIGS_PART_LEFT_HAND)) {
        digs_miner_set(plot, context, x + skeleton.x[4], y + skeleton.y[4],
                       VOX_MAT_FLESH);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_RIGHT_HAND)) {
        digs_miner_set(plot, context, x + skeleton.x[7], y + skeleton.y[7],
                       VOX_MAT_FLESH);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_LEFT_FOOT)) {
        digs_miner_set(plot, context, x + skeleton.x[9], y + skeleton.y[9],
                       VOX_MAT_COAL);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_RIGHT_FOOT)) {
        digs_miner_set(plot, context, x + skeleton.x[11], y + skeleton.y[11],
                       VOX_MAT_COAL);
    }
    if (pose->steam_pack != 0U) {
        int pack_x = x + skeleton.x[DIGS_MINER_JOINT_PACK];
        int pack_y = y + skeleton.y[DIGS_MINER_JOINT_PACK];
        digs_miner_set(plot, context, pack_x, pack_y, VOX_MAT_METAL);
        if (pose->steam_thrusting != 0U) {
            int drift = (int)(pose->steam_variant % 3U) - 1;
            plot(context, pack_x, pack_y + 3, VOX_MAT_LAVA);
            plot(context, pack_x + drift, pack_y + 4, VOX_MAT_LAVA);
            plot(context, pack_x - drift, pack_y + 5, VOX_MAT_SMOKE);
        }
    }
    return VOX_OK;
}

vox_result digs_miner_plot(int x, int y, const digs_miner_pose *pose,
                           digs_miner_plot_fn plot, void *context)
{
    int lamp_x;
    int pack_x;
    int row;
    int column;
    int left_arm_x;
    int left_arm_y;
    int right_arm_x;
    int right_arm_y;
    int left_thigh_x;
    int left_thigh_y;
    int right_thigh_x;
    int right_thigh_y;
    int left_foot_x;
    int left_foot_y;
    int right_foot_x;
    int right_foot_y;
    int direction;
    int phase;
    if (plot == 0 || !digs_miner_pose_valid(pose)) {
        return VOX_ERR_INVALID;
    }
    /* Preserve the authored launcher icon's stationary pose.  Gameplay uses
     * the articulated hierarchy for every moving and firing state. */
    if (pose->animation != DIGS_MINER_ANIMATION_IDLE ||
        (pose->animation_phase & DIGS_MINER_ARTICULATED) != 0U) {
        return digs_miner_plot_skeleton(x, y, pose, plot, context);
    }
    lamp_x = pose->facing_right != 0U ? x + 4 : x - 4;
    pack_x = pose->facing_right != 0U ? x - 5 : x + 5;
    left_arm_x = x - 4;
    left_arm_y = y - 2;
    right_arm_x = x + 4;
    right_arm_y = y - 2;
    left_thigh_x = x - 2;
    left_thigh_y = y + 2;
    right_thigh_x = x + 2;
    right_thigh_y = y + 2;
    left_foot_x = x - 2;
    left_foot_y = y + 4;
    right_foot_x = x + 2;
    right_foot_y = y + 4;
    direction = pose->facing_right != 0U ? 1 : -1;
    phase = (int)(pose->animation_phase & 3U);
    if (pose->animation == DIGS_MINER_ANIMATION_WALK) {
        if (phase == 0 || phase == 2) {
            int swing = phase == 0 ? 1 : -1;
            left_arm_x -= swing;
            right_arm_x += swing;
            left_foot_x += swing;
            right_foot_x -= swing;
            left_foot_y += swing < 0 ? 1 : 0;
            right_foot_y += swing > 0 ? 1 : 0;
        }
    } else if (pose->animation == DIGS_MINER_ANIMATION_JUMP ||
               pose->animation == DIGS_MINER_ANIMATION_STEAM) {
        left_arm_y -= 2;
        right_arm_y -= 2;
        left_foot_y -= 1;
        right_foot_y -= 1;
    } else if (pose->animation == DIGS_MINER_ANIMATION_FIRE) {
        if (direction > 0) {
            right_arm_x += 3;
            right_arm_y -= 1;
        } else {
            left_arm_x -= 3;
            left_arm_y -= 1;
        }
    } else if (pose->animation == DIGS_MINER_ANIMATION_PAIN) {
        int flinch = phase < 2 ? 1 : -1;
        left_arm_x -= flinch;
        right_arm_x -= flinch;
        left_foot_x -= flinch;
        right_foot_x -= flinch;
    } else if (pose->animation == DIGS_MINER_ANIMATION_DEATH) {
        left_arm_y += 2;
        right_arm_y += 2;
        left_foot_y += 2;
        right_foot_y += 2;
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_HEAD)) {
        for (column = -1; column <= 1; ++column) {
            digs_miner_set(plot, context, x + column * 2, y - 8,
                           pose->helmet_material);
            digs_miner_set(plot, context, x + column * 2, y - 6,
                           VOX_MAT_FLESH);
        }
        digs_miner_set(plot, context, lamp_x, y - 8, VOX_MAT_LAVA);
    }
    for (row = -2; row <= 0; ++row) {
        for (column = -1; column <= 1; ++column) {
            digs_miner_set(plot, context, x + column * 2, y + row * 2,
                           pose->coat_material);
        }
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_LEFT_UPPER_ARM)) {
        digs_miner_set(plot, context, left_arm_x, left_arm_y,
                       pose->coat_material);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_RIGHT_UPPER_ARM)) {
        digs_miner_set(plot, context, right_arm_x, right_arm_y,
                       pose->coat_material);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_LEFT_THIGH)) {
        digs_miner_set(plot, context, left_thigh_x, left_thigh_y,
                       VOX_MAT_METAL);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_RIGHT_THIGH)) {
        digs_miner_set(plot, context, right_thigh_x, right_thigh_y,
                       VOX_MAT_METAL);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_LEFT_FOOT)) {
        digs_miner_set(plot, context, left_foot_x, left_foot_y,
                       VOX_MAT_COAL);
    }
    if (digs_miner_part_present(pose, VOX_DIGS_PART_RIGHT_FOOT)) {
        digs_miner_set(plot, context, right_foot_x, right_foot_y,
                       VOX_MAT_COAL);
    }
    if (pose->steam_pack != 0U) {
        digs_miner_set(plot, context, pack_x, y - 2, VOX_MAT_METAL);
        plot(context, pack_x, y + 1, VOX_MAT_COAL);
        if (pose->steam_thrusting != 0U) {
            int drift = (int)(pose->steam_variant % 3U) - 1;
            plot(context, pack_x, y + 2, VOX_MAT_LAVA);
            plot(context, pack_x + drift, y + 3, VOX_MAT_LAVA);
            plot(context, pack_x - drift, y + 4, VOX_MAT_SMOKE);
            if ((pose->steam_variant & 1U) != 0U) {
                plot(context, pack_x + drift, y + 5, VOX_MAT_SMOKE);
            }
        }
    }
    return VOX_OK;
}

vox_result digs_miner_voxelize(vox_world *world, int x, int y,
                               const digs_miner_pose *pose)
{
    if (world == 0) {
        return VOX_ERR_INVALID;
    }
    return digs_miner_plot(x, y, pose, digs_miner_world_plot, world);
}

typedef struct digs_tool_model {
    const char *rows[7];
    int tip_x;
    int tip_y;
} digs_tool_model;

/* Authored here, with the grip at (0,3).  These are deliberately silhouettes
 * with a small material vocabulary so they survive native one-voxel detail.
 * The long rail, twin shotgun barrels, tapered drill, broad hammer, axe head,
 * tanks and sidearms remain distinguishable after rotating around the grip. */
static const digs_tool_model digs_tool_models[VOX_DIGS_TOOL_COUNT] = {
    {{"       MMM", "      MMMM", "      MM  ", "CCCCCCMM  ",
      "      MM  ", "       MMM", "        MM"}, 8, 3},
    {{"", "", " MMMM", "CCMMMM", " C", " C", ""}, 5, 3},
    {{"", "   CC", "  MSSM", "CCMSSM", "  MMMM", "", ""}, 5, 3},
    {{"", "  HHH", " MMHMHMM", "CCMMMMMHH", "  CCCC", "", ""}, 8, 3},
    {{"", "  WW", " MWWMMM", "CCMMMMMMW", " MWWM", "  CC", ""}, 8, 3},
    {{"       MMMM", "       MMMM", "       MMMM", "CCCCCCCMMMM",
      "       MMMM", "       MMMM", ""}, 10, 3},
    {{"", "   M", "  MMM", "CCMMMMMMMH", "  MMM", " CC", ""}, 9, 3},
    {{"", "", "CCCCMMMMMM", "CCMMMMMMMM", " CCCC", "", ""}, 9, 3},
    {{"", "    H", "  HHH", "CCHHHH", "  HH", "", ""}, 5, 3},
    {{"", " MMMMM", " MMMM MM", "CCMMMMMMM", " MMMM MM", " MMMMM", ""}, 8, 3},
    {{"    HHH", "  MMMM", "  MHHMMMMMMM", "CCMMMMMMMMMH", "  MHH", "   CC", ""}, 11, 3}
};

static int digs_tool_sqrt(int square)
{
    int result = 0;
    int bit = 1 << 28;
    while (bit > square) bit >>= 2;
    while (bit != 0) {
        if (square >= result + bit) {
            square -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

static int digs_tool_round(int numerator, int denominator)
{
    return numerator >= 0 ? (numerator + denominator / 2) / denominator :
                           -((-numerator + denominator / 2) / denominator);
}

static void digs_tool_transform(int grip_x, int grip_y, int local_x,
                                int local_y, int aim_x, int aim_y,
                                int magnitude, int *x, int *y)
{
    *x = grip_x + digs_tool_round(local_x * aim_x - local_y * aim_y,
                                 magnitude);
    *y = grip_y + digs_tool_round(local_x * aim_y + local_y * aim_x,
                                 magnitude);
}

vox_result digs_weapon_plot(int grip_x, int grip_y,
                            const digs_weapon_pose *pose,
                            digs_miner_plot_fn plot, void *context,
                            int *muzzle_x, int *muzzle_y)
{
    const digs_tool_model *model;
    int aim_x;
    int aim_y;
    int magnitude;
    int row;
    int column;
    int tip_x;
    int tip_y;
    if (pose == 0 || plot == 0 || pose->weapon >= VOX_DIGS_TOOL_COUNT ||
        pose->reserved != 0U || muzzle_x == 0 || muzzle_y == 0) {
        return VOX_ERR_INVALID;
    }
    model = &digs_tool_models[pose->weapon];
    aim_x = pose->aim_x;
    aim_y = pose->aim_y;
    if (aim_x < -1024) aim_x = -1024;
    if (aim_x > 1024) aim_x = 1024;
    if (aim_y < -1024) aim_y = -1024;
    if (aim_y > 1024) aim_y = 1024;
    if (aim_x == 0 && aim_y == 0) aim_x = 1;
    /* Melee swing is rotation of the entire model, including its endpoint. */
    if (pose->firing != 0U &&
        (pose->weapon == VOX_DIGS_TOOL_PULASKI ||
         pose->weapon == VOX_DIGS_TOOL_GIANT_HAMMER)) {
        int turn = (int)(pose->phase & 7U) - 3;
        int previous_x = aim_x;
        aim_x = aim_x * 4 - aim_y * turn;
        aim_y = aim_y * 4 + previous_x * turn;
    }
    magnitude = digs_tool_sqrt(aim_x * aim_x + aim_y * aim_y);
    if (magnitude < 1) magnitude = 1;
    if (pose->firing != 0U && pose->weapon != VOX_DIGS_TOOL_PULASKI &&
        pose->weapon != VOX_DIGS_TOOL_GIANT_HAMMER) {
        grip_x -= digs_tool_round(aim_x, magnitude);
        grip_y -= digs_tool_round(aim_y, magnitude);
    }
    for (row = 0; row < 7; ++row) {
        const char *cells = model->rows[row];
        for (column = 0; cells[column] != '\0'; ++column) {
            vox_u16 material;
            int x;
            int y;
            if (cells[column] == ' ') continue;
            material = cells[column] == 'C' ? VOX_MAT_COAL :
                       cells[column] == 'H' ? VOX_MAT_LAVA :
                       cells[column] == 'W' ? VOX_MAT_WATER :
                       cells[column] == 'S' ? VOX_MAT_SMOKE : VOX_MAT_METAL;
            digs_tool_transform(grip_x, grip_y, column, row - 3,
                                 aim_x, aim_y, magnitude, &x, &y);
            plot(context, x, y, material);
        }
    }
    digs_tool_transform(grip_x, grip_y, model->tip_x, model->tip_y - 3,
                         aim_x, aim_y, magnitude, &tip_x, &tip_y);
    *muzzle_x = tip_x;
    *muzzle_y = tip_y;
    if (pose->firing != 0U && pose->weapon != VOX_DIGS_TOOL_PULASKI &&
        pose->weapon != VOX_DIGS_TOOL_GIANT_HAMMER &&
        pose->weapon != VOX_DIGS_TOOL_BORE_DRILL) {
        int ray;
        for (ray = 1; ray <= 3; ++ray) {
            int x;
            int y;
            digs_tool_transform(tip_x, tip_y, ray, 0, aim_x, aim_y,
                                 magnitude, &x, &y);
            plot(context, x, y, VOX_MAT_LAVA);
            if (ray == 1) {
                digs_tool_transform(tip_x, tip_y, ray, 1, aim_x, aim_y,
                                     magnitude, &x, &y);
                plot(context, x, y, VOX_MAT_LAVA);
                digs_tool_transform(tip_x, tip_y, ray, -1, aim_x, aim_y,
                                     magnitude, &x, &y);
                plot(context, x, y, VOX_MAT_SMOKE);
            }
        }
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

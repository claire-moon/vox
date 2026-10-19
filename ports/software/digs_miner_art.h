/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DIGS_MINER_ART_H
#define DIGS_MINER_ART_H

#include "vox/vox_game.h"
#include "vox/vox_render.h"

#define DIGS_MINER_ICON_SIZE 256U

/* Renderer-only poses.  They are derived by a port from authoritative body
 * state and never become animation state in vox_digs_match. */
#define DIGS_MINER_ANIMATION_IDLE 0U
#define DIGS_MINER_ANIMATION_WALK 1U
#define DIGS_MINER_ANIMATION_JUMP 2U
#define DIGS_MINER_ANIMATION_STEAM 3U
#define DIGS_MINER_ANIMATION_PAIN 4U
#define DIGS_MINER_ANIMATION_FIRE 5U
#define DIGS_MINER_ANIMATION_DEATH 6U
#define DIGS_MINER_ANIMATION_COUNT 7U
#define DIGS_MINER_ARTICULATED 16U
#define DIGS_MINER_JOINT_COUNT 13U
#define DIGS_MINER_JOINT_ROOT 0U
#define DIGS_MINER_JOINT_NECK 1U
#define DIGS_MINER_JOINT_LEFT_SHOULDER 2U
#define DIGS_MINER_JOINT_LEFT_ELBOW 3U
#define DIGS_MINER_JOINT_LEFT_HAND 4U
#define DIGS_MINER_JOINT_RIGHT_SHOULDER 5U
#define DIGS_MINER_JOINT_RIGHT_ELBOW 6U
#define DIGS_MINER_JOINT_RIGHT_HAND 7U
#define DIGS_MINER_JOINT_LEFT_KNEE 8U
#define DIGS_MINER_JOINT_LEFT_FOOT 9U
#define DIGS_MINER_JOINT_RIGHT_KNEE 10U
#define DIGS_MINER_JOINT_RIGHT_FOOT 11U
#define DIGS_MINER_JOINT_PACK 12U

typedef struct digs_miner_pose {
    vox_u16 coat_material;
    vox_u16 helmet_material;
    vox_u16 facing_right;
    vox_u32 severed_mask;
    vox_u16 steam_pack;
    vox_u16 steam_thrusting;
    vox_u16 steam_variant;
    vox_u16 animation;
    vox_u16 animation_phase;
    vox_u16 reserved;
    int aim_x;
    int aim_y;
} digs_miner_pose;

typedef void (*digs_miner_plot_fn)(void *context, int x, int y,
                                   vox_u16 material);

/* Local voxel coordinates after the parent hierarchy has been evaluated.
 * The root is the physics body's centre; a port may use the hand joints to
 * attach held models without adding bones or animation clocks to the sim. */
typedef struct digs_miner_skeleton {
    int x[DIGS_MINER_JOINT_COUNT];
    int y[DIGS_MINER_JOINT_COUNT];
} digs_miner_skeleton;

typedef struct digs_weapon_pose {
    vox_u16 weapon;
    vox_u16 firing;
    vox_u16 phase;
    vox_u16 reserved;
    int aim_x;
    int aim_y;
} digs_weapon_pose;

#ifdef __cplusplus
extern "C" {
#endif

void digs_miner_pose_default(digs_miner_pose *pose);
vox_result digs_miner_skeleton_build(const digs_miner_pose *pose,
                                     digs_miner_skeleton *skeleton);
vox_result digs_miner_plot(int x, int y, const digs_miner_pose *pose,
                           digs_miner_plot_fn plot, void *context);
/* A tool is rotated around its grip in integer voxel space.  muzzle receives
 * the transformed endpoint, so the flash and held model share one origin. */
vox_result digs_weapon_plot(int grip_x, int grip_y,
                            const digs_weapon_pose *pose,
                            digs_miner_plot_fn plot, void *context,
                            int *muzzle_x, int *muzzle_y);
vox_result digs_miner_voxelize(vox_world *world, int x, int y,
                               const digs_miner_pose *pose);
vox_result digs_miner_write_icon_xpm(const char *path);

#ifdef __cplusplus
}
#endif

#endif

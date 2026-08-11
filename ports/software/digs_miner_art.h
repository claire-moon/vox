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
} digs_miner_pose;

typedef void (*digs_miner_plot_fn)(void *context, int x, int y,
                                   vox_u16 material);

#ifdef __cplusplus
extern "C" {
#endif

void digs_miner_pose_default(digs_miner_pose *pose);
vox_result digs_miner_plot(int x, int y, const digs_miner_pose *pose,
                           digs_miner_plot_fn plot, void *context);
vox_result digs_miner_voxelize(vox_world *world, int x, int y,
                               const digs_miner_pose *pose);
vox_result digs_miner_write_icon_xpm(const char *path);

#ifdef __cplusplus
}
#endif

#endif

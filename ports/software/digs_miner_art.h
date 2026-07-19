/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DIGS_MINER_ART_H
#define DIGS_MINER_ART_H

#include "vox/vox_game.h"
#include "vox/vox_render.h"

#define DIGS_MINER_ICON_SIZE 256U

typedef struct digs_miner_pose {
    vox_u16 coat_material;
    vox_u16 facing_right;
    vox_u32 severed_mask;
} digs_miner_pose;

#ifdef __cplusplus
extern "C" {
#endif

void digs_miner_pose_default(digs_miner_pose *pose);
vox_result digs_miner_voxelize(vox_world *world, int x, int y,
                               const digs_miner_pose *pose);
vox_result digs_miner_write_icon_xpm(const char *path);

#ifdef __cplusplus
}
#endif

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_RENDER_H
#define VOX_RENDER_H

#include "vox_kernel.h"

#define VOX_SOFTWARE_RGB_BYTES 3U

#define VOX_GI_COMPATIBILITY 0U
#define VOX_GI_BALANCED 1U
#define VOX_GI_SHOWCASE 2U

typedef struct vox_software_target {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 width;
    vox_u32 height;
    vox_u32 stride;
    vox_u8 *pixels;
} vox_software_target;

typedef struct vox_software_config {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u16 gi_quality;
    vox_u16 reserved;
} vox_software_config;

/*
 * A Q16.16 camera rectangle.  Keeping this interface integer-only makes the
 * visible-region renderer deterministic on hosts without identical floating
 * point behavior while still allowing sub-cell camera motion.
 */
typedef struct vox_software_view {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_i32 origin_x_q16;
    vox_i32 origin_y_q16;
    vox_i32 width_q16;
    vox_i32 height_q16;
} vox_software_view;

void vox_software_config_default(vox_software_config *config);
void vox_software_view_full(vox_software_view *view);
vox_result vox_software_render(const vox_world *world,
                               vox_software_target *target);
vox_result vox_software_render_ex(const vox_world *world,
                                  vox_software_target *target,
                                  const vox_software_config *config);
vox_result vox_software_render_view_ex(const vox_world *world,
                                       vox_software_target *target,
                                       const vox_software_config *config,
                                       const vox_software_view *view);
vox_u32 vox_software_hash(const vox_software_target *target);

#endif

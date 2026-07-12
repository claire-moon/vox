/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_RENDER_H
#define VOX_RENDER_H

#include "vox_kernel.h"

#define VOX_SOFTWARE_RGB_BYTES 3U

typedef struct vox_software_target {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 width;
    vox_u32 height;
    vox_u32 stride;
    vox_u8 *pixels;
} vox_software_target;

vox_result vox_software_render(const vox_world *world,
                               vox_software_target *target);
vox_u32 vox_software_hash(const vox_software_target *target);

#endif

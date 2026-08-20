/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_KERNEL_PRIVATE_H
#define VOX_KERNEL_PRIVATE_H

/*
 * DIGS-only kernel seam.
 *
 * These authored-fixture, terrain-stain, and fracture-record details are
 * deliberately outside the installed/public kernel contract.  They preserve
 * the ABI-11 world layout while allowing the game layer to route its bounded
 * fixture and fragment rules through the kernel's indexed cell mutations.
 */
#include "vox/vox_kernel.h"

/* Authored metal rope target; never participates in terrain structure. */
#define VOX_CELL_FIXTURE 64U
/* Presentation-only blood stain on an existing terrain voxel. */
#define VOX_CELL_BLOODY 128U

/* Bounded record of cells actually cleared by one fracture pass. */
#define VOX_BLAST_CAPTURE_MAX 160U

typedef struct vox_blast_cell {
    vox_u16 x;
    vox_u16 y;
    vox_u16 z;
    vox_u16 material;
    vox_u16 flags;
} vox_blast_cell;

typedef struct vox_blast_capture {
    vox_u16 count;
    vox_u16 truncated;
    /* All non-fixture support-bearing cells cleared, even past record cap. */
    vox_u16 structural_count;
    vox_blast_cell cells[VOX_BLAST_CAPTURE_MAX];
} vox_blast_capture;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These retain external C linkage only because DIGS links vox_kernel as a
 * separate static library.  This private declaration is the sole supported
 * cross-translation-unit seam; nothing below is part of the installed API.
 */
/* Mark authored metal as a static rope target, distinct from terrain. */
vox_result vox_world_set_fixture(vox_world *world, vox_u32 x, vox_u32 y,
                                 vox_u32 z, vox_u16 fixture);
int vox_world_is_fixture(const vox_world *world, vox_u32 x, vox_u32 y,
                         vox_u32 z);
/* Toggle a nonphysical blood stain without changing material or collision. */
vox_result vox_world_set_bloody(vox_world *world, vox_u32 x, vox_u32 y,
                                vox_u32 z, vox_u16 bloody);
/* Initialize a bounded fracture capture record before reuse. */
void vox_blast_capture_init(vox_blast_capture *capture);
/*
 * Apply the normal blast fracture while recording the pre-clear location and
 * material of each destroyed non-bedrock, non-fixture cell.  The record is
 * ordered by the deterministic fracture traversal; truncated is set when it
 * reaches capacity. structural_count records all cleared support-bearing
 * terrain, including cells beyond the bounded record.
 */
vox_result vox_world_blast_capture(vox_world *world, vox_u32 x, vox_u32 y,
                                   vox_u32 z, vox_u32 radius,
                                   vox_i32 heat_q16,
                                   vox_blast_capture *capture);

#ifdef __cplusplus
}
#endif

#endif

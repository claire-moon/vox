/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_ACCEL_H
#define VOX_ACCEL_H

#include "vox_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Deterministic FNV-1a byte hash used as a small acceleration contract probe.
 * A null pointer is valid only when length is zero. Invalid input returns zero.
 */
vox_u32 vox_fnv1a32_scalar(const void *data, vox_u32 length);
vox_u32 vox_fnv1a32(const void *data, vox_u32 length);
int vox_accel_nasm_enabled(void);

#ifdef __cplusplus
}
#endif

#endif

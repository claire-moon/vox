/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_accel.h"

#define VOX_FNV1A32_OFFSET 2166136261U
#define VOX_FNV1A32_PRIME 16777619U

#if defined(VOX_HAVE_NASM_FNV1A)
extern vox_u32 vox_fnv1a32_nasm(const void *data, vox_u32 length);
#endif

vox_u32 vox_fnv1a32_scalar(const void *data, vox_u32 length)
{
    const vox_u8 *bytes;
    vox_u32 hash;
    vox_u32 i;

    if (data == 0 && length != 0U) {
        return 0U;
    }

    bytes = (const vox_u8 *)data;
    hash = VOX_FNV1A32_OFFSET;
    for (i = 0U; i < length; ++i) {
        hash ^= (vox_u32)bytes[i];
        hash *= VOX_FNV1A32_PRIME;
    }
    return hash;
}

vox_u32 vox_fnv1a32(const void *data, vox_u32 length)
{
    if (data == 0 && length != 0U) {
        return 0U;
    }
#if defined(VOX_HAVE_NASM_FNV1A)
    return vox_fnv1a32_nasm(data, length);
#else
    return vox_fnv1a32_scalar(data, length);
#endif
}

int vox_accel_nasm_enabled(void)
{
#if defined(VOX_HAVE_NASM_FNV1A)
    return 1;
#else
    return 0;
#endif
}

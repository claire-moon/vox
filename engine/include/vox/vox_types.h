/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_TYPES_H
#define VOX_TYPES_H

#include <limits.h>

typedef signed char vox_i8;
typedef unsigned char vox_u8;
typedef signed short vox_i16;
typedef unsigned short vox_u16;
typedef signed int vox_i32;
typedef unsigned int vox_u32;

typedef char vox_require_8_bit_bytes[(CHAR_BIT == 8) ? 1 : -1];
typedef char vox_require_16_bit_short[(sizeof(vox_i16) * CHAR_BIT == 16) ?
                                       1 : -1];
typedef char vox_require_32_bit_int[(sizeof(vox_i32) * CHAR_BIT == 32) ?
                                      1 : -1];

#define VOX_ABI_VERSION 5U

typedef vox_u32 vox_result;

#define VOX_OK 0U
#define VOX_ERR_INVALID 1U
#define VOX_ERR_CAPACITY 2U

#endif

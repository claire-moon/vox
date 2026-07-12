/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_TYPES_H
#define VOX_TYPES_H

typedef signed char vox_i8;
typedef unsigned char vox_u8;
typedef signed short vox_i16;
typedef unsigned short vox_u16;
typedef signed int vox_i32;
typedef unsigned int vox_u32;

#define VOX_ABI_VERSION 1U

typedef vox_u32 vox_result;

#define VOX_OK 0U
#define VOX_ERR_INVALID 1U
#define VOX_ERR_CAPACITY 2U

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_DIGS_PRIVATE_H
#define VOX_DIGS_PRIVATE_H

/*
 * Internal DIGS representation details.
 *
 * Public ABI-11 match storage intentionally exposes its effect and replay
 * fields but not the private bit assignments used by this implementation.
 * Keep presentation and test consumers on this header rather than extending
 * the installed vox_game.h contract.
 */
#include "vox/vox_game.h"
#include "vox_kernel_private.h"

/* terrain_material[] carries this presentation flag beside its material. */
#define VOX_DIGS_REPLAY_TERRAIN_BLOODY 32768U

/* A landed effect remains presentation-only for its remaining TTL. */
#define VOX_DIGS_EFFECT_LANDED 1U
/* A terrain fragment owns one removed terrain material until it settles. */
#define VOX_DIGS_EFFECT_TERRAIN_FRAGMENT 2U
/* A stopped terrain fragment retries loose-terrain placement each tick. */
#define VOX_DIGS_EFFECT_FRAGMENT_SETTLING 4U

#endif

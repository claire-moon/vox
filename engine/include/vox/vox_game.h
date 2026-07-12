/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_GAME_H
#define VOX_GAME_H

#include "vox_kernel.h"

#define VOX_DIGS_MAX_BOTS 3U
#define VOX_DIGS_MAX_SLOTS 4U
#define VOX_DIGS_TICKS_PER_SECOND 60U

typedef enum vox_digs_phase {
    VOX_DIGS_SETUP = 0,
    VOX_DIGS_RUNNING = 1,
    VOX_DIGS_RESULTS = 2
} vox_digs_phase;

typedef struct vox_digs_rules {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 match_ticks;
    vox_u32 score_limit;
    vox_u32 lava_start_tick;
    vox_u16 bot_count;
    vox_u16 team_mode;
    vox_u32 seed;
} vox_digs_rules;

typedef struct vox_digs_match {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_digs_rules rules;
    vox_world world;
    vox_u32 tick;
    vox_u32 state_hash;
    vox_digs_phase phase;
    vox_u16 scores[VOX_DIGS_MAX_SLOTS];
    vox_u16 alive[VOX_DIGS_MAX_SLOTS];
    vox_u32 lava_level_q16;
} vox_digs_match;

void vox_digs_rules_classic(vox_digs_rules *rules);
vox_result vox_digs_match_init(vox_digs_match *match,
                               const vox_digs_rules *rules);
vox_result vox_digs_match_step(vox_digs_match *match);
vox_result vox_digs_record_kill(vox_digs_match *match, vox_u16 killer,
                                vox_u16 victim);
vox_u32 vox_digs_hash(const vox_digs_match *match);

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_game.h"

int main(void)
{
    vox_digs_rules rules;
    static vox_digs_match match;
    vox_digs_input input;
    vox_u32 i;
    vox_u16 minimum_steam = 65535U;
    vox_digs_rules_classic(&rules);
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 2;
    }
    input.abi_version = VOX_ABI_VERSION;
    input.struct_size = (vox_u32)sizeof(input);
    input.player = 0U;
    for (i = 0U; i < rules.match_ticks; ++i) {
        if (i < 60U) {
            input.actions = VOX_DIGS_ACTION_RIGHT;
        } else if (i == 60U) {
            input.actions = (vox_u16)(VOX_DIGS_ACTION_RIGHT |
                                      VOX_DIGS_ACTION_JUMP);
        } else if (i < 120U) {
            input.actions = (vox_u16)(VOX_DIGS_ACTION_RIGHT |
                                      VOX_DIGS_ACTION_STEAM);
        } else {
            input.actions = 0U;
        }
        if (vox_digs_submit_input(&match, &input) != VOX_OK) {
            return 3;
        }
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 4;
        }
        if (match.steam_q16[0] < minimum_steam) {
            minimum_steam = match.steam_q16[0];
        }
    }
    printf("DIGS headless phase=%u tick=%u lava=%u player=(%ld,%ld) steam=%u min_steam=%u hash=%08x\n",
           (unsigned int)match.phase,
           (unsigned int)match.tick,
           (unsigned int)match.lava_level_q16,
           (long)match.players[0].position_x.value_q16,
           (long)match.players[0].position_y.value_q16,
           (unsigned int)match.steam_q16[0],
           (unsigned int)minimum_steam,
           (unsigned int)match.state_hash);
    return 0;
}

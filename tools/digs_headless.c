/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_game.h"

int main(void)
{
    vox_digs_rules rules;
    static vox_digs_match match;
    vox_u32 i;
    vox_digs_rules_classic(&rules);
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 2;
    }
    for (i = 0U; i < rules.match_ticks; ++i) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 3;
        }
    }
    printf("DIGS headless phase=%u tick=%u lava=%u hash=%08x\n",
           (unsigned int)match.phase,
           (unsigned int)match.tick,
           (unsigned int)match.lava_level_q16,
           (unsigned int)match.state_hash);
    return 0;
}

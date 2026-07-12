/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "vox/vox_game.h"

static int run_match(vox_u32 *hash_out)
{
    vox_digs_rules rules;
    vox_digs_match match;
    vox_u32 i;
    vox_digs_rules_classic(&rules);
    rules.seed = 0xA11CE001U;
    rules.bot_count = 3U;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        return 1;
    }
    for (i = 0U; i < rules.match_ticks; ++i) {
        if (vox_digs_match_step(&match) != VOX_OK) {
            return 2;
        }
        if (i == rules.lava_start_tick && match.lava_level_q16 != 0U) {
            return 3;
        }
        if (i == rules.lava_start_tick + 1U && match.lava_level_q16 == 0U) {
            return 4;
        }
    }
    if (match.phase != VOX_DIGS_RESULTS || match.tick != rules.match_ticks) {
        return 5;
    }
    *hash_out = match.state_hash;
    return 0;
}

int main(void)
{
    vox_digs_rules rules;
    vox_digs_match match;
    vox_u32 first;
    vox_u32 second;
    vox_digs_rules_classic(&rules);
    if (rules.match_ticks != 18000U || rules.lava_start_tick != 12600U) {
        fprintf(stderr, "classic timing mismatch\n");
        return 1;
    }
    if (vox_digs_match_init(&match, &rules) != VOX_OK ||
        vox_digs_record_kill(&match, 0U, 1U) != VOX_OK ||
        match.scores[0] != 1U) {
        fprintf(stderr, "kill attribution mismatch\n");
        return 2;
    }
    if (run_match(&first) != 0 || run_match(&second) != 0 || first != second) {
        fprintf(stderr, "DIGS determinism mismatch\n");
        return 3;
    }
    printf("DIGS deterministic hash=%08x\n", (unsigned int)first);
    return 0;
}

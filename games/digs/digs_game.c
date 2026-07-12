/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_game.h"

static vox_u32 digs_hash_mix(vox_u32 hash, vox_u32 value)
{
    hash ^= value;
    hash *= 16777619U;
    return hash;
}

void vox_digs_rules_classic(vox_digs_rules *rules)
{
    if (rules == 0) {
        return;
    }
    rules->abi_version = VOX_ABI_VERSION;
    rules->struct_size = (vox_u32)sizeof(*rules);
    rules->match_ticks = 5U * VOX_DIGS_TICKS_PER_SECOND * 60U;
    rules->score_limit = 10U;
    rules->lava_start_tick = rules->match_ticks -
                             (90U * VOX_DIGS_TICKS_PER_SECOND);
    rules->bot_count = 1U;
    rules->team_mode = 0U;
    rules->seed = 0x564F5831U;
}

static vox_result digs_validate_rules(const vox_digs_rules *rules)
{
    if (rules == 0 || rules->abi_version != VOX_ABI_VERSION ||
        rules->struct_size < (vox_u32)sizeof(*rules)) {
        return VOX_ERR_INVALID;
    }
    if (rules->match_ticks == 0U || rules->score_limit == 0U ||
        rules->lava_start_tick >= rules->match_ticks ||
        rules->bot_count > VOX_DIGS_MAX_BOTS || rules->team_mode > 1U) {
        return VOX_ERR_INVALID;
    }
    return VOX_OK;
}

vox_result vox_digs_match_init(vox_digs_match *match,
                               const vox_digs_rules *rules)
{
    vox_u16 i;
    vox_result result = digs_validate_rules(rules);
    if (match == 0 || result != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    match->abi_version = VOX_ABI_VERSION;
    match->struct_size = (vox_u32)sizeof(*match);
    match->rules = *rules;
    vox_world_init(&match->world);
    match->tick = 0U;
    match->phase = VOX_DIGS_RUNNING;
    match->lava_level_q16 = 0U;
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        match->scores[i] = 0U;
        match->alive[i] = i <= rules->bot_count ? 1U : 0U;
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_result vox_digs_match_step(vox_digs_match *match)
{
    vox_u32 remaining;
    if (match == 0 || match->abi_version != VOX_ABI_VERSION ||
        match->struct_size < (vox_u32)sizeof(*match) ||
        match->phase != VOX_DIGS_RUNNING) {
        return VOX_ERR_INVALID;
    }
    if (vox_world_step(&match->world, 0) != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    if (match->tick >= match->rules.lava_start_tick) {
        remaining = match->rules.match_ticks - match->rules.lava_start_tick;
        match->lava_level_q16 = (match->tick - match->rules.lava_start_tick) *
                                65535U / remaining;
    }
    match->tick++;
    if (match->tick >= match->rules.match_ticks) {
        match->phase = VOX_DIGS_RESULTS;
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_result vox_digs_record_kill(vox_digs_match *match, vox_u16 killer,
                                vox_u16 victim)
{
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        killer >= VOX_DIGS_MAX_SLOTS || victim >= VOX_DIGS_MAX_SLOTS ||
        !match->alive[killer] || !match->alive[victim] || killer == victim) {
        return VOX_ERR_INVALID;
    }
    match->scores[killer]++;
    match->alive[victim] = 0U;
    if (match->scores[killer] >= match->rules.score_limit) {
        match->phase = VOX_DIGS_RESULTS;
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_u32 vox_digs_hash(const vox_digs_match *match)
{
    vox_u32 hash = 2166136261U;
    vox_u16 i;
    if (match == 0) {
        return 0U;
    }
    hash = digs_hash_mix(hash, match->rules.seed);
    hash = digs_hash_mix(hash, match->tick);
    hash = digs_hash_mix(hash, (vox_u32)match->phase);
    hash = digs_hash_mix(hash, match->lava_level_q16);
    hash = digs_hash_mix(hash, vox_world_hash(&match->world));
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        hash = digs_hash_mix(hash, (vox_u32)match->scores[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->alive[i]);
    }
    return hash;
}

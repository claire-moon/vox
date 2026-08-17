/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Deterministic DIGS simulation benchmark.
 *
 * v0.0.4 requires every new system to cost work proportional to the active
 * cell frontier rather than the whole 512x320x10 slab, and to carry an
 * explicit per-tick work budget.  Enforcing that needs a measurement that
 * cannot flake, so this tool separates two kinds of output:
 *
 *   work counters  derived purely from authoritative simulation state, so
 *                  they are byte-reproducible on any host and any
 *                  optimisation level.  These are the CI regression signal.
 *   timing         wall/CPU cost, recorded for humans but never enforced,
 *                  because shared CI runners cannot measure speed honestly.
 *
 * The scenario is fixed: four miners, two of them bots, the Deepworks map,
 * the full arsenal, the carnage effect budget, periodic blasts to keep the
 * world awake, and a scripted human input pattern that exercises movement,
 * jumping, steam, weapon cycling, and firing.  Nothing here reads the
 * clock to make a decision, so the state hash is independent of timing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "vox/vox_game.h"

#define BENCH_DEFAULT_TICKS 1800U
#define BENCH_MIN_TICKS 60U
#define BENCH_MAX_TICKS 100000U
#define BENCH_SEED 0x50303033UL
#define BENCH_BLAST_PERIOD 90U
#define BENCH_STRESS_TICKS 18000U
#define BENCH_STRESS_BLAST_PERIOD 45U

typedef struct bench_counters {
    unsigned long awake_total;
    unsigned long awake_peak;
    unsigned long effects_total;
    unsigned long effects_peak;
    unsigned long projectiles_total;
    unsigned long projectiles_peak;
    unsigned long weapon_fires;
    unsigned long explosions;
    unsigned long kills;
    unsigned long crushes;
    unsigned long limb_severs;
    unsigned long rope_events;
    unsigned long ai_state_changes;
    /*
     * Lines actually spoken.  Bark pacing is a taste judgement, and a taste
     * judgement without a number attached gets retuned every playtest by
     * whoever is most recently annoyed.
     */
    unsigned long speech_lines;
    unsigned long speech_repeats;
    /*
     * Lines that arrived while the previous one was still hanging in the
     * air.  Conversation is the same words arriving in bunches, so this is
     * the number that tells clustering apart from merely talking more --
     * without it "it feels more conversational" cannot be contradicted.
     */
    unsigned long speech_in_exchange;
    unsigned long speech_last_tick;
    unsigned long unattributed_deaths;
    unsigned long hazard_damage;
    unsigned long fluids_total;
    unsigned long fluids_peak;
    unsigned long rigid_total;
    unsigned long rigid_peak;
    unsigned long structure_frontier_total;
    unsigned long structure_frontier_peak;
} bench_counters;

static void bench_track_peak(unsigned long value, unsigned long *total,
                             unsigned long *peak)
{
    *total += value;
    if (value > *peak) {
        *peak = value;
    }
}

/* Drain the 128-slot event ring every tick so nothing is lost to wrap. */
static void bench_drain_events(vox_digs_match *match, bench_counters *counters)
{
    vox_u16 ordinal;
    vox_u16 count = match->event_count;
    for (ordinal = 0U; ordinal < count; ++ordinal) {
        const vox_digs_event *event = vox_digs_event_get(match, ordinal);
        if (event == 0) {
            continue;
        }
        switch (event->type) {
        case VOX_DIGS_EVENT_WEAPON_FIRE:
            counters->weapon_fires++;
            break;
        case VOX_DIGS_EVENT_EXPLOSION:
            counters->explosions++;
            break;
        case VOX_DIGS_EVENT_KILL:
            counters->kills++;
            /*
             * A kill with no source is the environment winning: lava,
             * burial, or a fall.  Bots currently have no hazard awareness
             * at all, so this is the headline number for "bots dying to
             * nothing" and the metric an AI pass has to move.
             */
            if (event->source == VOX_DIGS_NO_PLAYER) {
                counters->unattributed_deaths++;
            }
            break;
        case VOX_DIGS_EVENT_CRUSH:
            counters->crushes++;
            break;
        case VOX_DIGS_EVENT_LIMB_SEVER:
            counters->limb_severs++;
            break;
        case VOX_DIGS_EVENT_ROPE_ATTACH:
        case VOX_DIGS_EVENT_ROPE_DETACH:
        case VOX_DIGS_EVENT_ROPE_BREAK:
        case VOX_DIGS_EVENT_ROPE_CAST:
        case VOX_DIGS_EVENT_ROPE_HIT:
            counters->rope_events++;
            break;
        case VOX_DIGS_EVENT_AI_STATE:
            counters->ai_state_changes++;
            break;
        case VOX_DIGS_EVENT_AI_BARK:
            {
                /*
                 * A short window of what has just been said, so repetition
                 * shows up as a number rather than as a complaint.
                 */
                static vox_u16 recent[12];
                static unsigned int cursor;
                unsigned int i;
                for (i = 0U; i < 12U; ++i) {
                    if (recent[i] == event->variant &&
                        counters->speech_lines != 0UL) {
                        counters->speech_repeats++;
                        break;
                    }
                }
                recent[cursor] = event->variant;
                cursor = (cursor + 1U) % 12U;
                if (counters->speech_lines != 0UL &&
                    event->tick >= counters->speech_last_tick &&
                    event->tick - counters->speech_last_tick <= 240UL) {
                    counters->speech_in_exchange++;
                }
                counters->speech_last_tick = event->tick;
                counters->speech_lines++;
            }
            break;
        case VOX_DIGS_EVENT_DAMAGE:
            if (event->source == VOX_DIGS_NO_PLAYER) {
                counters->hazard_damage++;
            }
            break;
        default:
            break;
        }
    }
    if (count != 0U) {
        (void)vox_digs_consume_events(match, count);
    }
}

/*
 * Scripted, purely tick-derived human input.  No randomness and no clock,
 * so two runs of the same tick count are identical everywhere.
 */
static void bench_script_input(vox_digs_input *input, vox_u16 player,
                               vox_u32 tick)
{
    vox_u32 phase = (tick + (vox_u32)player * 37U) % 240U;
    vox_u16 actions = 0U;
    input->player = player;
    input->selected_weapon = (vox_u16)(((tick / 120U) + player) %
                                       VOX_DIGS_TOOL_COUNT);
    if (phase < 70U) {
        actions = VOX_DIGS_ACTION_RIGHT;
        input->move_x_q15 = 32767;
    } else if (phase < 140U) {
        actions = VOX_DIGS_ACTION_LEFT;
        input->move_x_q15 = -32767;
    } else {
        input->move_x_q15 = 0;
    }
    if (phase == 70U || phase == 150U) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_JUMP);
    }
    if (phase >= 150U && phase < 175U) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_STEAM);
    }
    if ((phase % 24U) < 8U) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_FIRE);
    }
    input->actions = actions;
    input->aim_x = (vox_u16)(((tick * 7U) + (vox_u32)player * 91U) % 512U);
    input->aim_y = (vox_u16)(((tick * 3U) + (vox_u32)player * 53U) % 320U);
}

int main(int argc, char **argv)
{
    vox_digs_rules rules;
    static vox_digs_match match;
    vox_digs_input input;
    bench_counters counters;
    vox_u32 ticks = BENCH_DEFAULT_TICKS;
    vox_u32 tick;
    vox_u16 player;
    vox_u32 blast_period = BENCH_BLAST_PERIOD;
    int stress = 0;
    int argument = 1;
    clock_t started;
    clock_t finished;
    unsigned long elapsed_us;
    unsigned long per_tick_us;

    if (argc >= 2 && strcmp(argv[1], "--stress") == 0) {
        stress = 1;
        ticks = BENCH_STRESS_TICKS;
        blast_period = BENCH_STRESS_BLAST_PERIOD;
        argument++;
    }
    if (argc > argument) {
        char *end = 0;
        unsigned long requested = strtoul(argv[argument], &end, 10);
        if (end == argv[argument] || *end != '\0' ||
            requested < BENCH_MIN_TICKS || requested > BENCH_MAX_TICKS) {
            fprintf(stderr, "vox_bench: ticks must be %u..%u\n",
                    (unsigned int)BENCH_MIN_TICKS,
                    (unsigned int)BENCH_MAX_TICKS);
            return 2;
        }
        ticks = (vox_u32)requested;
        argument++;
    }
    if (argc != argument) {
        fprintf(stderr, "vox_bench: usage: vox_bench [--stress] [ticks]\n");
        return 2;
    }

    vox_digs_rules_classic(&rules);
    rules.player_count = 4U;
    rules.bot_mask = (vox_u16)((1U << 2) | (1U << 3));
    rules.map_style = VOX_DIGS_MAP_DEEPWORKS;
    rules.weapon_mask = 0x07FFU;
    rules.fx_budget = VOX_DIGS_FX_CARNAGE;
    rules.match_ticks = ticks + 1200U;
    rules.lava_start_tick = ticks / 2U;
    rules.score_limit = 0U;
    rules.respawn_delay_ticks = 0U;
    rules.seed = (vox_u32)BENCH_SEED;
    if (vox_digs_match_init(&match, &rules) != VOX_OK) {
        fprintf(stderr, "vox_bench: match init failed\n");
        return 3;
    }
    if (vox_digs_dropship_begin(&match) != VOX_OK) {
        fprintf(stderr, "vox_bench: dropship begin failed\n");
        return 3;
    }

    memset(&counters, 0, sizeof(counters));
    memset(&input, 0, sizeof(input));
    input.abi_version = VOX_ABI_VERSION;
    input.struct_size = (vox_u32)sizeof(input);

    started = clock();
    for (tick = 0U; tick < ticks && match.phase == VOX_DIGS_RUNNING; ++tick) {
        if ((tick % blast_period) == 0U) {
            vox_u32 blast_x = VOX_WORLD_WIDTH / 5U +
                (tick / blast_period * 67U) %
                (VOX_WORLD_WIDTH * 3U / 5U);
            vox_u32 blast_y = VOX_WORLD_HEIGHT * 3U / 5U;
            vox_u16 source = (vox_u16)((tick / blast_period) % 2U);
            if (match.alive[source]) {
                (void)vox_digs_use_tool(&match, source,
                                        VOX_DIGS_TOOL_FIRECRACKER,
                                        blast_x, blast_y, 0U);
            }
        }
        for (player = 0U; player < 2U; ++player) {
            if (!match.alive[player]) {
                continue;
            }
            bench_script_input(&input, player, tick);
            if (vox_digs_submit_input(&match, &input) != VOX_OK) {
                fprintf(stderr, "vox_bench: input rejected at tick %lu\n",
                        (unsigned long)tick);
                return 4;
            }
        }
        if (vox_digs_match_step(&match) != VOX_OK) {
            fprintf(stderr, "vox_bench: step failed at tick %lu\n",
                    (unsigned long)tick);
            return 5;
        }
        bench_track_peak((unsigned long)match.world.awake_cells,
                         &counters.awake_total, &counters.awake_peak);
        bench_track_peak((unsigned long)match.effect_count,
                         &counters.effects_total, &counters.effects_peak);
        bench_track_peak((unsigned long)match.projectile_count,
                         &counters.projectiles_total,
                         &counters.projectiles_peak);
        bench_track_peak((unsigned long)match.fluids.active_cells,
                         &counters.fluids_total, &counters.fluids_peak);
        bench_track_peak((unsigned long)match.ragdolls.body_count,
                         &counters.rigid_total, &counters.rigid_peak);
        bench_track_peak((unsigned long)match.structure.frontier_count,
                         &counters.structure_frontier_total,
                         &counters.structure_frontier_peak);
        bench_drain_events(&match, &counters);
    }
    finished = clock();

    if (finished < started || CLOCKS_PER_SEC <= 0) {
        elapsed_us = 0UL;
    } else {
        elapsed_us = (unsigned long)((finished - started) /
                                     (clock_t)(CLOCKS_PER_SEC / 1000000L ?
                                               CLOCKS_PER_SEC / 1000000L : 1L));
    }
    per_tick_us = tick != 0U ? elapsed_us / (unsigned long)tick : 0UL;

    printf("# VOX + DIGS deterministic simulation benchmark\n");
    printf("scenario=%s\n", stress ?
           "deepworks_four_miner_destruction_stress" :
           "deepworks_four_miner_carnage");
    printf("seed=%08lx\n", (unsigned long)BENCH_SEED);
    printf("ticks_requested=%lu\n", (unsigned long)ticks);
    printf("ticks_run=%lu\n", (unsigned long)tick);
    printf("# deterministic work counters -- enforced\n");
    printf("state_hash=%08lx\n", (unsigned long)match.state_hash);
    printf("world_hash=%08lx\n", (unsigned long)vox_world_hash(&match.world));
    printf("awake_peak=%lu\n", counters.awake_peak);
    printf("awake_mean=%lu\n",
           tick != 0U ? counters.awake_total / (unsigned long)tick : 0UL);
    printf("effects_peak=%lu\n", counters.effects_peak);
    printf("effects_mean=%lu\n",
           tick != 0U ? counters.effects_total / (unsigned long)tick : 0UL);
    printf("projectiles_peak=%lu\n", counters.projectiles_peak);
    printf("fluids_peak=%lu\n", counters.fluids_peak);
    printf("fluids_mean=%lu\n",
           tick != 0U ? counters.fluids_total / (unsigned long)tick : 0UL);
    printf("rigid_bodies_peak=%lu\n", counters.rigid_peak);
    printf("rigid_bodies_mean=%lu\n",
           tick != 0U ? counters.rigid_total / (unsigned long)tick : 0UL);
    printf("structure_frontier_peak=%lu\n", counters.structure_frontier_peak);
    printf("structure_frontier_mean=%lu\n",
           tick != 0U ? counters.structure_frontier_total /
           (unsigned long)tick : 0UL);
    printf("occupied_cells=%lu\n", (unsigned long)match.world.occupied_cells);
    printf("weapon_fires=%lu\n", counters.weapon_fires);
    printf("explosions=%lu\n", counters.explosions);
    printf("kills=%lu\n", counters.kills);
    printf("crushes=%lu\n", counters.crushes);
    printf("limb_severs=%lu\n", counters.limb_severs);
    printf("rope_events=%lu\n", counters.rope_events);
    printf("speech_lines=%lu\n", counters.speech_lines);
    printf("speech_repeats=%lu\n", counters.speech_repeats);
    printf("speech_in_exchange=%lu\n", counters.speech_in_exchange);
    printf("ai_state_changes=%lu\n", counters.ai_state_changes);
    printf("unattributed_deaths=%lu\n", counters.unattributed_deaths);
    printf("hazard_damage=%lu\n", counters.hazard_damage);
    printf("# timing -- advisory, never enforced\n");
    printf("cpu_total_us=%lu\n", elapsed_us);
    printf("cpu_per_tick_us=%lu\n", per_tick_us);
    return 0;
}

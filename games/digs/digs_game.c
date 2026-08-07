/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_game.h"
#include "digs_lines.h"

#define DIGS_DENSITY_SCALE 2U
#define DIGS_SCALE(value) ((value) * DIGS_DENSITY_SCALE)
#define DIGS_RUN_SPEED_Q16 49152L
#define DIGS_GROUND_ACCEL_Q16 8192L
#define DIGS_AIR_ACCEL_Q16 4096L
#define DIGS_GROUND_DECEL_Q16 12288L
#define DIGS_AIR_DECEL_Q16 2048L
#define DIGS_JUMP_SPEED_Q16 (-106496L)
#define DIGS_JUMP_HOLD_ACCEL_Q16 3072L
#define DIGS_JUMP_HOLD_TICKS 8U
#define DIGS_COYOTE_TICKS 6U
#define DIGS_JUMP_BUFFER_TICKS 6U
/*
 * Burial budget.
 *
 * Collapsing terrain that engulfs a miner used to kill instantly and
 * silently on the tick that overlap recovery failed, which made ordinary
 * tunnelling feel arbitrarily lethal: dig, the roof settles, die, with no
 * signal and sometimes with the kill credited to whoever last hit you.
 *
 * Burial is now a survivable emergency.  The miner keeps their controls and
 * their tools while entombed, so digging free is real agency, and the crush
 * pressure is what kills.  Two health per tick gives a miner at full health
 * fifty ticks -- five sixths of a second -- to react, and less when already
 * wounded.  The lethal cap bounds the state so a miner cannot be pinned
 * indefinitely by a pocket that never resolves.
 *
 * Deliberately unchanged: overlap recovery still searches only two cells, so
 * a miner is never teleported out of trouble.  That was the v0.0.2 behaviour
 * v0.0.3 removed on purpose.
 */
#define DIGS_BURIED_DAMAGE_PER_TICK 2U
#define DIGS_BURIED_LETHAL_TICKS 180U
#define DIGS_STEAM_ACCEL_Q16 11264L
#define DIGS_STEAM_MAX_RISE_Q16 (-98304L)
#define DIGS_STEAM_LATERAL_Q16 3072L
#define DIGS_STEAM_USE_Q16 728U
#define DIGS_STEAM_RECHARGE_Q16 728U
#define DIGS_ROPE_MIN_LENGTH_Q16 (3L << 16)
#define DIGS_ROPE_MAX_LENGTH_Q16 (48L << 16)
#define DIGS_ROPE_REEL_SPEED_Q16 16384L
#define DIGS_ROPE_PULL_Q16 32768L
#define DIGS_ROPE_BREAK_TENSION_Q16 (5L << 16)
#define DIGS_SWEEP_STEP_Q16 8192L
#define DIGS_ROPE_HOOK_SPEED_Q16 (3L << 16)
#define DIGS_ROPE_HOOK_KNOCKBACK_Q16 49152L
#define DIGS_ROPE_WRAP_CLEARANCE_Q16 4096L
#define DIGS_RAIL_MAX_CHARGE_TICKS 72U
#define DIGS_RAIL_MIN_DAMAGE 20U
#define DIGS_RAIL_MAX_DAMAGE 100U
#define DIGS_RAIL_COOLDOWN_TICKS 75U
#define DIGS_RAIL_START_ENERGY 180U
#define DIGS_RAIL_MIN_ENERGY 12U
#define DIGS_RAIL_PLAYER_COST 52U
#define DIGS_RAIL_SOFT_COST 5U
#define DIGS_RAIL_STONE_COST 72U
#define DIGS_RAIL_RECOIL_Q16 98304L
#define DIGS_PROJECTILE_GRAVITY_Q16 6144L
#define DIGS_PULASKI_CHARGE_TICKS 30U
#define DIGS_PULASKI_RETURN_TICKS 24U
#define DIGS_BOLT_CHARGE_TICKS 30U
#define DIGS_BOLT_OVERHEAT_TICKS 150U
#define DIGS_BOLT_STREAK_LIMIT 4U
#define DIGS_FIRECRACKER_CHARGE_TICKS 60U
#define DIGS_SMOKER_TRAIL_TICKS 4U
/*
 * A direct canister strike hurts hard but is never lethal from full health:
 * 40 sits below the head's 45, the smallest vital part, so no single hit can
 * zero a vital part, and whole-body health lands at 60.  Limbs can still be
 * taken off, which is the intended "that hurt" moment.
 */
#define DIGS_SMOKER_DIRECT_DAMAGE 40U
/* Gentle downward nudge so a spent canister drops clear and rolls. */
#define DIGS_SMOKER_SETTLE_FALL_Q16 4096L
/* Charge window for the held throw, matching the firecracker's feel. */
#define DIGS_SMOKER_CHARGE_TICKS 45U
#define DIGS_HOT_RAIL_RANGE_CELLS 14U
#define DIGS_HYDROSHOT_RANGE_CELLS 18U
#define DIGS_REACTION_SAMPLES 128U
#define DIGS_AI_MEMORY_TICKS 180U
#define DIGS_AI_HEARING_TICKS 120U
/*
 * Gunfire carries 52 cells.  An explosion carries across the arena -- it is
 * the loudest thing in the mine and the surest sign of where the fight is.
 * Bots use it to choose where to roam, which is the difference between three
 * bots wandering separate pockets and three bots converging on trouble.
 */
#define DIGS_AI_COMMOTION_CELLS 220U
/* How long a roam destination stands before it is reconsidered. */
#define DIGS_AI_ROAM_GOAL_TICKS 300U
#define DIGS_AI_ROAM_ARRIVE_CELLS 6U
/*
 * Boring through an obstacle.
 *
 * A bore may be at most 2 * VOX_STRUCTURE_COHESION_CELLS + 1 cells thick.
 * That is exactly the span the structural rule will hold up, so a tunnel this
 * wide stays open; anything thicker caves in on the miner digging it, which
 * is precisely the crush death the burial work made survivable but did not
 * make pleasant.
 */
/*
 * Widest wall a miner will bore through, in cells.
 *
 * This is not a taste knob -- it is bounded by the structural rule.  A cell
 * is held up when solid material is within VOX_STRUCTURE_COHESION_CELLS
 * horizontally, so the middle of a hole of width W needs (W + 1) / 2 <= 4,
 * i.e. W <= 7.  Set to 9 the bots dug tunnels right at the collapse edge and
 * stood in them: crushes went from 9 to 24 over a 10800-tick soak and hazard
 * damage nearly doubled.  Six keeps a cell of margin.
 */
#define DIGS_AI_BREACH_MAX_CELLS 6U
#define DIGS_AI_BREACH_PROBE_ROWS 3U
#define DIGS_AI_BREACH_GIVEUP_TICKS 120U
/*
 * Quiet period after a bore before this miner may start another.
 *
 * Digging is the one thing a bot does that rearranges the world it is
 * standing in, and cohesion only holds a span of about nine cells, so a miner
 * that bores continuously eventually brings a roof down on itself.  Left
 * unbounded that cost the soak 14 extra crushes and nearly doubled hazard
 * damage -- the very "bots killing themselves randomly" this release set out
 * to fix.  Five seconds between bores keeps digging a deliberate act.
 */
#define DIGS_AI_BREACH_LOCK_TICKS 300U
/* How readily an archetype reaches for a tool rather than walking around. */
/*
 * How long a miner must be pinned against terrain before reaching for a tool.
 *
 * These were a quarter of a second, which is nothing -- a walking bot is
 * BLOCKED_X against some lip or boulder constantly, so bots bored their way
 * through the match and brought roofs down on themselves.  Digging has to be
 * the last resort *after* the jump reflex has had time to fail, so the floor
 * is a second and a half and the least eager archetype waits nearly three.
 */
#define DIGS_AI_STUCK_MIN_TICKS 90U
#define DIGS_AI_STUCK_SPAN_TICKS 96U
/*
 * How often a bot deliberates.
 *
 * The stored counter is one less than the period: the throttle decrements and
 * returns before it resets, so storing N yields N+1 ticks between
 * deliberations.  The old fixed value of 8 therefore ran at a true period of
 * 9, and every cadence figure derived from it was off by one.
 *
 * Period is personality-driven.  An impatient miner re-decides often and looks
 * twitchy; a patient one commits to a choice and looks deliberate.  This is
 * what makes CINDER frantic and RIVET measured, and it is the first use of the
 * patience trait, which was dead weight until now.
 *
 *   period = MIN + patience * SPAN / 255      CINDER 5, FLAMEY 9, RIVET 13
 */
#define DIGS_AI_PERIOD_MIN_TICKS 4U
#define DIGS_AI_PERIOD_SPAN_TICKS 12U
/*
 * Separate from the period: these are the widths of the duty-cycle windows the
 * fire and bark gates test against.  They happen to have been the same number
 * as the old period, which made the coupling look intentional when it was not.
 */
#define DIGS_AI_FIRE_WINDOW_TICKS 8U
#define DIGS_AI_BARK_WINDOW_TICKS 8U
/*
 * Retreat entry is aggression-scaled: a berserker fights until almost dead, an
 * engineer withdraws while it still has options.  RIVET 35, FLAMEY 26,
 * CINDER 13.
 */
#define DIGS_AI_RETREAT_HEALTH_MIN 12U
#define DIGS_AI_RETREAT_HEALTH_SPAN 36U
/*
 * A retreat is time-boxed, not health-boxed.  Nothing in the simulation heals
 * a living miner -- health is only ever raised at match init and respawn -- so
 * an exit condition phrased as "health recovered" is unreachable and a bot
 * that once dropped below its threshold would retreat forever.  It withdraws
 * for at most MAX ticks, then must fight for LOCK ticks before it may
 * withdraw again.
 */
#define DIGS_AI_RETREAT_MAX_TICKS 120U
#define DIGS_AI_RETREAT_LOCK_TICKS 180U
/*
 * Minimum ticks in a mode before a downgrade is allowed.  Escalations --
 * anything urgent -- bypass this, so a bot never ignores an enemy that just
 * appeared or a wound that just landed.  Without it modes flipped on every
 * deliberation, which read as twitching rather than deciding.
 */
#define DIGS_AI_DWELL_ROAMING 24U
#define DIGS_AI_DWELL_SEARCHING 20U
#define DIGS_AI_DWELL_ATTACKING 16U
#define DIGS_AI_DWELL_RETREATING 45U
/*
 * How far a bot looks for lava, in cells, before and after caution scaling.
 * A cautious miner starts backing away roughly twice as early as a reckless
 * one, which is most of what separates RIVET from CINDER in a hot cavern.
 */
/* Hazard level at which survival pre-empts the decision cadence entirely. */
#define DIGS_AI_HAZARD_CRITICAL 2U
/* Keep a little steam in reserve rather than arriving out of thrust. */
#define DIGS_AI_HAZARD_STEAM_RESERVE 4096U
#define DIGS_AI_HAZARD_BASE_MARGIN 4U
#define DIGS_AI_HAZARD_CAUTION_SPAN 8U
#define DIGS_SPAWN_HEADROOM_CELLS DIGS_SCALE(2U)
#define DIGS_SPAWN_SUPPORT_CELLS DIGS_SCALE(2U)
#define DIGS_LAVA_BASIN_TOP (VOX_WORLD_HEIGHT - DIGS_SCALE(12U))
#define DIGS_RESPAWN_RETRY_TICKS 30U
/*
 * Health returned to a miner for a kill.
 *
 * This is the first and only path in the simulation that raises the health of
 * a living miner -- every other write decrements or zeroes it -- so it is
 * deliberately modest and capped at full.  It also means any AI rule that
 * waits for health to recover is now merely rare rather than unreachable.
 */
#define DIGS_KILL_HEAL 50U

/*
 * Contracts: the running account between each pair of miners.
 *
 * Valence is the raw balance and tone is the band it falls in.  Two things
 * keep the tone from chattering the way AI modes used to flip every eight
 * ticks: a minimum dwell before any change, and a margin that makes leaving
 * a tone harder than entering it.  Without the margin a pair sitting exactly
 * on a boundary would oscillate every time a stray pebble hit somebody.
 *
 * Valence also creeps back toward zero while nothing is happening, so a
 * grudge earned in the first minute does not lock the rest of the match --
 * this is the mechanism by which peace is reachable at all.
 */
#define DIGS_CONTRACT_VALENCE_MAX 1000
#define DIGS_CONTRACT_BONDED_AT 600
#define DIGS_CONTRACT_TRUCE_AT 300
#define DIGS_CONTRACT_WARM_AT 100
#define DIGS_CONTRACT_NEEDLE_AT (-100)
#define DIGS_CONTRACT_HOSTILE_AT (-300)
#define DIGS_CONTRACT_FEUD_AT (-600)
#define DIGS_CONTRACT_TONE_MARGIN 40
#define DIGS_CONTRACT_TONE_DWELL_TICKS 180U
#define DIGS_CONTRACT_DECAY_TICKS 300U
#define DIGS_CONTRACT_DECAY_STEP 4

/*
 * Speech.  A miner does not answer instantly -- the pause before a reply is
 * one of the more legible things about a personality, so it comes from
 * patience, and how often they bother to speak at all comes from sociability.
 */
#define DIGS_SPEECH_DELAY_MIN 6U
#define DIGS_SPEECH_DELAY_SPAN 60U
#define DIGS_SPEECH_COOLDOWN_MIN 45U
#define DIGS_SPEECH_COOLDOWN_SPAN 150U
#define DIGS_SPEECH_REPLY_WINDOW 240U
/* Silence between any two lines, from anyone.  One voice at a time. */
/*
 * Two floors, because a conversation is not evenly spaced.
 *
 * A single 150-tick floor spread every line the same distance apart, which
 * reads as four people taking turns at a metronome rather than talking.
 * Inside an exchange the gap is short so an answer lands while the first
 * line is still hanging in the air; once an exchange dies out, the long
 * floor buys the silence that makes the next one feel like it started.
 */
#define DIGS_SPEECH_FLOOR_IN_EXCHANGE 42U
#define DIGS_SPEECH_FLOOR_BETWEEN 480U
/* How long a conversation stays live after the last thing said in it. */
/*
 * How long an exchange stays live after a line, on top of how long that line
 * took to say.
 *
 * This was a flat 200 ticks, which is shorter than a long line takes to
 * deliver at all -- so a patient miner, whose whole character is waiting for
 * you to finish, had its reply queued for longer than the exchange existed.
 * The window tore down underneath it, the between-exchange floor of 480 then
 * held the queued line, and RIVET answered seven to eleven seconds late into
 * a conversation whose heat and line count had already been zeroed.  Measured
 * before this: RIVET's replies averaged 452 ticks and peaked at 680, with two
 * of five landing inside the window; CINDER, who interrupts, was always
 * inside it.  The patient archetype was structurally excluded from the one
 * feature this release is built around.
 */
#define DIGS_SPEECH_ANSWER_SLACK 200U
/*
 * Lines in one exchange before it is closed off.
 *
 * Without a cap the replies fed each other: four miners all answering each
 * other's answers ran a single conversation to sixty lines and put the match
 * back where the playtest complained about.  A remark, an answer, and
 * somebody chiming in is a conversation; ten of them is a committee.
 */
#define DIGS_SPEECH_EXCHANGE_MAX 4U
/*
 * How long a line takes to say, per character.  A forty-character remark is
 * about two seconds, which is roughly how long its bubble is up -- so a
 * reply priced against it either lets the speaker finish or deliberately
 * does not.
 */
#define DIGS_SPEECH_TICKS_PER_CHAR 3U
#define DIGS_SPEECH_READ_MARGIN 66U
#define DIGS_SPEECH_DURATION_MIN 96U
#define DIGS_SPEECH_DURATION_MAX 264U
/*
 * How much of a line a miner sits through before answering, over 255.
 * Patience is added to this, so RIVET waits past the end of what you said
 * and CINDER is in at forty per cent.
 */
#define DIGS_SPEECH_PATIENCE_FLOOR 60U
/*
 * How hot an exchange has to get before it escalates, and how cool before it
 * starts winding down.  Heat moves by one per line, in the direction of what
 * that line was worth, so it takes a couple of exchanges' worth of agreement
 * or provocation to swing -- which is what stops a single stray remark
 * turning a truce into a shouting match.
 */
#define DIGS_SPEECH_HEAT_ANGRY (-2)
#define DIGS_SPEECH_HEAT_CALM 2
/* How near the crosshair a miner has to be to be the one you are addressing. */
#define DIGS_SPEECH_AIM_CELLS 14U
/* How long your own last line stays available as context for your next. */
#define DIGS_SPEECH_SELF_WINDOW 420U

/*
 * Pacing.
 *
 * Measured before this: 72 lines over a 10800-tick match, 26 of them
 * repeating something said in the previous twelve.  A line every two and a
 * half seconds is not a conversation, it is a commentary.  The target is
 * 10-15, weighted toward things that actually happened.
 *
 * The urge interval is long and jittered; the roll that follows it is scaled
 * by sociability, so RIVET speaks up about eight times in a hundred
 * opportunities and FLAMEY nearer thirty.  Neither number is a schedule.
 */
#define DIGS_SPEECH_URGE_MIN 300U
#define DIGS_SPEECH_URGE_SPAN 420U
#define DIGS_SPEECH_URGE_SCALE 22U
/* After the player speaks, everyone is briefly much likelier to answer. */
#define DIGS_SPEECH_ANSWER_WINDOW 240U
#define DIGS_SPEECH_ANSWER_BONUS 450U
#define DIGS_SPEECH_ANSWER_MIN 30U
#define DIGS_SPEECH_ANSWER_SPAN 60U
/* Answering someone who just spoke to you is easier than starting. */
#define DIGS_SPEECH_REPLY_BONUS 200U
/* Speaking up about something aimed at somebody you have feelings about. */
#define DIGS_SPEECH_CHIME_BONUS 60U
/* How far a remark carries, and how far a mutter carries. */
#define DIGS_SPEECH_EARSHOT_CELLS 70U
#define DIGS_SPEECH_MUTTER_CELLS 14U
/* What overhearing an attack on somebody is worth.  A fraction of a hit. */
#define DIGS_SPEECH_OVERHEARD_VALENCE 4L
/* Silence this long makes the next roll a certainty, so a quiet match still
 * has voices in it without the ordinary roll having to be generous. */
#define DIGS_SPEECH_DRY_TICKS 1500U
/* At or above this urgency a thing is said whatever the dice say. */
#define DIGS_SPEECH_ALWAYS_URGENCY 200U
#define DIGS_SPEECH_EVENT_SCALE 1U
/* Re-rolls allowed before taking whatever came up. */
#define DIGS_SPEECH_PICK_TRIES 8U
#define DIGS_MUZZLE_CLEARANCE_Q16 16384L

static const vox_digs_weapon_properties digs_weapons[VOX_DIGS_TOOL_COUNT] = {
    /* The direct swing remains melee.  The charged release shares this slot
     * but is a gravity-driven returning projectile, hence the gravity flag. */
    {"PULASKI", 8U, 32U, 2U, 1152U, 0U,
     VOX_DIGS_WEAPON_MELEE | VOX_DIGS_WEAPON_GRAVITY,
     DIGS_PULASKI_CHARGE_TICKS, 0U},
    {"POPPER", 6U, 18U, 2U, 0U, 0U,
     VOX_DIGS_WEAPON_HITSCAN | VOX_DIGS_WEAPON_EXPLOSIVE, 0U, 0U},
    {"SMOKER", 60U, DIGS_SMOKER_DIRECT_DAMAGE, 5U, 768U, 90U,
     VOX_DIGS_WEAPON_PROJECTILE | VOX_DIGS_WEAPON_DEPOSIT |
     VOX_DIGS_WEAPON_GRAVITY, DIGS_SMOKER_CHARGE_TICKS, 0U},
    {"HOT RAIL", 5U, 10U, 2U, 0U, 0U,
     VOX_DIGS_WEAPON_HITSCAN | VOX_DIGS_WEAPON_DEPOSIT, 0U, 0U},
    {"HYDROSHOT", 7U, 0U, 0U, 1664U, 14U,
     VOX_DIGS_WEAPON_PROJECTILE | VOX_DIGS_WEAPON_DEPOSIT, 0U, 0U},
    {"GIANT FUCKING HAMMER", 50U, 100U, 7U, 0U, 0U,
     VOX_DIGS_WEAPON_MELEE | VOX_DIGS_WEAPON_EXPLOSIVE, 0U, 0U},
    {"BOLT ACTION", 38U, 100U, 2U, 0U, 0U,
     VOX_DIGS_WEAPON_HITSCAN | VOX_DIGS_WEAPON_PENETRATING,
     DIGS_BOLT_CHARGE_TICKS, 0U},
    {"SCATTERBRAIN", 120U, 28U, 3U, 2048U, 20U,
     VOX_DIGS_WEAPON_PROJECTILE, 0U, 0U},
    {"FIRECRACKER", 48U, 42U, 8U, 768U, 50U,
     VOX_DIGS_WEAPON_PROJECTILE | VOX_DIGS_WEAPON_EXPLOSIVE |
     VOX_DIGS_WEAPON_GRAVITY, DIGS_FIRECRACKER_CHARGE_TICKS, 0U},
    {"BORING DRILL", 10U, 100U, 3U, 0U, 0U,
     VOX_DIGS_WEAPON_MELEE | VOX_DIGS_WEAPON_PENETRATING, 0U, 0U},
    {"RAILSHOT", DIGS_RAIL_COOLDOWN_TICKS, DIGS_RAIL_MAX_DAMAGE, 0U,
     0U, 0U, VOX_DIGS_WEAPON_HITSCAN | VOX_DIGS_WEAPON_PENETRATING,
     DIGS_RAIL_MAX_CHARGE_TICKS, DIGS_RAIL_START_ENERGY}
};

static void digs_step_projectiles(vox_digs_match *match);
static void digs_step_effects(vox_digs_match *match);
static void digs_step_contracts(vox_digs_match *match);
static vox_u32 digs_abs_i32(vox_i32 value);
static void digs_contract_adjust(vox_digs_match *match, vox_u16 a, vox_u16 b,
                                 vox_i32 delta);
static void digs_contract_note(vox_digs_match *match, vox_u16 actor,
                               vox_u16 subject, vox_u16 stimulus);
static void digs_speech_prompt(vox_digs_match *match, vox_u16 speaker,
                               vox_u16 subject, vox_u16 stimulus);
static void digs_speech_set(vox_digs_match *match, vox_u16 speaker,
                            vox_u16 subject, vox_u16 stimulus);
static vox_u32 digs_speech_chattiness(const vox_digs_match *match,
                                      vox_u16 player);
static int digs_speech_roll(const vox_digs_match *match, vox_u16 player,
                            vox_u32 chance, vox_u16 salt);
static vox_u16 digs_stimulus_mirror(vox_u16 stimulus);
static vox_u16 digs_speech_chime_in(vox_u16 heard, int defends);
static vox_u16 digs_speech_self_next(vox_u16 previous);

/*
 * What the miner you are driving would say if you pressed bark right now.
 *
 * Whoever last had dealings with you, if it was recent enough to still be on
 * anyone's mind; otherwise whoever is closest; otherwise the rock.  The point
 * is that the button is always in context without ever speaking for you.
 */
static void digs_speech_player_context(vox_digs_match *match, vox_u16 player)
{
    vox_u16 other;
    vox_u16 best = VOX_DIGS_NO_PLAYER;
    vox_u32 freshest = 0U;
    vox_u32 nearest = 0xFFFFFFFFUL;
    vox_u16 stimulus = (vox_u16)VOX_DIGS_STIMULUS_IDLE;
    for (other = 0U; other < match->rules.player_count; ++other) {
        const vox_digs_contract *contract;
        vox_i32 gap;
        if (other == player || !vox_digs_player_is_active(match, other)) {
            continue;
        }
        contract = vox_digs_contract_get(match, player, other);
        if (contract != 0 &&
            contract->last_stimulus != VOX_DIGS_STIMULUS_NONE &&
            contract->last_stimulus_tick + DIGS_SPEECH_REPLY_WINDOW >=
            match->tick &&
            contract->last_stimulus_tick >= freshest) {
            freshest = contract->last_stimulus_tick;
            best = other;
            /* Answer what was done to you, not what you did. */
            stimulus = contract->last_actor == player ?
                       contract->last_stimulus :
                       digs_stimulus_mirror(contract->last_stimulus);
            if (stimulus == VOX_DIGS_STIMULUS_NONE) {
                stimulus = (vox_u16)VOX_DIGS_STIMULUS_TAUNTED;
            }
        }
        if (freshest == 0U && match->alive[other]) {
            gap = digs_abs_i32(
                (match->players[other].position_x.value_q16 >> 16) -
                (match->players[player].position_x.value_q16 >> 16));
            /*
             * Only somebody actually within earshot.  Without the limit the
             * nearest miner was picked however far away they were, so a
             * player alone at one end of the map was addressing a bot at the
             * other -- which is why talking to yourself never happened and
             * the lines came out naming somebody who was not there.
             */
            if ((vox_u32)gap < nearest &&
                (vox_u32)gap <= DIGS_SPEECH_EARSHOT_CELLS) {
                nearest = (vox_u32)gap;
                best = other;
                stimulus = (vox_u16)VOX_DIGS_STIMULUS_SPOTTED;
            }
        }
    }
    /*
     * The crosshair wins.  If you are pointing at somebody, you are talking
     * to them -- still one button, but you get to pick who you needle rather
     * than having the game decide from whatever happened last.
     */
    {
        vox_u16 aimed = VOX_DIGS_NO_PLAYER;
        vox_u32 closest = DIGS_SPEECH_AIM_CELLS + 1U;
        for (other = 0U; other < match->rules.player_count; ++other) {
            vox_u32 gap;
            if (other == player || !match->alive[other] ||
                !vox_digs_player_is_active(match, other)) {
                continue;
            }
            gap = digs_abs_i32(
                (match->players[other].position_x.value_q16 >> 16) -
                (vox_i32)match->aim_x[player]) +
                digs_abs_i32(
                (match->players[other].position_y.value_q16 >> 16) -
                (vox_i32)match->aim_y[player]);
            if (gap < closest) {
                closest = gap;
                aimed = other;
            }
        }
        if (aimed != VOX_DIGS_NO_PLAYER) {
            const vox_digs_contract *aimed_at =
                vox_digs_contract_get(match, player, aimed);
            best = aimed;
            if (aimed_at != 0 &&
                aimed_at->last_stimulus != VOX_DIGS_STIMULUS_NONE &&
                aimed_at->last_stimulus_tick + DIGS_SPEECH_REPLY_WINDOW >=
                match->tick) {
                stimulus = aimed_at->last_actor == player ?
                           aimed_at->last_stimulus :
                           digs_stimulus_mirror(aimed_at->last_stimulus);
                if (stimulus == VOX_DIGS_STIMULUS_NONE) {
                    stimulus = (vox_u16)VOX_DIGS_STIMULUS_TAUNTED;
                }
            } else {
                stimulus = (vox_u16)VOX_DIGS_STIMULUS_SPOTTED;
            }
        }
    }
    /*
     * Nothing and nobody to talk about, but you said something recently --
     * so carry on from it.  You hear yourself the same way the others hear
     * you, and without this a miner alone in the mine just fires unrelated
     * remarks at the rock.
     */
    if (best == VOX_DIGS_NO_PLAYER &&
        match->speech_self_stimulus[player] != VOX_DIGS_STIMULUS_NONE &&
        match->speech_self_tick[player] + DIGS_SPEECH_SELF_WINDOW >=
        match->tick) {
        vox_u16 carried =
            digs_speech_self_next(match->speech_self_stimulus[player]);
        if (carried != VOX_DIGS_STIMULUS_NONE) {
            stimulus = carried;
        }
    }
    match->speech_stimulus[player] = stimulus;
    match->speech_subject[player] = best;
    match->speech_delay[player] = 0U;
}


static void digs_step_speech(vox_digs_match *match);
static void digs_step_reactions(vox_digs_match *match);
static void digs_update_lava(vox_digs_match *match);
static void digs_apply_lava_hazards(vox_digs_match *match);
static void digs_environment_defeat(vox_digs_match *match, vox_u16 victim);
static void digs_spawn_effect(vox_digs_match *match, vox_u16 material,
                              vox_i32 x_q16, vox_i32 y_q16,
                              vox_i32 velocity_x_q16,
                              vox_i32 velocity_y_q16, vox_u16 ttl);
static void digs_spawn_effect_variant(vox_digs_match *match,
                                      vox_u16 material,
                                      vox_i32 x_q16, vox_i32 y_q16,
                                      vox_i32 velocity_x_q16,
                                      vox_i32 velocity_y_q16, vox_u16 ttl,
                                      vox_u16 source, vox_u16 variant);
static void digs_emit_event(vox_digs_match *match, vox_u16 type,
                            vox_u16 source, vox_u16 target,
                            vox_u16 weapon, vox_u16 material,
                            vox_i32 x_q16, vox_i32 y_q16,
                            vox_u16 magnitude, vox_u16 variant);
static void digs_init_anatomy(vox_digs_match *match, vox_u16 player);
static void digs_step_bleeding(vox_digs_match *match);
static void digs_step_rope(vox_digs_match *match, vox_u16 player);
static void digs_detach_rope(vox_digs_match *match, vox_u16 player,
                             vox_u16 event_type);
static vox_u32 digs_abs_i32(vox_i32 value);
static vox_i32 digs_div_trunc_positive(vox_i32 value, vox_u32 divisor);
static vox_i32 digs_q16_to_cell(vox_i32 value);
static vox_u32 digs_scale_lava_level(vox_u32 numerator,
                                     vox_u32 denominator);
static int digs_point_hits_player(const vox_digs_match *match,
                                  vox_u16 player, vox_i32 x_q16,
                                  vox_i32 y_q16, vox_u16 *part_out);
static int digs_segment_is_clear(const vox_world *world,
                                 vox_i32 from_x_q16,
                                 vox_i32 from_y_q16,
                                 vox_i32 to_x_q16,
                                 vox_i32 to_y_q16,
                                 vox_i32 *last_x_q16,
                                 vox_i32 *last_y_q16);
static int digs_projectile_hits_solid(const vox_world *world, vox_u32 x,
                                      vox_u32 y);
static void digs_step_weapon_input(vox_digs_match *match, vox_u16 player);
static vox_result digs_fire_rail(vox_digs_match *match, vox_u16 player,
                                 vox_u16 charge_ticks,
                                 vox_u32 target_x, vox_u32 target_y);

static vox_u32 digs_hash_mix(vox_u32 hash, vox_u32 value)
{
    hash ^= value;
    hash *= 16777619U;
    return hash;
}

static vox_u32 digs_noise(vox_u32 seed, vox_u32 x, vox_u32 y, vox_u32 salt)
{
    vox_u32 hash = 2166136261U;
    hash = digs_hash_mix(hash, seed);
    hash = digs_hash_mix(hash, x);
    hash = digs_hash_mix(hash, y);
    hash = digs_hash_mix(hash, salt);
    hash ^= hash >> 16;
    hash *= 2246822519U;
    hash ^= hash >> 13;
    return hash;
}

vox_u16 vox_digs_map_landform(vox_u16 map_style, vox_u32 seed)
{
    if (map_style >= VOX_DIGS_MAP_COUNT) {
        return VOX_DIGS_LANDFORM_COUNT;
    }
    return (vox_u16)(seed % VOX_DIGS_LANDFORM_COUNT);
}

int vox_digs_player_is_active(const vox_digs_match *match, vox_u16 player)
{
    return match != 0 && player < match->rules.player_count &&
           player < VOX_DIGS_MAX_SLOTS;
}

int vox_digs_player_is_bot(const vox_digs_match *match, vox_u16 player)
{
    return vox_digs_player_is_active(match, player) &&
           (match->rules.bot_mask & (vox_u16)(1U << player)) != 0U;
}

static vox_u16 digs_count_bits(vox_u16 value)
{
    vox_u16 count = 0U;
    while (value != 0U) {
        count = (vox_u16)(count + (value & 1U));
        value = (vox_u16)(value >> 1);
    }
    return count;
}

static void digs_emit_event(vox_digs_match *match, vox_u16 type,
                            vox_u16 source, vox_u16 target,
                            vox_u16 weapon, vox_u16 material,
                            vox_i32 x_q16, vox_i32 y_q16,
                            vox_u16 magnitude, vox_u16 variant)
{
    vox_u16 slot;
    vox_digs_event *event;
    if (match->event_count < VOX_DIGS_MAX_EVENTS) {
        slot = (vox_u16)((match->event_head + match->event_count) %
                         VOX_DIGS_MAX_EVENTS);
        match->event_count++;
    } else {
        slot = match->event_head;
        match->event_head = (vox_u16)((match->event_head + 1U) %
                                      VOX_DIGS_MAX_EVENTS);
    }
    event = &match->events[slot];
    match->event_sequence++;
    event->sequence = match->event_sequence;
    event->tick = match->tick;
    event->position_x_q16 = x_q16;
    event->position_y_q16 = y_q16;
    event->type = type;
    event->source = source;
    event->target = target;
    event->weapon = weapon;
    event->material = material;
    event->magnitude = magnitude;
    event->variant = variant;
    event->reserved = 0U;
}

const vox_digs_event *vox_digs_event_get(const vox_digs_match *match,
                                         vox_u16 ordinal)
{
    vox_u16 slot;
    if (match == 0 || ordinal >= match->event_count) {
        return 0;
    }
    slot = (vox_u16)((match->event_head + ordinal) % VOX_DIGS_MAX_EVENTS);
    return &match->events[slot];
}

vox_result vox_digs_consume_events(vox_digs_match *match, vox_u16 count)
{
    if (match == 0 || count > match->event_count) {
        return VOX_ERR_INVALID;
    }
    match->event_head = (vox_u16)((match->event_head + count) %
                                  VOX_DIGS_MAX_EVENTS);
    match->event_count = (vox_u16)(match->event_count - count);
    return VOX_OK;
}

static vox_u16 digs_part_max_health(vox_u16 part)
{
    static const vox_u16 maximums[VOX_DIGS_ANATOMY_PART_COUNT] = {
        45U, 100U, 70U, 44U, 44U, 36U, 36U, 24U, 24U,
        58U, 58U, 46U, 46U, 28U, 28U
    };
    if (part >= VOX_DIGS_ANATOMY_PART_COUNT) {
        return 1U;
    }
    return maximums[part];
}

vox_result vox_digs_anatomy_hurtbox(vox_u16 part,
                                    vox_digs_hurtbox *hurtbox)
{
    static const vox_digs_hurtbox boxes[VOX_DIGS_ANATOMY_PART_COUNT] = {
        {0L, -458752L, 196608L, 131072L, VOX_DIGS_PART_HEAD, 0U},
        {0L, -131072L, 196608L, 131072L, VOX_DIGS_PART_TORSO, 0U},
        {0L, 65536L, 196608L, 65536L, VOX_DIGS_PART_PELVIS, 0U},
        {-262144L, -131072L, 65536L, 98304L,
         VOX_DIGS_PART_LEFT_UPPER_ARM, 0U},
        {262144L, -131072L, 65536L, 98304L,
         VOX_DIGS_PART_RIGHT_UPPER_ARM, 0U},
        {-327680L, 0L, 65536L, 65536L,
         VOX_DIGS_PART_LEFT_FOREARM, 0U},
        {327680L, 0L, 65536L, 65536L,
         VOX_DIGS_PART_RIGHT_FOREARM, 0U},
        {-327680L, 98304L, 49152L, 49152L,
         VOX_DIGS_PART_LEFT_HAND, 0U},
        {327680L, 98304L, 49152L, 49152L,
         VOX_DIGS_PART_RIGHT_HAND, 0U},
        {-131072L, 163840L, 65536L, 65536L,
         VOX_DIGS_PART_LEFT_THIGH, 0U},
        {131072L, 163840L, 65536L, 65536L,
         VOX_DIGS_PART_RIGHT_THIGH, 0U},
        {-131072L, 262144L, 65536L, 65536L,
         VOX_DIGS_PART_LEFT_SHIN, 0U},
        {131072L, 262144L, 65536L, 65536L,
         VOX_DIGS_PART_RIGHT_SHIN, 0U},
        {-131072L, 344064L, 81920L, 49152L,
         VOX_DIGS_PART_LEFT_FOOT, 0U},
        {131072L, 344064L, 81920L, 49152L,
         VOX_DIGS_PART_RIGHT_FOOT, 0U}
    };
    if (hurtbox == 0 || part >= VOX_DIGS_ANATOMY_PART_COUNT) {
        return VOX_ERR_INVALID;
    }
    *hurtbox = boxes[part];
    return VOX_OK;
}

static int digs_point_hits_player(const vox_digs_match *match,
                                  vox_u16 player, vox_i32 x_q16,
                                  vox_i32 y_q16, vox_u16 *part_out)
{
    vox_u16 part;
    vox_i32 center_x = match->players[player].position_x.value_q16;
    vox_i32 center_y = match->players[player].position_y.value_q16;
    if (!match->alive[player]) {
        return 0;
    }
    for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
        vox_digs_hurtbox box;
        vox_i32 part_x;
        vox_i32 part_y;
        if ((match->anatomy[player][part].flags &
             VOX_DIGS_PART_SEVERED) != 0U ||
            vox_digs_anatomy_hurtbox(part, &box) != VOX_OK) {
            continue;
        }
        part_x = center_x + box.offset_x_q16;
        part_y = center_y + box.offset_y_q16;
        if (x_q16 >= part_x - box.half_width_q16 &&
            x_q16 <= part_x + box.half_width_q16 &&
            y_q16 >= part_y - box.half_height_q16 &&
            y_q16 <= part_y + box.half_height_q16) {
            if (part_out != 0) {
                *part_out = part;
            }
            return 1;
        }
    }
    return 0;
}

static void digs_init_anatomy(vox_digs_match *match, vox_u16 player)
{
    vox_u16 part;
    for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
        vox_digs_anatomy_part *anatomy = &match->anatomy[player][part];
        anatomy->max_health = digs_part_max_health(part);
        anatomy->health = anatomy->max_health;
        anatomy->flags = part <= VOX_DIGS_PART_TORSO ?
                         VOX_DIGS_PART_VITAL : VOX_DIGS_PART_LIMB;
        anatomy->bleed_rate_q8 = 0U;
    }
    match->bleed_accumulator_q8[player] = 0U;
    match->clot_ticks[player] = 0U;
    match->buried_ticks[player] = 0U;
}

static vox_u32 digs_abs_difference(vox_u32 left, vox_u32 right)
{
    return left > right ? left - right : right - left;
}

static vox_u32 digs_lerp_height(vox_u32 left, vox_u32 right,
                                vox_u32 offset, vox_u32 span)
{
    if (span == 0U || offset >= span) {
        return right;
    }
    if (right >= left) {
        return left + ((right - left) * offset) / span;
    }
    return left - ((left - right) * offset) / span;
}

static vox_u32 digs_rolling_height(vox_u32 seed, vox_u32 x,
                                   vox_u32 span, vox_u32 base,
                                   vox_u32 radius, vox_u32 salt)
{
    vox_u32 anchor = x / span;
    vox_u32 offset = x % span;
    vox_u32 range = radius * 2U + 1U;
    vox_u32 left_noise = digs_noise(seed, anchor, 0U, salt);
    vox_u32 right_noise = digs_noise(seed, anchor + 1U, 0U, salt);
    vox_u32 left = base + (left_noise % range) - radius;
    vox_u32 right = base + (right_noise % range) - radius;
    return digs_lerp_height(left, right, offset, span);
}

static vox_u32 digs_furnace_level(vox_u32 seed, vox_u32 sector)
{
    return VOX_WORLD_HEIGHT / 2U - 4U +
           (digs_noise(seed, sector, 0U, 413U) % 4U) * DIGS_SCALE(2U);
}

static int digs_land_bounds(vox_u32 seed, vox_u32 x, vox_u16 map_style,
                            vox_u32 *left, vox_u32 *right,
                            vox_u32 *region)
{
    vox_u16 landform = vox_digs_map_landform(map_style, seed);
    vox_u32 edge;
    if (left == 0 || right == 0 || region == 0 ||
        landform >= VOX_DIGS_LANDFORM_COUNT) {
        return 0;
    }
    if (landform == VOX_DIGS_LANDFORM_ARCHIPELAGO) {
        vox_u32 usable;
        vox_u32 span;
        vox_u32 slot;
        vox_u32 gap;
        edge = DIGS_SCALE(8U);
        if (x < edge || x >= VOX_WORLD_WIDTH - edge) {
            return 0;
        }
        usable = VOX_WORLD_WIDTH - edge * 2U;
        span = usable / 4U;
        if (span <= DIGS_SCALE(12U)) {
            return 0;
        }
        slot = (x - edge) / span;
        if (slot > 3U) {
            slot = 3U;
        }
        gap = DIGS_SCALE(7U) +
              digs_noise(seed, slot, map_style, 809U) %
              (DIGS_SCALE(3U) + 1U);
        *left = edge + slot * span + gap / 2U;
        *right = slot == 3U ? VOX_WORLD_WIDTH - edge - gap / 2U - 1U :
                 edge + (slot + 1U) * span - gap / 2U - 1U;
        *region = slot;
        return x >= *left && x <= *right;
    }
    edge = landform == VOX_DIGS_LANDFORM_CONTINENT ?
           VOX_WORLD_WIDTH / 24U : VOX_WORLD_WIDTH / 16U;
    edge += digs_noise(seed, map_style, landform, 821U) %
            (DIGS_SCALE(3U) + 1U);
    *left = edge;
    *right = VOX_WORLD_WIDTH - edge - 1U;
    *region = 0U;
    return x >= *left && x <= *right;
}

static vox_u32 digs_land_edge_distance(vox_u32 seed, vox_u32 x,
                                       vox_u16 map_style,
                                       vox_u32 *region)
{
    vox_u32 left;
    vox_u32 right;
    vox_u32 local_region = 0U;
    if (!digs_land_bounds(seed, x, map_style, &left, &right,
                          &local_region)) {
        if (region != 0) {
            *region = local_region;
        }
        return 0U;
    }
    if (region != 0) {
        *region = local_region;
    }
    return x - left < right - x ? x - left : right - x;
}

static vox_u32 digs_landform_surface(vox_u32 seed, vox_u32 x,
                                     vox_u16 map_style, vox_u32 surface)
{
    vox_u32 left;
    vox_u32 right;
    vox_u32 region;
    vox_u32 edge_distance;
    vox_u16 landform = vox_digs_map_landform(map_style, seed);
    if (!digs_land_bounds(seed, x, map_style, &left, &right, &region)) {
        return VOX_WORLD_HEIGHT;
    }
    edge_distance = x - left < right - x ? x - left : right - x;
    if (edge_distance < DIGS_SCALE(8U)) {
        surface += (DIGS_SCALE(8U) - edge_distance) *
                   DIGS_SCALE(8U) / DIGS_SCALE(8U);
    }
    if (landform == VOX_DIGS_LANDFORM_ARCHIPELAGO) {
        vox_u32 lift = digs_noise(seed, region, map_style, 839U) %
                       (DIGS_SCALE(5U) + 1U);
        if (surface > lift) {
            surface -= lift;
        }
    } else if (landform == VOX_DIGS_LANDFORM_TWIN_HILLS) {
        vox_u32 span = right - left + 1U;
        vox_u32 radius = span / 5U;
        vox_u32 first_center = left + span / 4U;
        vox_u32 second_center = left + (span * 3U) / 4U;
        vox_u32 first_distance = digs_abs_difference(x, first_center);
        vox_u32 second_distance = digs_abs_difference(x, second_center);
        vox_u32 distance = first_distance < second_distance ?
                           first_distance : second_distance;
        if (radius != 0U && distance < radius) {
            vox_u32 lift = (radius - distance) * DIGS_SCALE(10U) / radius;
            if (surface > lift) {
                surface -= lift;
            }
        }
    }
    if (surface < DIGS_SCALE(28U)) {
        surface = DIGS_SCALE(28U);
    }
    if (surface + DIGS_SCALE(18U) >= DIGS_LAVA_BASIN_TOP) {
        surface = DIGS_LAVA_BASIN_TOP - DIGS_SCALE(18U);
    }
    return surface;
}

static vox_u32 digs_surface_y(vox_u32 seed, vox_u32 x, vox_u16 map_style)
{
    vox_u32 surface;
    if (map_style == VOX_DIGS_MAP_COAL_RIDGE) {
        surface = digs_rolling_height(seed, x, DIGS_SCALE(16U),
                                      VOX_WORLD_HEIGHT / 2U +
                                      DIGS_SCALE(2U),
                                      DIGS_SCALE(4U), 101U);
    } else if (map_style == VOX_DIGS_MAP_DEEPWORKS) {
        surface = digs_rolling_height(seed, x, DIGS_SCALE(12U),
                                      VOX_WORLD_HEIGHT / 2U + 1U,
                                      DIGS_SCALE(4U), 211U);
    } else {
        vox_u32 span = DIGS_SCALE(16U);
        vox_u32 blend = DIGS_SCALE(4U);
        vox_u32 sector = x / span;
        vox_u32 offset = x % span;
        vox_u32 current = digs_furnace_level(seed, sector);
        vox_u32 previous = sector == 0U ? current :
                           digs_furnace_level(seed, sector - 1U);
        if (offset < blend) {
            surface = digs_lerp_height(previous, current, offset, blend);
        } else {
            surface = current;
        }
    }
    return digs_landform_surface(seed, x, map_style, surface);
}

static vox_u32 digs_coal_seam_depth(vox_u32 seed, vox_u32 x,
                                    vox_u32 seam)
{
    vox_u32 span = DIGS_SCALE(12U);
    vox_u32 anchor = x / span;
    vox_u32 offset = x % span;
    vox_u32 base = DIGS_SCALE(5U) + seam * DIGS_SCALE(8U);
    vox_u32 left = base + digs_noise(seed, anchor, seam, 307U) %
                          DIGS_SCALE(3U);
    vox_u32 right = base + digs_noise(seed, anchor + 1U, seam, 307U) %
                           DIGS_SCALE(3U);
    return digs_lerp_height(left, right, offset, span);
}

static vox_u32 digs_sand_drift_depth(vox_u32 seed, vox_u32 x)
{
    vox_u32 span = DIGS_SCALE(10U);
    vox_u32 segment = x / span;
    vox_u32 offset = x % span;
    vox_u32 center;
    vox_u32 distance;
    if (((segment + seed) % 3U) != 0U) {
        return 0U;
    }
    center = DIGS_SCALE(3U) +
             digs_noise(seed, segment, 0U, 349U) % DIGS_SCALE(4U);
    distance = digs_abs_difference(offset, center);
    if (distance > DIGS_SCALE(3U)) {
        return 0U;
    }
    return 1U + (DIGS_SCALE(3U) - distance) / DIGS_SCALE(2U);
}

static vox_u32 digs_deep_tunnel_y(vox_u32 seed, vox_u32 x)
{
    return digs_rolling_height(seed, x, DIGS_SCALE(16U),
                               VOX_WORLD_HEIGHT / 2U + DIGS_SCALE(14U),
                               DIGS_SCALE(2U), 503U);
}

static int digs_inside_ellipse(vox_u32 x, vox_u32 y, vox_u32 center_x,
                               vox_u32 center_y, vox_u32 radius_x,
                               vox_u32 radius_y)
{
    vox_u32 delta_x = digs_abs_difference(x, center_x);
    vox_u32 delta_y = digs_abs_difference(y, center_y);
    vox_u32 left;
    vox_u32 right;
    if (delta_x > radius_x || delta_y > radius_y) {
        return 0;
    }
    left = delta_x * delta_x * radius_y * radius_y +
           delta_y * delta_y * radius_x * radius_x;
    right = radius_x * radius_x * radius_y * radius_y;
    return left <= right;
}

static int digs_deep_void(vox_u32 seed, vox_u32 x, vox_u32 y,
                          vox_u32 surface)
{
    vox_u32 tunnel_y = digs_deep_tunnel_y(seed, x);
    vox_u32 center;
    if (x > DIGS_SCALE(3U) && x + DIGS_SCALE(3U) < VOX_WORLD_WIDTH &&
        digs_abs_difference(y, tunnel_y) <= DIGS_SCALE(2U)) {
        return 1;
    }
    for (center = DIGS_SCALE(16U); center < VOX_WORLD_WIDTH;
         center += DIGS_SCALE(32U)) {
        vox_u32 chamber_y = digs_deep_tunnel_y(seed, center);
        if (digs_inside_ellipse(x, y, center, chamber_y,
                                DIGS_SCALE(7U), DIGS_SCALE(5U))) {
            return 1;
        }
        if (digs_abs_difference(x, center) <= DIGS_SCALE(1U) &&
            y >= surface && y <= chamber_y) {
            return 1;
        }
    }
    return 0;
}

static int digs_deep_firedamp(vox_u32 seed, vox_u32 x, vox_u32 y)
{
    vox_u32 center;
    vox_u32 pocket_index = 0U;
    for (center = DIGS_SCALE(16U); center < VOX_WORLD_WIDTH;
         center += DIGS_SCALE(32U)) {
        vox_u32 chamber_y = digs_deep_tunnel_y(seed, center);
        vox_u32 shift = digs_noise(seed, pocket_index, 0U, 557U) %
                        DIGS_SCALE(5U);
        vox_u32 pocket_x = center + shift - DIGS_SCALE(2U);
        vox_u32 pocket_y = chamber_y - DIGS_SCALE(3U);
        if (digs_inside_ellipse(x, y, pocket_x, pocket_y,
                                DIGS_SCALE(2U), DIGS_SCALE(1U))) {
            return 1;
        }
        pocket_index++;
    }
    return 0;
}

static int digs_furnace_pocket(vox_u32 seed, vox_u32 x, vox_u32 y,
                               int *hot)
{
    vox_u32 pocket;
    for (pocket = 0U; pocket < 4U; ++pocket) {
        vox_u32 nominal_x = ((pocket * 2U + 1U) * VOX_WORLD_WIDTH) / 8U;
        vox_u32 jitter = digs_noise(seed, pocket, 0U, 601U) %
                         DIGS_SCALE(5U);
        vox_u32 center_x = nominal_x + jitter - DIGS_SCALE(2U);
        vox_u32 center_y = digs_surface_y(seed, center_x,
                                          VOX_DIGS_MAP_FURNACE_YARD) +
                           DIGS_SCALE(6U);
        if (digs_inside_ellipse(x, y, center_x, center_y,
                                DIGS_SCALE(4U), DIGS_SCALE(3U))) {
            *hot = y >= center_y + DIGS_SCALE(1U);
            return 1;
        }
    }
    return 0;
}

static vox_u16 digs_map_material(vox_u16 map_style, vox_u32 seed,
                                  vox_u32 x, vox_u32 y)
{
    vox_u32 surface = digs_surface_y(seed, x, map_style);
    vox_u32 depth = y >= surface ? y - surface : 0U;
    vox_u32 noise = digs_noise(seed, x, y, (vox_u32)map_style + 17U);
    if (y >= VOX_WORLD_HEIGHT - DIGS_SCALE(2U)) {
        return VOX_MAT_BEDROCK;
    }
    if (y >= DIGS_LAVA_BASIN_TOP) {
        return VOX_MAT_LAVA;
    }
    if (surface >= VOX_WORLD_HEIGHT || y < surface) {
        return VOX_MAT_AIR;
    }
    if (vox_digs_map_landform(map_style, seed) ==
        VOX_DIGS_LANDFORM_ARCHIPELAGO) {
        vox_u32 region;
        vox_u32 edge_distance = digs_land_edge_distance(seed, x, map_style,
                                                        &region);
        vox_u32 thickness = DIGS_SCALE(12U) +
                            (edge_distance < DIGS_SCALE(18U) ?
                             edge_distance : DIGS_SCALE(18U));
        (void)region;
        if (y > surface + thickness) {
            return VOX_MAT_AIR;
        }
    }
    if (map_style == VOX_DIGS_MAP_COAL_RIDGE) {
        vox_u32 drift_depth = digs_sand_drift_depth(seed, x);
        if (depth < drift_depth) {
            return VOX_MAT_SAND;
        }
        if (y == surface) {
            return VOX_MAT_BIOMASS;
        }
        if (depth >= DIGS_SCALE(3U) &&
            (digs_abs_difference(depth,
                                  digs_coal_seam_depth(seed, x, 0U)) <= 1U ||
             digs_abs_difference(depth,
                                  digs_coal_seam_depth(seed, x, 1U)) <= 1U)) {
            return VOX_MAT_COAL;
        }
        return depth > DIGS_SCALE(20U) ? VOX_MAT_STONE : VOX_MAT_SOIL;
    }
    if (map_style == VOX_DIGS_MAP_DEEPWORKS) {
        if (digs_deep_void(seed, x, y, surface)) {
            return digs_deep_firedamp(seed, x, y) ? VOX_MAT_FIREDAMP :
                                                   VOX_MAT_AIR;
        }
        if (y == surface) {
            return VOX_MAT_SOIL;
        }
        if (depth > DIGS_SCALE(3U) &&
            digs_abs_difference(depth,
                                 digs_coal_seam_depth(seed ^ 0x51ed270bU,
                                                      x, 1U)) <= 1U) {
            return VOX_MAT_COAL;
        }
        return VOX_MAT_STONE;
    }
    {
        int hot = 0;
        if (digs_furnace_pocket(seed, x, y, &hot)) {
            return hot ? VOX_MAT_LAVA : VOX_MAT_AIR;
        }
    }
    if (y == surface) {
        if ((x % DIGS_SCALE(16U)) >= DIGS_SCALE(12U)) {
            return VOX_MAT_SAND;
        }
        return VOX_MAT_METAL;
    }
    if ((x % DIGS_SCALE(16U)) < DIGS_SCALE(1U) &&
        depth < DIGS_SCALE(8U)) {
        return VOX_MAT_METAL;
    }
    if (depth > DIGS_SCALE(12U) && (noise % 19U) == 0U) {
        return VOX_MAT_COAL;
    }
    return depth > DIGS_SCALE(14U) ? VOX_MAT_STONE : VOX_MAT_SOIL;
}

static vox_result digs_set_column(vox_world *world, vox_u32 x, vox_u32 y,
                                  vox_u16 material)
{
    vox_u32 z;
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        if (vox_world_set(world, x, y, z, material, 20L << 16) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    }
    return VOX_OK;
}

static vox_result digs_add_metal_span(vox_world *world, vox_u32 left,
                                      vox_u32 right, vox_u32 y)
{
    vox_u32 x;
    if (right >= VOX_WORLD_WIDTH || y >= VOX_WORLD_HEIGHT || left > right) {
        return VOX_ERR_INVALID;
    }
    for (x = left; x <= right; ++x) {
        if (digs_set_column(world, x, y, VOX_MAT_METAL) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    }
    return VOX_OK;
}

static vox_result digs_add_upward_hanger(vox_world *world, vox_u32 x,
                                         vox_u32 rail_y, vox_u32 height)
{
    vox_u32 y;
    vox_u32 top = rail_y > height ? rail_y - height : 1U;
    for (y = top; y < rail_y; ++y) {
        if (digs_set_column(world, x, y, VOX_MAT_METAL) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    }
    return VOX_OK;
}

static int digs_find_fixture_land(vox_u32 seed, vox_u32 nominal_x,
                                  vox_u16 map_style, vox_u32 half_width,
                                  vox_u32 *fixture_x, vox_u32 *surface_y)
{
    vox_u32 radius;
    vox_u32 search_radius = DIGS_SCALE(8U);
    for (radius = 0U; radius <= search_radius; ++radius) {
        vox_u32 candidate;
        vox_u32 surface;
        if (nominal_x >= radius + half_width) {
            candidate = nominal_x - radius;
            surface = digs_surface_y(seed, candidate, map_style);
            if (surface < VOX_WORLD_HEIGHT) {
                *fixture_x = candidate;
                *surface_y = surface;
                return 1;
            }
        }
        if (radius != 0U && nominal_x + radius + half_width <
            VOX_WORLD_WIDTH) {
            candidate = nominal_x + radius;
            surface = digs_surface_y(seed, candidate, map_style);
            if (surface < VOX_WORLD_HEIGHT) {
                *fixture_x = candidate;
                *surface_y = surface;
                return 1;
            }
        }
    }
    return 0;
}

static vox_result digs_add_overhead_fixtures(vox_world *world,
                                             vox_u16 map_style,
                                             vox_u32 seed)
{
    vox_u32 center;
    vox_u32 fixture = 0U;
    vox_u32 previous_fixture_x = 0U;
    int have_previous_fixture = 0;
    for (center = DIGS_SCALE(8U);
         center + DIGS_SCALE(2U) < VOX_WORLD_WIDTH;
         center += DIGS_SCALE(8U)) {
        vox_u32 fixture_x;
        vox_u32 surface;
        vox_u32 half_width = DIGS_SCALE(1U);
        vox_u32 clearance = DIGS_SCALE(18U) +
                            digs_noise(seed, fixture, map_style, 701U) %
                            (DIGS_SCALE(3U) + 1U);
        vox_u32 rail_y;
        vox_u32 left;
        vox_u32 right;
        vox_u32 sample_x;
        vox_u32 hanger_height;
        vox_u32 cap_y;
        if (!digs_find_fixture_land(seed, center, map_style, half_width,
                                    &fixture_x, &surface)) {
            fixture++;
            continue;
        }
        if (have_previous_fixture &&
            fixture_x <= previous_fixture_x + DIGS_SCALE(2U)) {
            fixture++;
            continue;
        }
        left = fixture_x - half_width;
        right = fixture_x + half_width;
        for (sample_x = left; sample_x <= right; ++sample_x) {
            vox_u32 sample_surface = digs_surface_y(seed, sample_x,
                                                    map_style);
            if (sample_surface < surface) {
                surface = sample_surface;
            }
        }
        rail_y = surface > clearance ? surface - clearance : 2U;
        hanger_height = DIGS_SCALE(2U) +
                        digs_noise(seed, fixture, 0U, 733U) %
                        (DIGS_SCALE(2U) + 1U);
        cap_y = rail_y > hanger_height ? rail_y - hanger_height : 1U;
        /*
         * A rope fixture is a small, high T-shaped sky anchor.  Nothing is
         * allowed to descend from it toward the walking surface: long rails
         * and ground-to-rail legs turn useful traversal tools into ceilings
         * and cages.  The 36--42 cell clearance leaves the complete normal
         * jump envelope open while the 16-cell spacing keeps an anchor in
         * reach of the extended rope.
        */
        if (digs_add_metal_span(world, left, right, rail_y) != VOX_OK ||
            digs_add_upward_hanger(world, fixture_x, rail_y,
                                    hanger_height) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
        if (map_style == VOX_DIGS_MAP_FURNACE_YARD) {
            if (digs_add_metal_span(world,
                                    fixture_x - DIGS_SCALE(1U),
                                    fixture_x + DIGS_SCALE(1U),
                                    cap_y) != VOX_OK) {
                return VOX_ERR_INVALID;
            }
        }
        previous_fixture_x = fixture_x;
        have_previous_fixture = 1;
        fixture++;
    }
    return VOX_OK;
}

vox_result vox_digs_generate_map(vox_world *world, vox_u16 map_style,
                                 vox_u32 seed)
{
    vox_u32 x;
    vox_u32 y;
    if (world == 0 || map_style >= VOX_DIGS_MAP_COUNT) {
        return VOX_ERR_INVALID;
    }
    vox_world_init(world);
    for (y = 0U; y < VOX_WORLD_HEIGHT; ++y) {
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            vox_u16 material = digs_map_material(map_style, seed, x, y);
            if (material != VOX_MAT_AIR &&
                digs_set_column(world, x, y, material) != VOX_OK) {
                return VOX_ERR_INVALID;
            }
        }
    }
    if (digs_add_overhead_fixtures(world, map_style, seed) != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    return vox_world_sleep_all(world);
}

static int digs_cell_is_solid(const vox_world *world, vox_u32 x, vox_u32 y)
{
    return vox_world_collision_classify(world, x, y) ==
           VOX_WORLD_COLLISION_SOLID;
}

static int digs_spawn_floor_is_supported(const vox_world *world,
                                         vox_u32 x, vox_u32 floor_y)
{
    vox_i32 offset_x;
    vox_u32 distance;
    if (x == 0U || x + 1U >= VOX_WORLD_WIDTH ||
        floor_y < DIGS_SPAWN_HEADROOM_CELLS ||
        floor_y + DIGS_SPAWN_SUPPORT_CELLS >= VOX_WORLD_HEIGHT) {
        return 0;
    }
    for (offset_x = -1; offset_x <= 1; ++offset_x) {
        vox_u32 sample_x = (vox_u32)((vox_i32)x + offset_x);
        for (distance = 1U; distance <= DIGS_SPAWN_HEADROOM_CELLS;
             ++distance) {
            if (digs_cell_is_solid(world, sample_x, floor_y - distance)) {
                return 0;
            }
        }
        for (distance = 0U; distance < DIGS_SPAWN_SUPPORT_CELLS;
             ++distance) {
            if (!digs_cell_is_solid(world, sample_x, floor_y + distance)) {
                return 0;
            }
        }
    }
    return 1;
}

static vox_result digs_find_spawn(const vox_digs_match *match,
                                  vox_u32 preferred_x,
                                  vox_physics_body *candidate)
{
    vox_physics_step_config spawn_config = match->physics_config;
    vox_u32 attempt;
    if (candidate == 0) {
        return VOX_ERR_INVALID;
    }
    spawn_config.gravity_q16 = 0;
    for (attempt = 0U; attempt < VOX_WORLD_WIDTH; ++attempt) {
        vox_u32 x = (preferred_x + attempt * 11U) % VOX_WORLD_WIDTH;
        vox_u32 y;
        if (x == 0U || x + 1U >= VOX_WORLD_WIDTH) {
            continue;
        }
        for (y = 1U; y < VOX_WORLD_HEIGHT; ++y) {
            if (!digs_cell_is_solid(&match->world, x, y)) {
                continue;
            }
            if (!digs_spawn_floor_is_supported(&match->world, x, y)) {
                continue;
            }
            vox_physics_body_init(candidate);
            candidate->half_width_q16 *= (vox_i32)DIGS_DENSITY_SCALE;
            candidate->half_height_q16 *= (vox_i32)DIGS_DENSITY_SCALE;
            candidate->position_x.value_q16 =
                (vox_i32)(x << 16) + 32768L;
            candidate->position_y.value_q16 = (vox_i32)(y << 16) -
                                               candidate->half_height_q16;
            if (vox_physics_step_world(candidate, &match->world,
                                       &spawn_config) == VOX_OK) {
                return VOX_OK;
            }
        }
    }
    return VOX_ERR_CAPACITY;
}

static vox_result digs_spawn_player(vox_digs_match *match, vox_u16 player,
                                    vox_u32 preferred_x)
{
    vox_physics_body candidate;
    vox_result result = digs_find_spawn(match, preferred_x, &candidate);
    if (result != VOX_OK) {
        return result;
    }
    match->players[player] = candidate;
    match->respawn_target_x_q16[player] = candidate.position_x.value_q16;
    match->respawn_target_y_q16[player] = candidate.position_y.value_q16;
    return VOX_OK;
}

static void digs_prepare_respawn(vox_digs_match *match, vox_u16 player)
{
    vox_physics_body candidate;
    vox_u32 preferred_x = (vox_u32)(player + 1U) * VOX_WORLD_WIDTH /
                          (vox_u32)(match->rules.player_count + 1U);
    match->respawn_ticks[player] = match->rules.respawn_delay_ticks;
    match->respawn_ready[player] = 0U;
    match->respawn_requested[player] = 0U;
    if (digs_find_spawn(match, preferred_x, &candidate) == VOX_OK) {
        match->respawn_target_x_q16[player] =
            candidate.position_x.value_q16;
        match->respawn_target_y_q16[player] =
            candidate.position_y.value_q16;
    } else {
        match->respawn_target_x_q16[player] =
            match->players[player].position_x.value_q16;
        match->respawn_target_y_q16[player] =
            match->players[player].position_y.value_q16;
    }
}

static int digs_cell_is_rope_anchor(const vox_world *world,
                                    vox_u32 x, vox_u32 y)
{
    vox_u32 z;
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        const vox_cell *cell = vox_world_cell(world, x, y, z);
        if (cell != 0 &&
            (cell->flags & VOX_CELL_LOOSE) == 0U &&
            (cell->material == VOX_MAT_BEDROCK ||
             cell->material == VOX_MAT_STONE ||
             cell->material == VOX_MAT_METAL ||
             cell->material == VOX_MAT_COAL)) {
            return 1;
        }
    }
    return 0;
}

static vox_i32 digs_distance_approx(vox_i32 delta_x, vox_i32 delta_y)
{
    vox_u32 absolute_x = digs_abs_i32(delta_x);
    vox_u32 absolute_y = digs_abs_i32(delta_y);
    vox_u32 largest = absolute_x > absolute_y ? absolute_x : absolute_y;
    vox_u32 smallest = absolute_x > absolute_y ? absolute_y : absolute_x;
    if (largest > 2147483647U - smallest / 2U) {
        return 2147483647L;
    }
    return (vox_i32)(largest + smallest / 2U);
}

static void digs_detach_rope(vox_digs_match *match, vox_u16 player,
                             vox_u16 event_type)
{
    vox_digs_rope *rope = &match->ropes[player];
    if (rope->state == VOX_DIGS_ROPE_IDLE) {
        return;
    }
    rope->active = 0U;
    rope->state = VOX_DIGS_ROPE_IDLE;
    rope->point_count = 0U;
    rope->target_player = VOX_DIGS_NO_PLAYER;
    rope->tension_q16 = 0L;
    digs_emit_event(match, event_type, player, VOX_DIGS_NO_PLAYER,
                    VOX_DIGS_TOOL_PICK, VOX_MAT_METAL,
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    rope->integrity, 0U);
}

static int digs_segment_is_clear(const vox_world *world,
                                 vox_i32 from_x_q16,
                                 vox_i32 from_y_q16,
                                 vox_i32 to_x_q16,
                                 vox_i32 to_y_q16,
                                 vox_i32 *last_x_q16,
                                 vox_i32 *last_y_q16)
{
    vox_i32 delta_x = to_x_q16 - from_x_q16;
    vox_i32 delta_y = to_y_q16 - from_y_q16;
    vox_i32 distance = digs_distance_approx(delta_x, delta_y);
    vox_u32 steps;
    vox_u32 step;
    vox_i32 previous_x = from_x_q16;
    vox_i32 previous_y = from_y_q16;
    vox_i32 destination_x = digs_q16_to_cell(to_x_q16);
    vox_i32 destination_y = digs_q16_to_cell(to_y_q16);
    if (last_x_q16 != 0) {
        *last_x_q16 = from_x_q16;
    }
    if (last_y_q16 != 0) {
        *last_y_q16 = from_y_q16;
    }
    if (distance <= 0) {
        return 1;
    }
    steps = (vox_u32)(distance / DIGS_SWEEP_STEP_Q16);
    if ((vox_i32)(steps * (vox_u32)DIGS_SWEEP_STEP_Q16) < distance) {
        steps++;
    }
    if (steps == 0U) {
        steps = 1U;
    }
    for (step = 1U; step <= steps; ++step) {
        vox_i32 sample_x_q16 = from_x_q16 +
            digs_div_trunc_positive(delta_x, steps) * (vox_i32)step;
        vox_i32 sample_y_q16 = from_y_q16 +
            digs_div_trunc_positive(delta_y, steps) * (vox_i32)step;
        vox_i32 sample_x = digs_q16_to_cell(sample_x_q16);
        vox_i32 sample_y = digs_q16_to_cell(sample_y_q16);
        if (sample_x < 0 || sample_y < 0 ||
            sample_x >= (vox_i32)VOX_WORLD_WIDTH ||
            sample_y >= (vox_i32)VOX_WORLD_HEIGHT) {
            return 0;
        }
        if (digs_cell_is_solid(world, (vox_u32)sample_x,
                               (vox_u32)sample_y) &&
            (sample_x != destination_x || sample_y != destination_y)) {
            return 0;
        }
        previous_x = sample_x_q16;
        previous_y = sample_y_q16;
        if (last_x_q16 != 0) {
            *last_x_q16 = previous_x;
        }
        if (last_y_q16 != 0) {
            *last_y_q16 = previous_y;
        }
    }
    return 1;
}

static int digs_begin_rope_cast(vox_digs_match *match, vox_u16 player)
{
    vox_digs_rope *rope = &match->ropes[player];
    vox_i32 source_x = match->players[player].position_x.value_q16;
    vox_i32 source_y = match->players[player].position_y.value_q16;
    vox_i32 delta_x = ((vox_i32)match->aim_x[player] << 16) + 32768L -
                      source_x;
    vox_i32 delta_y = ((vox_i32)match->aim_y[player] << 16) + 32768L -
                      source_y;
    vox_i32 distance = digs_distance_approx(delta_x, delta_y);
    vox_i32 divisor;
    vox_i32 direction_x_q8;
    vox_i32 direction_y_q8;
    if (distance <= 0 || rope->state != VOX_DIGS_ROPE_IDLE) {
        return 0;
    }
    divisor = distance / 256L;
    if (divisor <= 0L) {
        divisor = 1L;
    }
    direction_x_q8 = delta_x / divisor;
    direction_y_q8 = delta_y / divisor;
    if (direction_x_q8 > 256L) direction_x_q8 = 256L;
    if (direction_x_q8 < -256L) direction_x_q8 = -256L;
    if (direction_y_q8 > 256L) direction_y_q8 = 256L;
    if (direction_y_q8 < -256L) direction_y_q8 = -256L;
    rope->hook_x_q16 = source_x;
    rope->hook_y_q16 = source_y;
    rope->hook_velocity_x_q16 =
        (DIGS_ROPE_HOOK_SPEED_Q16 * direction_x_q8) / 256L;
    rope->hook_velocity_y_q16 =
        (DIGS_ROPE_HOOK_SPEED_Q16 * direction_y_q8) / 256L;
    rope->hook_travel_q16 = 0L;
    rope->state = VOX_DIGS_ROPE_CASTING;
    rope->active = 1U;
    rope->integrity = 100U;
    rope->point_count = 0U;
    rope->target_player = VOX_DIGS_NO_PLAYER;
    digs_emit_event(match, VOX_DIGS_EVENT_ROPE_CAST, player,
                    VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_PICK,
                    VOX_MAT_METAL, source_x, source_y, 0U,
                    (vox_u16)(digs_noise(match->rules.seed, match->tick,
                                         player, 0x43415354U) & 7U));
    return 1;
}

static void digs_rope_hook_player(vox_digs_match *match, vox_u16 owner,
                                  vox_u16 victim, vox_u16 part)
{
    vox_digs_rope *rope = &match->ropes[owner];
    vox_u16 previous_health = match->health[victim];
    if (match->spawn_shield_ticks[victim] > 0U) {
        digs_emit_event(match, VOX_DIGS_EVENT_SHIELD_BLOCK, owner, victim,
                        VOX_DIGS_TOOL_PICK, VOX_MAT_METAL,
                        rope->hook_x_q16, rope->hook_y_q16,
                        previous_health, part);
    } else {
        vox_i32 impulse = rope->hook_velocity_x_q16 < 0L ?
                          -DIGS_ROPE_HOOK_KNOCKBACK_Q16 :
                          DIGS_ROPE_HOOK_KNOCKBACK_Q16;
        match->health[victim] = 1U;
        match->last_attacker[victim] = owner;
        match->last_attacker_tick[victim] = match->tick;
        match->last_damage_weapon[victim] = VOX_DIGS_TOOL_PICK;
        match->last_damage_part[victim] = part;
        match->players[victim].velocity_x.value_q16 = impulse;
        match->players[victim].velocity_y.value_q16 =
            -DIGS_ROPE_HOOK_KNOCKBACK_Q16;
        digs_emit_event(match, VOX_DIGS_EVENT_ROPE_HIT, owner, victim,
                        VOX_DIGS_TOOL_PICK, VOX_MAT_FLESH,
                        rope->hook_x_q16, rope->hook_y_q16,
                        previous_health > 1U ?
                        (vox_u16)(previous_health - 1U) : 0U, part);
    }
    rope->active = 0U;
    rope->state = VOX_DIGS_ROPE_IDLE;
    rope->point_count = 0U;
    rope->target_player = VOX_DIGS_NO_PLAYER;
}

static void digs_step_rope_cast(vox_digs_match *match, vox_u16 player)
{
    vox_digs_rope *rope = &match->ropes[player];
    vox_i32 distance = digs_distance_approx(rope->hook_velocity_x_q16,
                                            rope->hook_velocity_y_q16);
    vox_u32 steps = (vox_u32)(distance / DIGS_SWEEP_STEP_Q16);
    vox_u32 step;
    if ((vox_i32)(steps * (vox_u32)DIGS_SWEEP_STEP_Q16) < distance) {
        steps++;
    }
    if (steps == 0U) steps = 1U;
    for (step = 0U; step < steps; ++step) {
        vox_i32 x_cell;
        vox_i32 y_cell;
        vox_u16 victim;
        rope->hook_x_q16 += digs_div_trunc_positive(
            rope->hook_velocity_x_q16, steps);
        rope->hook_y_q16 += digs_div_trunc_positive(
            rope->hook_velocity_y_q16, steps);
        rope->hook_travel_q16 += digs_div_trunc_positive(distance, steps);
        x_cell = digs_q16_to_cell(rope->hook_x_q16);
        y_cell = digs_q16_to_cell(rope->hook_y_q16);
        if (x_cell < 0 || y_cell < 0 ||
            x_cell >= (vox_i32)VOX_WORLD_WIDTH ||
            y_cell >= (vox_i32)VOX_WORLD_HEIGHT) {
            digs_detach_rope(match, player, VOX_DIGS_EVENT_ROPE_BREAK);
            return;
        }
        /* Terrain wins a same-sample tie, preventing hooks through cover. */
        if (digs_cell_is_solid(&match->world, (vox_u32)x_cell,
                               (vox_u32)y_cell)) {
            if (digs_cell_is_rope_anchor(&match->world, (vox_u32)x_cell,
                                         (vox_u32)y_cell)) {
                vox_digs_rope_point *point = &rope->points[0];
                rope->anchor_x_q16 = (x_cell << 16) + 32768L;
                rope->anchor_y_q16 = (y_cell << 16) + 32768L;
                rope->length_q16 = digs_distance_approx(
                    rope->anchor_x_q16 -
                        match->players[player].position_x.value_q16,
                    rope->anchor_y_q16 -
                        match->players[player].position_y.value_q16);
                if (rope->length_q16 < DIGS_ROPE_MIN_LENGTH_Q16) {
                    rope->length_q16 = DIGS_ROPE_MIN_LENGTH_Q16;
                }
                point->position_x_q16 = rope->anchor_x_q16;
                point->position_y_q16 = rope->anchor_y_q16;
                point->previous_x_q16 = rope->anchor_x_q16;
                point->previous_y_q16 = rope->anchor_y_q16;
                rope->point_count = 1U;
                rope->state = VOX_DIGS_ROPE_ATTACHED;
                rope->tension_q16 = 0L;
                digs_emit_event(match, VOX_DIGS_EVENT_ROPE_ATTACH, player,
                                VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_PICK,
                                VOX_MAT_METAL, rope->anchor_x_q16,
                                rope->anchor_y_q16,
                                (vox_u16)(rope->length_q16 >> 16),
                                (vox_u16)(digs_noise(match->rules.seed,
                                    match->tick, player,
                                    0x524F5045U) & 7U));
            } else {
                digs_detach_rope(match, player,
                                 VOX_DIGS_EVENT_ROPE_BREAK);
            }
            return;
        }
        for (victim = 0U; victim < match->rules.player_count; ++victim) {
            vox_u16 part = VOX_DIGS_NO_PART;
            if (victim != player &&
                digs_point_hits_player(match, victim, rope->hook_x_q16,
                                       rope->hook_y_q16, &part)) {
                digs_rope_hook_player(match, player, victim, part);
                return;
            }
        }
        if (rope->hook_travel_q16 >= DIGS_ROPE_MAX_LENGTH_Q16) {
            digs_detach_rope(match, player, VOX_DIGS_EVENT_ROPE_BREAK);
            return;
        }
    }
}

static void digs_step_rope(vox_digs_match *match, vox_u16 player)
{
    vox_digs_rope *rope = &match->ropes[player];
    vox_u16 broken = 0U;
    vox_i16 reel = match->move_y_q15[player];
    vox_i32 anchor_x;
    vox_i32 anchor_y;
    vox_i32 route_length = 0L;
    vox_i32 constraint_length;
    vox_i32 constraint_x;
    vox_i32 constraint_y;
    vox_u16 point;
    if (rope->state == VOX_DIGS_ROPE_CASTING) {
        digs_step_rope_cast(match, player);
        return;
    }
    if (rope->state != VOX_DIGS_ROPE_ATTACHED ||
        rope->point_count == 0U) {
        return;
    }
    anchor_x = rope->anchor_x_q16 >> 16;
    anchor_y = rope->anchor_y_q16 >> 16;
    if (anchor_x < 0 || anchor_y < 0 ||
        anchor_x >= (vox_i32)VOX_WORLD_WIDTH ||
        anchor_y >= (vox_i32)VOX_WORLD_HEIGHT ||
        !digs_cell_is_rope_anchor(&match->world, (vox_u32)anchor_x,
                                  (vox_u32)anchor_y)) {
        digs_detach_rope(match, player, VOX_DIGS_EVENT_ROPE_BREAK);
        return;
    }
    if (reel < -4096) {
        vox_i32 amount = (DIGS_ROPE_REEL_SPEED_Q16 *
                          -(vox_i32)reel) / 32767L;
        if (rope->length_q16 - amount < DIGS_ROPE_MIN_LENGTH_Q16) {
            rope->length_q16 = DIGS_ROPE_MIN_LENGTH_Q16;
        } else {
            rope->length_q16 -= amount;
        }
    } else if (reel > 4096) {
        vox_i32 amount = (DIGS_ROPE_REEL_SPEED_Q16 *
                          (vox_i32)reel) / 32767L;
        if (rope->length_q16 + amount > DIGS_ROPE_MAX_LENGTH_Q16) {
            rope->length_q16 = DIGS_ROPE_MAX_LENGTH_Q16;
        } else {
            rope->length_q16 += amount;
        }
    }
    while (rope->point_count > 1U &&
           digs_segment_is_clear(&match->world,
               match->players[player].position_x.value_q16,
               match->players[player].position_y.value_q16,
               rope->points[rope->point_count - 2U].position_x_q16,
               rope->points[rope->point_count - 2U].position_y_q16,
               0, 0)) {
        rope->point_count--;
    }
    {
        vox_i32 last_x;
        vox_i32 last_y;
        vox_digs_rope_point *route =
            &rope->points[rope->point_count - 1U];
        if (!digs_segment_is_clear(&match->world,
                match->players[player].position_x.value_q16,
                match->players[player].position_y.value_q16,
                route->position_x_q16, route->position_y_q16,
                &last_x, &last_y) &&
            rope->point_count < VOX_DIGS_ROPE_MAX_POINTS &&
            digs_distance_approx(
                last_x - match->players[player].position_x.value_q16,
                last_y - match->players[player].position_y.value_q16) >
                DIGS_ROPE_WRAP_CLEARANCE_Q16) {
            vox_digs_rope_point *wrap =
                &rope->points[rope->point_count];
            wrap->position_x_q16 = last_x;
            wrap->position_y_q16 = last_y;
            wrap->previous_x_q16 = last_x;
            wrap->previous_y_q16 = last_y;
            rope->point_count++;
        }
    }
    for (point = 1U; point < rope->point_count; ++point) {
        route_length += digs_distance_approx(
            rope->points[point].position_x_q16 -
                rope->points[point - 1U].position_x_q16,
            rope->points[point].position_y_q16 -
                rope->points[point - 1U].position_y_q16);
    }
    constraint_length = rope->length_q16 - route_length;
    if (constraint_length < DIGS_ROPE_MIN_LENGTH_Q16) {
        constraint_length = DIGS_ROPE_MIN_LENGTH_Q16;
    }
    constraint_x = rope->points[rope->point_count - 1U].position_x_q16;
    constraint_y = rope->points[rope->point_count - 1U].position_y_q16;
    if (vox_physics_rope_constraint(&match->players[player], &match->world,
                                    constraint_x, constraint_y,
                                    constraint_length,
                                    DIGS_ROPE_PULL_Q16,
                                    DIGS_ROPE_BREAK_TENSION_Q16,
                                    &rope->tension_q16, &broken) != VOX_OK ||
        broken) {
        digs_detach_rope(match, player, VOX_DIGS_EVENT_ROPE_BREAK);
    } else if (rope->tension_q16 > (3L << 16) && rope->integrity > 0U) {
        rope->integrity--;
        if (rope->integrity == 0U) {
            digs_detach_rope(match, player, VOX_DIGS_EVENT_ROPE_BREAK);
        }
    }
}

static vox_i32 digs_run_speed(const vox_digs_match *match, vox_u16 player)
{
    vox_i32 speed = DIGS_RUN_SPEED_Q16;
    if (match->anatomy[player][VOX_DIGS_PART_LEFT_FOOT].flags &
        VOX_DIGS_PART_SEVERED) {
        speed = (speed * 7L) / 8L;
    }
    if (match->anatomy[player][VOX_DIGS_PART_RIGHT_FOOT].flags &
        VOX_DIGS_PART_SEVERED) {
        speed = (speed * 7L) / 8L;
    }
    if (match->anatomy[player][VOX_DIGS_PART_LEFT_SHIN].flags &
        VOX_DIGS_PART_SEVERED) {
        speed = (speed * 3L) / 4L;
    }
    if (match->anatomy[player][VOX_DIGS_PART_RIGHT_SHIN].flags &
        VOX_DIGS_PART_SEVERED) {
        speed = (speed * 3L) / 4L;
    }
    return speed;
}

static int digs_player_on_slippery_material(const vox_digs_match *match,
                                            vox_u16 player)
{
    vox_i32 x = digs_q16_to_cell(
        match->players[player].position_x.value_q16);
    vox_i32 y = digs_q16_to_cell(
        match->players[player].position_y.value_q16 +
        match->players[player].half_height_q16 + 32768L);
    vox_u32 z;
    if (x < 0 || y < 0 || x >= (vox_i32)VOX_WORLD_WIDTH ||
        y >= (vox_i32)VOX_WORLD_HEIGHT) {
        return 0;
    }
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        const vox_cell *cell = vox_world_cell(&match->world, (vox_u32)x,
                                               (vox_u32)y, z);
        if (cell != 0 && (cell->material == VOX_MAT_BLOOD ||
                          cell->material == VOX_MAT_WATER)) {
            return 1;
        }
    }
    return 0;
}

static void digs_apply_player_controls(vox_digs_match *match, vox_u16 player)
{
    vox_physics_body *body = &match->players[player];
    vox_u16 actions = match->player_actions[player];
    vox_u16 pressed = (vox_u16)(actions &
                                 (vox_u16)~match->previous_actions[player]);
    vox_i32 target_speed = 0L;
    vox_i16 analog_x = match->move_x_q15[player];
    vox_i32 acceleration;
    vox_i32 deceleration;
    if (body->flags & VOX_PHYSICS_BODY_GROUNDED) {
        match->coyote_ticks[player] = DIGS_COYOTE_TICKS;
    } else if (match->coyote_ticks[player] > 0U) {
        match->coyote_ticks[player]--;
    }
    if (pressed & VOX_DIGS_ACTION_JUMP) {
        match->jump_buffer_ticks[player] = DIGS_JUMP_BUFFER_TICKS;
    } else if (match->jump_buffer_ticks[player] > 0U) {
        match->jump_buffer_ticks[player]--;
    }
    if (analog_x != 0) {
        target_speed = (digs_run_speed(match, player) *
                        (vox_i32)analog_x) / 32767L;
        match->facing_right[player] = analog_x > 0 ? 1U : 0U;
    } else if ((actions & (VOX_DIGS_ACTION_LEFT | VOX_DIGS_ACTION_RIGHT)) ==
        VOX_DIGS_ACTION_LEFT) {
        target_speed = -digs_run_speed(match, player);
        match->facing_right[player] = 0U;
    } else if ((actions & (VOX_DIGS_ACTION_LEFT | VOX_DIGS_ACTION_RIGHT)) ==
               VOX_DIGS_ACTION_RIGHT) {
        target_speed = digs_run_speed(match, player);
        match->facing_right[player] = 1U;
    }
    acceleration = (body->flags & VOX_PHYSICS_BODY_GROUNDED) ?
                   DIGS_GROUND_ACCEL_Q16 : DIGS_AIR_ACCEL_Q16;
    deceleration = (body->flags & VOX_PHYSICS_BODY_GROUNDED) ?
                   DIGS_GROUND_DECEL_Q16 : DIGS_AIR_DECEL_Q16;
    if ((body->flags & VOX_PHYSICS_BODY_GROUNDED) &&
        digs_player_on_slippery_material(match, player)) {
        acceleration /= 2L;
        deceleration /= 4L;
    }
    vox_physics_accelerate_x(body, target_speed, acceleration, deceleration);
    if (match->jump_buffer_ticks[player] > 0U &&
        match->coyote_ticks[player] > 0U) {
        body->velocity_y.value_q16 = DIGS_JUMP_SPEED_Q16;
        match->jump_buffer_ticks[player] = 0U;
        match->coyote_ticks[player] = 0U;
        match->jump_hold_ticks[player] = DIGS_JUMP_HOLD_TICKS;
    } else if ((actions & VOX_DIGS_ACTION_JUMP) &&
               match->jump_hold_ticks[player] > 0U &&
               body->velocity_y.value_q16 < 0L) {
        body->velocity_y.value_q16 -= DIGS_JUMP_HOLD_ACCEL_Q16;
        match->jump_hold_ticks[player]--;
    } else if (!(actions & VOX_DIGS_ACTION_JUMP)) {
        match->jump_hold_ticks[player] = 0U;
    }
    if ((actions & VOX_DIGS_ACTION_STEAM) &&
        match->steam_q16[player] != 0U) {
        if (body->velocity_y.value_q16 >
            DIGS_STEAM_MAX_RISE_Q16 + DIGS_STEAM_ACCEL_Q16) {
            body->velocity_y.value_q16 -= DIGS_STEAM_ACCEL_Q16;
        } else {
            body->velocity_y.value_q16 = DIGS_STEAM_MAX_RISE_Q16;
        }
        if (analog_x > 4096 &&
            body->velocity_x.value_q16 < digs_run_speed(match, player)) {
            body->velocity_x.value_q16 += DIGS_STEAM_LATERAL_Q16;
        } else if (analog_x < -4096 &&
                   body->velocity_x.value_q16 >
                       -digs_run_speed(match, player)) {
            body->velocity_x.value_q16 -= DIGS_STEAM_LATERAL_Q16;
        }
        if (match->steam_q16[player] <= DIGS_STEAM_USE_Q16) {
            match->steam_q16[player] = 0U;
        } else {
            match->steam_q16[player] = (vox_u16)(match->steam_q16[player] -
                                                  DIGS_STEAM_USE_Q16);
        }
    } else if (body->flags & VOX_PHYSICS_BODY_GROUNDED) {
        if (match->steam_q16[player] >
            (vox_u16)(65535U - DIGS_STEAM_RECHARGE_Q16)) {
            match->steam_q16[player] = 65535U;
        } else {
            match->steam_q16[player] = (vox_u16)(match->steam_q16[player] +
                                                  DIGS_STEAM_RECHARGE_Q16);
        }
    }
    if ((actions & VOX_DIGS_ACTION_ROPE) &&
        match->ropes[player].state == VOX_DIGS_ROPE_IDLE) {
        (void)digs_begin_rope_cast(match, player);
    } else if (!(actions & VOX_DIGS_ACTION_ROPE) &&
               match->ropes[player].state != VOX_DIGS_ROPE_IDLE) {
        digs_detach_rope(match, player, VOX_DIGS_EVENT_ROPE_DETACH);
    }
}

void vox_digs_rules_classic(vox_digs_rules *rules)
{
    if (rules == 0) {
        return;
    }
    rules->abi_version = VOX_ABI_VERSION;
    rules->struct_size = (vox_u32)sizeof(*rules);
    rules->match_ticks = 2U * VOX_DIGS_TICKS_PER_SECOND * 60U;
    rules->score_limit = 10U;
    rules->lava_start_tick = rules->match_ticks -
                             (30U * VOX_DIGS_TICKS_PER_SECOND);
    rules->seed = 0x564F5831U;
    rules->player_count = 2U;
    rules->bot_mask = 0x0002U;
    rules->map_style = VOX_DIGS_MAP_COAL_RIDGE;
    rules->weapon_mask = (vox_u16)((1U << VOX_DIGS_TOOL_COUNT) - 1U);
    rules->fx_budget = VOX_DIGS_FX_STANDARD;
    rules->respawn_mode = VOX_DIGS_RESPAWN_AUTO;
    rules->respawn_delay_ticks = VOX_DIGS_RESPAWN_TICKS;
    rules->reserved = 0U;
}

static vox_result digs_validate_rules(const vox_digs_rules *rules)
{
    vox_u16 active_mask;
    vox_u16 bot_count;
    vox_u16 human_count;
    if (rules == 0 || rules->abi_version != VOX_ABI_VERSION ||
        rules->struct_size < (vox_u32)sizeof(*rules)) {
        return VOX_ERR_INVALID;
    }
    if (rules->match_ticks == 0U || rules->score_limit > 65535U ||
        rules->lava_start_tick >= rules->match_ticks ||
        rules->player_count == 0U ||
        rules->player_count > VOX_DIGS_MAX_SLOTS ||
        rules->map_style >= VOX_DIGS_MAP_COUNT || rules->weapon_mask == 0U ||
        (rules->weapon_mask &
         (vox_u16)~((1U << VOX_DIGS_TOOL_COUNT) - 1U)) != 0U ||
        (rules->fx_budget != VOX_DIGS_FX_RETRO &&
         rules->fx_budget != VOX_DIGS_FX_STANDARD &&
         rules->fx_budget != VOX_DIGS_FX_CARNAGE) ||
        rules->respawn_mode > VOX_DIGS_RESPAWN_ON_FIRE ||
        rules->respawn_delay_ticks >
            (vox_u16)(60U * VOX_DIGS_TICKS_PER_SECOND) ||
        rules->reserved != 0U) {
        return VOX_ERR_INVALID;
    }
    active_mask = (vox_u16)((1U << rules->player_count) - 1U);
    if ((rules->bot_mask & (vox_u16)~active_mask) != 0U) {
        return VOX_ERR_INVALID;
    }
    bot_count = digs_count_bits(rules->bot_mask);
    human_count = (vox_u16)(rules->player_count - bot_count);
    if (bot_count > VOX_DIGS_MAX_BOTS || human_count == 0U ||
        human_count > 2U) {
        return VOX_ERR_INVALID;
    }
    return VOX_OK;
}


/*
 * Identity, not slot.  RIVET is RIVET whichever seat he spawns in, and the
 * account the miners keep has to follow the person rather than the position
 * or "they remember you" is a lie the moment the roster shuffles.
 */
vox_u16 vox_digs_memory_identity(const vox_digs_match *match, vox_u16 player)
{
    vox_u16 archetype;
    if (match == 0 || !vox_digs_player_is_active(match, player)) {
        return (vox_u16)VOX_DIGS_IDENTITY_COUNT;
    }
    if (!vox_digs_player_is_bot(match, player)) {
        return (vox_u16)VOX_DIGS_IDENTITY_PLAYER;
    }
    archetype = vox_digs_bot_archetype(match, player);
    switch (archetype) {
    case VOX_DIGS_ARCHETYPE_ENGINEER:
        return (vox_u16)VOX_DIGS_IDENTITY_RIVET;
    case VOX_DIGS_ARCHETYPE_BERSERKER:
        return (vox_u16)VOX_DIGS_IDENTITY_CINDER;
    default:
        break;
    }
    return (vox_u16)VOX_DIGS_IDENTITY_FLAMEY;
}

vox_u16 vox_digs_regard_index(vox_u16 a, vox_u16 b)
{
    vox_u16 low;
    vox_u16 high;
    if (a == b || a >= VOX_DIGS_IDENTITY_COUNT ||
        b >= VOX_DIGS_IDENTITY_COUNT) {
        return (vox_u16)VOX_DIGS_MAX_PAIRS;
    }
    low = a < b ? a : b;
    high = a < b ? b : a;
    return (vox_u16)((low * (2U * VOX_DIGS_IDENTITY_COUNT - low - 1U)) / 2U +
                     (high - low - 1U));
}

void vox_digs_memory_init(vox_digs_bot_memory *memory)
{
    vox_u16 i;
    vox_u16 j;
    if (memory == 0) {
        return;
    }
    memory->abi_version = VOX_ABI_VERSION;
    memory->struct_size = (vox_u32)sizeof(*memory);
    memory->memory_version = VOX_DIGS_MEMORY_VERSION;
    memory->launch_counter = 0U;
    memory->elapsed_coarse = 0U;
    for (i = 0U; i < VOX_DIGS_IDENTITY_COUNT; ++i) {
        const vox_digs_personality *base =
            i < VOX_DIGS_ARCHETYPE_COUNT ?
            vox_digs_personality_get(i) : 0;
        if (base != 0) {
            memory->identities[i].traits = *base;
        } else {
            /* The player's own miner: middling on every axis. */
            memory->identities[i].traits.aggression = 128U;
            memory->identities[i].traits.patience = 128U;
            memory->identities[i].traits.caution = 128U;
            memory->identities[i].traits.grudge = 128U;
            memory->identities[i].traits.sociability = 128U;
            memory->identities[i].traits.reserved = 0U;
        }
        memory->identities[i].matches_played = 0U;
        memory->identities[i].wins = 0U;
        memory->identities[i].kills = 0U;
        memory->identities[i].deaths = 0U;
        memory->identities[i].reserved = 0U;
    }
    for (j = 0U; j < VOX_DIGS_MAX_PAIRS; ++j) {
        memory->regard[j].tone = (vox_u16)VOX_DIGS_TONE_NEUTRAL;
        memory->regard[j].valence = 0;
        memory->regard[j].matches_met = 0U;
        memory->regard[j].kills_for = 0U;
        memory->regard[j].kills_against = 0U;
        memory->regard[j].truces = 0U;
        memory->regard[j].betrayals = 0U;
        memory->regard[j].reserved = 0U;
    }
    (void)vox_digs_memory_hash(memory);
}

vox_u32 vox_digs_memory_hash(vox_digs_bot_memory *memory)
{
    vox_u32 hash = 2166136261U;
    vox_u16 i;
    if (memory == 0) {
        return 0U;
    }
    hash = digs_hash_mix(hash, memory->memory_version);
    /*
     * launch_counter and elapsed_coarse are deliberately NOT folded in.
     *
     * They come from the port's wall clock, and this digest is folded into
     * vox_digs_hash -- so including them put time(0) into the authoritative
     * match hash.  The simulation never reads either field, so two matches
     * one launch apart were provably identical tick for tick, with the same
     * world hash, the same scores and the same deaths, and a different state
     * hash from tick zero.  Any replay verifier or desync detector comparing
     * state hashes would have called an identical match a divergence, and a
     * recorded replay could never be reproduced by the same binary on the
     * same machine the next day.
     *
     * This digest is not persisted -- the chronicle has its own checksum --
     * so its only consumer is the match hash, and the match hash must be a
     * function of what the simulation can see.
     */
    for (i = 0U; i < VOX_DIGS_IDENTITY_COUNT; ++i) {
        const vox_digs_identity_record *r = &memory->identities[i];
        hash = digs_hash_mix(hash, (vox_u32)r->traits.aggression);
        hash = digs_hash_mix(hash, (vox_u32)r->traits.patience);
        hash = digs_hash_mix(hash, (vox_u32)r->traits.caution);
        hash = digs_hash_mix(hash, (vox_u32)r->traits.grudge);
        hash = digs_hash_mix(hash, (vox_u32)r->traits.sociability);
        hash = digs_hash_mix(hash, (vox_u32)r->matches_played);
        hash = digs_hash_mix(hash, (vox_u32)r->wins);
        hash = digs_hash_mix(hash, (vox_u32)r->kills);
        hash = digs_hash_mix(hash, (vox_u32)r->deaths);
    }
    for (i = 0U; i < VOX_DIGS_MAX_PAIRS; ++i) {
        const vox_digs_regard *g = &memory->regard[i];
        hash = digs_hash_mix(hash, (vox_u32)g->tone);
        hash = digs_hash_mix(hash, (vox_u32)(vox_u16)g->valence);
        hash = digs_hash_mix(hash, (vox_u32)g->matches_met);
        hash = digs_hash_mix(hash, (vox_u32)g->kills_for);
        hash = digs_hash_mix(hash, (vox_u32)g->kills_against);
        hash = digs_hash_mix(hash, (vox_u32)g->truces);
        hash = digs_hash_mix(hash, (vox_u32)g->betrayals);
    }
    memory->memory_hash = hash;
    return hash;
}

vox_result vox_digs_match_init(vox_digs_match *match,
                               const vox_digs_rules *rules)
{
    return vox_digs_match_init_ex(match, rules, 0);
}


/*
 * How far a trait may move in one match, per archetype.  RIVET revises
 * slowly and remembers; CINDER swings and forgets; FLAMEY is unpredictable.
 * Bounded hard, because a bot that drifts far enough stops being the
 * character the player learned, which is the opposite of the point.
 */
static const vox_u16
digs_drift_rate[VOX_DIGS_ARCHETYPE_COUNT] = {3U, 9U, 6U};
#define DIGS_DRIFT_FLOOR 40U
#define DIGS_DRIFT_CEILING 235U
/* Exchanges in a match past which a miner counts as talkative. */
#define DIGS_DRIFT_TALKATIVE 6U

static vox_u16 digs_drift_trait(vox_u16 value, vox_i32 direction,
                                vox_u16 rate)
{
    vox_i32 moved = (vox_i32)value + direction * (vox_i32)rate;
    if (moved < (vox_i32)DIGS_DRIFT_FLOOR) {
        moved = (vox_i32)DIGS_DRIFT_FLOOR;
    }
    if (moved > (vox_i32)DIGS_DRIFT_CEILING) {
        moved = (vox_i32)DIGS_DRIFT_CEILING;
    }
    return (vox_u16)moved;
}

vox_result vox_digs_match_export_memory(const vox_digs_match *match,
                                        vox_digs_bot_memory *memory)
{
    vox_u16 a;
    vox_u16 b;
    if (match == 0 || memory == 0) {
        return VOX_ERR_INVALID;
    }
    *memory = match->memory;
    memory->abi_version = VOX_ABI_VERSION;
    memory->struct_size = (vox_u32)sizeof(*memory);
    memory->memory_version = VOX_DIGS_MEMORY_VERSION;
    for (a = 0U; a < match->rules.player_count; ++a) {
        vox_u16 identity = vox_digs_memory_identity(match, a);
        vox_digs_identity_record *record;
        vox_u16 rate;
        vox_u16 archetype;
        if (identity >= VOX_DIGS_IDENTITY_COUNT) {
            continue;
        }
        record = &memory->identities[identity];
        if (record->matches_played < 65535U) {
            record->matches_played++;
        }
        if (record->kills + match->scores[a] < 65535U) {
            record->kills = (vox_u16)(record->kills + match->scores[a]);
        }
        if (record->deaths + match->deaths[a] < 65535U) {
            record->deaths = (vox_u16)(record->deaths + match->deaths[a]);
        }
        if (match->phase == VOX_DIGS_RESULTS && !match->result_draw &&
            match->winner_player == a && record->wins < 65535U) {
            record->wins++;
        }
        archetype = vox_digs_bot_archetype(match, a);
        rate = archetype < VOX_DIGS_ARCHETYPE_COUNT ?
               digs_drift_rate[archetype] : 4U;
        /*
         * Drift follows what the match actually did to them.  Small steps --
         * a nudge per match, not a personality transplant -- and every axis
         * points at a decision the AI visibly makes, so a drifted bot plays
         * differently rather than merely carrying different numbers.
         */
        {
            vox_i32 fortune = (vox_i32)match->scores[a] -
                              (vox_i32)match->deaths[a];
            vox_i32 sign = fortune > 0 ? 1L : (fortune < 0 ? -1L : 0L);
            vox_u32 talk = 0U;
            vox_u16 betrayed = 0U;
            vox_u16 other;
            for (other = 0U; other < match->rules.player_count; ++other) {
                vox_u16 pair = vox_digs_pair_index(a, other);
                const vox_digs_contract *contract;
                if (pair >= VOX_DIGS_MAX_PAIRS) {
                    continue;
                }
                contract = &match->contracts[pair];
                talk += contract->exchanges;
                if (contract->last_stimulus == VOX_DIGS_STIMULUS_BETRAYED &&
                    contract->last_actor != a) {
                    betrayed++;
                }
            }
            /* Winning makes a miner bolder and less careful. */
            record->traits.aggression =
                digs_drift_trait(record->traits.aggression, sign, rate);
            record->traits.caution =
                digs_drift_trait(record->traits.caution, -sign, rate);
            /*
             * A miner who spent the match dying stops waiting around; one who
             * survived can afford to take their time.  Patience is the
             * decision cadence, so this is visible as twitchiness.
             */
            record->traits.patience =
                digs_drift_trait(record->traits.patience, sign, rate);
            /*
             * Being betrayed is what makes a grudge, and a grudge is what
             * makes a miner hunt somebody past whoever is nearer.  Nothing
             * else earns it; winning quietly lets it fade.
             */
            record->traits.grudge = digs_drift_trait(record->traits.grudge,
                betrayed > 0U ? 1L : (sign > 0L ? -1L : 0L), rate);
            /*
             * Talking begets talking.  A match spent in conversation leaves
             * them chattier next time, a silent one leaves them quieter --
             * which feeds straight back into how often they speak up.
             */
            record->traits.sociability =
                digs_drift_trait(record->traits.sociability,
                                 talk >= DIGS_DRIFT_TALKATIVE ? 1L : -1L,
                                 rate);
        }
    }
    for (a = 0U; a < match->rules.player_count; ++a) {
        for (b = (vox_u16)(a + 1U); b < match->rules.player_count; ++b) {
            vox_u16 pair = vox_digs_pair_index(a, b);
            vox_u16 slot = vox_digs_regard_index(
                vox_digs_memory_identity(match, a),
                vox_digs_memory_identity(match, b));
            const vox_digs_contract *contract;
            vox_digs_regard *regard;
            if (pair >= VOX_DIGS_MAX_PAIRS || slot >= VOX_DIGS_MAX_PAIRS) {
                continue;
            }
            contract = &match->contracts[pair];
            regard = &memory->regard[slot];
            regard->tone = contract->tone;
            regard->valence = contract->valence;
            if (contract->met && regard->matches_met < 65535U) {
                regard->matches_met++;
            }
            if (contract->tone >= VOX_DIGS_TONE_TRUCE &&
                regard->truces < 65535U) {
                regard->truces++;
            }
            if (contract->last_stimulus == VOX_DIGS_STIMULUS_BETRAYED &&
                regard->betrayals < 65535U) {
                regard->betrayals++;
            }
        }
    }
    (void)vox_digs_memory_hash(memory);
    return VOX_OK;
}

vox_result vox_digs_match_init_ex(vox_digs_match *match,
                                  const vox_digs_rules *rules,
                                  const vox_digs_bot_memory *memory)
{
    vox_u16 i;
    vox_digs_bot_memory canonical;
    vox_result result = digs_validate_rules(rules);
    if (match == 0 || result != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    /*
     * A missing or foreign snapshot is not an error, it is a first meeting.
     * Anything that does not match this build is discarded rather than
     * reinterpreted -- a snapshot read the wrong way round would produce
     * plausible traits and a wrong match, which is worse than no memory.
     */
    if (memory == 0 || memory->abi_version != VOX_ABI_VERSION ||
        memory->struct_size < (vox_u32)sizeof(*memory) ||
        memory->memory_version != VOX_DIGS_MEMORY_VERSION) {
        vox_digs_memory_init(&canonical);
        memory = &canonical;
    }
    match->memory = *memory;
    match->abi_version = VOX_ABI_VERSION;
    match->struct_size = (vox_u32)sizeof(*match);
    match->rules = *rules;
    if (vox_digs_generate_map(&match->world, rules->map_style,
                              rules->seed) != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    {
        vox_u16 pair;
        for (pair = 0U; pair < VOX_DIGS_MAX_PAIRS; ++pair) {
            vox_u16 slot;
            match->contracts[pair].tone = (vox_u16)VOX_DIGS_TONE_NEUTRAL;
            match->contracts[pair].valence = 0;
            match->contracts[pair].tone_ticks = 0U;
            match->contracts[pair].quiet_ticks = 0U;
            match->contracts[pair].last_speaker = VOX_DIGS_NO_PLAYER;
            match->contracts[pair].exchanges = 0U;
            match->contracts[pair].met = 0U;
            match->contracts[pair].recent_cursor = 0U;
            match->contracts[pair].last_stimulus =
                (vox_u16)VOX_DIGS_STIMULUS_NONE;
            match->contracts[pair].last_actor = VOX_DIGS_NO_PLAYER;
            match->contracts[pair].last_stimulus_tick = 0U;
            for (slot = 0U; slot < VOX_DIGS_RECENT_LINES; ++slot) {
                match->contracts[pair].recent_lines[slot] = 0U;
            }
        }
    }
    /*
     * Carry the accounts in.  Two miners who left the last match hating each
     * other start this one hating each other, which is the entire point of
     * the exercise -- but the tone dwells from zero, so the first thing that
     * happens can still move it.
     */
    {
        vox_u16 a;
        vox_u16 b;
        for (a = 0U; a < match->rules.player_count; ++a) {
            for (b = (vox_u16)(a + 1U); b < match->rules.player_count; ++b) {
                vox_u16 pair = vox_digs_pair_index(a, b);
                vox_u16 slot = vox_digs_regard_index(
                    vox_digs_memory_identity(match, a),
                    vox_digs_memory_identity(match, b));
                if (pair >= VOX_DIGS_MAX_PAIRS ||
                    slot >= VOX_DIGS_MAX_PAIRS) {
                    continue;
                }
                match->contracts[pair].tone = match->memory.regard[slot].tone;
                match->contracts[pair].valence =
                    match->memory.regard[slot].valence;
                match->contracts[pair].met =
                    match->memory.regard[slot].matches_met > 0U ? 1U : 0U;
            }
        }
    }
    match->tick = 0U;
    match->phase = VOX_DIGS_RUNNING;
    match->result_reason = VOX_DIGS_END_NONE;
    match->result_draw = 0U;
    match->winner_player = VOX_DIGS_NO_PLAYER;
    match->speech_floor_ticks = 0U;
    match->speech_answer_ticks = 0U;
    match->speech_dry_ticks = 0U;
    match->speech_exchange_lines = 0U;
    match->speech_last_line = 0U;
    match->speech_exchange_heat = 0;
    match->lava_level_q16 = 0U;
    match->lava_surface_y = (vox_u16)DIGS_LAVA_BASIN_TOP;
    match->projectile_count = 0U;
    match->effect_count = 0U;
    match->effect_cursor = 0U;
    match->event_head = 0U;
    match->event_count = 0U;
    match->event_sequence = 0U;
    match->terrain_hash = vox_world_hash(&match->world);
    vox_physics_step_config_default(&match->physics_config);
    match->physics_config.gravity_q16 *= (vox_i32)DIGS_DENSITY_SCALE;
    match->physics_config.max_step_q16 *= (vox_i32)DIGS_DENSITY_SCALE;
    for (i = 0U; i < VOX_DIGS_MAX_EVENTS; ++i) {
        match->events[i].sequence = 0U;
        match->events[i].tick = 0U;
        match->events[i].position_x_q16 = 0L;
        match->events[i].position_y_q16 = 0L;
        match->events[i].type = VOX_DIGS_EVENT_NONE;
        match->events[i].source = VOX_DIGS_NO_PLAYER;
        match->events[i].target = VOX_DIGS_NO_PLAYER;
        match->events[i].weapon = VOX_DIGS_TOOL_PICK;
        match->events[i].material = VOX_MAT_AIR;
        match->events[i].magnitude = 0U;
        match->events[i].variant = 0U;
        match->events[i].reserved = 0U;
    }
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        match->scores[i] = 0U;
        match->alive[i] = i < rules->player_count ? 1U : 0U;
        match->health[i] = match->alive[i] ? VOX_DIGS_MAX_HEALTH : 0U;
        match->deaths[i] = 0U;
        match->respawn_ticks[i] = 0U;
        match->respawn_ready[i] = 0U;
        match->respawn_requested[i] = 0U;
        match->respawn_target_x_q16[i] = 0L;
        match->respawn_target_y_q16[i] = 0L;
        match->spawn_shield_ticks[i] = match->alive[i] ?
                                       VOX_DIGS_SPAWN_SHIELD_TICKS : 0U;
        match->player_actions[i] = 0U;
        match->previous_actions[i] = 0U;
        match->aim_x[i] = 0U;
        match->aim_y[i] = 0U;
        match->move_x_q15[i] = 0;
        match->move_y_q15[i] = 0;
        match->coyote_ticks[i] = 0U;
        match->jump_buffer_ticks[i] = 0U;
        match->jump_hold_ticks[i] = 0U;
        match->steam_q16[i] = 65535U;
        match->weapon_cooldown[i] = 0U;
        match->selected_weapon[i] = VOX_DIGS_TOOL_PICK;
        match->facing_right[i] = (vox_u16)(i < 2U ? 1U : 0U);
        match->last_attacker[i] = VOX_DIGS_NO_PLAYER;
        match->last_attacker_tick[i] = 0U;
        match->last_damage_weapon[i] = VOX_DIGS_TOOL_PICK;
        match->last_damage_part[i] = VOX_DIGS_NO_PART;
        match->rail_charge_ticks[i] = 0U;
        match->rail_charging[i] = 0U;
        match->speech_stimulus[i] = (vox_u16)VOX_DIGS_STIMULUS_NONE;
        match->speech_subject[i] = VOX_DIGS_NO_PLAYER;
        match->speech_delay[i] = 0U;
        match->speech_cooldown[i] = 0U;
        match->speech_urge_ticks[i] = (vox_u16)(DIGS_SPEECH_URGE_MIN +
            (digs_noise(rules->seed, 0U, i, 0xC1A7U) %
             DIGS_SPEECH_URGE_SPAN));
        match->speech_audience[i] = (vox_u16)VOX_DIGS_AUDIENCE_ONE;
        match->speech_self_stimulus[i] = (vox_u16)VOX_DIGS_STIMULUS_NONE;
        match->speech_self_tick[i] = 0U;
        match->speech_recent_cursor[i] = 0U;
        {
            vox_u16 slot;
            for (slot = 0U; slot < VOX_DIGS_SPEAKER_RECENT; ++slot) {
                match->speech_recent[i][slot] = 0U;
            }
        }
        match->weapon_charge_ticks[i] = 0U;
        match->weapon_charging[i] = 0U;
        match->bolt_shot_streak[i] = 0U;
        match->ropes[i].anchor_x_q16 = 0L;
        match->ropes[i].anchor_y_q16 = 0L;
        match->ropes[i].length_q16 = DIGS_ROPE_MIN_LENGTH_Q16;
        match->ropes[i].tension_q16 = 0L;
        match->ropes[i].hook_x_q16 = 0L;
        match->ropes[i].hook_y_q16 = 0L;
        match->ropes[i].hook_velocity_x_q16 = 0L;
        match->ropes[i].hook_velocity_y_q16 = 0L;
        match->ropes[i].hook_travel_q16 = 0L;
        match->ropes[i].active = 0U;
        match->ropes[i].integrity = 0U;
        match->ropes[i].state = VOX_DIGS_ROPE_IDLE;
        match->ropes[i].point_count = 0U;
        match->ropes[i].target_player = VOX_DIGS_NO_PLAYER;
        match->ropes[i].flags = 0U;
        {
            vox_u16 point;
            for (point = 0U; point < VOX_DIGS_ROPE_MAX_POINTS; ++point) {
                match->ropes[i].points[point].position_x_q16 = 0L;
                match->ropes[i].points[point].position_y_q16 = 0L;
                match->ropes[i].points[point].previous_x_q16 = 0L;
                match->ropes[i].points[point].previous_y_q16 = 0L;
            }
        }
        match->bots[i].mode = VOX_DIGS_AI_ROAMING;
        match->bots[i].target = VOX_DIGS_NO_PLAYER;
        match->bots[i].memory_ticks = 0U;
        match->bots[i].state_ticks = 0U;
        match->bots[i].roam_direction = (vox_i16)((i & 1U) ? -1 : 1);
        match->bots[i].decision_ticks = (vox_u16)(i * 2U);
        match->bots[i].retreat_lock_ticks = 0U;
        match->bots[i].roam_goal_x = 0U;
        match->bots[i].roam_goal_ticks = 0U;
        match->bots[i].stuck_ticks = 0U;
        match->bots[i].breach_lock_ticks = 0U;
        match->bots[i].breach_ticks = 0U;
        match->bots[i].last_seen_x_q16 = 0L;
        match->bots[i].last_seen_y_q16 = 0L;
        digs_init_anatomy(match, i);
        vox_physics_body_init(&match->players[i]);
        if (match->alive[i] &&
            digs_spawn_player(match, i,
                              (vox_u32)(i + 1U) * VOX_WORLD_WIDTH /
                              (vox_u32)(rules->player_count + 1U)) != VOX_OK) {
            return VOX_ERR_CAPACITY;
        }
        if (match->alive[i]) {
            vox_i32 player_x = digs_q16_to_cell(
                match->players[i].position_x.value_q16);
            vox_i32 player_y = digs_q16_to_cell(
                match->players[i].position_y.value_q16);
            match->aim_x[i] = (vox_u16)(player_x +
                (match->facing_right[i] ? 12L : -12L));
            match->aim_y[i] = (vox_u16)(player_y > 8L ?
                                        player_y - 8L : 0L);
            digs_emit_event(match, VOX_DIGS_EVENT_SPAWN, i,
                            VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_PICK,
                            VOX_MAT_FLESH,
                            match->players[i].position_x.value_q16,
                            match->players[i].position_y.value_q16,
                            VOX_DIGS_SPAWN_SHIELD_TICKS, i);
        }
    }
    for (i = 0U; i < VOX_DIGS_MAX_PROJECTILES; ++i) {
        match->projectiles[i].position_x_q16 = 0L;
        match->projectiles[i].position_y_q16 = 0L;
        match->projectiles[i].velocity_x_q16 = 0L;
        match->projectiles[i].velocity_y_q16 = 0L;
        match->projectiles[i].launch_min_x_q16 = 0L;
        match->projectiles[i].launch_max_x_q16 = 0L;
        match->projectiles[i].launch_min_y_q16 = 0L;
        match->projectiles[i].launch_max_y_q16 = 0L;
        match->projectiles[i].active = 0U;
        match->projectiles[i].owner = VOX_DIGS_NO_PLAYER;
        match->projectiles[i].weapon = VOX_DIGS_TOOL_PICK;
        match->projectiles[i].material = VOX_MAT_AIR;
        match->projectiles[i].fuse_ticks = 0U;
        match->projectiles[i].age_ticks = 0U;
        match->projectiles[i].damage = 0U;
        match->projectiles[i].blast_radius = 0U;
        match->projectiles[i].owner_clear = 0U;
        match->projectiles[i].arming_ticks = 0U;
    }
    for (i = 0U; i < VOX_DIGS_MAX_EFFECTS; ++i) {
        match->effects[i].position_x_q16 = 0L;
        match->effects[i].position_y_q16 = 0L;
        match->effects[i].velocity_x_q16 = 0L;
        match->effects[i].velocity_y_q16 = 0L;
        match->effects[i].active = 0U;
        match->effects[i].material = VOX_MAT_AIR;
        match->effects[i].ttl_ticks = 0U;
        match->effects[i].variant = 0U;
        match->effects[i].source = VOX_DIGS_NO_PLAYER;
        match->effects[i].depth = 0U;
        match->effects[i].flags = 0U;
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

static int digs_score_limit_reached(const vox_digs_match *match)
{
    vox_u16 i;
    if (match->rules.score_limit == 0U) {
        return 0;
    }
    for (i = 0U; i < match->rules.player_count; ++i) {
        if ((vox_u32)match->scores[i] >= match->rules.score_limit) {
            return 1;
        }
    }
    return 0;
}

static void digs_finish_match(vox_digs_match *match, vox_u16 reason)
{
    vox_u16 i;
    if (match->phase != VOX_DIGS_RUNNING) {
        return;
    }
    match->result_reason = reason;
    match->result_draw = 0U;
    match->winner_player = VOX_DIGS_NO_PLAYER;
    {
        vox_u16 best_score = 0U;
        vox_u16 best_player = VOX_DIGS_NO_PLAYER;
        vox_u16 tied = 0U;
        for (i = 0U; i < match->rules.player_count; ++i) {
            if (best_player == VOX_DIGS_NO_PLAYER ||
                match->scores[i] > best_score) {
                best_score = match->scores[i];
                best_player = i;
                tied = 0U;
            } else if (match->scores[i] == best_score) {
                tied = 1U;
            }
        }
        if (tied) {
            match->result_draw = 1U;
        } else {
            match->winner_player = best_player;
        }
    }
    match->phase = VOX_DIGS_RESULTS;
    for (i = 0U; i < match->rules.player_count; ++i) {
        match->player_actions[i] = 0U;
        match->move_x_q15[i] = 0;
        match->move_y_q15[i] = 0;
    }
    digs_emit_event(match, VOX_DIGS_EVENT_MATCH_END,
                    match->winner_player, VOX_DIGS_NO_PLAYER,
                    VOX_DIGS_TOOL_PICK, VOX_MAT_METAL, 0L, 0L,
                    reason, match->result_draw);
}

static int digs_last_attacker_is_recent(const vox_digs_match *match,
                                        vox_u16 victim)
{
    vox_u16 attacker = match->last_attacker[victim];
    return attacker != VOX_DIGS_NO_PLAYER && attacker != victim &&
           vox_digs_player_is_active(match, attacker) &&
           match->tick >= match->last_attacker_tick[victim] &&
           match->tick - match->last_attacker_tick[victim] <=
               VOX_DIGS_LAST_ATTACKER_TICKS;
}

static void digs_mark_respawn_ready(vox_digs_match *match, vox_u16 player)
{
    if (match->respawn_ready[player]) {
        return;
    }
    match->respawn_ready[player] = 1U;
    digs_emit_event(match, VOX_DIGS_EVENT_RESPAWN_READY, player,
                    VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_PICK,
                    VOX_MAT_FLESH,
                    match->respawn_target_x_q16[player],
                    match->respawn_target_y_q16[player], 0U,
                    match->rules.respawn_mode);
}

static void digs_try_respawn(vox_digs_match *match, vox_u16 player)
{
    vox_u32 preferred_x;
    vox_i32 target_cell;
    if (!match->respawn_ready[player] ||
        !match->respawn_requested[player]) {
        return;
    }
    target_cell = digs_q16_to_cell(match->respawn_target_x_q16[player]);
    if (target_cell <= 0 || target_cell >= (vox_i32)VOX_WORLD_WIDTH - 1) {
        preferred_x = (vox_u32)(player + 1U) * VOX_WORLD_WIDTH /
                      (vox_u32)(match->rules.player_count + 1U);
    } else {
        preferred_x = (vox_u32)target_cell;
    }
    if (digs_spawn_player(match, player, preferred_x) == VOX_OK) {
        match->alive[player] = 1U;
        match->health[player] = VOX_DIGS_MAX_HEALTH;
        match->steam_q16[player] = 65535U;
        match->last_attacker[player] = VOX_DIGS_NO_PLAYER;
        match->last_attacker_tick[player] = 0U;
        match->last_damage_weapon[player] = VOX_DIGS_TOOL_PICK;
        match->last_damage_part[player] = VOX_DIGS_NO_PART;
        match->rail_charge_ticks[player] = 0U;
        match->rail_charging[player] = 0U;
        match->weapon_charge_ticks[player] = 0U;
        match->weapon_charging[player] = 0U;
        match->bolt_shot_streak[player] = 0U;
        match->respawn_ticks[player] = 0U;
        match->respawn_ready[player] = 0U;
        match->respawn_requested[player] = 0U;
        match->spawn_shield_ticks[player] =
            VOX_DIGS_SPAWN_SHIELD_TICKS;
        match->player_actions[player] = 0U;
        match->previous_actions[player] = 0U;
        match->move_x_q15[player] = 0;
        match->move_y_q15[player] = 0;
        match->ropes[player].active = 0U;
        match->ropes[player].state = VOX_DIGS_ROPE_IDLE;
        match->ropes[player].point_count = 0U;
        digs_init_anatomy(match, player);
        digs_emit_event(match, VOX_DIGS_EVENT_SPAWN, player,
                        VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_PICK,
                        VOX_MAT_FLESH,
                        match->players[player].position_x.value_q16,
                        match->players[player].position_y.value_q16,
                        VOX_DIGS_SPAWN_SHIELD_TICKS,
                        (vox_u16)(match->deaths[player] & 7U));
    } else {
        match->respawn_ready[player] = 0U;
        match->respawn_ticks[player] = DIGS_RESPAWN_RETRY_TICKS;
    }
}

vox_result vox_digs_request_respawn(vox_digs_match *match,
                                    vox_u16 player)
{
    if (match == 0 || match->abi_version != VOX_ABI_VERSION ||
        match->struct_size < (vox_u32)sizeof(*match) ||
        match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_active(match, player) || match->alive[player] ||
        !match->respawn_ready[player]) {
        return VOX_ERR_INVALID;
    }
    match->respawn_requested[player] = 1U;
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_result vox_digs_match_step(vox_digs_match *match)
{
    vox_u32 remaining;
    vox_u16 i;
    if (match == 0 || match->abi_version != VOX_ABI_VERSION ||
        match->struct_size < (vox_u32)sizeof(*match) ||
        match->phase != VOX_DIGS_RUNNING) {
        return VOX_ERR_INVALID;
    }
    if (digs_score_limit_reached(match)) {
        digs_finish_match(match, VOX_DIGS_END_SCORE);
        match->state_hash = vox_digs_hash(match);
        return VOX_OK;
    }
    if (match->tick >= match->rules.match_ticks) {
        digs_finish_match(match, VOX_DIGS_END_TIME);
        match->state_hash = vox_digs_hash(match);
        return VOX_OK;
    }
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        if (!vox_digs_player_is_active(match, i)) {
            continue;
        }
        if (match->last_attacker[i] != VOX_DIGS_NO_PLAYER &&
            match->tick >= match->last_attacker_tick[i] &&
            match->tick - match->last_attacker_tick[i] >
                VOX_DIGS_LAST_ATTACKER_TICKS) {
            match->last_attacker[i] = VOX_DIGS_NO_PLAYER;
            match->last_attacker_tick[i] = 0U;
        }
        if (match->weapon_cooldown[i] > 0U) {
            match->weapon_cooldown[i]--;
        }
        if (match->alive[i] && match->spawn_shield_ticks[i] > 0U) {
            match->spawn_shield_ticks[i]--;
            if (match->spawn_shield_ticks[i] == 0U) {
                digs_emit_event(match, VOX_DIGS_EVENT_SHIELD_END, i,
                                VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_PICK,
                                VOX_MAT_METAL,
                                match->players[i].position_x.value_q16,
                                match->players[i].position_y.value_q16,
                                0U, 0U);
            }
        }
        if (!match->alive[i]) {
            if (!match->respawn_ready[i]) {
                if (match->respawn_ticks[i] > 0U) {
                    match->respawn_ticks[i]--;
                }
                if (match->respawn_ticks[i] == 0U) {
                    digs_mark_respawn_ready(match, i);
                }
            }
            if (match->respawn_ready[i] &&
                (match->rules.respawn_mode == VOX_DIGS_RESPAWN_AUTO ||
                 vox_digs_player_is_bot(match, i))) {
                match->respawn_requested[i] = 1U;
            }
            digs_try_respawn(match, i);
        }
    }
    for (i = 0U; i < match->rules.player_count; ++i) {
        if (match->phase != VOX_DIGS_RUNNING) {
            break;
        }
        if (match->alive[i] && vox_digs_player_is_bot(match, i) &&
            vox_digs_bot_think(match, i) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    }
    if (match->phase != VOX_DIGS_RUNNING) {
        match->state_hash = vox_digs_hash(match);
        return VOX_OK;
    }
    if (vox_world_step(&match->world, 0) != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        vox_result physics_result;
        if (!match->alive[i]) {
            continue;
        }
        digs_apply_player_controls(match, i);
        digs_step_weapon_input(match, i);
        if (!match->alive[i]) {
            continue;
        }
        physics_result = vox_physics_step_world(
            &match->players[i], &match->world, &match->physics_config);
        if (physics_result == VOX_ERR_COLLISION) {
            if (match->spawn_shield_ticks[i] != 0U) {
                /*
                 * A spawn-shielded miner is invulnerable, so burial cannot
                 * hurt them.  Hold the struggle timer at zero rather than
                 * letting it run invisibly, or the shield expiring would
                 * kill them instantly with an already-elapsed countdown.
                 */
                match->buried_ticks[i] = 0U;
            } else {
                vox_u16 source = digs_last_attacker_is_recent(match, i) ?
                                 match->last_attacker[i] :
                                 VOX_DIGS_NO_PLAYER;
                /*
                 * Announce the burial once, on entry, so a host can react to
                 * it without the event ring filling with one crush per tick
                 * for the whole struggle.
                 */
                if (match->buried_ticks[i] == 0U) {
                    digs_emit_event(match, VOX_DIGS_EVENT_CRUSH, source, i,
                                    match->last_damage_weapon[i],
                                    VOX_MAT_STONE,
                                    match->players[i].position_x.value_q16,
                                    match->players[i].position_y.value_q16,
                                    match->health[i],
                                    match->last_damage_part[i]);
                }
                if (match->buried_ticks[i] < 65535U) {
                    match->buried_ticks[i]++;
                }
                /*
                 * Crush pressure is ordinary blunt damage, so it routes
                 * through the one damage funnel: anatomy, bleeding, and kill
                 * attribution all behave exactly as they do for any other
                 * source, and a miner crushed shortly after being shot still
                 * credits the shooter.
                 */
                (void)vox_digs_apply_hit(match, source, i,
                                         VOX_DIGS_TOOL_SLEDGE,
                                         VOX_DIGS_PART_TORSO,
                                         DIGS_BURIED_DAMAGE_PER_TICK,
                                         VOX_DIGS_DAMAGE_BLUNT);
                if (match->alive[i] &&
                    match->buried_ticks[i] >= DIGS_BURIED_LETHAL_TICKS) {
                    if (source != VOX_DIGS_NO_PLAYER) {
                        (void)vox_digs_record_kill(match, source, i);
                    } else {
                        digs_environment_defeat(match, i);
                    }
                }
            }
        } else if (physics_result != VOX_OK) {
            return VOX_ERR_INVALID;
        } else {
            /* Free again: the struggle timer only counts consecutive ticks. */
            match->buried_ticks[i] = 0U;
        }
    }
    digs_step_projectiles(match);
    digs_step_effects(match);
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        if (match->alive[i]) {
            digs_step_rope(match, i);
        }
        match->previous_actions[i] = match->player_actions[i];
    }
    digs_step_bleeding(match);
    digs_step_reactions(match);
    digs_step_contracts(match);
    digs_step_speech(match);
    if (match->tick >= match->rules.lava_start_tick) {
        remaining = match->rules.match_ticks - match->rules.lava_start_tick;
        match->lava_level_q16 = digs_scale_lava_level(
            match->tick - match->rules.lava_start_tick, remaining);
    }
    digs_update_lava(match);
    digs_apply_lava_hazards(match);
    match->tick++;
    if (digs_score_limit_reached(match)) {
        digs_finish_match(match, VOX_DIGS_END_SCORE);
    } else if (match->tick >= match->rules.match_ticks) {
        digs_finish_match(match, VOX_DIGS_END_TIME);
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

static void digs_spawn_death_gore(vox_digs_match *match, vox_u16 victim,
                                  vox_u16 killer)
{
    vox_u16 part;
    vox_u16 blood_count;
    vox_u16 weapon = match->last_damage_weapon[victim] <
                     VOX_DIGS_TOOL_COUNT ?
                     match->last_damage_weapon[victim] :
                     VOX_DIGS_TOOL_PICK;
    vox_i32 x_q16 = match->players[victim].position_x.value_q16;
    vox_i32 y_q16 = match->players[victim].position_y.value_q16;
    if (match->rules.fx_budget == VOX_DIGS_FX_RETRO) {
        blood_count = 24U;
    } else if (match->rules.fx_budget == VOX_DIGS_FX_CARNAGE) {
        blood_count = 72U;
    } else {
        blood_count = 44U;
    }
    for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
        vox_u32 noise = digs_noise(match->rules.seed, match->tick,
                                   (vox_u32)victim,
                                   0xA1100000U + part +
                                   (vox_u32)match->deaths[victim] * 37U);
        vox_i32 velocity_x = ((vox_i32)(noise % 21U) - 10L) * 7168L;
        vox_i32 velocity_y = -16384L -
            (vox_i32)((noise >> 8) % 15U) * 5120L;
        if (part >= VOX_DIGS_PART_LEFT_UPPER_ARM) {
            match->anatomy[victim][part].flags = (vox_u16)(
                match->anatomy[victim][part].flags |
                VOX_DIGS_PART_SEVERED);
        }
        digs_spawn_effect_variant(match, VOX_MAT_FLESH, x_q16, y_q16,
                                  velocity_x, velocity_y,
                                  (vox_u16)(70U + noise % 75U), victim,
                                  part);
        if (part >= VOX_DIGS_PART_LEFT_UPPER_ARM) {
            digs_emit_event(match, VOX_DIGS_EVENT_LIMB_SEVER, killer,
                            victim, weapon,
                            VOX_MAT_FLESH, x_q16, y_q16, part,
                            (vox_u16)(noise & 15U));
        }
    }
    for (part = 0U; part < blood_count; ++part) {
        vox_u32 noise = digs_noise(match->rules.seed,
                                   match->tick + part,
                                   (vox_u32)victim,
                                   0xB1000000U +
                                   (vox_u32)match->deaths[victim] * 53U);
        vox_i32 velocity_x = ((vox_i32)(noise % 25U) - 12L) * 6144L;
        vox_i32 velocity_y = -8192L -
            (vox_i32)((noise >> 7) % 18U) * 4608L;
        digs_spawn_effect_variant(match, VOX_MAT_BLOOD, x_q16, y_q16,
                                  velocity_x, velocity_y,
                                  (vox_u16)(38U + noise % 90U), victim,
                                  (vox_u16)(noise & 31U));
    }
}


/*
 * Pairs are unordered, so (a, b) and (b, a) are the same account.  Four slots
 * give six of them: (0,1) (0,2) (0,3) (1,2) (1,3) (2,3).
 */
vox_u16 vox_digs_speech_duration(vox_u16 line_id)
{
    vox_u32 life = (vox_u32)digs_lines_length(line_id) *
                   DIGS_SPEECH_TICKS_PER_CHAR + DIGS_SPEECH_READ_MARGIN;
    if (life < DIGS_SPEECH_DURATION_MIN) {
        life = DIGS_SPEECH_DURATION_MIN;
    }
    if (life > DIGS_SPEECH_DURATION_MAX) {
        life = DIGS_SPEECH_DURATION_MAX;
    }
    return (vox_u16)life;
}

vox_u16 vox_digs_pair_index(vox_u16 a, vox_u16 b)
{
    vox_u16 low;
    vox_u16 high;
    if (a == b || a >= VOX_DIGS_MAX_SLOTS || b >= VOX_DIGS_MAX_SLOTS) {
        return (vox_u16)VOX_DIGS_MAX_PAIRS;
    }
    low = a < b ? a : b;
    high = a < b ? b : a;
    return (vox_u16)((low * (2U * VOX_DIGS_MAX_SLOTS - low - 1U)) / 2U +
                     (high - low - 1U));
}

const vox_digs_contract *vox_digs_contract_get(const vox_digs_match *match,
                                               vox_u16 a, vox_u16 b)
{
    vox_u16 index;
    if (match == 0) {
        return 0;
    }
    index = vox_digs_pair_index(a, b);
    if (index >= VOX_DIGS_MAX_PAIRS) {
        return 0;
    }
    return &match->contracts[index];
}

const char *vox_digs_tone_name(vox_u16 tone)
{
    static const char *names[VOX_DIGS_TONE_COUNT] = {
        "FEUD", "HOSTILE", "NEEDLING", "NEUTRAL",
        "WARY", "THAWING", "TRUCE", "BONDED"
    };
    return tone < VOX_DIGS_TONE_COUNT ? names[tone] : "NEUTRAL";
}

/* Which band of feeling a balance falls in, read from where we already are. */
static vox_u16 digs_contract_band(vox_i32 valence, vox_u16 current)
{
    if (valence >= DIGS_CONTRACT_BONDED_AT) {
        return (vox_u16)VOX_DIGS_TONE_BONDED;
    }
    if (valence >= DIGS_CONTRACT_TRUCE_AT) {
        return (vox_u16)VOX_DIGS_TONE_TRUCE;
    }
    if (valence >= DIGS_CONTRACT_WARM_AT) {
        return (current == (vox_u16)VOX_DIGS_TONE_TRUCE ||
                current == (vox_u16)VOX_DIGS_TONE_BONDED ||
                current == (vox_u16)VOX_DIGS_TONE_WARY) ?
               (vox_u16)VOX_DIGS_TONE_WARY : (vox_u16)VOX_DIGS_TONE_THAWING;
    }
    if (valence <= DIGS_CONTRACT_FEUD_AT) {
        return (vox_u16)VOX_DIGS_TONE_FEUD;
    }
    if (valence <= DIGS_CONTRACT_HOSTILE_AT) {
        return (vox_u16)VOX_DIGS_TONE_HOSTILE;
    }
    if (valence <= DIGS_CONTRACT_NEEDLE_AT) {
        return (vox_u16)VOX_DIGS_TONE_NEEDLING;
    }
    return (vox_u16)VOX_DIGS_TONE_NEUTRAL;
}

/*
 * Settle the tone against the balance.  The margin is applied toward wherever
 * we already are, which is what makes leaving a tone cost more than entering
 * it; the dwell stops even a decisive swing from flipping twice in a second.
 */
static void digs_contract_settle(vox_digs_contract *contract)
{
    vox_i32 biased = contract->valence;
    vox_u16 raw = digs_contract_band(contract->valence, contract->tone);
    vox_u16 target;
    if (contract->tone > raw) {
        biased += DIGS_CONTRACT_TONE_MARGIN;
    } else if (contract->tone < raw) {
        biased -= DIGS_CONTRACT_TONE_MARGIN;
    }
    target = digs_contract_band(biased, contract->tone);
    if (target != contract->tone &&
        contract->tone_ticks >= DIGS_CONTRACT_TONE_DWELL_TICKS) {
        contract->tone = target;
        contract->tone_ticks = 0U;
    }
}

/*
 * Move the balance between two miners.  Everything that happens between them
 * -- a shot, a kill, a near miss, a word -- arrives here.
 */
static void digs_contract_adjust(vox_digs_match *match, vox_u16 a, vox_u16 b,
                                 vox_i32 delta)
{
    vox_digs_contract *contract;
    vox_i32 valence;
    vox_u16 index = vox_digs_pair_index(a, b);
    if (index >= VOX_DIGS_MAX_PAIRS) {
        return;
    }
    contract = &match->contracts[index];
    contract->met = 1U;
    valence = (vox_i32)contract->valence + delta;
    if (valence > DIGS_CONTRACT_VALENCE_MAX) {
        valence = DIGS_CONTRACT_VALENCE_MAX;
    } else if (valence < -DIGS_CONTRACT_VALENCE_MAX) {
        valence = -DIGS_CONTRACT_VALENCE_MAX;
    }
    contract->valence = (vox_i16)valence;
    digs_contract_settle(contract);
}


/*
 * What each thing that can happen between two miners is worth to the account.
 *
 * Zero is not "nothing happened" -- it is "this is worth saying something
 * about, but it does not change what they think of each other".  Meeting
 * someone is not yet an opinion, and the lava rising is nobody's fault.
 */
static const vox_i16
digs_stimulus_valence[VOX_DIGS_STIMULUS_COUNT] = {
    0,      /* NONE           */
    0,      /* FIRST_MEETING  */
    0,      /* SPOTTED        */
    -6,     /* HURT_THEM      */
    -6,     /* HURT_BY        */
    -4,     /* NEAR_MISS      */
    -50,    /* LIMB_TAKEN     */
    -50,    /* LIMB_LOST      */
    -140,   /* KILLED_THEM    */
    -140,   /* KILLED_BY      */
    -60,    /* REVENGE        -- settling a score stings less than starting one */
    -180,   /* HUMILIATED     */
    0,      /* STREAK         */
    150,    /* SAVED_BY       */
    90,     /* TEAMED_UP      */
    -260,   /* BETRAYED       */
    40,     /* TRUCE_OFFERED  */
    200,    /* TRUCE_ACCEPTED */
    -300,   /* TRUCE_BROKEN   */
    -20,    /* TAUNTED        */
    0,      /* LAVA_CLOSE     */
    0,      /* BURIED         */
    0,      /* DOOMED         */
    0,      /* LONG_ABSENCE   */
    0,      /* MATCH_START    */
    0,      /* MATCH_END      */
    0       /* IDLE           */
};

const char *vox_digs_stimulus_name(vox_u16 stimulus)
{
    static const char *names[VOX_DIGS_STIMULUS_COUNT] = {
        "NONE", "FIRST MEETING", "SPOTTED", "HURT THEM", "HURT BY",
        "NEAR MISS", "LIMB TAKEN", "LIMB LOST", "KILLED THEM", "KILLED BY",
        "REVENGE", "HUMILIATED", "STREAK", "SAVED BY", "TEAMED UP",
        "BETRAYED", "TRUCE OFFERED", "TRUCE ACCEPTED", "TRUCE BROKEN",
        "TAUNTED", "LAVA CLOSE", "BURIED", "DOOMED", "LONG ABSENCE",
        "MATCH START", "MATCH END", "IDLE"
    };
    return stimulus < VOX_DIGS_STIMULUS_COUNT ? names[stimulus] : "NONE";
}

/*
 * Record that something happened between two miners.  This is the single door
 * every stimulus comes through: it moves the account by the table above and
 * leaves the event on the contract for whoever speaks next to talk about.
 */
static void digs_contract_note(vox_digs_match *match, vox_u16 actor,
                               vox_u16 subject, vox_u16 stimulus)
{
    vox_digs_contract *contract;
    vox_u16 index;
    if (stimulus >= VOX_DIGS_STIMULUS_COUNT) {
        return;
    }
    index = vox_digs_pair_index(actor, subject);
    if (index >= VOX_DIGS_MAX_PAIRS) {
        return;
    }
    contract = &match->contracts[index];
    /*
     * Shooting a miner you have an arrangement with is not an ordinary hit.
     * It is the arrangement ending, and it costs accordingly -- this is the
     * only way a truce can be spent, and it has to hurt or a truce would be
     * free to take and free to break.
     */
    if ((stimulus == VOX_DIGS_STIMULUS_HURT_THEM ||
         stimulus == VOX_DIGS_STIMULUS_KILLED_THEM) &&
        (contract->tone == VOX_DIGS_TONE_TRUCE ||
         contract->tone == VOX_DIGS_TONE_BONDED)) {
        stimulus = (vox_u16)VOX_DIGS_STIMULUS_TRUCE_BROKEN;
    }
    contract->last_stimulus = stimulus;
    contract->last_actor = actor;
    contract->last_stimulus_tick = match->tick;
    digs_contract_adjust(match, actor, subject,
                         (vox_i32)digs_stimulus_valence[stimulus]);
    digs_speech_prompt(match, actor, subject, stimulus);
    digs_speech_prompt(match, subject, actor, digs_stimulus_mirror(stimulus));
}


/*
 * The same event seen from the other side.  Somebody has to be hurt for
 * somebody else to have hurt them, and each of them has different things to
 * say about it.
 */
static vox_u16 digs_stimulus_mirror(vox_u16 stimulus)
{
    switch (stimulus) {
    case VOX_DIGS_STIMULUS_HURT_THEM:
        return (vox_u16)VOX_DIGS_STIMULUS_HURT_BY;
    case VOX_DIGS_STIMULUS_HURT_BY:
        return (vox_u16)VOX_DIGS_STIMULUS_HURT_THEM;
    case VOX_DIGS_STIMULUS_LIMB_TAKEN:
        return (vox_u16)VOX_DIGS_STIMULUS_LIMB_LOST;
    case VOX_DIGS_STIMULUS_LIMB_LOST:
        return (vox_u16)VOX_DIGS_STIMULUS_LIMB_TAKEN;
    case VOX_DIGS_STIMULUS_KILLED_THEM:
    case VOX_DIGS_STIMULUS_REVENGE:
        return (vox_u16)VOX_DIGS_STIMULUS_KILLED_BY;
    case VOX_DIGS_STIMULUS_KILLED_BY:
        return (vox_u16)VOX_DIGS_STIMULUS_KILLED_THEM;
    case VOX_DIGS_STIMULUS_TRUCE_BROKEN:
        return (vox_u16)VOX_DIGS_STIMULUS_BETRAYED;
    case VOX_DIGS_STIMULUS_TRUCE_OFFERED:
        return (vox_u16)VOX_DIGS_STIMULUS_TRUCE_ACCEPTED;
    case VOX_DIGS_STIMULUS_FIRST_MEETING:
        return (vox_u16)VOX_DIGS_STIMULUS_FIRST_MEETING;
    default:
        break;
    }
    return (vox_u16)VOX_DIGS_STIMULUS_NONE;
}

/* How badly this wants saying -- louder things interrupt quieter ones. */
static vox_u16 digs_stimulus_urgency(vox_u16 stimulus)
{
    vox_i32 weight;
    if (stimulus >= VOX_DIGS_STIMULUS_COUNT) {
        return 0U;
    }
    weight = (vox_i32)digs_stimulus_valence[stimulus];
    if (weight < 0) {
        weight = -weight;
    }
    /* The wordless emergencies are urgent without moving the account. */
    if (stimulus == VOX_DIGS_STIMULUS_DOOMED ||
        stimulus == VOX_DIGS_STIMULUS_BURIED ||
        stimulus == VOX_DIGS_STIMULUS_LAVA_CLOSE) {
        weight += 220;
    }
    /*
     * So are announcements.  Urgency is derived from how far something moves
     * the account, and meeting somebody for the first time moves it by
     * nothing at all -- so a first meeting rolled at four in ten thousand
     * and was never once spoken.  These are rare by construction (a first
     * meeting happens once per pair, a match starts once) so making them
     * certain costs almost nothing and they are the lines that place
     * everyone in the room.
     */
    if (stimulus == VOX_DIGS_STIMULUS_FIRST_MEETING ||
        stimulus == VOX_DIGS_STIMULUS_MATCH_START ||
        stimulus == VOX_DIGS_STIMULUS_MATCH_END ||
        stimulus == VOX_DIGS_STIMULUS_STREAK) {
        weight += 210;
    }
    return (vox_u16)(weight + 1);
}

/*
 * A temperament for whoever is speaking, including the human.
 *
 * vox_digs_personality_get returns null for anyone who is not a bot, and the
 * speech pacing needs a temperament for every speaker -- the player's miner
 * waits before answering like the rest of them.  Middle of the road on every
 * axis, so the three opponents stay the distinctive ones.
 */
/*
 * The traits this miner is actually running on: the drifted ones from the
 * snapshot, not the archetype table.  A bot that has been through a dozen
 * matches should not still be reading its factory settings.
 */
static const vox_digs_personality *digs_player_traits(
    const vox_digs_match *match, vox_u16 player)
{
    vox_u16 identity = vox_digs_memory_identity(match, player);
    if (identity < VOX_DIGS_IDENTITY_COUNT) {
        return &match->memory.identities[identity].traits;
    }
    return vox_digs_personality_get(VOX_DIGS_ARCHETYPE_ENGINEER);
}

static const vox_digs_personality *digs_speaker_personality(
    const vox_digs_match *match, vox_u16 player)
{
    static const vox_digs_personality miner = {
        128U, 128U, 128U, 128U, 128U, 0U
    };
    const vox_digs_personality *personality = digs_player_traits(match,
                                                                 player);
    return personality != 0 ? personality : &miner;
}

/*
 * Queue something for this miner to say.  Louder news displaces quieter news
 * already waiting, so a miner who is shot while still working up to a remark
 * about the weather says the thing that actually happened.
 */
/* A dice roll, in parts per thousand, off the one noise source there is. */
static int digs_speech_roll(const vox_digs_match *match, vox_u16 player,
                            vox_u32 chance, vox_u16 salt)
{
    if (chance == 0U) {
        return 0;
    }
    if (chance >= 1000U) {
        return 1;
    }
    return (digs_noise(match->rules.seed, match->tick, player, salt) %
            1000U) < chance;
}

/*
 * How likely this miner is to speak up unprompted, per thousand.
 *
 * Sociability is the whole of it in the ordinary case, which is what makes
 * one of them audibly chattier than another.  Two things override it: the
 * window after the player has spoken, when everyone is keener to answer, and
 * a long enough silence, which makes the next roll a certainty so a quiet
 * match still has voices in it.
 */
static vox_u32 digs_speech_chattiness(const vox_digs_match *match,
                                      vox_u16 player)
{
    const vox_digs_personality *personality =
        digs_speaker_personality(match, player);
    vox_u32 chance = ((vox_u32)personality->sociability *
                      DIGS_SPEECH_URGE_SCALE) / 255U;
    if (match->speech_dry_ticks >= DIGS_SPEECH_DRY_TICKS) {
        return 1000U;
    }
    if (match->speech_answer_ticks > 0U) {
        chance += DIGS_SPEECH_ANSWER_BONUS;
    }
    return chance;
}

/* Record an intention to speak.  The decision has already been made. */
static void digs_speech_set(vox_digs_match *match, vox_u16 speaker,
                            vox_u16 subject, vox_u16 stimulus)
{
    const vox_digs_personality *personality;
    vox_u32 noise;
    vox_u32 delay;
    if (!vox_digs_player_is_active(match, speaker) || !match->alive[speaker] ||
        stimulus == VOX_DIGS_STIMULUS_NONE ||
        stimulus >= VOX_DIGS_STIMULUS_COUNT) {
        return;
    }
    /*
     * The chatter cooldown exists to stop a miner monologuing across a whole
     * match.  Inside a live exchange it does the opposite -- it silences the
     * answer, which is the one line that makes it a conversation.  The floor
     * still spaces everything, so nobody can run away with it.
     */
    if (match->speech_cooldown[speaker] != 0U &&
        match->speech_answer_ticks == 0U &&
        digs_stimulus_urgency(stimulus) < DIGS_SPEECH_ALWAYS_URGENCY) {
        return;
    }
    if (match->speech_stimulus[speaker] != VOX_DIGS_STIMULUS_NONE &&
        digs_stimulus_urgency(match->speech_stimulus[speaker]) >=
        digs_stimulus_urgency(stimulus)) {
        return;
    }
    personality = digs_speaker_personality(match, speaker);
    noise = digs_noise(match->rules.seed, match->tick, speaker, 0x5EEDU);
    delay = DIGS_SPEECH_DELAY_MIN +
            ((vox_u32)personality->patience * DIGS_SPEECH_DELAY_SPAN) / 255U;
    /*
     * Wait for the other miner to finish -- or do not, which is the point.
     *
     * A reply used to land fifteen to seventy ticks in against a bubble that
     * is up for a hundred and fifty, so a bot cut the player off every single
     * time.  How much of the say-time a miner waits out is patience: RIVET
     * nearly always lets you finish, CINDER talks over you, and the jitter is
     * wide enough that FLAMEY is a coin toss.
     */
    if (match->speech_last_line != 0U) {
        vox_u32 life = vox_digs_speech_duration(match->speech_last_line);
        delay += (life * ((vox_u32)personality->patience +
                          DIGS_SPEECH_PATIENCE_FLOOR)) / 255U;
        delay += noise % (1U + life / 3U);
    }
    /* Chatty miners are also erratic about when they pipe up. */
    delay += (noise % 25U) * (vox_u32)personality->sociability / 255U;
    /*
     * Never queue a reply for longer than the exchange it belongs to.  A
     * delay past the window is not merely late: the exchange is torn down
     * underneath it and the between-exchange floor then sits on the line, so
     * it arrives many seconds later into a conversation that no longer
     * exists.  Waiting out the line is the intent; waiting past the end of
     * the conversation is not.
     */
    if (match->speech_answer_ticks > 0U &&
        delay >= (vox_u32)match->speech_answer_ticks) {
        delay = match->speech_answer_ticks > 1U ?
                (vox_u32)(match->speech_answer_ticks - 1U) : 1U;
    }
    match->speech_stimulus[speaker] = stimulus;
    match->speech_subject[speaker] = subject;
    match->speech_delay[speaker] = (vox_u16)delay;
}

/*
 * Something happened -- does this miner remark on it?
 *
 * Loud news always gets said: a kill, a betrayal, being buried.  Everything
 * quieter is rolled for, scaled by how loud it was and by how talkative the
 * miner is.  This is what stopped every pellet in a firefight earning two
 * lines of commentary.
 */
static void digs_speech_prompt(vox_digs_match *match, vox_u16 speaker,
                               vox_u16 subject, vox_u16 stimulus)
{
    vox_u32 urgency;
    if (stimulus == VOX_DIGS_STIMULUS_NONE ||
        stimulus >= VOX_DIGS_STIMULUS_COUNT ||
        !vox_digs_player_is_active(match, speaker)) {
        return;
    }
    urgency = digs_stimulus_urgency(stimulus);
    if (urgency < DIGS_SPEECH_ALWAYS_URGENCY) {
        const vox_digs_personality *personality =
            digs_speaker_personality(match, speaker);
        /*
         * Cut hard against the old denominator.  Every kill used to be
         * worth a remark from both parties, and a violent match has fifty of
         * them -- which spent the whole budget on isolated event barks and
         * left nothing for the exchanges they are supposed to start.  Fewer
         * things get remarked on now, and the ones that do turn into a
         * conversation.
         */
        vox_u32 chance = (urgency * DIGS_SPEECH_EVENT_SCALE *
                          (vox_u32)personality->sociability) / 900U;
        if (match->speech_answer_ticks > 0U) {
            chance += DIGS_SPEECH_ANSWER_BONUS;
        }
        if (!digs_speech_roll(match, speaker, chance,
                              (vox_u16)(0x0517U + stimulus))) {
            return;
        }
    }
    digs_speech_set(match, speaker, subject, stimulus);
}


/*
 * Has this line been heard lately -- by this pair, or from this speaker to
 * anyone at all?
 *
 * The second half is the new one.  Only the pair remembered before, so RIVET
 * could say the same thing to CINDER and then to the player and neither of
 * them was any the wiser, while the player heard it twice.
 *
 * depth bounds how far back to look, because a pool of five lines against a
 * memory of eight would otherwise exclude everything it has and the caller
 * would fall back to whatever it started on -- which is the repetition this
 * is meant to prevent, arrived at from the other direction.
 */
static int digs_speech_heard(const vox_digs_match *match, vox_u16 speaker,
                             const vox_digs_contract *contract,
                             vox_u16 line, vox_u16 depth)
{
    vox_u16 i;
    for (i = 0U; i < depth && i < VOX_DIGS_SPEAKER_RECENT; ++i) {
        vox_u16 at = (vox_u16)((match->speech_recent_cursor[speaker] +
                                VOX_DIGS_SPEAKER_RECENT - 1U - i) %
                               VOX_DIGS_SPEAKER_RECENT);
        if (match->speech_recent[speaker][at] == line) {
            return 1;
        }
    }
    if (contract != 0) {
        for (i = 0U; i < depth && i < VOX_DIGS_RECENT_LINES; ++i) {
            vox_u16 at = (vox_u16)((contract->recent_cursor +
                                    VOX_DIGS_RECENT_LINES - 1U - i) %
                                   VOX_DIGS_RECENT_LINES);
            if (contract->recent_lines[at] == line) {
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Choose the words.  The pool comes from the speaker's voice, how they feel
 * about the subject, and what just happened; the recent ring keeps a pair
 * from saying the same thing twice running.
 */
static vox_u16 digs_speech_choose(vox_digs_match *match, vox_u16 speaker,
                                  vox_u16 subject, vox_u16 stimulus,
                                  vox_u16 *tone_out)
{
    digs_line_pool pool;
    vox_digs_contract *contract = 0;
    vox_u16 tone = (vox_u16)VOX_DIGS_TONE_NEUTRAL;
    vox_u16 pair = vox_digs_pair_index(speaker, subject);
    vox_u32 roll;
    vox_u16 attempt;
    vox_u16 chosen;
    if (pair < VOX_DIGS_MAX_PAIRS) {
        contract = &match->contracts[pair];
        tone = contract->tone;
    }
    *tone_out = tone;
    pool = digs_lines_pool(digs_lines_voice_for(match, speaker), tone,
                           stimulus);
    if (pool.count == 0U) {
        return 65535U;
    }
    /*
     * Re-roll past anything heard lately, rather than walking forward.
     *
     * Stepping to the next line on a collision looks harmless and is not:
     * once two picks collide the walk hands out consecutive lines, so a pool
     * gets read out in order and the writing sounds like a list.  A fresh
     * roll each attempt keeps the distribution flat.
     */
    {
        vox_u16 depth = pool.count > 1U ? (vox_u16)(pool.count - 1U) : 0U;
        roll = digs_noise(match->rules.seed, match->tick, speaker,
                          (vox_u16)(0x1A1EU + stimulus));
        chosen = (vox_u16)(pool.first + (vox_u16)(roll % pool.count));
        for (attempt = 0U; attempt < DIGS_SPEECH_PICK_TRIES; ++attempt) {
            int unusable = digs_speech_heard(match, speaker, contract,
                                             chosen, depth);
            /*
             * A line that names somebody is nonsense with nobody to name --
             * "SOMEBODY. COULD BE WORSE." is what a miner muttering to
             * itself used to come out with.
             */
            if (!unusable && subject >= match->rules.player_count &&
                digs_lines_addresses(chosen)) {
                unusable = 1;
            }
            if (!unusable) {
                break;
            }
            roll = digs_noise(match->rules.seed, match->tick, speaker,
                              (vox_u16)(0x1A1EU + stimulus + attempt + 1U));
            chosen = (vox_u16)(pool.first + (vox_u16)(roll % pool.count));
        }
    }
    match->speech_recent[speaker][match->speech_recent_cursor[speaker]] =
        chosen;
    match->speech_recent_cursor[speaker] =
        (vox_u16)((match->speech_recent_cursor[speaker] + 1U) %
                  VOX_DIGS_SPEAKER_RECENT);
    if (contract != 0) {
        contract->recent_lines[contract->recent_cursor] = chosen;
        contract->recent_cursor = (vox_u16)((contract->recent_cursor + 1U) %
                                            VOX_DIGS_RECENT_LINES);
        contract->last_speaker = speaker;
        contract->quiet_ticks = 0U;
        if (contract->exchanges < 65535U) {
            contract->exchanges++;
        }
    }
    return chosen;
}


/*
 * Who is this aimed at?
 *
 * Muttering is muttering whoever is nearby; an announcement is for the room;
 * everything else is aimed at somebody.  Deriving it from the stimulus keeps
 * it in one place -- the alternative is every caller deciding, and callers
 * disagree.
 */
static vox_u16 digs_stimulus_audience(vox_u16 stimulus)
{
    switch (stimulus) {
    case VOX_DIGS_STIMULUS_IDLE:
    case VOX_DIGS_STIMULUS_BURIED:
    case VOX_DIGS_STIMULUS_LAVA_CLOSE:
    case VOX_DIGS_STIMULUS_DOOMED:
        return (vox_u16)VOX_DIGS_AUDIENCE_SELF;
    case VOX_DIGS_STIMULUS_MATCH_START:
    case VOX_DIGS_STIMULUS_MATCH_END:
    case VOX_DIGS_STIMULUS_FIRST_MEETING:
    case VOX_DIGS_STIMULUS_TRUCE_OFFERED:
    case VOX_DIGS_STIMULUS_HUMILIATED:
    case VOX_DIGS_STIMULUS_STREAK:
        return (vox_u16)VOX_DIGS_AUDIENCE_ALL;
    default:
        break;
    }
    return (vox_u16)VOX_DIGS_AUDIENCE_ONE;
}

/* Can this miner make out what was said, and from how far? */
static int digs_speech_within_earshot(const vox_digs_match *match,
                                      vox_u16 listener, vox_u16 speaker,
                                      vox_u16 audience)
{
    vox_i32 gap;
    vox_u32 reach;
    if (audience == VOX_DIGS_AUDIENCE_ALL) {
        return 1;
    }
    reach = audience == VOX_DIGS_AUDIENCE_SELF ?
            DIGS_SPEECH_MUTTER_CELLS : DIGS_SPEECH_EARSHOT_CELLS;
    gap = digs_abs_i32(
        (match->players[listener].position_x.value_q16 >> 16) -
        (match->players[speaker].position_x.value_q16 >> 16)) +
        digs_abs_i32(
        (match->players[listener].position_y.value_q16 >> 16) -
        (match->players[speaker].position_y.value_q16 >> 16));
    return (vox_u32)gap <= reach;
}

/*
 * What a listener says back to something aimed at somebody else.
 *
 * Answering "you talked" to everything was what made replies feel like a
 * reflex rather than a response.  Picking off what was actually heard is
 * what lets a third miner take a side.
 */
static vox_u16 digs_speech_chime_in(vox_u16 heard, int defends)
{
    switch (heard) {
    case VOX_DIGS_STIMULUS_KILLED_THEM:
    case VOX_DIGS_STIMULUS_REVENGE:
        return defends ? (vox_u16)VOX_DIGS_STIMULUS_HUMILIATED :
                         (vox_u16)VOX_DIGS_STIMULUS_TEAMED_UP;
    case VOX_DIGS_STIMULUS_TRUCE_BROKEN:
    case VOX_DIGS_STIMULUS_BETRAYED:
        return (vox_u16)VOX_DIGS_STIMULUS_BETRAYED;
    case VOX_DIGS_STIMULUS_LIMB_TAKEN:
    case VOX_DIGS_STIMULUS_HURT_THEM:
        return defends ? (vox_u16)VOX_DIGS_STIMULUS_TAUNTED :
                         (vox_u16)VOX_DIGS_STIMULUS_SPOTTED;
    default:
        break;
    }
    return (vox_u16)VOX_DIGS_STIMULUS_TAUNTED;
}

/*
 * Where a thought goes next when there is nobody to have it at.
 *
 * Feeding a miner's own last line back through the reply mapping collapsed
 * immediately -- almost everything answers with TAUNTED, and TAUNTED answers
 * with TAUNTED, so a miner alone said four things and then repeated them.
 * This is a small arc instead: bored, then sardonic, then fed up, then
 * defiant, then bored again.  Pressing bark in an empty mine walks it.
 */
static vox_u16 digs_speech_self_next(vox_u16 previous)
{
    switch (previous) {
    case VOX_DIGS_STIMULUS_IDLE:
        return (vox_u16)VOX_DIGS_STIMULUS_TAUNTED;
    case VOX_DIGS_STIMULUS_TAUNTED:
        return (vox_u16)VOX_DIGS_STIMULUS_HUMILIATED;
    case VOX_DIGS_STIMULUS_HUMILIATED:
        return (vox_u16)VOX_DIGS_STIMULUS_STREAK;
    case VOX_DIGS_STIMULUS_STREAK:
        return (vox_u16)VOX_DIGS_STIMULUS_MATCH_END;
    default:
        break;
    }
    return (vox_u16)VOX_DIGS_STIMULUS_IDLE;
}


/*
 * Where a reply goes, given how the exchange is running.
 *
 * Without this a row was three insults in a queue: every answer was picked
 * from what was just said and nothing tracked where the whole thing was
 * heading.  Heat is the memory of that -- an exchange that keeps drawing
 * blood escalates, one that keeps finding agreement winds down, and the last
 * line before the cap closes on whichever it turned out to be.
 */
static vox_u16 digs_speech_arc(vox_digs_match *match, vox_u16 base,
                               int closing)
{
    int heat = (int)match->speech_exchange_heat;
    if (closing) {
        /* Somebody has to have the last word, and it should fit. */
        if (heat <= DIGS_SPEECH_HEAT_ANGRY) {
            return (vox_u16)VOX_DIGS_STIMULUS_HUMILIATED;
        }
        if (heat >= DIGS_SPEECH_HEAT_CALM) {
            return (vox_u16)VOX_DIGS_STIMULUS_TRUCE_OFFERED;
        }
        return (vox_u16)VOX_DIGS_STIMULUS_MATCH_END;
    }
    if (heat <= DIGS_SPEECH_HEAT_ANGRY) {
        /* It has turned into a row.  Answer in kind. */
        switch (base) {
        case VOX_DIGS_STIMULUS_TAUNTED:
            return (vox_u16)VOX_DIGS_STIMULUS_HUMILIATED;
        case VOX_DIGS_STIMULUS_HUMILIATED:
            return (vox_u16)VOX_DIGS_STIMULUS_BETRAYED;
        case VOX_DIGS_STIMULUS_SPOTTED:
            return (vox_u16)VOX_DIGS_STIMULUS_TAUNTED;
        default:
            break;
        }
        return base;
    }
    if (heat >= DIGS_SPEECH_HEAT_CALM) {
        /* It is going somewhere better.  Let it. */
        switch (base) {
        case VOX_DIGS_STIMULUS_TAUNTED:
        case VOX_DIGS_STIMULUS_SPOTTED:
            return (vox_u16)VOX_DIGS_STIMULUS_TEAMED_UP;
        case VOX_DIGS_STIMULUS_HUMILIATED:
            return (vox_u16)VOX_DIGS_STIMULUS_TRUCE_OFFERED;
        default:
            break;
        }
    }
    return base;
}

/*
 * Everybody hears it, and what they make of it depends on who they like.
 *
 * A listener that hears the speaker go after somebody updates its account
 * with the *speaker*, signed by how it feels about the *subject*: close to
 * the subject and it sours on the speaker, already hostile to the subject
 * and it warms to them slightly.  Kept well below the weight of a direct
 * hit, so overhearing colours the room without deciding it -- this is what
 * lets alliances form on their own rather than three private two-way
 * channels running side by side.
 */
static void digs_speech_broadcast(vox_digs_match *match, vox_u16 speaker,
                                  vox_u16 subject, vox_u16 stimulus,
                                  vox_u16 audience, int may_reply)
{
    vox_u16 listener;
    for (listener = 0U; listener < match->rules.player_count; ++listener) {
        const vox_digs_contract *feeling;
        vox_i32 lean = 0L;
        int addressed = listener == subject;
        int defends = 0;
        vox_u32 chance;
        if (listener == speaker || !match->alive[listener] ||
            !vox_digs_player_is_active(match, listener)) {
            continue;
        }
        if (!addressed &&
            !digs_speech_within_earshot(match, listener, speaker, audience)) {
            continue;
        }
        if (!addressed && subject < match->rules.player_count &&
            subject != listener) {
            feeling = vox_digs_contract_get(match, listener, subject);
            if (feeling != 0 && feeling->met) {
                if (feeling->tone >= VOX_DIGS_TONE_TRUCE) {
                    lean = -DIGS_SPEECH_OVERHEARD_VALENCE;
                    defends = 1;
                } else if (feeling->tone <= VOX_DIGS_TONE_HOSTILE) {
                    lean = DIGS_SPEECH_OVERHEARD_VALENCE;
                }
            }
        }
        if (lean != 0L && digs_stimulus_valence[stimulus] < 0) {
            digs_contract_adjust(match, listener, speaker, lean);
        }
        /*
         * Anyone may answer, not only whoever it was aimed at.  Feeling
         * strongly about the subject is what makes a third miner speak up.
         */
        if (!may_reply) {
            /*
             * The exchange just closed on its last line.  Everyone still
             * heard it -- the opinions above have already moved -- but
             * nobody answers a conversation that is over.  Queueing a reply
             * here was how a patient miner ended up speaking into a torn
             * down exchange after the between-exchange floor had run.
             */
            continue;
        }
        chance = digs_speech_chattiness(match, listener);
        if (addressed) {
            chance += DIGS_SPEECH_REPLY_BONUS;
        } else if (lean != 0L) {
            chance += DIGS_SPEECH_CHIME_BONUS;
        } else {
            chance = chance / 2U;
        }
        if (digs_speech_roll(match, listener, chance,
                             (vox_u16)(0x7A1BU + listener))) {
            digs_speech_set(match, listener, speaker,
                            digs_speech_arc(match,
                                addressed ?
                                digs_speech_chime_in(stimulus, 1) :
                                digs_speech_chime_in(stimulus, defends),
                                match->speech_exchange_lines + 1U >=
                                DIGS_SPEECH_EXCHANGE_MAX));
        }
    }
}

/*
 * Say the queued thing, then decide whether the other one answers.
 *
 * The reply is what turns two miners shouting into a conversation.  It is
 * gated on sociability, so FLAMEY answers nearly everything and RIVET mostly
 * lets it go, and it only fires while the exchange is still fresh.
 */
static void digs_step_speech(vox_digs_match *match)
{
    vox_u16 player;
    if (match->speech_floor_ticks > 0U) {
        match->speech_floor_ticks--;
    }
    if (match->speech_answer_ticks > 0U) {
        match->speech_answer_ticks--;
        if (match->speech_answer_ticks == 0U) {
            /* The exchange died out.  Now the mine goes quiet for a while. */
            match->speech_floor_ticks = DIGS_SPEECH_FLOOR_BETWEEN;
            match->speech_exchange_lines = 0U;
    match->speech_last_line = 0U;
    match->speech_exchange_heat = 0;
        }
    }
    if (match->speech_dry_ticks < 65535U) {
        match->speech_dry_ticks++;
    }
    for (player = 0U; player < match->rules.player_count; ++player) {
        vox_u16 stimulus;
        vox_u16 subject;
        vox_u16 line;
        vox_u16 tone = (vox_u16)VOX_DIGS_TONE_NEUTRAL;
        vox_u16 audience;
        const vox_digs_personality *personality;
        int pressed;
        if (match->speech_cooldown[player] > 0U) {
            match->speech_cooldown[player]--;
        }
        /*
         * Read the press once and carry it.  Consuming the bit up front and
         * then testing the same bit further down meant the gate always found
         * it already cleared, and the miner never said anything at all.
         */
        pressed = !vox_digs_player_is_bot(match, player) &&
                  match->alive[player] &&
                  (match->player_actions[player] &
                   VOX_DIGS_ACTION_BARK) != 0U;
        if (pressed) {
            /*
             * A bark is an edge, not a state, but player_actions holds
             * whatever was last submitted -- so any caller that submits
             * input less often than it steps would leave the bit set and the
             * miner would talk on every tick the cooldown allowed.
             */
            match->player_actions[player] =
                (vox_u16)(match->player_actions[player] &
                          (vox_u16)~VOX_DIGS_ACTION_BARK);
        }
        /*
         * The button is the timing.  A press works out the context if there
         * is none queued, then clears the pause and takes the floor off
         * whoever is talking -- the press is a single tick, so anything that
         * defers it loses it, and a bark button that sometimes does nothing
         * is worse than one that interrupts.
         */
        if (pressed) {
            if (match->speech_stimulus[player] == VOX_DIGS_STIMULUS_NONE) {
                digs_speech_player_context(match, player);
            }
            match->speech_delay[player] = 0U;
            match->speech_cooldown[player] = 0U;
            match->speech_floor_ticks = 0U;
        }
        stimulus = match->speech_stimulus[player];
        if (stimulus == VOX_DIGS_STIMULUS_NONE) {
            continue;
        }
        if (!match->alive[player]) {
            /* The dead stop mid-sentence, except about dying. */
            if (stimulus != VOX_DIGS_STIMULUS_KILLED_BY &&
                stimulus != VOX_DIGS_STIMULUS_DOOMED) {
                match->speech_stimulus[player] =
                    (vox_u16)VOX_DIGS_STIMULUS_NONE;
                continue;
            }
        }
        if (match->speech_delay[player] > 0U) {
            match->speech_delay[player]--;
            continue;
        }
        /*
         * A miner you are driving does not talk by itself.  The simulation
         * works out what you would say and holds it; pressing bark is what
         * says it.  Bots have no button, so they speak when they are ready.
         */
        if (!vox_digs_player_is_bot(match, player) && !pressed) {
            continue;
        }
        /* Somebody else is mid-sentence.  Wait. */
        if (match->speech_floor_ticks > 0U) {
            continue;
        }
        subject = match->speech_subject[player];
        match->speech_stimulus[player] = (vox_u16)VOX_DIGS_STIMULUS_NONE;
        line = digs_speech_choose(match, player, subject, stimulus, &tone);
        if (line == 65535U) {
            continue;
        }
        personality = digs_speaker_personality(match, player);
        match->speech_last_line = line;
        match->speech_self_stimulus[player] = stimulus;
        match->speech_self_tick[player] = match->tick;
        match->speech_dry_ticks = 0U;
        if (match->speech_exchange_lines < 65535U) {
            match->speech_exchange_lines++;
        }
        {
            vox_i32 worth = (vox_i32)digs_stimulus_valence[stimulus];
            vox_i32 heat = (vox_i32)match->speech_exchange_heat +
                           (worth < 0 ? -1L : (worth > 0 ? 1L : 0L));
            if (heat > 6L) heat = 6L;
            if (heat < -6L) heat = -6L;
            match->speech_exchange_heat = (vox_i16)heat;
        }
        if (match->speech_exchange_lines >= DIGS_SPEECH_EXCHANGE_MAX) {
            /* Enough said.  Close it and buy the silence now. */
            match->speech_answer_ticks = 0U;
            match->speech_exchange_lines = 0U;
    match->speech_last_line = 0U;
    match->speech_exchange_heat = 0;
            match->speech_floor_ticks = DIGS_SPEECH_FLOOR_BETWEEN;
        } else {
            /* The window follows the line: a long remark earns a long pause
             * for somebody to answer it in. */
            vox_u32 window = (vox_u32)vox_digs_speech_duration(line) +
                             DIGS_SPEECH_ANSWER_SLACK;
            match->speech_floor_ticks = DIGS_SPEECH_FLOOR_IN_EXCHANGE;
            match->speech_answer_ticks = (vox_u16)window;
        }
        /*
         * When the player speaks, the mine turns to look.  Everyone is
         * briefly likelier to answer, and everyone's clock is pulled in so
         * the answer arrives while it still reads as an answer -- but it is
         * still a roll, so it is a conversation rather than a vending
         * machine.
         */
        if (!vox_digs_player_is_bot(match, player)) {
            vox_u16 other;
            if (match->speech_answer_ticks < DIGS_SPEECH_ANSWER_WINDOW) {
                match->speech_answer_ticks = DIGS_SPEECH_ANSWER_WINDOW;
            }
            for (other = 0U; other < match->rules.player_count; ++other) {
                vox_u32 spin;
                vox_u16 soon;
                if (other == player || !vox_digs_player_is_bot(match, other)) {
                    continue;
                }
                spin = digs_noise(match->rules.seed, match->tick, other,
                                  0xA715U);
                soon = (vox_u16)(DIGS_SPEECH_ANSWER_MIN +
                                 (spin % DIGS_SPEECH_ANSWER_SPAN));
                if (match->speech_urge_ticks[other] > soon) {
                    match->speech_urge_ticks[other] = soon;
                }
            }
        }
        match->speech_cooldown[player] = (vox_u16)(DIGS_SPEECH_COOLDOWN_MIN +
            ((255U - (vox_u32)personality->sociability) *
             DIGS_SPEECH_COOLDOWN_SPAN) / 255U);
        /*
         * The audience rides on `material`, which for speech only ever held
         * a decorative VOX_MAT_SMOKE that nothing read.  Documented at both
         * ends so it is a deliberate reuse and not a surprise to the next
         * person who greps for it.
         */
        audience = digs_stimulus_audience(stimulus);
        match->speech_audience[player] = audience;
        digs_emit_event(match, VOX_DIGS_EVENT_AI_BARK, player, subject,
                        match->selected_weapon[player], audience,
                        match->players[player].position_x.value_q16,
                        match->players[player].position_y.value_q16,
                        stimulus, line);
        match->events[(match->event_head + match->event_count - 1U) %
                      VOX_DIGS_MAX_EVENTS].reserved = tone;
        /*
         * And now everybody who could hear it decides what to make of it.
         * Replies used to be the addressee's alone, which is precisely why
         * three bots in a room produced three private conversations.
         */
        digs_speech_broadcast(match, player, subject, stimulus, audience,
                              match->speech_answer_ticks > 0U);
    }
}

/* One tick of drift back toward indifference, plus the dwell clocks. */
static void digs_step_contracts(vox_digs_match *match)
{
    vox_u16 a;
    vox_u16 b;
    int decay = (match->tick % DIGS_CONTRACT_DECAY_TICKS) == 0U;
    for (a = 0U; a < VOX_DIGS_MAX_SLOTS; ++a)
    for (b = (vox_u16)(a + 1U); b < VOX_DIGS_MAX_SLOTS; ++b) {
        vox_u16 index = vox_digs_pair_index(a, b);
        vox_digs_contract *contract = &match->contracts[index];
        vox_u16 was = contract->tone;
        if (contract->tone_ticks < 65535U) {
            contract->tone_ticks++;
        }
        if (contract->quiet_ticks < 65535U) {
            contract->quiet_ticks++;
        }
        if (decay && contract->valence > 0) {
            contract->valence = (vox_i16)(contract->valence >
                DIGS_CONTRACT_DECAY_STEP ?
                contract->valence - DIGS_CONTRACT_DECAY_STEP : 0);
        } else if (decay && contract->valence < 0) {
            contract->valence = (vox_i16)(contract->valence <
                -DIGS_CONTRACT_DECAY_STEP ?
                contract->valence + DIGS_CONTRACT_DECAY_STEP : 0);
        }
        digs_contract_settle(contract);
        /*
         * Announce crossings, without moving the account.  A stimulus that
         * pushed the tone that produced it would be a feedback loop, so this
         * only gives the pair something to say about where they have got to.
         */
        if (contract->tone != was && contract->met) {
            vox_u16 crossing = (vox_u16)VOX_DIGS_STIMULUS_NONE;
            if (contract->tone >= VOX_DIGS_TONE_TRUCE &&
                was < VOX_DIGS_TONE_TRUCE) {
                crossing = (vox_u16)VOX_DIGS_STIMULUS_TRUCE_ACCEPTED;
            } else if (was >= VOX_DIGS_TONE_TRUCE &&
                       contract->tone < VOX_DIGS_TONE_TRUCE) {
                crossing = (vox_u16)VOX_DIGS_STIMULUS_TRUCE_BROKEN;
            } else if (contract->tone == VOX_DIGS_TONE_FEUD &&
                       was != VOX_DIGS_TONE_FEUD) {
                crossing = (vox_u16)VOX_DIGS_STIMULUS_HUMILIATED;
            }
            if (crossing != VOX_DIGS_STIMULUS_NONE) {
                contract->last_stimulus = crossing;
                contract->last_stimulus_tick = match->tick;
                digs_speech_prompt(match, a, b, crossing);
                digs_speech_prompt(match, b, a, crossing);
            }
        }
    }
}

vox_result vox_digs_record_kill(vox_digs_match *match, vox_u16 killer,
                                vox_u16 victim)
{
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_active(match, killer) ||
        !vox_digs_player_is_active(match, victim) || !match->alive[victim] ||
        killer == victim) {
        return VOX_ERR_INVALID;
    }
    if (match->scores[killer] < 65535U) {
        match->scores[killer]++;
    }
    /*
     * Killing the miner who last killed you is a different thing from picking
     * a fight, and the account should read it that way.
     */
    digs_contract_note(match, killer, victim,
                       match->last_attacker[killer] == victim ?
                       (vox_u16)VOX_DIGS_STIMULUS_REVENGE :
                       (vox_u16)VOX_DIGS_STIMULUS_KILLED_THEM);
    if (match->alive[killer]) {
        vox_u32 healed = (vox_u32)match->health[killer] + DIGS_KILL_HEAL;
        match->health[killer] = healed > (vox_u32)VOX_DIGS_MAX_HEALTH ?
                                (vox_u16)VOX_DIGS_MAX_HEALTH :
                                (vox_u16)healed;
    }
    match->alive[victim] = 0U;
    match->health[victim] = 0U;
    match->deaths[victim]++;
    digs_prepare_respawn(match, victim);
    match->spawn_shield_ticks[victim] = 0U;
    match->player_actions[victim] = 0U;
    match->move_x_q15[victim] = 0;
    match->move_y_q15[victim] = 0;
    match->last_attacker[victim] = killer;
    match->last_attacker_tick[victim] = match->tick;
    digs_detach_rope(match, victim, VOX_DIGS_EVENT_ROPE_DETACH);
    digs_spawn_death_gore(match, victim, killer);
    digs_emit_event(match, VOX_DIGS_EVENT_KILL, killer, victim,
                    match->last_damage_weapon[victim], VOX_MAT_BLOOD,
                    match->players[victim].position_x.value_q16,
                    match->players[victim].position_y.value_q16,
                    match->scores[killer],
                    (vox_u16)(digs_noise(match->rules.seed, match->tick,
                                         killer, victim) & 15U));
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_result vox_digs_submit_input(vox_digs_match *match,
                                 const vox_digs_input *input)
{
    if (match == 0 || input == 0 || match->phase != VOX_DIGS_RUNNING ||
        input->abi_version != VOX_ABI_VERSION ||
        input->struct_size < (vox_u32)sizeof(*input) ||
        !vox_digs_player_is_active(match, input->player) ||
        vox_digs_player_is_bot(match, input->player) ||
        !match->alive[input->player] ||
        (input->actions & (vox_u16)~VOX_DIGS_ACTION_MASK) != 0U ||
        input->aim_x >= VOX_WORLD_WIDTH ||
        input->aim_y >= VOX_WORLD_HEIGHT ||
        input->move_x_q15 == (vox_i16)-32768L ||
        input->move_y_q15 == (vox_i16)-32768L ||
        input->selected_weapon >= VOX_DIGS_TOOL_COUNT) {
        return VOX_ERR_INVALID;
    }
    match->player_actions[input->player] = input->actions;
    match->aim_x[input->player] = input->aim_x;
    match->aim_y[input->player] = input->aim_y;
    match->move_x_q15[input->player] = input->move_x_q15;
    match->move_y_q15[input->player] = input->move_y_q15;
    match->selected_weapon[input->player] = input->selected_weapon;
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

static void digs_end_spawn_shield(vox_digs_match *match, vox_u16 player)
{
    if (match->spawn_shield_ticks[player] == 0U) {
        return;
    }
    match->spawn_shield_ticks[player] = 0U;
    digs_emit_event(match, VOX_DIGS_EVENT_SHIELD_END, player,
                    VOX_DIGS_NO_PLAYER, match->selected_weapon[player],
                    VOX_MAT_METAL,
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    0U, 1U);
}

vox_result vox_digs_use_tool(vox_digs_match *match, vox_u16 player,
                             vox_u16 tool, vox_u32 x, vox_u32 y, vox_u32 z)
{
    const vox_cell *target;
    vox_result result;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_active(match, player) || !match->alive[player] ||
        tool >= VOX_DIGS_TOOL_COUNT) {
        return VOX_ERR_INVALID;
    }
    target = vox_world_cell(&match->world, x, y, z);
    if (target == 0 || target->material == VOX_MAT_BEDROCK) {
        return VOX_ERR_INVALID;
    }
    if (tool == VOX_DIGS_TOOL_PICK) {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_AIR,
                               20L << 16);
    } else if (tool == VOX_DIGS_TOOL_BLAST_CHARGE) {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(3U), 700L << 16);
    } else if (tool == VOX_DIGS_TOOL_SMOKE_POT) {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_SMOKE,
                               180L << 16);
    } else if (tool == VOX_DIGS_TOOL_CINDER_FLASK) {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_LAVA,
                               700L << 16);
    } else if (tool == VOX_DIGS_TOOL_PRESSURE_HOSE) {
        result = vox_world_set(&match->world, x, y, z, VOX_MAT_WATER,
                               20L << 16);
    } else if (tool == VOX_DIGS_TOOL_SLEDGE) {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(2U), 80L << 16);
    } else if (tool == VOX_DIGS_TOOL_NAIL_GUN) {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(1U), 40L << 16);
    } else if (tool == VOX_DIGS_TOOL_BOILER_SHOTGUN) {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(2U), 180L << 16);
    } else if (tool == VOX_DIGS_TOOL_CONCUSSION_GRENADE) {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(5U), 400L << 16);
    } else {
        result = vox_world_blast(&match->world, x, y, z,
                                 DIGS_SCALE(4U), 500L << 16);
    }
    if (result != VOX_OK) {
        return result;
    }
    match->selected_weapon[player] = tool;
    digs_end_spawn_shield(match, player);
    digs_emit_event(match, VOX_DIGS_EVENT_WEAPON_FIRE, player,
                    VOX_DIGS_NO_PLAYER, tool, target->material,
                    (vox_i32)(x << 16), (vox_i32)(y << 16),
                    digs_weapons[tool].damage,
                    (vox_u16)(digs_noise(match->rules.seed, match->tick,
                                         player, tool) & 15U));
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

const vox_digs_weapon_properties *vox_digs_weapon_get(vox_u16 weapon)
{
    if (weapon >= VOX_DIGS_TOOL_COUNT) {
        return 0;
    }
    return &digs_weapons[weapon];
}

static vox_u32 digs_abs_i32(vox_i32 value)
{
    if (value < 0) {
        return (vox_u32)(-(value + 1)) + 1U;
    }
    return (vox_u32)value;
}

/* C89 permits either rounding direction for negative signed division. */
static vox_i32 digs_div_trunc_positive(vox_i32 value, vox_u32 divisor)
{
    vox_u32 quotient;
    if (divisor == 0U) {
        return 0;
    }
    quotient = digs_abs_i32(value) / divisor;
    if (value >= 0) {
        return (vox_i32)quotient;
    }
    if (quotient == 0x80000000U) {
        return (vox_i32)(-2147483647L - 1L);
    }
    return -(vox_i32)quotient;
}

static vox_i32 digs_q16_to_cell(vox_i32 value)
{
    return digs_div_trunc_positive(value, 65536U);
}

/* Returns floor(numerator * 65535 / denominator) without a 64-bit type. */
static vox_u32 digs_scale_lava_level(vox_u32 numerator,
                                     vox_u32 denominator)
{
    vox_u32 quotient = 0U;
    vox_u32 remainder;
    vox_u16 bit;
    if (denominator == 0U || numerator == 0U) {
        return 0U;
    }
    if (numerator >= denominator) {
        return 65535U;
    }
    remainder = numerator;
    for (bit = 0U; bit < 16U; ++bit) {
        quotient <<= 1;
        if (remainder >= denominator - remainder) {
            remainder -= denominator - remainder;
            quotient |= 1U;
        } else {
            remainder += remainder;
        }
    }
    if (remainder < numerator && quotient > 0U) {
        quotient--;
    }
    return quotient;
}

static void digs_spawn_effect(vox_digs_match *match, vox_u16 material,
                              vox_i32 x_q16, vox_i32 y_q16,
                              vox_i32 velocity_x_q16,
                              vox_i32 velocity_y_q16, vox_u16 ttl)
{
    vox_u16 variant = (vox_u16)(digs_noise(match->rules.seed,
                                           match->tick,
                                           match->effect_cursor,
                                           material) & 31U);
    digs_spawn_effect_variant(match, material, x_q16, y_q16,
                              velocity_x_q16, velocity_y_q16, ttl,
                              VOX_DIGS_NO_PLAYER, variant);
}

static void digs_spawn_effect_variant(vox_digs_match *match,
                                      vox_u16 material,
                                      vox_i32 x_q16, vox_i32 y_q16,
                                      vox_i32 velocity_x_q16,
                                      vox_i32 velocity_y_q16, vox_u16 ttl,
                                      vox_u16 source, vox_u16 variant)
{
    vox_u16 search;
    vox_u16 slot = match->effect_cursor;
    vox_u16 capacity = match->rules.fx_budget;
    int found = 0;
    for (search = 0U; search < capacity; ++search) {
        vox_u16 candidate = (vox_u16)((match->effect_cursor + search) %
                                       capacity);
        if (!match->effects[candidate].active) {
            slot = candidate;
            found = 1;
            break;
        }
    }
    if (found) {
        match->effect_count++;
    }
    match->effects[slot].position_x_q16 = x_q16;
    match->effects[slot].position_y_q16 = y_q16;
    match->effects[slot].velocity_x_q16 = velocity_x_q16;
    match->effects[slot].velocity_y_q16 = velocity_y_q16;
    match->effects[slot].active = 1U;
    match->effects[slot].material = material;
    match->effects[slot].ttl_ticks = ttl;
    match->effects[slot].variant = variant;
    match->effects[slot].source = source;
    match->effects[slot].depth = (vox_u16)(variant % VOX_WORLD_DEPTH);
    match->effects[slot].flags = 0U;
    match->effect_cursor = (vox_u16)((slot + 1U) % capacity);
}

static void digs_environment_defeat(vox_digs_match *match, vox_u16 victim)
{
    match->alive[victim] = 0U;
    match->health[victim] = 0U;
    match->deaths[victim]++;
    digs_prepare_respawn(match, victim);
    match->spawn_shield_ticks[victim] = 0U;
    match->player_actions[victim] = 0U;
    match->move_x_q15[victim] = 0;
    match->move_y_q15[victim] = 0;
    match->last_attacker[victim] = VOX_DIGS_NO_PLAYER;
    match->last_attacker_tick[victim] = 0U;
    digs_detach_rope(match, victim, VOX_DIGS_EVENT_ROPE_DETACH);
    digs_spawn_death_gore(match, victim, VOX_DIGS_NO_PLAYER);
    digs_emit_event(match, VOX_DIGS_EVENT_KILL, VOX_DIGS_NO_PLAYER,
                    victim, VOX_DIGS_TOOL_PICK, VOX_MAT_BLOOD,
                    match->players[victim].position_x.value_q16,
                    match->players[victim].position_y.value_q16,
                    match->deaths[victim],
                    (vox_u16)(digs_noise(match->rules.seed, match->tick,
                                         victim, 0xDEADU) & 15U));
}

static vox_u16 digs_choose_hit_part(const vox_digs_match *match,
                                    vox_u16 attacker, vox_u16 victim,
                                    vox_u16 weapon, vox_u16 damage)
{
    vox_u32 noise = digs_noise(match->rules.seed, match->tick,
                               (vox_u32)victim,
                               (vox_u32)attacker * 31U +
                               (vox_u32)weapon * 131U + damage);
    if (weapon == VOX_DIGS_TOOL_SLEDGE || weapon == VOX_DIGS_TOOL_PICK) {
        static const vox_u16 melee_parts[6] = {
            VOX_DIGS_PART_HEAD, VOX_DIGS_PART_TORSO,
            VOX_DIGS_PART_PELVIS, VOX_DIGS_PART_LEFT_UPPER_ARM,
            VOX_DIGS_PART_RIGHT_UPPER_ARM, VOX_DIGS_PART_TORSO
        };
        return melee_parts[noise % 6U];
    }
    if (weapon == VOX_DIGS_TOOL_NAIL_GUN ||
        weapon == VOX_DIGS_TOOL_BOILER_SHOTGUN ||
        weapon == VOX_DIGS_TOOL_RAIL_GUN) {
        return (vox_u16)(noise % VOX_DIGS_ANATOMY_PART_COUNT);
    }
    return (vox_u16)((noise + (noise >> 9)) %
                     VOX_DIGS_ANATOMY_PART_COUNT);
}

static int digs_part_descends_from(vox_u16 candidate, vox_u16 ancestor)
{
    if (candidate == ancestor) {
        return 1;
    }
    if ((ancestor == VOX_DIGS_PART_LEFT_UPPER_ARM &&
         (candidate == VOX_DIGS_PART_LEFT_FOREARM ||
          candidate == VOX_DIGS_PART_LEFT_HAND)) ||
        (ancestor == VOX_DIGS_PART_RIGHT_UPPER_ARM &&
         (candidate == VOX_DIGS_PART_RIGHT_FOREARM ||
          candidate == VOX_DIGS_PART_RIGHT_HAND)) ||
        (ancestor == VOX_DIGS_PART_LEFT_FOREARM &&
         candidate == VOX_DIGS_PART_LEFT_HAND) ||
        (ancestor == VOX_DIGS_PART_RIGHT_FOREARM &&
         candidate == VOX_DIGS_PART_RIGHT_HAND) ||
        (ancestor == VOX_DIGS_PART_LEFT_THIGH &&
         (candidate == VOX_DIGS_PART_LEFT_SHIN ||
          candidate == VOX_DIGS_PART_LEFT_FOOT)) ||
        (ancestor == VOX_DIGS_PART_RIGHT_THIGH &&
         (candidate == VOX_DIGS_PART_RIGHT_SHIN ||
          candidate == VOX_DIGS_PART_RIGHT_FOOT)) ||
        (ancestor == VOX_DIGS_PART_LEFT_SHIN &&
         candidate == VOX_DIGS_PART_LEFT_FOOT) ||
        (ancestor == VOX_DIGS_PART_RIGHT_SHIN &&
         candidate == VOX_DIGS_PART_RIGHT_FOOT)) {
        return 1;
    }
    return 0;
}

static void digs_sever_limb_chain(vox_digs_match *match, vox_u16 attacker,
                                  vox_u16 victim, vox_u16 weapon,
                                  vox_u16 part, vox_u16 damage_flags,
                                  vox_i32 wound_x_q16, vox_i32 wound_y_q16)
{
    vox_u16 candidate;
    for (candidate = 0U; candidate < VOX_DIGS_ANATOMY_PART_COUNT;
         ++candidate) {
        vox_digs_anatomy_part *anatomy = &match->anatomy[victim][candidate];
        vox_u16 gib;
        if (!digs_part_descends_from(candidate, part) ||
            (anatomy->flags & VOX_DIGS_PART_VITAL) != 0U ||
            (anatomy->flags & VOX_DIGS_PART_SEVERED) != 0U) {
            continue;
        }
        anatomy->flags = (vox_u16)(anatomy->flags |
                                   VOX_DIGS_PART_SEVERED |
                                   VOX_DIGS_PART_BLEEDING);
        anatomy->health = 0U;
        if (anatomy->bleed_rate_q8 < 12U) {
            anatomy->bleed_rate_q8 = 12U;
        }
        for (gib = 0U; gib < 4U; ++gib) {
            vox_u32 noise = digs_noise(match->rules.seed, match->tick + gib,
                                       candidate, victim + weapon * 41U);
            digs_spawn_effect_variant(match, VOX_MAT_FLESH,
                wound_x_q16 + ((vox_i32)(noise % 7U) - 3L) * 4096L,
                wound_y_q16 + ((vox_i32)((noise >> 5) % 5U) - 2L) * 4096L,
                ((vox_i32)((noise >> 9) % 17U) - 8L) * 6144L,
                -12288L - (vox_i32)((noise >> 15) % 10U) * 4096L,
                (vox_u16)(54U + noise % 72U), victim,
                (vox_u16)((candidate << 3) | gib));
        }
        if (attacker != VOX_DIGS_NO_PLAYER && attacker != victim) {
            digs_contract_note(match, attacker, victim,
                               (vox_u16)VOX_DIGS_STIMULUS_LIMB_TAKEN);
        }
        digs_emit_event(match, VOX_DIGS_EVENT_LIMB_SEVER, attacker, victim,
                        weapon, VOX_MAT_FLESH, wound_x_q16, wound_y_q16,
                        candidate, (vox_u16)(damage_flags & 15U));
    }
}

vox_result vox_digs_apply_hit(vox_digs_match *match, vox_u16 attacker,
                              vox_u16 victim, vox_u16 weapon,
                              vox_u16 part, vox_u16 damage,
                              vox_u16 damage_flags)
{
    vox_digs_anatomy_part *anatomy;
    vox_u16 particle_count;
    vox_u16 i;
    vox_u16 fatal = 0U;
    vox_u16 aggregate_damage = damage;
    vox_digs_hurtbox wound_box;
    vox_i32 wound_x_q16;
    vox_i32 wound_y_q16;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_active(match, victim) ||
        !match->alive[victim] || damage == 0U ||
        weapon >= VOX_DIGS_TOOL_COUNT ||
        (part != VOX_DIGS_NO_PART &&
         part >= VOX_DIGS_ANATOMY_PART_COUNT) ||
        (damage_flags & (vox_u16)~(VOX_DIGS_DAMAGE_BALLISTIC |
                                  VOX_DIGS_DAMAGE_BLUNT |
                                  VOX_DIGS_DAMAGE_EXPLOSIVE |
                                  VOX_DIGS_DAMAGE_HEAT)) != 0U ||
        (attacker != VOX_DIGS_NO_PLAYER &&
         !vox_digs_player_is_active(match, attacker))) {
        return VOX_ERR_INVALID;
    }
    if (attacker != VOX_DIGS_NO_PLAYER && attacker != victim) {
        digs_contract_note(match, attacker, victim,
                           (vox_u16)VOX_DIGS_STIMULUS_HURT_THEM);
    }
    if (match->spawn_shield_ticks[victim] > 0U) {
        digs_emit_event(match, VOX_DIGS_EVENT_SHIELD_BLOCK, attacker, victim,
                        weapon, VOX_MAT_METAL,
                        match->players[victim].position_x.value_q16,
                        match->players[victim].position_y.value_q16,
                        damage, 1U);
        return VOX_OK;
    }
    if (part == VOX_DIGS_NO_PART) {
        part = digs_choose_hit_part(match, attacker, victim, weapon, damage);
    }
    if (vox_digs_anatomy_hurtbox(part, &wound_box) != VOX_OK) {
        return VOX_ERR_INVALID;
    }
    wound_x_q16 = match->players[victim].position_x.value_q16 +
                   wound_box.offset_x_q16;
    wound_y_q16 = match->players[victim].position_y.value_q16 +
                   wound_box.offset_y_q16;
    anatomy = &match->anatomy[victim][part];
    if (weapon == VOX_DIGS_TOOL_RAIL_GUN &&
        part >= VOX_DIGS_PART_LEFT_UPPER_ARM) {
        aggregate_damage = (vox_u16)((damage + 1U) / 2U);
    }
    if (attacker != VOX_DIGS_NO_PLAYER && attacker != victim) {
        match->last_attacker[victim] = attacker;
        match->last_attacker_tick[victim] = match->tick;
    }
    match->last_damage_weapon[victim] = weapon;
    match->last_damage_part[victim] = part;
    particle_count = (vox_u16)(damage / 6U + 3U);
    if (match->rules.fx_budget == VOX_DIGS_FX_CARNAGE) {
        particle_count = (vox_u16)(particle_count * 2U);
    } else if (match->rules.fx_budget == VOX_DIGS_FX_RETRO &&
               particle_count > 12U) {
        particle_count = 12U;
    }
    if (particle_count > 40U) {
        particle_count = 40U;
    }
    for (i = 0U; i < particle_count; ++i) {
        vox_u32 noise = digs_noise(match->rules.seed, match->tick + i,
                                   (vox_u32)victim,
                                   (vox_u32)part * 257U + damage);
        vox_i32 spread_x = ((vox_i32)(noise % 17U) - 8L) * 5120L;
        vox_i32 spread_y = -10240L -
            (vox_i32)((noise >> 8) % 13U) * 4096L;
        digs_spawn_effect_variant(match, VOX_MAT_BLOOD,
            wound_x_q16, wound_y_q16,
            spread_x, spread_y, (vox_u16)(34U + noise % 60U), victim,
            (vox_u16)((part << 4) | (noise & 15U)));
    }
    if (damage < anatomy->health) {
        anatomy->health = (vox_u16)(anatomy->health - damage);
    } else {
        anatomy->health = 0U;
        if (anatomy->flags & VOX_DIGS_PART_VITAL) {
            fatal = 1U;
        } else if (!(anatomy->flags & VOX_DIGS_PART_SEVERED)) {
            digs_sever_limb_chain(match, attacker, victim, weapon, part,
                                  damage_flags, wound_x_q16, wound_y_q16);
        }
    }
    if (damage_flags & VOX_DIGS_DAMAGE_HEAT) {
        anatomy->flags = (vox_u16)((anatomy->flags |
                                    VOX_DIGS_PART_CAUTERIZED) &
                                   (vox_u16)~VOX_DIGS_PART_BLEEDING);
        anatomy->bleed_rate_q8 = 0U;
    } else {
        vox_u16 bleed = (vox_u16)(damage / 10U + 1U);
        if (bleed > 16U) {
            bleed = 16U;
        }
        anatomy->flags = (vox_u16)(anatomy->flags |
                                   VOX_DIGS_PART_BLEEDING);
        if (anatomy->bleed_rate_q8 <= (vox_u16)(255U - bleed)) {
            anatomy->bleed_rate_q8 = (vox_u16)(anatomy->bleed_rate_q8 +
                                                bleed);
        } else {
            anatomy->bleed_rate_q8 = 255U;
        }
        match->clot_ticks[victim] = 0U;
    }
    digs_emit_event(match, VOX_DIGS_EVENT_DAMAGE, attacker, victim,
                    weapon, VOX_MAT_BLOOD,
                    wound_x_q16, wound_y_q16,
                    damage, (vox_u16)((part << 4) |
                                      (damage_flags & 15U)));
    if (aggregate_damage < match->health[victim]) {
        match->health[victim] = (vox_u16)(match->health[victim] -
                                           aggregate_damage);
    } else {
        match->health[victim] = 0U;
        fatal = 1U;
    }
    if (fatal && attacker != VOX_DIGS_NO_PLAYER && attacker != victim) {
        if (vox_digs_record_kill(match, attacker, victim) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    } else if (fatal && digs_last_attacker_is_recent(match, victim)) {
        if (vox_digs_record_kill(match, match->last_attacker[victim],
                                 victim) != VOX_OK) {
            return VOX_ERR_INVALID;
        }
    } else if (fatal) {
        digs_environment_defeat(match, victim);
    }
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

vox_result vox_digs_apply_damage(vox_digs_match *match, vox_u16 attacker,
                                 vox_u16 victim, vox_u16 damage)
{
    vox_u16 weapon = VOX_DIGS_TOOL_PICK;
    vox_u16 flags = VOX_DIGS_DAMAGE_BLUNT;
    if (attacker != VOX_DIGS_NO_PLAYER && match != 0 &&
        vox_digs_player_is_active(match, attacker)) {
        weapon = match->selected_weapon[attacker];
    }
    if (weapon == VOX_DIGS_TOOL_NAIL_GUN ||
        weapon == VOX_DIGS_TOOL_BOILER_SHOTGUN) {
        flags = VOX_DIGS_DAMAGE_BALLISTIC;
    } else if (weapon == VOX_DIGS_TOOL_BLAST_CHARGE ||
               weapon == VOX_DIGS_TOOL_CONCUSSION_GRENADE ||
               weapon == VOX_DIGS_TOOL_NAIL_BOMB) {
        flags = VOX_DIGS_DAMAGE_EXPLOSIVE;
    } else if (weapon == VOX_DIGS_TOOL_CINDER_FLASK) {
        flags = VOX_DIGS_DAMAGE_HEAT;
    }
    return vox_digs_apply_hit(match, attacker, victim, weapon,
                              VOX_DIGS_NO_PART, damage, flags);
}

static vox_u16 digs_projectile_material(vox_u16 weapon)
{
    if (weapon == VOX_DIGS_TOOL_SMOKER) {
        return VOX_MAT_SMOKE;
    }
    if (weapon == VOX_DIGS_TOOL_HOT_RAIL) {
        return VOX_MAT_LAVA;
    }
    if (weapon == VOX_DIGS_TOOL_HYDROSHOT) {
        return VOX_MAT_WATER;
    }
    if (weapon == VOX_DIGS_TOOL_POPPER ||
        weapon == VOX_DIGS_TOOL_FIRECRACKER) {
        return VOX_MAT_COAL;
    }
    return VOX_MAT_METAL;
}

static vox_result digs_spawn_projectile(vox_digs_match *match,
                                        vox_u16 player, vox_u16 weapon,
                                        vox_u32 target_x,
                                        vox_u32 target_y,
                                        vox_u16 speed_scale_q8)
{
    const vox_digs_weapon_properties *properties = &digs_weapons[weapon];
    vox_digs_projectile *projectile = 0;
    vox_i32 source_x;
    vox_i32 source_y;
    vox_i32 delta_x;
    vox_i32 delta_y;
    vox_u32 divisor;
    vox_i32 speed_q16;
    vox_i32 muzzle_distance_q16;
    vox_i32 distance_x_q16;
    vox_i32 distance_y_q16;
    vox_i32 center_x_q16;
    vox_i32 center_y_q16;
    vox_u16 i;
    for (i = 0U; i < VOX_DIGS_MAX_PROJECTILES; ++i) {
        if (!match->projectiles[i].active) {
            projectile = &match->projectiles[i];
            break;
        }
    }
    if (projectile == 0) {
        return VOX_ERR_CAPACITY;
    }
    source_x = digs_q16_to_cell(
        match->players[player].position_x.value_q16);
    source_y = digs_q16_to_cell(
        match->players[player].position_y.value_q16);
    delta_x = (vox_i32)target_x - source_x;
    delta_y = (vox_i32)target_y - source_y;
    if (delta_x == 0 && delta_y == 0) {
        delta_x = match->facing_right[player] ? 1L : -1L;
    }
    divisor = digs_abs_i32(delta_x);
    if (digs_abs_i32(delta_y) > divisor) {
        divisor = digs_abs_i32(delta_y);
    }
    if (divisor == 0U) {
        divisor = 1U;
    }
    speed_q16 = ((vox_i32)properties->projectile_speed_q8 << 8) *
                (vox_i32)speed_scale_q8 / 256L;
    center_x_q16 = match->players[player].position_x.value_q16;
    center_y_q16 = match->players[player].position_y.value_q16;
    distance_x_q16 = 2147483647L;
    distance_y_q16 = 2147483647L;
    if (delta_x != 0) {
        distance_x_q16 = digs_div_trunc_positive(
            match->players[player].half_width_q16 * (vox_i32)divisor,
            digs_abs_i32(delta_x));
    }
    if (delta_y != 0) {
        distance_y_q16 = digs_div_trunc_positive(
            match->players[player].half_height_q16 * (vox_i32)divisor,
            digs_abs_i32(delta_y));
    }
    muzzle_distance_q16 = distance_x_q16 < distance_y_q16 ?
                          distance_x_q16 : distance_y_q16;
    if (muzzle_distance_q16 == 2147483647L) {
        muzzle_distance_q16 = match->players[player].half_width_q16;
    }
    muzzle_distance_q16 += DIGS_MUZZLE_CLEARANCE_Q16;
    projectile->position_x_q16 = center_x_q16 +
        digs_div_trunc_positive(delta_x * muzzle_distance_q16, divisor);
    projectile->position_y_q16 = center_y_q16 +
        digs_div_trunc_positive(delta_y * muzzle_distance_q16, divisor);
    projectile->velocity_x_q16 = digs_div_trunc_positive(
        delta_x * speed_q16, divisor);
    projectile->velocity_y_q16 = digs_div_trunc_positive(
        delta_y * speed_q16, divisor);
    projectile->active = 1U;
    projectile->owner = player;
    projectile->weapon = weapon;
    projectile->material = digs_projectile_material(weapon);
    projectile->fuse_ticks = properties->fuse_ticks;
    projectile->age_ticks = 0U;
    projectile->damage = properties->damage;
    projectile->blast_radius = properties->blast_radius;
    projectile->launch_min_x_q16 = center_x_q16 -
        match->players[player].half_width_q16 -
        DIGS_MUZZLE_CLEARANCE_Q16;
    projectile->launch_max_x_q16 = center_x_q16 +
        match->players[player].half_width_q16 +
        DIGS_MUZZLE_CLEARANCE_Q16;
    projectile->launch_min_y_q16 = center_y_q16 -
        match->players[player].half_height_q16 -
        DIGS_MUZZLE_CLEARANCE_Q16;
    projectile->launch_max_y_q16 = center_y_q16 +
        match->players[player].half_height_q16 +
        DIGS_MUZZLE_CLEARANCE_Q16;
    projectile->owner_clear = 1U;
    projectile->arming_ticks = VOX_DIGS_PROJECTILE_OWNER_CLEAR_TICKS;
    match->projectile_count++;
    return VOX_OK;
}

static vox_i32 digs_nearest_hurtbox_distance(const vox_digs_match *match,
                                             vox_u16 player,
                                             vox_i32 x_q16,
                                             vox_i32 y_q16,
                                             vox_u16 *part_out,
                                             vox_i32 *nearest_x_q16,
                                             vox_i32 *nearest_y_q16)
{
    vox_i32 best = 2147483647L;
    vox_u16 best_part = VOX_DIGS_NO_PART;
    vox_i32 best_x = match->players[player].position_x.value_q16;
    vox_i32 best_y = match->players[player].position_y.value_q16;
    vox_u16 part;
    for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
        vox_digs_hurtbox box;
        vox_i32 center_x;
        vox_i32 center_y;
        vox_i32 point_x;
        vox_i32 point_y;
        vox_i32 distance;
        if ((match->anatomy[player][part].flags &
             VOX_DIGS_PART_SEVERED) != 0U ||
            vox_digs_anatomy_hurtbox(part, &box) != VOX_OK) {
            continue;
        }
        center_x = match->players[player].position_x.value_q16 +
                   box.offset_x_q16;
        center_y = match->players[player].position_y.value_q16 +
                   box.offset_y_q16;
        point_x = x_q16;
        point_y = y_q16;
        if (point_x < center_x - box.half_width_q16) {
            point_x = center_x - box.half_width_q16;
        } else if (point_x > center_x + box.half_width_q16) {
            point_x = center_x + box.half_width_q16;
        }
        if (point_y < center_y - box.half_height_q16) {
            point_y = center_y - box.half_height_q16;
        } else if (point_y > center_y + box.half_height_q16) {
            point_y = center_y + box.half_height_q16;
        }
        distance = digs_distance_approx(point_x - x_q16,
                                        point_y - y_q16);
        if (distance < best) {
            best = distance;
            best_part = part;
            best_x = point_x;
            best_y = point_y;
        }
    }
    if (part_out != 0) *part_out = best_part;
    if (nearest_x_q16 != 0) *nearest_x_q16 = best_x;
    if (nearest_y_q16 != 0) *nearest_y_q16 = best_y;
    return best;
}

static void digs_damage_radius(vox_digs_match *match, vox_u16 attacker,
                               vox_u32 x, vox_u32 y, vox_u16 radius,
                               vox_u16 damage, vox_u16 weapon,
                               vox_u16 ignore_attacker)
{
    vox_u16 player;
    vox_i32 radius_q16 = (vox_i32)radius << 16;
    vox_i32 blast_x_q16 = (vox_i32)(x << 16) + 32768L;
    vox_i32 blast_y_q16 = (vox_i32)(y << 16) + 32768L;
    for (player = 0U; player < match->rules.player_count; ++player) {
        vox_i32 nearest_x;
        vox_i32 nearest_y;
        vox_i32 distance;
        vox_i32 delta_x;
        vox_u16 dealt;
        vox_u16 part;
        if (!match->alive[player] ||
            (ignore_attacker && player == attacker)) {
            continue;
        }
        distance = digs_nearest_hurtbox_distance(
            match, player, blast_x_q16, blast_y_q16, &part,
            &nearest_x, &nearest_y);
        if (distance > radius_q16 || part == VOX_DIGS_NO_PART) {
            continue;
        }
        dealt = (vox_u16)((vox_u32)damage *
            (vox_u32)(radius_q16 - distance + 65536L) /
            (vox_u32)(radius_q16 + 65536L));
        if (dealt == 0U) {
            dealt = 1U;
        }
        (void)vox_digs_apply_hit(match, attacker, player, weapon,
                                 part, dealt,
                                 VOX_DIGS_DAMAGE_EXPLOSIVE);
        if (match->alive[player] && radius != 0U &&
            match->spawn_shield_ticks[player] == 0U) {
            vox_i32 impulse = weapon == VOX_DIGS_TOOL_CONCUSSION_GRENADE ?
                              65536L : 32768L;
            delta_x = nearest_x - blast_x_q16;
            match->players[player].velocity_x.value_q16 +=
                delta_x < 0 ? -impulse : impulse;
            match->players[player].velocity_y.value_q16 = -impulse;
        }
    }
}

static vox_result digs_fire_melee(vox_digs_match *match, vox_u16 player,
                                  vox_u16 weapon, vox_u32 target_x,
                                  vox_u32 target_y)
{
    const vox_digs_weapon_properties *properties = &digs_weapons[weapon];
    vox_i32 player_x = digs_q16_to_cell(
        match->players[player].position_x.value_q16);
    vox_i32 player_y = digs_q16_to_cell(
        match->players[player].position_y.value_q16);
    vox_i32 delta_x = (vox_i32)target_x - player_x;
    vox_i32 delta_y = (vox_i32)target_y - player_y;
    vox_u16 victim;
    vox_u16 reach = weapon == VOX_DIGS_TOOL_GIANT_HAMMER ?
                    DIGS_SCALE(5U) : DIGS_SCALE(4U);
    int terrain_blocked = digs_cell_is_solid(&match->world, target_x,
                                              target_y);
    if (digs_abs_i32(delta_x) > reach || digs_abs_i32(delta_y) > reach) {
        return VOX_ERR_INVALID;
    }
    if (target_x > 0U && target_y > 0U &&
        target_x + 1U < VOX_WORLD_WIDTH &&
        target_y + 1U < VOX_WORLD_HEIGHT) {
        (void)vox_world_blast(&match->world, target_x, target_y,
                              VOX_WORLD_DEPTH - 1U,
                              properties->blast_radius, 120L << 16);
    }
    for (victim = 0U; victim < match->rules.player_count; ++victim) {
        vox_u16 part = VOX_DIGS_NO_PART;
        if (victim == player || !match->alive[victim]) {
            continue;
        }
        if (!terrain_blocked && digs_point_hits_player(
                match, victim, (vox_i32)(target_x << 16) + 32768L,
                (vox_i32)(target_y << 16) + 32768L, &part)) {
            (void)vox_digs_apply_hit(match, player, victim, weapon,
                                     part,
                                     properties->damage,
                                     weapon == VOX_DIGS_TOOL_PULASKI ?
                                     VOX_DIGS_DAMAGE_BALLISTIC :
                                     VOX_DIGS_DAMAGE_BLUNT);
        }
    }
    if (weapon == VOX_DIGS_TOOL_GIANT_HAMMER) {
        digs_damage_radius(match, player, target_x, target_y,
                           DIGS_SCALE(4U), (vox_u16)(properties->damage / 2U),
                           weapon, 1U);
        match->players[player].velocity_x.value_q16 -=
            delta_x < 0L ? -65536L : 65536L;
        match->players[player].velocity_y.value_q16 = -49152L;
    }
    digs_spawn_effect(match, VOX_MAT_METAL,
                      (vox_i32)(target_x << 16),
                      (vox_i32)(target_y << 16), 0L, -16384L, 12U);
    return VOX_OK;
}

static int digs_rail_terrain(vox_digs_match *match, vox_u32 x, vox_u32 y,
                             vox_u16 *energy)
{
    vox_u32 z;
    int occupied = 0;
    int hard_blocker = 0;
    int stone = 0;
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        const vox_cell *cell = vox_world_cell(&match->world, x, y, z);
        if (cell == 0 || cell->material == VOX_MAT_AIR ||
            cell->material == VOX_MAT_SMOKE) {
            continue;
        }
        occupied = 1;
        if ((cell->flags & VOX_CELL_LOOSE) != 0U) {
            continue;
        }
        if (cell->material == VOX_MAT_BEDROCK ||
            cell->material == VOX_MAT_METAL) {
            hard_blocker = 1;
            break;
        }
        if (cell->material == VOX_MAT_STONE) {
            stone = 1;
        }
    }
    if (!occupied) {
        return 0;
    }
    if (hard_blocker) {
        return 2;
    }
    if (stone) {
        if (*energy <= DIGS_RAIL_STONE_COST) {
            *energy = 0U;
            return 2;
        }
        *energy = (vox_u16)(*energy - DIGS_RAIL_STONE_COST);
    } else if (*energy <= DIGS_RAIL_SOFT_COST) {
        *energy = 0U;
        return 2;
    } else {
        *energy = (vox_u16)(*energy - DIGS_RAIL_SOFT_COST);
    }
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        const vox_cell *cell = vox_world_cell(&match->world, x, y, z);
        if (cell != 0 && cell->material != VOX_MAT_AIR &&
            cell->material != VOX_MAT_BEDROCK &&
            cell->material != VOX_MAT_METAL) {
            (void)vox_world_set(&match->world, x, y, z, VOX_MAT_AIR,
                                80L << 16);
        }
    }
    return 1;
}

static vox_result digs_fire_rail(vox_digs_match *match, vox_u16 player,
                                 vox_u16 charge_ticks,
                                 vox_u32 target_x, vox_u32 target_y)
{
    vox_i32 source_x = match->players[player].position_x.value_q16;
    vox_i32 source_y = match->players[player].position_y.value_q16;
    vox_i32 delta_x = ((vox_i32)target_x << 16) + 32768L - source_x;
    vox_i32 delta_y = ((vox_i32)target_y << 16) + 32768L - source_y;
    vox_i32 distance = digs_distance_approx(delta_x, delta_y);
    vox_i32 divisor;
    vox_i32 direction_x_q8;
    vox_i32 direction_y_q8;
    vox_i32 step_x;
    vox_i32 step_y;
    vox_i32 trace_x = source_x;
    vox_i32 trace_y = source_y;
    vox_i32 last_cell_x = -1L;
    vox_i32 last_cell_y = -1L;
    vox_u16 damage;
    vox_u16 energy;
    vox_u16 hit_mask = 0U;
    vox_u16 hit_count = 0U;
    vox_u32 step;
    vox_u32 max_steps = (vox_u32)((128L << 16) /
                                   DIGS_SWEEP_STEP_Q16);
    if (charge_ticks == 0U) charge_ticks = 1U;
    if (charge_ticks > DIGS_RAIL_MAX_CHARGE_TICKS) {
        charge_ticks = DIGS_RAIL_MAX_CHARGE_TICKS;
    }
    if (distance <= 0L) {
        delta_x = match->facing_right[player] ? 65536L : -65536L;
        delta_y = 0L;
        distance = 65536L;
    }
    divisor = distance / 256L;
    if (divisor <= 0L) divisor = 1L;
    direction_x_q8 = delta_x / divisor;
    direction_y_q8 = delta_y / divisor;
    if (direction_x_q8 > 256L) direction_x_q8 = 256L;
    if (direction_x_q8 < -256L) direction_x_q8 = -256L;
    if (direction_y_q8 > 256L) direction_y_q8 = 256L;
    if (direction_y_q8 < -256L) direction_y_q8 = -256L;
    step_x = (DIGS_SWEEP_STEP_Q16 * direction_x_q8) / 256L;
    step_y = (DIGS_SWEEP_STEP_Q16 * direction_y_q8) / 256L;
    if (step_x == 0L && step_y == 0L) {
        step_x = match->facing_right[player] ? 1L : -1L;
    }
    damage = (vox_u16)(DIGS_RAIL_MIN_DAMAGE +
        ((DIGS_RAIL_MAX_DAMAGE - DIGS_RAIL_MIN_DAMAGE) *
         (vox_u32)charge_ticks) / DIGS_RAIL_MAX_CHARGE_TICKS);
    energy = (vox_u16)(60U +
        ((DIGS_RAIL_START_ENERGY - 60U) * (vox_u32)charge_ticks) /
        DIGS_RAIL_MAX_CHARGE_TICKS);
    for (step = 0U; step < max_steps && energy >= DIGS_RAIL_MIN_ENERGY;
         ++step) {
        vox_i32 x_cell;
        vox_i32 y_cell;
        int terrain;
        vox_u16 victim;
        trace_x += step_x;
        trace_y += step_y;
        x_cell = digs_q16_to_cell(trace_x);
        y_cell = digs_q16_to_cell(trace_y);
        if (x_cell < 0 || y_cell < 0 ||
            x_cell >= (vox_i32)VOX_WORLD_WIDTH ||
            y_cell >= (vox_i32)VOX_WORLD_HEIGHT) {
            break;
        }
        terrain = 0;
        if (x_cell != last_cell_x || y_cell != last_cell_y) {
            terrain = digs_rail_terrain(match, (vox_u32)x_cell,
                                        (vox_u32)y_cell, &energy);
            last_cell_x = x_cell;
            last_cell_y = y_cell;
        }
        /* Even pierced soft cover owns this sample; bodies begin behind it. */
        if (terrain == 2) {
            break;
        }
        if (terrain == 1) {
            continue;
        }
        for (victim = 0U; victim < match->rules.player_count; ++victim) {
            vox_u16 part = VOX_DIGS_NO_PART;
            vox_u16 dealt;
            vox_u16 health_before;
            if (victim == player || (hit_mask & (1U << victim)) != 0U ||
                !digs_point_hits_player(match, victim, trace_x, trace_y,
                                        &part)) {
                continue;
            }
            dealt = (vox_u16)(((vox_u32)damage * energy) /
                               DIGS_RAIL_START_ENERGY);
            if (dealt < DIGS_RAIL_MIN_DAMAGE) {
                dealt = DIGS_RAIL_MIN_DAMAGE;
            }
            health_before = match->health[victim];
            (void)vox_digs_apply_hit(match, player, victim,
                                     VOX_DIGS_TOOL_RAIL_GUN, part, dealt,
                                     VOX_DIGS_DAMAGE_BALLISTIC);
            /*
             * The public anatomy damage primitive intentionally has no
             * charge parameter: a 100-point rail-tagged limb hit may sever
             * without being lethal.  The charged weapon path owns the
             * precision-lethal rule.  Requiring an observed health change
             * preserves spawn shields and friendly-fire suppression.
             */
            if (charge_ticks == DIGS_RAIL_MAX_CHARGE_TICKS &&
                match->alive[victim] &&
                match->health[victim] < health_before) {
                (void)vox_digs_record_kill(match, player, victim);
            }
            hit_mask = (vox_u16)(hit_mask | (vox_u16)(1U << victim));
            hit_count++;
            if (energy <= DIGS_RAIL_PLAYER_COST) {
                energy = 0U;
            } else {
                energy = (vox_u16)(energy - DIGS_RAIL_PLAYER_COST);
            }
            break;
        }
    }
    match->players[player].velocity_x.value_q16 -=
        (DIGS_RAIL_RECOIL_Q16 * direction_x_q8) / 256L;
    match->players[player].velocity_y.value_q16 -=
        (DIGS_RAIL_RECOIL_Q16 * direction_y_q8) / 512L;
    digs_emit_event(match, VOX_DIGS_EVENT_RAIL_TRACE, player,
                    VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_RAIL_GUN,
                    VOX_MAT_METAL, trace_x, trace_y, charge_ticks,
                    hit_count);
    return VOX_OK;
}

static void digs_commit_weapon_fire(vox_digs_match *match, vox_u16 player,
                                    vox_u16 weapon, vox_u32 target_x,
                                    vox_u32 target_y, vox_u16 magnitude,
                                    vox_u16 variant)
{
    const vox_digs_weapon_properties *properties = &digs_weapons[weapon];
    match->selected_weapon[player] = weapon;
    match->weapon_cooldown[player] = properties->cooldown_ticks;
    match->aim_x[player] = (vox_u16)target_x;
    match->aim_y[player] = (vox_u16)target_y;
    match->facing_right[player] =
        target_x >= (vox_u32)digs_q16_to_cell(
            match->players[player].position_x.value_q16) ? 1U : 0U;
    digs_end_spawn_shield(match, player);
    digs_emit_event(match, VOX_DIGS_EVENT_WEAPON_FIRE, player,
                    VOX_DIGS_NO_PLAYER, weapon,
                    digs_projectile_material(weapon),
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    magnitude, variant);
}

static void digs_trace_direction(const vox_digs_match *match, vox_u16 player,
                                 vox_u32 target_x, vox_u32 target_y,
                                 vox_i32 *step_x_out, vox_i32 *step_y_out,
                                 vox_i32 *trace_x_out, vox_i32 *trace_y_out)
{
    vox_i32 source_x = match->players[player].position_x.value_q16;
    vox_i32 source_y = match->players[player].position_y.value_q16;
    vox_i32 delta_x = ((vox_i32)target_x << 16) + 32768L - source_x;
    vox_i32 delta_y = ((vox_i32)target_y << 16) + 32768L - source_y;
    vox_i32 distance = digs_distance_approx(delta_x, delta_y);
    vox_i32 divisor;
    vox_i32 direction_x_q8;
    vox_i32 direction_y_q8;
    if (distance <= 0L) {
        delta_x = match->facing_right[player] ? 65536L : -65536L;
        delta_y = 0L;
        distance = 65536L;
    }
    divisor = distance / 256L;
    if (divisor <= 0L) {
        divisor = 1L;
    }
    direction_x_q8 = delta_x / divisor;
    direction_y_q8 = delta_y / divisor;
    if (direction_x_q8 > 256L) direction_x_q8 = 256L;
    if (direction_x_q8 < -256L) direction_x_q8 = -256L;
    if (direction_y_q8 > 256L) direction_y_q8 = 256L;
    if (direction_y_q8 < -256L) direction_y_q8 = -256L;
    *step_x_out = (DIGS_SWEEP_STEP_Q16 * direction_x_q8) / 256L;
    *step_y_out = (DIGS_SWEEP_STEP_Q16 * direction_y_q8) / 256L;
    if (*step_x_out == 0L && *step_y_out == 0L) {
        *step_x_out = match->facing_right[player] ? 1L : -1L;
    }
    *trace_x_out = source_x;
    *trace_y_out = source_y;
}

static vox_result digs_fire_popper(vox_digs_match *match, vox_u16 player,
                                   vox_u32 target_x, vox_u32 target_y)
{
    const vox_digs_weapon_properties *properties =
        &digs_weapons[VOX_DIGS_TOOL_POPPER];
    vox_i32 step_x;
    vox_i32 step_y;
    vox_i32 trace_x;
    vox_i32 trace_y;
    vox_u32 step;
    digs_trace_direction(match, player, target_x, target_y, &step_x,
                         &step_y, &trace_x, &trace_y);
    for (step = 0U; step < 128U; ++step) {
        vox_i32 x_cell;
        vox_i32 y_cell;
        vox_u16 victim;
        trace_x += step_x;
        trace_y += step_y;
        x_cell = digs_q16_to_cell(trace_x);
        y_cell = digs_q16_to_cell(trace_y);
        if (x_cell < 0L || y_cell < 0L ||
            x_cell >= (vox_i32)VOX_WORLD_WIDTH ||
            y_cell >= (vox_i32)VOX_WORLD_HEIGHT) {
            break;
        }
        if (digs_projectile_hits_solid(&match->world, (vox_u32)x_cell,
                                       (vox_u32)y_cell)) {
            (void)vox_world_blast(&match->world, (vox_u32)x_cell,
                                  (vox_u32)y_cell, VOX_WORLD_DEPTH - 1U,
                                  properties->blast_radius, 550L << 16);
            digs_damage_radius(match, player, (vox_u32)x_cell,
                               (vox_u32)y_cell, properties->blast_radius,
                               properties->damage, VOX_DIGS_TOOL_POPPER,
                               1U);
            break;
        }
        for (victim = 0U; victim < match->rules.player_count; ++victim) {
            vox_u16 part = VOX_DIGS_NO_PART;
            if (victim == player || !match->alive[victim] ||
                !digs_point_hits_player(match, victim, trace_x, trace_y,
                                        &part)) {
                continue;
            }
            (void)vox_digs_apply_hit(match, player, victim,
                                     VOX_DIGS_TOOL_POPPER, part,
                                     properties->damage,
                                     VOX_DIGS_DAMAGE_BALLISTIC);
            (void)vox_world_blast(&match->world, (vox_u32)x_cell,
                                  (vox_u32)y_cell, VOX_WORLD_DEPTH - 1U,
                                  1U, 550L << 16);
            return VOX_OK;
        }
    }
    return VOX_OK;
}

static vox_result digs_fire_hot_rail(vox_digs_match *match, vox_u16 player,
                                     vox_u32 target_x, vox_u32 target_y)
{
    const vox_digs_weapon_properties *properties =
        &digs_weapons[VOX_DIGS_TOOL_HOT_RAIL];
    vox_i32 step_x;
    vox_i32 step_y;
    vox_i32 trace_x;
    vox_i32 trace_y;
    vox_u32 step;
    digs_trace_direction(match, player, target_x, target_y, &step_x,
                         &step_y, &trace_x, &trace_y);
    for (step = 0U; step < DIGS_HOT_RAIL_RANGE_CELLS * 8U; ++step) {
        vox_i32 x_cell;
        vox_i32 y_cell;
        vox_u16 victim;
        trace_x += step_x;
        trace_y += step_y;
        x_cell = digs_q16_to_cell(trace_x);
        y_cell = digs_q16_to_cell(trace_y);
        if (x_cell < 0L || y_cell < 0L ||
            x_cell >= (vox_i32)VOX_WORLD_WIDTH ||
            y_cell >= (vox_i32)VOX_WORLD_HEIGHT) {
            break;
        }
        if (digs_projectile_hits_solid(&match->world, (vox_u32)x_cell,
                                       (vox_u32)y_cell)) {
            vox_u32 z;
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(&match->world,
                    (vox_u32)x_cell, (vox_u32)y_cell, z);
                if (cell == 0 || cell->material == VOX_MAT_AIR ||
                    cell->material == VOX_MAT_BEDROCK) {
                    continue;
                }
                /*
                 * Heat the bore, do not liquefy it.
                 *
                 * This used to convert coal and biomass straight to lava.
                 * Tunnelling down through a seam therefore turned the
                 * miner's own floor molten, they fell into the pool they
                 * had just made, and lava contact killed them at twelve
                 * health per tick -- attributed, confusingly, to the hot
                 * rail itself.  That is the reported "dying in the tunnel
                 * being made by the miner while making the tunnel".
                 *
                 * Coal and biomass are flammable, so heating them past
                 * ignition still sets the tunnel alight through the normal
                 * material reactions.  The tool keeps its scorched-earth
                 * character and its tunnels stay walkable.
                 */
                (void)vox_world_set(&match->world, (vox_u32)x_cell,
                                    (vox_u32)y_cell, z,
                                    cell->material, 850L << 16);
            }
            if ((step & 7U) == 0U) {
                (void)vox_world_blast(&match->world, (vox_u32)x_cell,
                                      (vox_u32)y_cell,
                                      VOX_WORLD_DEPTH - 1U, 1U,
                                      800L << 16);
            }
        }
        for (victim = 0U; victim < match->rules.player_count; ++victim) {
            vox_u16 part = VOX_DIGS_NO_PART;
            if (victim == player || !match->alive[victim] ||
                !digs_point_hits_player(match, victim, trace_x, trace_y,
                                        &part)) {
                continue;
            }
            (void)vox_digs_apply_hit(match, player, victim,
                                     VOX_DIGS_TOOL_HOT_RAIL, part,
                                     properties->damage,
                                     VOX_DIGS_DAMAGE_HEAT);
            return VOX_OK;
        }
    }
    digs_spawn_effect_variant(match, VOX_MAT_LAVA, trace_x, trace_y,
                              0L, -2048L, 8U, player,
                              (vox_u16)(match->tick & 15U));
    return VOX_OK;
}

static vox_result digs_fire_bore_drill(vox_digs_match *match,
                                       vox_u16 player)
{
    const vox_digs_weapon_properties *properties =
        &digs_weapons[VOX_DIGS_TOOL_BORE_DRILL];
    vox_i32 source_x = digs_q16_to_cell(
        match->players[player].position_x.value_q16);
    vox_i32 source_y = digs_q16_to_cell(
        match->players[player].position_y.value_q16);
    vox_u16 step;
    for (step = 1U; step <= DIGS_SCALE(12U); ++step) {
        vox_i32 y_cell = source_y + (vox_i32)step;
        vox_u16 victim;
        if (source_x < 0L || y_cell < 0L ||
            source_x >= (vox_i32)VOX_WORLD_WIDTH ||
            y_cell >= (vox_i32)VOX_WORLD_HEIGHT) {
            break;
        }
        if (digs_projectile_hits_solid(&match->world, (vox_u32)source_x,
                                       (vox_u32)y_cell)) {
            (void)vox_world_blast(&match->world, (vox_u32)source_x,
                                  (vox_u32)y_cell, VOX_WORLD_DEPTH - 1U,
                                  1U, 500L << 16);
        }
        if (match->players[player].velocity_y.value_q16 <= 0L) {
            continue;
        }
        for (victim = 0U; victim < match->rules.player_count; ++victim) {
            vox_u16 part = VOX_DIGS_NO_PART;
            if (victim == player || !match->alive[victim] ||
                !digs_point_hits_player(match, victim,
                    (source_x << 16) + 32768L,
                    (y_cell << 16) + 32768L, &part)) {
                continue;
            }
            (void)vox_digs_apply_hit(match, player, victim,
                                     VOX_DIGS_TOOL_BORE_DRILL, part,
                                     properties->damage,
                                     VOX_DIGS_DAMAGE_BLUNT);
            return VOX_OK;
        }
    }
    return VOX_OK;
}

static vox_result digs_fire_bolt_action(vox_digs_match *match,
                                        vox_u16 player, vox_u32 target_x,
                                        vox_u32 target_y)
{
    const vox_digs_weapon_properties *properties =
        &digs_weapons[VOX_DIGS_TOOL_BOLT_ACTION];
    vox_i32 step_x;
    vox_i32 step_y;
    vox_i32 trace_x;
    vox_i32 trace_y;
    vox_u16 bores = 0U;
    vox_u32 step;
    digs_trace_direction(match, player, target_x, target_y, &step_x,
                         &step_y, &trace_x, &trace_y);
    for (step = 0U; step < 224U; ++step) {
        vox_i32 x_cell;
        vox_i32 y_cell;
        vox_u16 victim;
        trace_x += step_x;
        trace_y += step_y;
        x_cell = digs_q16_to_cell(trace_x);
        y_cell = digs_q16_to_cell(trace_y);
        if (x_cell < 0L || y_cell < 0L ||
            x_cell >= (vox_i32)VOX_WORLD_WIDTH ||
            y_cell >= (vox_i32)VOX_WORLD_HEIGHT) {
            break;
        }
        if (digs_projectile_hits_solid(&match->world, (vox_u32)x_cell,
                                       (vox_u32)y_cell)) {
            (void)vox_world_blast(&match->world, (vox_u32)x_cell,
                                  (vox_u32)y_cell, VOX_WORLD_DEPTH - 1U,
                                  1U, 900L << 16);
            bores++;
            if (bores >= 6U) {
                break;
            }
            continue;
        }
        for (victim = 0U; victim < match->rules.player_count; ++victim) {
            vox_u16 part = VOX_DIGS_NO_PART;
            if (victim == player || !match->alive[victim] ||
                !digs_point_hits_player(match, victim, trace_x, trace_y,
                                        &part)) {
                continue;
            }
            (void)vox_digs_apply_hit(match, player, victim,
                                     VOX_DIGS_TOOL_BOLT_ACTION, part,
                                     properties->damage,
                                     VOX_DIGS_DAMAGE_BALLISTIC);
            return VOX_OK;
        }
    }
    return VOX_OK;
}

vox_result vox_digs_fire_weapon(vox_digs_match *match, vox_u16 player,
                                vox_u16 weapon, vox_u32 target_x,
                                vox_u32 target_y)
{
    const vox_digs_weapon_properties *properties;
    vox_result result;
    vox_u16 spawned;
    int offset;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_active(match, player) ||
        !match->alive[player] || weapon >= VOX_DIGS_TOOL_COUNT ||
        target_x >= VOX_WORLD_WIDTH || target_y >= VOX_WORLD_HEIGHT ||
        match->weapon_cooldown[player] != 0U ||
        (match->rules.weapon_mask & (vox_u16)(1U << weapon)) == 0U) {
        return VOX_ERR_INVALID;
    }
    properties = &digs_weapons[weapon];
    result = VOX_OK;
    if (weapon == VOX_DIGS_TOOL_RAIL_GUN) {
        result = digs_fire_rail(match, player,
                                DIGS_RAIL_MAX_CHARGE_TICKS,
                                target_x, target_y);
    } else if (weapon == VOX_DIGS_TOOL_POPPER) {
        result = digs_fire_popper(match, player, target_x, target_y);
    } else if (weapon == VOX_DIGS_TOOL_HOT_RAIL) {
        result = digs_fire_hot_rail(match, player, target_x, target_y);
    } else if (weapon == VOX_DIGS_TOOL_BORE_DRILL) {
        result = digs_fire_bore_drill(match, player);
    } else if (weapon == VOX_DIGS_TOOL_BOLT_ACTION) {
        result = digs_fire_bolt_action(match, player, target_x, target_y);
    } else if (properties->flags & VOX_DIGS_WEAPON_MELEE) {
        result = digs_fire_melee(match, player, weapon, target_x, target_y);
    } else if (weapon == VOX_DIGS_TOOL_SCATTERBRAIN) {
        spawned = 0U;
        for (offset = -4; offset <= 4; ++offset) {
            long pellet_y = (long)target_y + (long)offset;
            if (pellet_y < 0L) {
                pellet_y = 0L;
            }
            if (pellet_y >= (long)VOX_WORLD_HEIGHT) {
                pellet_y = (long)VOX_WORLD_HEIGHT - 1L;
            }
            if (digs_spawn_projectile(match, player, weapon, target_x,
                                      (vox_u32)pellet_y, 256U) == VOX_OK) {
                spawned++;
            }
        }
        result = spawned == 0U ? VOX_ERR_CAPACITY : VOX_OK;
    } else {
        result = digs_spawn_projectile(match, player, weapon,
                                       target_x, target_y, 256U);
    }
    if (result != VOX_OK) {
        return result;
    }
    digs_commit_weapon_fire(match, player, weapon, target_x, target_y,
                            properties->damage,
                            (vox_u16)(digs_noise(match->rules.seed,
                                                 match->tick, player,
                                                 (vox_u32)weapon * 19U) &
                                      31U));
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

static void digs_release_rail(vox_digs_match *match, vox_u16 player)
{
    vox_u16 charge = match->rail_charge_ticks[player];
    vox_u32 target_x = match->aim_x[player];
    vox_u32 target_y = match->aim_y[player];
    if (charge == 0U || match->weapon_cooldown[player] != 0U ||
        (match->rules.weapon_mask &
         (vox_u16)(1U << VOX_DIGS_TOOL_RAIL_GUN)) == 0U) {
        match->rail_charge_ticks[player] = 0U;
        match->rail_charging[player] = 0U;
        return;
    }
    if (digs_fire_rail(match, player, charge, target_x, target_y) == VOX_OK) {
        match->weapon_cooldown[player] = DIGS_RAIL_COOLDOWN_TICKS;
        match->selected_weapon[player] = VOX_DIGS_TOOL_RAIL_GUN;
        match->facing_right[player] =
            target_x >= (vox_u32)digs_q16_to_cell(
                match->players[player].position_x.value_q16) ? 1U : 0U;
        digs_end_spawn_shield(match, player);
        digs_emit_event(match, VOX_DIGS_EVENT_WEAPON_FIRE, player,
                        VOX_DIGS_NO_PLAYER, VOX_DIGS_TOOL_RAIL_GUN,
                        VOX_MAT_METAL,
                        match->players[player].position_x.value_q16,
                        match->players[player].position_y.value_q16,
                        (vox_u16)(DIGS_RAIL_MIN_DAMAGE +
                            ((DIGS_RAIL_MAX_DAMAGE -
                              DIGS_RAIL_MIN_DAMAGE) *
                             (vox_u32)charge) /
                            DIGS_RAIL_MAX_CHARGE_TICKS),
                        charge);
    }
    match->rail_charge_ticks[player] = 0U;
    match->rail_charging[player] = 0U;
}

static void digs_release_charged_weapon(vox_digs_match *match,
                                        vox_u16 player)
{
    vox_u16 weapon = match->selected_weapon[player];
    vox_u16 charge = match->weapon_charge_ticks[player];
    vox_u32 target_x = match->aim_x[player];
    vox_u32 target_y = match->aim_y[player];
    vox_result result = VOX_ERR_INVALID;
    if (weapon == VOX_DIGS_TOOL_PULASKI) {
        if (charge >= DIGS_PULASKI_CHARGE_TICKS &&
            match->weapon_cooldown[player] == 0U) {
            result = digs_spawn_projectile(match, player, weapon, target_x,
                                           target_y, 256U);
        }
    } else if (weapon == VOX_DIGS_TOOL_BOLT_ACTION) {
        if (charge >= DIGS_BOLT_CHARGE_TICKS &&
            match->weapon_cooldown[player] == 0U) {
            result = digs_fire_bolt_action(match, player, target_x, target_y);
        }
    } else if (weapon == VOX_DIGS_TOOL_SMOKER) {
        vox_u16 scale_q8 = (vox_u16)(128U +
            ((vox_u32)charge * 256U) / DIGS_SMOKER_CHARGE_TICKS);
        if (scale_q8 > 384U) {
            scale_q8 = 384U;
        }
        if (match->weapon_cooldown[player] == 0U) {
            result = digs_spawn_projectile(match, player, weapon, target_x,
                                           target_y, scale_q8);
        }
    } else if (weapon == VOX_DIGS_TOOL_FIRECRACKER) {
        vox_u16 scale_q8 = (vox_u16)(128U +
            ((vox_u32)charge * 256U) / DIGS_FIRECRACKER_CHARGE_TICKS);
        if (scale_q8 > 384U) {
            scale_q8 = 384U;
        }
        if (match->weapon_cooldown[player] == 0U) {
            result = digs_spawn_projectile(match, player, weapon, target_x,
                                           target_y, scale_q8);
        }
    }
    if (result == VOX_OK) {
        const vox_digs_weapon_properties *properties = &digs_weapons[weapon];
        digs_commit_weapon_fire(match, player, weapon, target_x, target_y,
                                properties->damage, charge);
        if (weapon == VOX_DIGS_TOOL_BOLT_ACTION) {
            match->bolt_shot_streak[player]++;
            if (match->bolt_shot_streak[player] >= DIGS_BOLT_STREAK_LIMIT) {
                match->bolt_shot_streak[player] = 0U;
                match->weapon_cooldown[player] = DIGS_BOLT_OVERHEAT_TICKS;
            }
        }
    }
    match->weapon_charge_ticks[player] = 0U;
    match->weapon_charging[player] = 0U;
}

static void digs_step_weapon_input(vox_digs_match *match, vox_u16 player)
{
    vox_u16 actions = match->player_actions[player];
    vox_u16 weapon = match->selected_weapon[player];
    if (!match->alive[player] || weapon >= VOX_DIGS_TOOL_COUNT) {
        return;
    }
    if (weapon == VOX_DIGS_TOOL_RAIL_GUN) {
        if ((match->rules.weapon_mask &
             (vox_u16)(1U << VOX_DIGS_TOOL_RAIL_GUN)) == 0U) {
            match->rail_charge_ticks[player] = 0U;
            match->rail_charging[player] = 0U;
            return;
        }
        if ((actions & VOX_DIGS_ACTION_FIRE) != 0U &&
            match->weapon_cooldown[player] == 0U) {
            if (!match->rail_charging[player]) {
                match->rail_charging[player] = 1U;
                match->rail_charge_ticks[player] = 0U;
                digs_end_spawn_shield(match, player);
                digs_emit_event(match, VOX_DIGS_EVENT_RAIL_CHARGE, player,
                                VOX_DIGS_NO_PLAYER,
                                VOX_DIGS_TOOL_RAIL_GUN, VOX_MAT_METAL,
                                match->players[player].position_x.value_q16,
                                match->players[player].position_y.value_q16,
                                0U, 0U);
            }
            if (match->rail_charge_ticks[player] <
                DIGS_RAIL_MAX_CHARGE_TICKS) {
                match->rail_charge_ticks[player]++;
            }
        } else if (match->rail_charging[player]) {
            digs_release_rail(match, player);
        }
        return;
    }
    if (match->rail_charging[player]) {
        match->rail_charge_ticks[player] = 0U;
        match->rail_charging[player] = 0U;
    }
    if (weapon == VOX_DIGS_TOOL_PULASKI ||
        weapon == VOX_DIGS_TOOL_BOLT_ACTION ||
        weapon == VOX_DIGS_TOOL_FIRECRACKER ||
        weapon == VOX_DIGS_TOOL_SMOKER) {
        if ((actions & VOX_DIGS_ACTION_FIRE) != 0U) {
            if (!match->weapon_charging[player]) {
                match->weapon_charging[player] = 1U;
                match->weapon_charge_ticks[player] = 0U;
                if (weapon == VOX_DIGS_TOOL_PULASKI &&
                    match->weapon_cooldown[player] == 0U) {
                    (void)vox_digs_fire_weapon(match, player, weapon,
                                               match->aim_x[player],
                                               match->aim_y[player]);
                }
                digs_end_spawn_shield(match, player);
            }
            if (match->weapon_charge_ticks[player] <
                digs_weapons[weapon].charge_ticks) {
                match->weapon_charge_ticks[player]++;
            }
        } else if (match->weapon_charging[player]) {
            digs_release_charged_weapon(match, player);
        }
        return;
    }
    if (match->weapon_charging[player]) {
        match->weapon_charge_ticks[player] = 0U;
        match->weapon_charging[player] = 0U;
    }
    if ((actions & VOX_DIGS_ACTION_FIRE) != 0U &&
        match->weapon_cooldown[player] == 0U) {
        (void)vox_digs_fire_weapon(match, player, weapon,
                                   match->aim_x[player],
                                   match->aim_y[player]);
    }
}

static int digs_ai_is_enemy(const vox_digs_match *match, vox_u16 player,
                            vox_u16 candidate)
{
    if (candidate == player || !match->alive[candidate]) {
        return 0;
    }
    return 1;
}


/*
 * How appealing a target is, as a multiplier on the distance to them.
 *
 * Below 100 they feel closer than they are and get picked over someone
 * nearer; above 100 they feel further away.  This is the whole mechanism
 * behind "a bunch of aggressive back and forths will create a hostile
 * environment": who a miner shoots at stops being a question of geometry as
 * soon as there is any history.
 *
 * Scaled by grudge, so RIVET -- who remembers -- swings much further on the
 * same history than CINDER, who does not.
 */
static vox_u32 digs_ai_contract_pull(const vox_digs_match *match,
                                     vox_u16 player, vox_u16 other,
                                     vox_u16 grudge)
{
    static const vox_i32 pull[VOX_DIGS_TONE_COUNT] = {
        -60,    /* FEUD     -- hunt them past anyone closer */
        -30,    /* HOSTILE  */
        -10,    /* NEEDLING */
        0,      /* NEUTRAL  */
        20,     /* WARY     */
        60,     /* THAWING  */
        150,    /* TRUCE    */
        220     /* BONDED   */
    };
    const vox_digs_contract *contract = vox_digs_contract_get(match, player,
                                                              other);
    vox_i32 bias;
    if (contract == 0 || contract->tone >= VOX_DIGS_TONE_COUNT) {
        return 100U;
    }
    bias = (pull[contract->tone] * (vox_i32)grudge) / 255L;
    return (vox_u32)(100L + bias);
}

/* Nobody shoots at somebody they have an arrangement with -- if there is
 * anyone else to shoot at. */
static int digs_ai_under_truce(const vox_digs_match *match, vox_u16 player,
                               vox_u16 other)
{
    const vox_digs_contract *contract = vox_digs_contract_get(match, player,
                                                              other);
    return contract != 0 && (contract->tone == VOX_DIGS_TONE_TRUCE ||
                             contract->tone == VOX_DIGS_TONE_BONDED);
}

static int digs_ai_line_of_sight(const vox_world *world,
                                 vox_i32 from_x, vox_i32 from_y,
                                 vox_i32 to_x, vox_i32 to_y)
{
    vox_i32 delta_x = to_x - from_x;
    vox_i32 delta_y = to_y - from_y;
    vox_u32 steps = digs_abs_i32(delta_x);
    vox_u32 step;
    if (digs_abs_i32(delta_y) > steps) {
        steps = digs_abs_i32(delta_y);
    }
    if (steps == 0U) {
        return 1;
    }
    for (step = 1U; step < steps; ++step) {
        vox_i32 x = from_x + digs_div_trunc_positive(
            delta_x * (vox_i32)step, steps);
        vox_i32 y = from_y + digs_div_trunc_positive(
            delta_y * (vox_i32)step, steps);
        if (x < 0 || y < 0 || x >= (vox_i32)VOX_WORLD_WIDTH ||
            y >= (vox_i32)VOX_WORLD_HEIGHT ||
            digs_cell_is_solid(world, (vox_u32)x, (vox_u32)y)) {
            return 0;
        }
    }
    return 1;
}

/*
 * Where the loudest recent explosion was, in cells, or -1 for silence.
 * Unlike hearing a target this does not need line of sight or a live enemy --
 * it is the sound of trouble, and it is what gives roaming a direction.
 */
static vox_i32 digs_ai_commotion_x(const vox_digs_match *match,
                                   vox_u16 player, vox_i32 bot_x,
                                   vox_i32 bot_y)
{
    vox_u16 slot;
    vox_u32 newest_sequence = 0U;
    vox_i32 found = -1L;
    for (slot = 0U; slot < VOX_DIGS_MAX_EVENTS; ++slot) {
        const vox_digs_event *event = &match->events[slot];
        vox_i32 event_x;
        vox_i32 event_y;
        if (event->sequence == 0U || event->sequence <= newest_sequence ||
            event->tick > match->tick ||
            match->tick - event->tick > DIGS_AI_HEARING_TICKS) {
            continue;
        }
        if (event->type != VOX_DIGS_EVENT_EXPLOSION) {
            continue;
        }
        if (event->source == player) {
            continue;
        }
        event_x = digs_q16_to_cell(event->position_x_q16);
        event_y = digs_q16_to_cell(event->position_y_q16);
        if (digs_abs_i32(event_x - bot_x) + digs_abs_i32(event_y - bot_y) <=
            DIGS_AI_COMMOTION_CELLS) {
            newest_sequence = event->sequence;
            found = event_x;
        }
    }
    return found;
}

static vox_u16 digs_ai_heard_target(const vox_digs_match *match,
                                    vox_u16 player, vox_i32 bot_x,
                                    vox_i32 bot_y)
{
    vox_u16 slot;
    vox_u16 heard = VOX_DIGS_NO_PLAYER;
    vox_u32 newest_sequence = 0U;
    /*
     * Presentation is allowed to consume the public event queue.  Retained
     * slots still contain the last bounded event history, so authoritative
     * AI scans by sequence and age rather than by the queue's consumer-owned
     * head/count window.  This keeps bot decisions invariant under render
     * cadence and event-drain cadence.
     */
    for (slot = 0U; slot < VOX_DIGS_MAX_EVENTS; ++slot) {
        const vox_digs_event *event = &match->events[slot];
        vox_i32 event_x;
        vox_i32 event_y;
        if (event->sequence == 0U || event->sequence <= newest_sequence ||
            event->tick > match->tick ||
            match->tick - event->tick > DIGS_AI_HEARING_TICKS) {
            continue;
        }
        if ((event->type == VOX_DIGS_EVENT_WEAPON_FIRE ||
             event->type == VOX_DIGS_EVENT_EXPLOSION) &&
            event->source != VOX_DIGS_NO_PLAYER &&
            vox_digs_player_is_active(match, event->source) &&
            digs_ai_is_enemy(match, player, event->source)) {
            event_x = digs_q16_to_cell(event->position_x_q16);
            event_y = digs_q16_to_cell(event->position_y_q16);
            if (digs_abs_i32(event_x - bot_x) +
                digs_abs_i32(event_y - bot_y) <= 52U) {
                newest_sequence = event->sequence;
                heard = event->source;
            }
        }
    }
    return heard;
}

static void digs_ai_set_mode(vox_digs_match *match, vox_u16 player,
                             vox_u16 mode)
{
    vox_digs_ai_state *state = &match->bots[player];
    if (state->mode == mode) {
        return;
    }
    state->mode = mode;
    state->state_ticks = 0U;
    digs_emit_event(match, VOX_DIGS_EVENT_AI_STATE, player,
                    state->target, VOX_DIGS_TOOL_PICK, VOX_MAT_METAL,
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    mode, (vox_u16)(digs_noise(match->rules.seed,
                                               match->tick, player,
                                               mode) & 15U));
}

/*
 * Effective engagement band per tool, in cells.  These are the ranges each
 * weapon is actually good at, read off its own properties: the hot rail
 * traces 14 cells, the hammer reaches 10, the rail gun penetrates far enough
 * to snipe, the lobbed explosives need room for their arc and their blast.
 */
typedef struct digs_weapon_band {
    vox_u16 ideal_min;
    vox_u16 ideal_max;
} digs_weapon_band;

static const digs_weapon_band digs_weapon_bands[VOX_DIGS_TOOL_COUNT] = {
    {0U, 10U},    /* PULASKI      melee tap, or a thrown arc */
    {4U, 40U},    /* POPPER       cheap hitscan chip damage */
    {8U, 30U},    /* SMOKER       lobbed, wants room for the arc */
    {0U, 14U},    /* HOT RAIL     DIGS_HOT_RAIL_RANGE_CELLS */
    {2U, 18U},    /* HYDROSHOT    utility, no damage */
    {0U, 10U},    /* GIANT HAMMER reach is DIGS_SCALE(5) */
    {10U, 60U},   /* BOLT ACTION  charged precision shot */
    {4U, 24U},    /* SCATTERBRAIN nine pellets, spreads with range */
    {10U, 40U},   /* FIRECRACKER  blast radius 8, needs standoff */
    /*
     * The bore drill only damages while the wielder is falling onto its
     * target, which the AI has no way to arrange, so it is weighted low
     * everywhere and kept here as a digging tool rather than a duel option.
     */
    {0U, 8U},     /* BORING DRILL straight down, point blank */
    {20U, 120U}   /* RAIL GUN     penetrating, the sniping tool */
};

/*
 * How much each archetype likes each tool, 0 meaning "never by choice".
 * This is the main thing that makes the three read as different opponents,
 * so it is deliberately lopsided rather than balanced.
 */
static const vox_u8
digs_archetype_weapon_weight[VOX_DIGS_ARCHETYPE_COUNT][VOX_DIGS_TOOL_COUNT] = {
    /* RIVET -- the engineer: reach, penetration, and tunnels. */
    { 40U,  90U,  30U, 170U,  20U,  35U, 200U,  60U,  45U,  60U, 255U},
    /* CINDER -- the berserker: get close, hit hard. */
    {200U,  70U,  25U,  60U,  15U, 255U, 110U, 230U,  55U,  40U,  50U},
    /* FLAMEY -- the trickster: fire, smoke, and mischief. */
    { 70U, 140U, 230U, 190U,  60U,  40U,  55U,  90U, 255U,  30U,  45U}
};

static const vox_digs_personality
digs_archetype_personality[VOX_DIGS_ARCHETYPE_COUNT] = {
    /*        aggr  patience  caution  grudge  social  reserved */
    {  90U,     210U,    190U,   220U,     70U, 0U},  /* RIVET   */
    { 245U,      40U,     45U,   120U,    200U, 0U},  /* CINDER  */
    { 150U,     110U,    120U,    80U,    235U, 0U}   /* FLAMEY  */
};

static const char *digs_archetype_names[VOX_DIGS_ARCHETYPE_COUNT] = {
    "RIVET", "CINDER", "FLAMEY"
};

const vox_digs_personality *vox_digs_personality_get(vox_u16 archetype)
{
    if (archetype >= VOX_DIGS_ARCHETYPE_COUNT) {
        return 0;
    }
    return &digs_archetype_personality[archetype];
}

const char *vox_digs_archetype_name(vox_u16 archetype)
{
    if (archetype >= VOX_DIGS_ARCHETYPE_COUNT) {
        return 0;
    }
    return digs_archetype_names[archetype];
}

/*
 * A bot's identity is its ordinal among the bots, so the first bot in a match
 * is always RIVET whichever slot it occupies.  bot_mask is already hashed, so
 * this needs no extra authoritative state.
 */
vox_u16 vox_digs_bot_archetype(const vox_digs_match *match, vox_u16 player)
{
    vox_u16 below;
    if (match == 0 || !vox_digs_player_is_bot(match, player)) {
        return VOX_DIGS_ARCHETYPE_COUNT;
    }
    below = (vox_u16)(match->rules.bot_mask &
                      (vox_u16)((1U << player) - 1U));
    return (vox_u16)(digs_count_bits(below) % VOX_DIGS_ARCHETYPE_COUNT);
}

/*
 * How much trouble is this miner standing in, and which way is out?
 *
 * Bots read no hazards at all before this: vox_digs_bot_think never looked at
 * lava_surface_y, never sampled a material, and never checked how far it was
 * to the floor.  They walked into lava and died there, and those deaths were
 * the largest single cause of bots appearing to kill themselves -- every one
 * of the twenty-three unattributed deaths in a four-match sample carried the
 * lava damage tag, with no crush events at all.
 *
 * Returns 0 clear, 1 wary, 2 get out now, and writes a horizontal escape
 * direction.  Caution scales how early a miner starts worrying, so RIVET backs
 * off long before CINDER does.  Everything read here is either a scalar on the
 * match or a bounded cell probe, so the cost does not scale with the world.
 */
/*
 * How much appetite each archetype has for digging its way through rather
 * than going around.  RIVET tunnels by nature, CINDER smashes whatever is
 * directly in his way, FLAMEY would rather go around and leave something
 * unpleasant behind.
 */
static const vox_u16
digs_archetype_dig_appetite[VOX_DIGS_ARCHETYPE_COUNT] = {230U, 150U, 90U};

/* Ticks pinned against terrain before this miner reaches for a tool. */
static vox_u16 digs_ai_stuck_threshold(vox_u16 archetype)
{
    vox_u32 appetite = archetype < VOX_DIGS_ARCHETYPE_COUNT ?
                       digs_archetype_dig_appetite[archetype] : 128U;
    return (vox_u16)(DIGS_AI_STUCK_MIN_TICKS +
                     ((255U - appetite) * DIGS_AI_STUCK_SPAN_TICKS) / 255U);
}

/*
 * Can this tool open a horizontal hole without hurting the miner holding it?
 *
 * Breaching fires at rock two or three cells away, so blast radius matters
 * more than damage.  The FIRECRACKER (radius 8, thrown on a fifty-tick fuse)
 * and the GIANT HAMMER (radius 7 on a melee swing) both engulf the digger,
 * and including them cost measurable self-inflicted damage in the soak.
 * The RAILSHOT is excluded for the opposite reason -- it is too good at
 * digging.  Soil costs it DIGS_RAIL_SOFT_COST (5) of a 180-energy shot, so
 * one trigger pull bores some thirty-six cells: far past what cohesion holds
 * up, and the miner walks into the tunnel it just undermined.  What is left
 * cuts terrain at arm's length: the HOT RAIL that exists to tunnel, plus the
 * POPPER and the PULASKI at radius 2.
 *
 * The bore drill is excluded despite being the obvious digging tool:
 * digs_fire_bore_drill only ever cuts straight down, so a bot facing a wall
 * would fire it into the floor forever.
 *
 * Every archetype keeps at least one of these -- RIVET the rails, CINDER the
 * pulaski, FLAMEY the hot rail -- so nobody is left unable to dig.
 */
static int digs_ai_tool_breaches(vox_u16 weapon)
{
    return weapon == VOX_DIGS_TOOL_HOT_RAIL ||
           weapon == VOX_DIGS_TOOL_POPPER ||
           weapon == VOX_DIGS_TOOL_PULASKI;
}

/*
 * Best breaching tool this arsenal allows, weighted by archetype taste so the
 * three dig with the tools that suit them.  VOX_DIGS_TOOL_COUNT if none.
 */
static vox_u16 digs_ai_breach_tool(const vox_digs_match *match,
                                   vox_u16 archetype)
{
    vox_u16 best = VOX_DIGS_TOOL_COUNT;
    vox_u16 best_weight = 0U;
    vox_u16 choice;
    if (archetype >= VOX_DIGS_ARCHETYPE_COUNT) {
        archetype = VOX_DIGS_ARCHETYPE_ENGINEER;
    }
    for (choice = 0U; choice < VOX_DIGS_TOOL_COUNT; ++choice) {
        vox_u16 weight;
        if ((match->rules.weapon_mask & (vox_u16)(1U << choice)) == 0U ||
            !digs_ai_tool_breaches(choice)) {
            continue;
        }
        weight = digs_archetype_weapon_weight[archetype][choice];
        if (best == VOX_DIGS_TOOL_COUNT || weight > best_weight) {
            best = choice;
            best_weight = weight;
        }
    }
    return best;
}

/*
 * Should a bot hold FIRE this tick for the weapon it has selected?
 *
 * Charge weapons fire on *release*, so a miner that holds the trigger down
 * forever -- which is exactly what boring through rock wants to do -- charges
 * to maximum and never actually shoots.  Dropping FIRE for the one tick after
 * the charge tops out is what pulls the trigger.
 */
static int digs_ai_hold_fire(const vox_digs_match *match, vox_u16 player)
{
    vox_u16 weapon = match->selected_weapon[player];
    if (weapon >= VOX_DIGS_TOOL_COUNT) {
        return 0;
    }
    if (weapon == VOX_DIGS_TOOL_RAIL_GUN) {
        return !match->rail_charging[player] ||
               match->rail_charge_ticks[player] < DIGS_RAIL_MAX_CHARGE_TICKS;
    }
    if (weapon == VOX_DIGS_TOOL_PULASKI ||
        weapon == VOX_DIGS_TOOL_BOLT_ACTION ||
        weapon == VOX_DIGS_TOOL_FIRECRACKER ||
        weapon == VOX_DIGS_TOOL_SMOKER) {
        return !match->weapon_charging[player] ||
               match->weapon_charge_ticks[player] <
               digs_weapons[weapon].charge_ticks;
    }
    return 1;   /* uncharged tools fire on the press */
}

/*
 * Find the wall ahead at body height: the distance in cells to the first solid
 * column within reach, with *thickness set to how many solid columns follow
 * it.  Returns 0 when nothing solid is in reach, meaning there is no wall to
 * breach and the miner should simply keep walking.
 *
 * Reporting the face separately from the thickness is what lets the shot be
 * aimed past the far side.  Measuring thickness alone found "open one cell
 * ahead" and aimed there, so a miner standing in the mouth of its own hole
 * fired into the air it had already cleared.
 *
 * Bounded by DIGS_AI_BREACH_MAX_CELLS columns times DIGS_AI_BREACH_PROBE_ROWS
 * rows, so the cost is a named constant and not a function of world size.
 */
static vox_u16 digs_ai_wall_ahead(const vox_digs_match *match,
                                  vox_i32 bot_x, vox_i32 bot_y,
                                  vox_i16 direction, vox_u16 *thickness)
{
    vox_i32 step = direction > 0 ? 1L : -1L;
    vox_u16 depth;
    vox_u16 face = 0U;
    *thickness = (vox_u16)(DIGS_AI_BREACH_MAX_CELLS + 1U);
    for (depth = 1U; depth <= DIGS_AI_BREACH_MAX_CELLS; ++depth) {
        vox_i32 sample_x = bot_x + (vox_i32)depth * step;
        vox_u16 row;
        int blocked = 0;
        if (sample_x < 0L || sample_x >= (vox_i32)VOX_WORLD_WIDTH) {
            return 0U;
        }
        for (row = 0U; row < DIGS_AI_BREACH_PROBE_ROWS; ++row) {
            vox_i32 sample_y = bot_y - (vox_i32)row;
            if (sample_y < 0L || sample_y >= (vox_i32)VOX_WORLD_HEIGHT) {
                continue;
            }
            if (digs_cell_is_solid(&match->world, (vox_u32)sample_x,
                                   (vox_u32)sample_y)) {
                blocked = 1;
            }
        }
        if (blocked) {
            if (face == 0U) {
                face = depth;
            }
        } else if (face != 0U) {
            *thickness = (vox_u16)(depth - face);
            return face;
        }
    }
    return face;    /* solid to the probe limit: thickness stays over budget */
}

/*
 * Health at or below which this miner disengages.  Aggression scales it, so
 * the same code makes CINDER reckless and RIVET careful.
 */
static vox_u16 digs_ai_retreat_threshold(const vox_digs_personality *personality)
{
    vox_u32 aggression = personality != 0 ? personality->aggression : 128U;
    return (vox_u16)(DIGS_AI_RETREAT_HEALTH_MIN +
                     ((255U - aggression) * DIGS_AI_RETREAT_HEALTH_SPAN) /
                     255U);
}

static vox_u16 digs_ai_min_dwell(vox_u16 mode)
{
    if (mode == VOX_DIGS_AI_RETREATING) return DIGS_AI_DWELL_RETREATING;
    if (mode == VOX_DIGS_AI_ATTACKING) return DIGS_AI_DWELL_ATTACKING;
    if (mode == VOX_DIGS_AI_SEARCHING) return DIGS_AI_DWELL_SEARCHING;
    return DIGS_AI_DWELL_ROAMING;
}

/*
 * How alert a mode is.  Becoming MORE alert is always urgent -- a miner that
 * hears gunfire or sees an enemy must react on the spot -- so only becoming
 * LESS alert waits on a dwell timer.  That is the direction where flicker
 * looks wrong: losing sight for a moment should not reset the hunt, and
 * giving up should look like a decision rather than a twitch.
 */
static vox_u16 digs_ai_alertness(vox_u16 mode)
{
    if (mode == VOX_DIGS_AI_ATTACKING) {
        return 3U;
    }
    if (mode == VOX_DIGS_AI_RETREATING) {
        return 2U;
    }
    if (mode == VOX_DIGS_AI_SEARCHING) {
        return 1U;
    }
    return 0U;
}

static int digs_ai_mode_change_allowed(const vox_digs_ai_state *state,
                                       vox_u16 want)
{
    /* Withdrawing is a survival response and never waits. */
    if (want == VOX_DIGS_AI_RETREATING) {
        return 1;
    }
    if (digs_ai_alertness(want) > digs_ai_alertness(state->mode)) {
        return 1;
    }
    return state->state_ticks >= digs_ai_min_dwell(state->mode);
}

/*
 * Ticks a bot waits before deliberating again.  Returns the value to STORE in
 * decision_ticks, which is one less than the true period -- see the constants.
 */
static vox_u16 digs_ai_decision_delay(const vox_digs_personality *personality)
{
    vox_u32 period = DIGS_AI_PERIOD_MIN_TICKS;
    if (personality != 0) {
        period += ((vox_u32)personality->patience *
                   DIGS_AI_PERIOD_SPAN_TICKS) / 255U;
    }
    if (period < 1U) {
        period = 1U;
    }
    return (vox_u16)(period - 1U);
}

static vox_u16 digs_ai_hazard(const vox_digs_match *match, vox_u16 player,
                              vox_u16 caution, vox_i16 *escape_x)
{
    vox_i32 bot_x = digs_q16_to_cell(match->players[player].position_x.value_q16);
    vox_i32 bot_y = digs_q16_to_cell(match->players[player].position_y.value_q16 +
                                     match->players[player].half_height_q16);
    vox_i32 margin = (vox_i32)DIGS_AI_HAZARD_BASE_MARGIN +
                     (vox_i32)((vox_u32)caution * DIGS_AI_HAZARD_CAUTION_SPAN /
                               255U);
    vox_i32 probe;
    vox_u16 level = 0U;
    vox_i32 left_clear = 0;
    vox_i32 right_clear = 0;
    *escape_x = 0;
    /* The rising basin is an exact line, so no sampling is needed for it. */
    if (bot_y + margin >= (vox_i32)match->lava_surface_y) {
        level = bot_y + (margin / 2L) >= (vox_i32)match->lava_surface_y ? 2U : 1U;
    }
    /* Pooled or tool-made lava has to be looked for. */
    for (probe = -margin; probe <= margin; ++probe) {
        vox_i32 sample_x = bot_x + probe;
        vox_i32 sample_y;
        if (sample_x < 0L || sample_x >= (vox_i32)VOX_WORLD_WIDTH) {
            continue;
        }
        for (sample_y = bot_y; sample_y <= bot_y + 2L; ++sample_y) {
            const vox_cell *cell;
            if (sample_y < 0L || sample_y >= (vox_i32)VOX_WORLD_HEIGHT) {
                continue;
            }
            cell = vox_world_cell(&match->world, (vox_u32)sample_x,
                                  (vox_u32)sample_y, VOX_WORLD_DEPTH - 1U);
            if (cell == 0 || cell->material != VOX_MAT_LAVA) {
                continue;
            }
            if (probe == 0L) {
                level = 2U;
            } else if (level < 1U) {
                level = 1U;
            }
            if (probe < 0L) {
                left_clear = 1;
            } else if (probe > 0L) {
                right_clear = 1;
            }
        }
    }
    if (level != 0U) {
        /* Run from the side that has lava; ties break away from the basin. */
        if (left_clear && !right_clear) {
            *escape_x = 1;
        } else if (right_clear && !left_clear) {
            *escape_x = -1;
        } else {
            *escape_x = bot_x < (vox_i32)(VOX_WORLD_WIDTH / 2U) ? 1 : -1;
        }
    }
    return level;
}

/*
 * Score every tool the arsenal allows and take the best.
 *
 * This replaced flat distance bands that handed every bot the same weapon at
 * the same range.  Score is the archetype's taste for the tool scaled by how
 * well its band covers the current distance, so preference decides between
 * comparable options while range still rules out the absurd ones.  A small
 * deterministic jitter keeps two bots of the same archetype from moving in
 * lockstep.
 */
static vox_u16 digs_ai_weapon(const vox_digs_match *match, vox_u16 player,
                              vox_u32 distance)
{
    vox_u16 archetype = vox_digs_bot_archetype(match, player);
    vox_u16 best = VOX_DIGS_TOOL_COUNT;
    vox_u32 best_score = 0U;
    vox_u16 choice;
    if (archetype >= VOX_DIGS_ARCHETYPE_COUNT) {
        archetype = VOX_DIGS_ARCHETYPE_ENGINEER;
    }
    for (choice = 0U; choice < VOX_DIGS_TOOL_COUNT; ++choice) {
        const digs_weapon_band *band = &digs_weapon_bands[choice];
        vox_u32 fit;
        vox_u32 score;
        if ((match->rules.weapon_mask & (vox_u16)(1U << choice)) == 0U) {
            continue;
        }
        if (distance < band->ideal_min) {
            vox_u32 under = band->ideal_min - distance;
            fit = under >= 16U ? 1U : (16U - under) * 16U;
        } else if (distance > band->ideal_max) {
            vox_u32 over = distance - band->ideal_max;
            fit = over >= 16U ? 1U : (16U - over) * 16U;
        } else {
            fit = 256U;
        }
        score = (vox_u32)digs_archetype_weapon_weight[archetype][choice] * fit;
        /* Break ties without ever overturning a real preference. */
        score += digs_noise(match->rules.seed, match->tick, player,
                            (vox_u32)choice) % 97U;
        if (best == VOX_DIGS_TOOL_COUNT || score > best_score) {
            best = choice;
            best_score = score;
        }
    }
    if (best == VOX_DIGS_TOOL_COUNT) {
        /* weapon_mask is validated non-zero, so this cannot normally
         * happen; fall back to the lowest legal tool rather than to a
         * hardcoded one the arsenal might not contain. */
        for (choice = 0U; choice < VOX_DIGS_TOOL_COUNT; ++choice) {
            if (match->rules.weapon_mask & (vox_u16)(1U << choice)) {
                return choice;
            }
        }
        return VOX_DIGS_TOOL_PICK;
    }
    return best;
}

vox_result vox_digs_bot_think(vox_digs_match *match, vox_u16 player)
{
    vox_digs_ai_state *state;
    vox_u16 target = VOX_DIGS_NO_PLAYER;
    vox_u16 candidate;
    vox_u16 pass;
    vox_u32 nearest = 0xffffffffU;
    vox_i32 bot_x;
    vox_i32 bot_y;
    vox_i32 goal_x;
    vox_i32 goal_y;
    vox_u16 actions = 0U;
    vox_i16 move_x = 0;
    vox_u16 visible = 0U;
    vox_u16 hazard;
    vox_i16 escape_x = 0;
    vox_u16 archetype;
    int breaching = 0;
    const vox_digs_personality *personality;
    if (match == 0 || match->phase != VOX_DIGS_RUNNING ||
        !vox_digs_player_is_bot(match, player) || !match->alive[player]) {
        return VOX_ERR_INVALID;
    }
    state = &match->bots[player];
    state->state_ticks++;
    if (state->memory_ticks > 0U) {
        state->memory_ticks--;
    }
    if (state->retreat_lock_ticks > 0U) {
        state->retreat_lock_ticks--;
    }
    /*
     * Being pinned is measured per tick, not per deliberation.  Counting it
     * on the decision cadence meant RIVET needed fifteen deliberations --
     * nearly two hundred ticks -- to notice a wall, and any single airborne
     * tick in between reset the count, so in practice it never noticed at all.
     */
    if (state->breach_lock_ticks > 0U) {
        state->breach_lock_ticks--;
    }
    if ((match->players[player].flags & VOX_PHYSICS_BODY_BLOCKED_X) != 0U) {
        if (state->stuck_ticks < 65535U) {
            state->stuck_ticks++;
        }
    } else if (state->stuck_ticks > 0U) {
        state->stuck_ticks--;
    }
    /*
     * The urge to say something unprompted: a clock that always runs down,
     * and a dice roll when it lands.
     *
     * This replaced a metronome -- a fixed window every 240 ticks per bot --
     * which is why the mine used to sound like a scheduled broadcast.  The
     * interval is long and jittered and the roll is scaled by sociability,
     * so nothing about it is regular and one of them is plainly chattier
     * than the others.  It lives in the reflex because the deliberation
     * throttle returns early, and a clock that only ticks when a bot happens
     * to be thinking is not a clock.
     */
    if (match->speech_urge_ticks[player] > 0U) {
        match->speech_urge_ticks[player]--;
    } else {
        vox_u32 spin = digs_noise(match->rules.seed, match->tick, player,
                                  0xC1A7U);
        if (digs_speech_roll(match, player,
                             digs_speech_chattiness(match, player),
                             0xBA4BU)) {
            vox_u16 remark = (vox_u16)VOX_DIGS_STIMULUS_IDLE;
            vox_u16 about = VOX_DIGS_NO_PLAYER;
            vox_i32 feet = digs_q16_to_cell(
                match->players[player].position_y.value_q16);
            if (match->buried_ticks[player] > 0U) {
                remark = (vox_u16)VOX_DIGS_STIMULUS_BURIED;
            } else if (feet + 8L >= (vox_i32)match->lava_surface_y) {
                remark = (vox_u16)VOX_DIGS_STIMULUS_LAVA_CLOSE;
            } else if (state->target != VOX_DIGS_NO_PLAYER &&
                       state->memory_ticks > 0U) {
                remark = (vox_u16)VOX_DIGS_STIMULUS_SPOTTED;
                about = state->target;
            }
            digs_speech_set(match, player, about, remark);
        }
        match->speech_urge_ticks[player] =
            (vox_u16)(DIGS_SPEECH_URGE_MIN +
                      (spin % DIGS_SPEECH_URGE_SPAN));
    }
    archetype = vox_digs_bot_archetype(match, player);
    /*
     * Drifted traits, not the archetype table.  A bot twenty matches in
     * should not still be running its factory settings -- that is what makes
     * "no longer the same bot the player met at the beginning" true rather
     * than merely claimed.
     */
    personality = digs_player_traits(match, player);
    if (personality == 0) {
        personality = vox_digs_personality_get(VOX_DIGS_ARCHETYPE_ENGINEER);
    }
    /*
     * Reflex pass -- runs every tick, ahead of the decision throttle.
     *
     * Hazard used to be read inside the throttled path, so a patient miner
     * only noticed lava when it next deliberated.  Once cadence became
     * personality-driven that got measurably worse: RIVET deliberates every
     * thirteen ticks, and lava deals twelve damage per tick, so he could take
     * more than a full health bar of damage before looking down.  Making the
     * cadence expressive therefore required making survival exempt from it.
     *
     * The probe is small -- about 2 * margin + 1 columns at one depth -- so
     * paying it every tick for three bots is far cheaper than the deliberation
     * it guards.
     */
    hazard = digs_ai_hazard(match, player, personality->caution, &escape_x);
    if (hazard >= DIGS_AI_HAZARD_CRITICAL && escape_x != 0) {
        vox_u16 reflex = (vox_u16)(escape_x > 0 ? VOX_DIGS_ACTION_RIGHT :
                                                  VOX_DIGS_ACTION_LEFT);
        reflex = (vox_u16)(reflex | VOX_DIGS_ACTION_JUMP);
        if (match->steam_q16[player] > DIGS_AI_HAZARD_STEAM_RESERVE) {
            reflex = (vox_u16)(reflex | VOX_DIGS_ACTION_STEAM);
        }
        match->player_actions[player] = reflex;
        match->move_x_q15[player] = escape_x > 0 ? 32767 : -32767;
        match->move_y_q15[player] = 0;
        /*
         * Still age the throttle, so escaping does not also buy a free
         * deliberation the moment the miner is clear.
         */
        if (state->decision_ticks > 0U) {
            state->decision_ticks--;
        }
        return VOX_OK;
    }
    if (state->decision_ticks > 0U) {
        state->decision_ticks--;
        return VOX_OK;
    }
    state->decision_ticks = digs_ai_decision_delay(personality);
    hazard = digs_ai_hazard(match, player, personality->caution, &escape_x);
    bot_x = digs_q16_to_cell(match->players[player].position_x.value_q16);
    bot_y = digs_q16_to_cell(match->players[player].position_y.value_q16);
    /*
     * Two passes, because a truce has to be able to lose.  The first ignores
     * anyone this miner has an arrangement with; only if that finds nobody
     * does the second consider them, so a truce holds right up until the
     * moment it is the only thing left in the mine.
     */
    for (pass = 0U; pass < 2U && target == VOX_DIGS_NO_PLAYER; ++pass) {
        nearest = 0xFFFFFFFFUL;
        for (candidate = 0U; candidate < match->rules.player_count;
             ++candidate) {
            vox_i32 other_x;
            vox_i32 other_y;
            vox_u32 distance;
            vox_u32 weighted;
            if (!digs_ai_is_enemy(match, player, candidate)) {
                continue;
            }
            if (pass == 0U &&
                digs_ai_under_truce(match, player, candidate)) {
                continue;
            }
            other_x = digs_q16_to_cell(
                match->players[candidate].position_x.value_q16);
            other_y = digs_q16_to_cell(
                match->players[candidate].position_y.value_q16);
            distance = digs_abs_i32(other_x - bot_x) +
                       digs_abs_i32(other_y - bot_y);
            if (distance > 84U ||
                !digs_ai_line_of_sight(&match->world, bot_x, bot_y,
                                       other_x, other_y)) {
                continue;
            }
            /* History decides who, sight and range decide whether. */
            weighted = (distance *
                        digs_ai_contract_pull(match, player, candidate,
                                              personality->grudge)) / 100U;
            if (weighted < nearest) {
                nearest = weighted;
                target = candidate;
            }
        }
    }
    if (target != VOX_DIGS_NO_PLAYER) {
        const vox_digs_contract *seen = vox_digs_contract_get(match, player,
                                                              target);
        if (seen != 0 && !seen->met) {
            digs_contract_note(match, player, target,
                               (vox_u16)VOX_DIGS_STIMULUS_FIRST_MEETING);
        }
        visible = 1U;
        state->target = target;
        state->memory_ticks = DIGS_AI_MEMORY_TICKS;
        state->last_seen_x_q16 = match->players[target].position_x.value_q16;
        state->last_seen_y_q16 = match->players[target].position_y.value_q16;
    } else {
        target = digs_ai_heard_target(match, player, bot_x, bot_y);
        if (target != VOX_DIGS_NO_PLAYER) {
            state->target = target;
            state->memory_ticks = DIGS_AI_MEMORY_TICKS / 2U;
            state->last_seen_x_q16 =
                match->players[target].position_x.value_q16;
            state->last_seen_y_q16 =
                match->players[target].position_y.value_q16;
        } else if (state->memory_ticks > 0U &&
                   state->target != VOX_DIGS_NO_PLAYER) {
            target = state->target;
        } else {
            state->target = VOX_DIGS_NO_PLAYER;
        }
    }
    {
        vox_u16 want;
        vox_u16 threshold = digs_ai_retreat_threshold(personality);
        int may_retreat = state->retreat_lock_ticks == 0U &&
                          target != VOX_DIGS_NO_PLAYER;
        /* Once withdrawing, stay withdrawn until the retreat times out. */
        int still_retreating = state->mode == VOX_DIGS_AI_RETREATING &&
                               state->state_ticks < DIGS_AI_RETREAT_MAX_TICKS &&
                               target != VOX_DIGS_NO_PLAYER;
        if (still_retreating ||
            (may_retreat && match->health[player] <= threshold)) {
            want = VOX_DIGS_AI_RETREATING;
        } else if (visible) {
            want = VOX_DIGS_AI_ATTACKING;
        } else if (target != VOX_DIGS_NO_PLAYER) {
            want = VOX_DIGS_AI_SEARCHING;
        } else {
            want = VOX_DIGS_AI_ROAMING;
        }
        if (want != state->mode &&
            digs_ai_mode_change_allowed(state, want)) {
            if (state->mode == VOX_DIGS_AI_RETREATING) {
                /* Earned a breather; now it has to commit to fighting. */
                state->retreat_lock_ticks = DIGS_AI_RETREAT_LOCK_TICKS;
            }
            digs_ai_set_mode(match, player, want);
        }
    }
    goal_x = digs_q16_to_cell(state->last_seen_x_q16);
    goal_y = digs_q16_to_cell(state->last_seen_y_q16);
    if (state->mode == VOX_DIGS_AI_ROAMING) {
        /*
         * Roam toward somewhere, not merely away from a wall.
         *
         * This used to walk until BLOCKED_X and reverse, which trapped bots
         * in whatever pocket of the map they spawned in: measured over 3600
         * ticks they spent 70-79% of a match roaming and 1-2% attacking,
         * because they simply never found each other. A destination is chosen
         * from the loudest recent explosion when there is one -- trouble is
         * worth walking toward -- and otherwise from a deterministic sweep
         * across the arena so separate pockets still eventually meet.
         */
        vox_i32 commotion;
        if (state->roam_goal_ticks > 0U) {
            state->roam_goal_ticks--;
        }
        commotion = digs_ai_commotion_x(match, player, bot_x, bot_y);
        if (commotion >= 0L) {
            state->roam_goal_x = (vox_u16)commotion;
            state->roam_goal_ticks = DIGS_AI_ROAM_GOAL_TICKS;
        } else if (state->roam_goal_ticks == 0U ||
                   digs_abs_i32((vox_i32)state->roam_goal_x - bot_x) <=
                       DIGS_AI_ROAM_ARRIVE_CELLS) {
            /*
             * No trouble to walk toward, so sweep. The salt keeps the three
             * bots from picking the same spot, and the destination is a
             * genuine map position rather than a few cells ahead of the nose.
             */
            vox_u32 pick = digs_noise(match->rules.seed, match->tick, player,
                                      0x5EEDU) % (vox_u32)VOX_WORLD_WIDTH;
            state->roam_goal_x = (vox_u16)pick;
            state->roam_goal_ticks = DIGS_AI_ROAM_GOAL_TICKS;
        }
        goal_x = (vox_i32)state->roam_goal_x;
        goal_y = bot_y - 8L;
        if (goal_x + (vox_i32)DIGS_AI_ROAM_ARRIVE_CELLS < bot_x) {
            state->roam_direction = -1;
        } else if (goal_x > bot_x + (vox_i32)DIGS_AI_ROAM_ARRIVE_CELLS) {
            state->roam_direction = 1;
        }
        move_x = state->roam_direction > 0 ? 32767 : -32767;
    } else if (state->mode == VOX_DIGS_AI_RETREATING) {
        move_x = goal_x < bot_x ? 32767 : -32767;
    } else if (goal_x + 2L < bot_x) {
        move_x = -32767;
    } else if (goal_x > bot_x + 2L) {
        move_x = 32767;
    }
    if (state->mode == VOX_DIGS_AI_ATTACKING && nearest < 10U) {
        move_x = goal_x < bot_x ? 24575 : -24575;
    }
    /*
     * Dig through what cannot be walked around.
     *
     * Roaming now aims at a real destination, which exposed the next problem:
     * a bot that wants to be somewhere on the far side of a ridge walks into
     * it and stays there. Being pinned against terrain that stands between
     * the miner and its goal accrues stuck_ticks; past an archetype-scaled
     * threshold it stops walking, points a tool at the obstruction, and bores.
     *
     * The bore is refused if the wall is thicker than the structural rule
     * will hold open, because that tunnel collapses on the miner digging it.
     * In that case the miner keeps walking and the wall stays a wall -- going
     * around is still the fallback, and stuck_ticks is capped so it does not
     * spend the match trying.
     */
    {
        /*
         * Breaching is a commitment, not a per-deliberation vote.
         *
         * The first cut of this reached for a tool only while the probe
         * agreed, and re-ran the probe every deliberation.  A miner jitters
         * by a cell as it settles, which moves the three probed rows, which
         * flipped the reading between "one cell of rock" and "solid past the
         * probe limit" -- and the too-thick branch zeroed stuck_ticks, so it
         * forgot it was stuck and started over.  The rail needs seventy-two
         * ticks of held FIRE to charge and never got past a handful.
         *
         * So the probe decides only whether to *start*.  After that the miner
         * keeps cutting until it is through, or until DIGS_AI_BREACH_GIVEUP
         * ticks say this rock has won and walking is the better idea.
         */
        vox_u16 tool = digs_ai_breach_tool(match, archetype);
        int committed = state->breach_ticks > 0U;
        int may_start = state->breach_lock_ticks == 0U &&
                        state->stuck_ticks >=
                        digs_ai_stuck_threshold(archetype);
        if (state->breach_ticks >= DIGS_AI_BREACH_GIVEUP_TICKS) {
            state->breach_ticks = 0U;
            state->stuck_ticks = 0U;
            state->breach_lock_ticks = DIGS_AI_BREACH_LOCK_TICKS;
        } else if ((committed || may_start) && tool < VOX_DIGS_TOOL_COUNT &&
                   hazard == 0U &&
                   digs_abs_i32(goal_x - bot_x) >
                   (vox_i32)DIGS_AI_ROAM_ARRIVE_CELLS) {
            vox_i16 heading = goal_x < bot_x ? (vox_i16)-1 : (vox_i16)1;
            vox_u16 thickness = 0U;
            vox_u16 face = digs_ai_wall_ahead(match, bot_x, bot_y, heading,
                                              &thickness);
            if (face == 0U) {
                /* Nothing solid ahead: through it, or never walled in. */
                if (committed) {
                    state->breach_lock_ticks = DIGS_AI_BREACH_LOCK_TICKS;
                }
                state->breach_ticks = 0U;
                state->stuck_ticks = 0U;
            } else if (!committed && thickness > DIGS_AI_BREACH_MAX_CELLS) {
                /*
                 * Too thick to start on.  Bore it and the tunnel caves in on
                 * the miner digging it, so walk instead and let the wall win.
                 */
                state->stuck_ticks = 0U;
            } else {
                /*
                 * Aim one cell past the far side so the shot cuts the whole
                 * wall rather than stopping inside it, and alternate between
                 * the feet row and the one above so the hole ends up tall
                 * enough to walk into.  Cutting only at bot_y left a
                 * knee-high slot the miner could see through and not enter.
                 */
                vox_u16 span = thickness > DIGS_AI_BREACH_MAX_CELLS ?
                               DIGS_AI_BREACH_MAX_CELLS : thickness;
                vox_i32 reach = (vox_i32)face + (vox_i32)span;
                vox_i32 aim_cell_x = bot_x + (vox_i32)heading * reach;
                vox_i32 aim_cell_y = bot_y -
                                     (vox_i32)((match->tick >> 4) & 1UL);
                if (aim_cell_x < 0L) {
                    aim_cell_x = 0L;
                }
                if (aim_cell_x >= (vox_i32)VOX_WORLD_WIDTH) {
                    aim_cell_x = (vox_i32)VOX_WORLD_WIDTH - 1L;
                }
                if (aim_cell_y < 0L) {
                    aim_cell_y = 0L;
                }
                match->selected_weapon[player] = tool;
                match->aim_x[player] = (vox_u16)aim_cell_x;
                match->aim_y[player] = (vox_u16)aim_cell_y;
                if (digs_ai_hold_fire(match, player)) {
                    actions = (vox_u16)(actions | VOX_DIGS_ACTION_FIRE);
                }
                /*
                 * Stop shoving only once actually against the rock; while the
                 * face is still cells away, walk into the hole just cut.
                 */
                if (face <= 1U) {
                    move_x = 0;
                }
                breaching = 1;
                if (state->breach_ticks < 65535U) {
                    state->breach_ticks++;
                }
            }
        } else if (committed) {
            /* Lost the reason to dig -- a hazard, or the goal moved. */
            state->breach_ticks = 0U;
        }
    }
    /*
     * Nothing is worth standing in lava for.  This overrides the combat and
     * roaming goals entirely, because a bot that keeps walking at its target
     * through a magma pool is the "killing themselves randomly" the lead
     * reported -- and it should be personality, not pathfinding, that decides
     * a miner dies.
     */
    if (hazard != 0U && escape_x != 0) {
        move_x = escape_x > 0 ? 32767 : -32767;
        goal_x = bot_x + (escape_x > 0 ? 12L : -12L);
        if (hazard >= 2U) {
            /* Climb out: jump, and burn steam if there is any left. */
            actions = (vox_u16)(actions | VOX_DIGS_ACTION_JUMP);
            if (match->steam_q16[player] > 4096U) {
                actions = (vox_u16)(actions | VOX_DIGS_ACTION_STEAM);
            }
            goal_y = bot_y - 12L;
        }
    }
    if ((match->players[player].flags & VOX_PHYSICS_BODY_BLOCKED_X) ||
        ((match->tick + (vox_u32)player * 31U) % 181U) == 0U) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_JUMP);
    }
    if (goal_y + 4L < bot_y && match->steam_q16[player] > 8192U) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_STEAM);
    }
    if ((goal_y + 9L < bot_y ||
         (match->players[player].flags & VOX_PHYSICS_BODY_BLOCKED_X)) &&
        state->mode != VOX_DIGS_AI_RETREATING) {
        vox_i32 rope_x = bot_x + (move_x >= 0 ? 14L : -14L);
        vox_i32 rope_y = bot_y - 20L;
        if (rope_x < 0L) {
            rope_x = 0L;
        }
        if (rope_x >= (vox_i32)VOX_WORLD_WIDTH) {
            rope_x = (vox_i32)VOX_WORLD_WIDTH - 1L;
        }
        if (rope_y < 0L) {
            rope_y = 0L;
        }
        match->aim_x[player] = (vox_u16)rope_x;
        match->aim_y[player] = (vox_u16)rope_y;
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_ROPE);
    } else if (target != VOX_DIGS_NO_PLAYER) {
        match->aim_x[player] = (vox_u16)goal_x;
        match->aim_y[player] = (vox_u16)goal_y;
    }
    if (breaching) {
        /* Aim, weapon and FIRE are already set by the bore. */
    } else if (visible && match->rail_charging[player] &&
        match->selected_weapon[player] == VOX_DIGS_TOOL_RAIL_GUN) {
        if (match->rail_charge_ticks[player] < DIGS_RAIL_MAX_CHARGE_TICKS) {
            actions = (vox_u16)(actions | VOX_DIGS_ACTION_FIRE);
        }
    } else if (match->weapon_charging[player] &&
               match->weapon_charge_ticks[player] <
                   digs_weapons[match->selected_weapon[player]].charge_ticks) {
        /*
         * See a charge through instead of dropping it at the next decision.
         *
         * A bot only holds fire for one eight-tick decision window, so it
         * reached a charge of eight and released.  The Bolt Action needs
         * thirty before digs_release_charged_weapon will fire it at all --
         * and it is the weapon bots picked for mid range, so they simply
         * never shot.  The Firecracker and Smoker did fire, but always at
         * near-minimum power.  The rail gun escaped this only because it has
         * its own re-hold immediately above; this generalises that to every
         * charge weapon.
         *
         * Deliberately no weapon re-selection on this branch: switching
         * mid-charge would move the target the charge is being measured
         * against.
         */
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_FIRE);
    } else if (match->weapon_cooldown[player] == 0U && visible &&
               ((match->tick + (vox_u32)player * 17U) % 24U) <
                   DIGS_AI_FIRE_WINDOW_TICKS) {
        match->selected_weapon[player] =
            digs_ai_weapon(match, player, nearest);
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_FIRE);
    }
    match->player_actions[player] = actions;
    match->move_x_q15[player] = move_x;
    match->move_y_q15[player] = 0;
    match->state_hash = vox_digs_hash(match);
    return VOX_OK;
}

static int digs_projectile_hits_solid(const vox_world *world,
                                      vox_u32 x, vox_u32 y)
{
    return vox_world_collision_classify(world, x, y) ==
           VOX_WORLD_COLLISION_SOLID;
}

static void digs_deposit_projectile(vox_digs_match *match,
                                    const vox_digs_projectile *projectile,
                                    vox_u32 x, vox_u32 y)
{
    long radius = (long)projectile->blast_radius;
    long offset_y;
    long offset_x;
    vox_u32 z;
    for (offset_y = -radius; offset_y <= radius; ++offset_y) {
        for (offset_x = -radius; offset_x <= radius; ++offset_x) {
            long sample_x = (long)x + offset_x;
            long sample_y = (long)y + offset_y;
            if (sample_x < 0L || sample_y < 0L ||
                sample_x >= (long)VOX_WORLD_WIDTH ||
                sample_y >= (long)VOX_WORLD_HEIGHT ||
                offset_x * offset_x + offset_y * offset_y > radius * radius) {
                continue;
            }
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(
                    &match->world, (vox_u32)sample_x, (vox_u32)sample_y, z);
                if (cell == 0 || cell->material == VOX_MAT_BEDROCK) {
                    continue;
                }
                if (projectile->material == VOX_MAT_SMOKE &&
                    cell->material != VOX_MAT_AIR) {
                    continue;
                }
                if (projectile->material == VOX_MAT_WATER &&
                    cell->material != VOX_MAT_AIR &&
                    cell->material != VOX_MAT_LAVA) {
                    continue;
                }
                (void)vox_world_set(&match->world, (vox_u32)sample_x,
                                    (vox_u32)sample_y, z,
                                    projectile->material,
                                    projectile->material == VOX_MAT_LAVA ?
                                    750L << 16 : 80L << 16);
            }
        }
    }
}

static void digs_detonate_projectile(vox_digs_match *match, vox_u16 slot,
                                     vox_u32 x, vox_u32 y,
                                     vox_u16 hit_player,
                                     vox_u16 hit_part)
{
    vox_digs_projectile projectile = match->projectiles[slot];
    const vox_digs_weapon_properties *properties =
        &digs_weapons[projectile.weapon];
    vox_u16 spark;
    vox_u16 particle_count;
    vox_u16 debris_materials[160];
    vox_u32 explosion_noise = digs_noise(match->rules.seed, match->tick,
                                         slot, projectile.weapon * 97U +
                                         projectile.owner);
    vox_u16 terrain_radius = projectile.blast_radius;
    particle_count = (vox_u16)(projectile.blast_radius *
                               projectile.blast_radius * 4U + 16U);
    if (match->rules.fx_budget == VOX_DIGS_FX_RETRO) {
        particle_count = (vox_u16)(particle_count * 3U / 4U);
    } else if (match->rules.fx_budget == VOX_DIGS_FX_CARNAGE) {
        particle_count = (vox_u16)(particle_count * 3U / 2U);
    }
    if (particle_count > 160U) {
        particle_count = 160U;
    }
    if (terrain_radius > 1U) {
        vox_i32 radius_variation = (vox_i32)(explosion_noise % 3U) - 1L;
        terrain_radius = (vox_u16)((vox_i32)terrain_radius +
                                   radius_variation);
        if (terrain_radius > VOX_BLAST_MAX_RADIUS) {
            terrain_radius = VOX_BLAST_MAX_RADIUS;
        }
    }
    for (spark = 0U; spark < particle_count; ++spark) {
        vox_u32 noise = digs_noise(explosion_noise, spark, x, y);
        vox_u32 span = (vox_u32)terrain_radius * 2U + 5U;
        long sample_x = (long)x +
            (long)((vox_i32)(noise % span) - (vox_i32)(span / 2U));
        long sample_y = (long)y +
            (long)((vox_i32)((noise >> 9) % span) -
                   (vox_i32)(span / 2U));
        const vox_cell *sample = 0;
        if (sample_x >= 0L && sample_y >= 0L &&
            sample_x < (long)VOX_WORLD_WIDTH &&
            sample_y < (long)VOX_WORLD_HEIGHT) {
            sample = vox_world_cell(&match->world, (vox_u32)sample_x,
                                    (vox_u32)sample_y,
                                    VOX_WORLD_DEPTH - 1U);
        }
        debris_materials[spark] = sample != 0 &&
                                  sample->material != VOX_MAT_AIR &&
                                  sample->material != VOX_MAT_BEDROCK ?
                                  sample->material : projectile.material;
    }
    match->projectiles[slot].active = 0U;
    if (match->projectile_count > 0U) {
        match->projectile_count--;
    }
    if ((properties->flags & VOX_DIGS_WEAPON_EXPLOSIVE) &&
        projectile.blast_radius > 0U) {
        vox_u16 lobe;
        vox_u16 lobe_count = (vox_u16)(2U + explosion_noise % 4U);
        (void)vox_world_blast(&match->world, x, y,
                              VOX_WORLD_DEPTH - 1U,
                              terrain_radius, 700L << 16);
        for (lobe = 0U; lobe < lobe_count; ++lobe) {
            vox_u32 noise = digs_noise(explosion_noise, lobe, x, y);
            vox_i32 offset_x = (vox_i32)(noise %
                (terrain_radius + 1U)) - (vox_i32)(terrain_radius / 2U);
            vox_i32 offset_y = (vox_i32)((noise >> 8) %
                (terrain_radius + 1U)) - (vox_i32)(terrain_radius / 2U);
            vox_i32 lobe_x = (vox_i32)x + offset_x;
            vox_i32 lobe_y = (vox_i32)y + offset_y;
            vox_u16 lobe_radius = (vox_u16)(terrain_radius / 3U +
                                             1U + (noise >> 16) % 3U);
            if (lobe_x > 0 && lobe_y > 0 &&
                lobe_x < (vox_i32)VOX_WORLD_WIDTH &&
                lobe_y < (vox_i32)VOX_WORLD_HEIGHT) {
                (void)vox_world_blast(&match->world, (vox_u32)lobe_x,
                                      (vox_u32)lobe_y,
                                      VOX_WORLD_DEPTH - 1U,
                                      lobe_radius, 700L << 16);
            }
        }
        digs_damage_radius(match, projectile.owner, x, y,
                           projectile.blast_radius, projectile.damage,
                           projectile.weapon,
                           projectile.arming_ticks > 0U ? 1U : 0U);
        if (projectile.weapon == VOX_DIGS_TOOL_FIRECRACKER) {
            for (spark = 0U; spark < 18U; ++spark) {
                vox_u32 flame_noise = digs_noise(explosion_noise, spark,
                                                  x, y + 17U);
                digs_spawn_effect_variant(match, VOX_MAT_LAVA,
                    (vox_i32)(x << 16), (vox_i32)(y << 16),
                    ((vox_i32)(flame_noise % 13U) - 6L) * 4096L,
                    -4096L - (vox_i32)((flame_noise >> 8) % 9U) * 3072L,
                    (vox_u16)(18U + flame_noise % 24U), projectile.owner,
                    (vox_u16)(flame_noise & 15U));
            }
        }
        digs_emit_event(match, VOX_DIGS_EVENT_EXPLOSION,
                        projectile.owner, hit_player, projectile.weapon,
                        projectile.material, (vox_i32)(x << 16),
                        (vox_i32)(y << 16), terrain_radius,
                        (vox_u16)(explosion_noise & 31U));
    } else if (hit_player != VOX_DIGS_NO_PLAYER && projectile.damage > 0U) {
        vox_u16 damage_flags =
            projectile.weapon == VOX_DIGS_TOOL_CINDER_FLASK ?
            VOX_DIGS_DAMAGE_HEAT : VOX_DIGS_DAMAGE_BALLISTIC;
        (void)vox_digs_apply_hit(match, projectile.owner, hit_player,
                                 projectile.weapon, hit_part,
                                 projectile.damage, damage_flags);
    } else if (projectile.blast_radius > 0U &&
               projectile.weapon == VOX_DIGS_TOOL_BOILER_SHOTGUN) {
        (void)vox_world_blast(&match->world, x, y,
                              VOX_WORLD_DEPTH - 1U,
                              DIGS_SCALE(1U), 100L << 16);
    }
    if (properties->flags & VOX_DIGS_WEAPON_DEPOSIT) {
        digs_deposit_projectile(match, &projectile, x, y);
    }
    for (spark = 0U; spark < particle_count; ++spark) {
        vox_u32 noise = digs_noise(explosion_noise, spark,
                                   projectile.owner, projectile.weapon);
        vox_i32 velocity_x = ((vox_i32)(noise % 25U) - 12L) * 6656L;
        vox_i32 velocity_y = -6144L -
                             (vox_i32)((noise >> 7) % 20U) * 4608L;
        vox_u16 material = debris_materials[spark];
        if (projectile.weapon == VOX_DIGS_TOOL_BORE_DRILL &&
            (noise & 1U) == 0U) {
            material = VOX_MAT_METAL;
        } else if ((properties->flags & VOX_DIGS_WEAPON_EXPLOSIVE) &&
                   noise % 7U == 0U) {
            material = VOX_MAT_SMOKE;
        } else if ((properties->flags & VOX_DIGS_WEAPON_EXPLOSIVE) &&
                   noise % 17U == 0U) {
            material = VOX_MAT_LAVA;
        }
        digs_spawn_effect_variant(match, material, (vox_i32)(x << 16),
                                  (vox_i32)(y << 16), velocity_x, velocity_y,
                                  (vox_u16)(24U + noise % 72U),
                                  projectile.owner,
                                  (vox_u16)(noise & 31U));
    }
}

static void digs_step_projectiles(vox_digs_match *match)
{
    vox_u16 slot;
    for (slot = 0U; slot < VOX_DIGS_MAX_PROJECTILES; ++slot) {
        vox_digs_projectile *projectile = &match->projectiles[slot];
        const vox_digs_weapon_properties *properties;
        vox_u32 substeps;
        vox_u32 substep;
        int detonated = 0;
        if (!projectile->active) {
            continue;
        }
        properties = &digs_weapons[projectile->weapon];
        if (projectile->weapon == VOX_DIGS_TOOL_SMOKER &&
            projectile->age_ticks % DIGS_SMOKER_TRAIL_TICKS == 0U) {
            vox_u16 cloud;
            for (cloud = 0U; cloud < 3U; ++cloud) {
                vox_u32 noise = digs_noise(match->rules.seed, match->tick,
                                           slot, cloud + projectile->age_ticks);
                digs_spawn_effect_variant(match, VOX_MAT_SMOKE,
                    projectile->position_x_q16, projectile->position_y_q16,
                    ((vox_i32)(noise % 5U) - 2L) * 2048L,
                    -2048L - (vox_i32)((noise >> 7) % 3U) * 1024L,
                    (vox_u16)(36U + noise % 32U), projectile->owner,
                    (vox_u16)(noise & 15U));
            }
        }
        if (projectile->weapon == VOX_DIGS_TOOL_PULASKI &&
            projectile->age_ticks >= DIGS_PULASKI_RETURN_TICKS) {
            vox_i32 return_x = match->players[projectile->owner]
                               .position_x.value_q16 -
                               projectile->position_x_q16;
            vox_i32 return_y = match->players[projectile->owner]
                               .position_y.value_q16 -
                               projectile->position_y_q16;
            if (digs_distance_approx(return_x, return_y) < (2L << 16)) {
                projectile->active = 0U;
                if (match->projectile_count > 0U) {
                    match->projectile_count--;
                }
                continue;
            }
            projectile->velocity_x_q16 = return_x / 5L;
            projectile->velocity_y_q16 = return_y / 5L;
        }
        if (properties->flags & VOX_DIGS_WEAPON_GRAVITY) {
            projectile->velocity_y_q16 += DIGS_PROJECTILE_GRAVITY_Q16;
        }
        substeps = (vox_u32)(digs_distance_approx(
            projectile->velocity_x_q16,
            projectile->velocity_y_q16) / DIGS_SWEEP_STEP_Q16);
        if (substeps == 0U) {
            substeps = 1U;
        }
        if ((vox_i32)(substeps * (vox_u32)DIGS_SWEEP_STEP_Q16) <
            digs_distance_approx(projectile->velocity_x_q16,
                                 projectile->velocity_y_q16)) {
            substeps++;
        }
        for (substep = 0U; substep < substeps; ++substep) {
            vox_i32 x_cell;
            vox_i32 y_cell;
            vox_i32 previous_x_q16;
            vox_i32 previous_y_q16;
            vox_u16 player;
            int bounced = 0;
            previous_x_q16 = projectile->position_x_q16;
            previous_y_q16 = projectile->position_y_q16;
            projectile->position_x_q16 += digs_div_trunc_positive(
                projectile->velocity_x_q16, substeps);
            projectile->position_y_q16 += digs_div_trunc_positive(
                projectile->velocity_y_q16, substeps);
            x_cell = digs_q16_to_cell(projectile->position_x_q16);
            y_cell = digs_q16_to_cell(projectile->position_y_q16);
            if (x_cell < 0 || y_cell < 0 ||
                x_cell >= (vox_i32)VOX_WORLD_WIDTH ||
                y_cell >= (vox_i32)VOX_WORLD_HEIGHT) {
                match->projectiles[slot].active = 0U;
                if (match->projectile_count > 0U) {
                    match->projectile_count--;
                }
                detonated = 1;
                break;
            }
            if (projectile->owner_clear &&
                (projectile->position_x_q16 <
                     projectile->launch_min_x_q16 ||
                 projectile->position_x_q16 >
                     projectile->launch_max_x_q16 ||
                 projectile->position_y_q16 <
                     projectile->launch_min_y_q16 ||
                 projectile->position_y_q16 >
                     projectile->launch_max_y_q16) &&
                !digs_point_hits_player(match, projectile->owner,
                                        projectile->position_x_q16,
                                        projectile->position_y_q16, 0)) {
                projectile->owner_clear = 0U;
            }
            if (!projectile->owner_clear &&
                digs_projectile_hits_solid(&match->world,
                                           (vox_u32)x_cell,
                                           (vox_u32)y_cell)) {
                if (projectile->weapon == VOX_DIGS_TOOL_SMOKER) {
                    projectile->position_x_q16 = previous_x_q16;
                    projectile->position_y_q16 = previous_y_q16;
                    projectile->velocity_x_q16 =
                        -(projectile->velocity_x_q16 * 3L / 5L);
                    projectile->velocity_y_q16 =
                        -(projectile->velocity_y_q16 * 2L / 5L) - 4096L;
                    break;
                }
                if (projectile->weapon == VOX_DIGS_TOOL_PULASKI) {
                    /* A charged Pulaski shaves a small, noisy notch into a
                     * wall before it turns around.  It is intentionally not
                     * an explosion: the axe stays in flight and returns. */
                    (void)vox_world_blast(&match->world,
                                          (vox_u32)x_cell,
                                          (vox_u32)y_cell,
                                          VOX_WORLD_DEPTH - 1U,
                                          projectile->blast_radius,
                                          360L << 16);
                    digs_spawn_effect_variant(match, VOX_MAT_METAL,
                        projectile->position_x_q16,
                        projectile->position_y_q16,
                        projectile->velocity_x_q16 / -4L,
                        -8192L, 12U, projectile->owner,
                        (vox_u16)(projectile->age_ticks & 15U));
                    projectile->position_x_q16 = previous_x_q16;
                    projectile->position_y_q16 = previous_y_q16;
                    projectile->age_ticks = DIGS_PULASKI_RETURN_TICKS;
                    break;
                }
                digs_detonate_projectile(match, slot, (vox_u32)x_cell,
                                         (vox_u32)y_cell,
                                         VOX_DIGS_NO_PLAYER,
                                         VOX_DIGS_NO_PART);
                detonated = 1;
                break;
            }
            for (player = 0U; player < match->rules.player_count; ++player) {
                vox_u16 hit_part = VOX_DIGS_NO_PART;
                if (!match->alive[player] ||
                    (player == projectile->owner &&
                     projectile->arming_ticks > 0U &&
                     !(projectile->weapon == VOX_DIGS_TOOL_PULASKI &&
                       projectile->age_ticks >=
                       DIGS_PULASKI_RETURN_TICKS))) {
                    continue;
                }
                if (digs_point_hits_player(match, player,
                                           projectile->position_x_q16,
                                           projectile->position_y_q16,
                                           &hit_part)) {
                    if (projectile->weapon == VOX_DIGS_TOOL_SMOKER) {
                        /*
                         * One solid blow, then the canister is spent.
                         *
                         * This used to rewind the canister to its previous
                         * position and reverse its velocity on every body
                         * overlap.  Rewinding put it back inside the miner,
                         * so it struck again next tick, and the damping
                         * converged it to a dead stop while still embedded:
                         * a single throw pinned itself to its victim and
                         * dealt its one damage roughly ninety times over the
                         * fuse, spraying an event per tick.  That is the
                         * reported "bounces too much and causes a bunch of
                         * noise/damage".
                         *
                         * Now a direct hit lands once and hurts properly,
                         * then zeroes its own damage as a spent marker.  A
                         * spent canister passes through bodies without
                         * interacting, so it drops and rolls away down the
                         * landscape while it lays smoke, exactly as asked.
                         *
                         * DIGS_SMOKER_DIRECT_DAMAGE stays below the smallest
                         * vital part -- the head at 45 -- so a direct hit can
                         * never kill a miner who was at full health, though
                         * it can still take an arm off.
                         */
                        if (projectile->damage > 0U) {
                            (void)vox_digs_apply_hit(match,
                                projectile->owner, player,
                                projectile->weapon, hit_part,
                                projectile->damage,
                                VOX_DIGS_DAMAGE_BLUNT);
                            projectile->damage = 0U;
                            projectile->velocity_x_q16 /= 4L;
                            projectile->velocity_y_q16 =
                                DIGS_SMOKER_SETTLE_FALL_Q16;
                            bounced = 1;
                        }
                        break;
                    }
                    if (projectile->weapon == VOX_DIGS_TOOL_PULASKI) {
                        if (player != projectile->owner) {
                            (void)vox_digs_apply_hit(match, projectile->owner,
                                player, projectile->weapon, hit_part,
                                projectile->damage,
                                VOX_DIGS_DAMAGE_BALLISTIC);
                            projectile->age_ticks =
                                DIGS_PULASKI_RETURN_TICKS;
                        } else {
                            projectile->active = 0U;
                            if (match->projectile_count > 0U) {
                                match->projectile_count--;
                            }
                        }
                        detonated = 1;
                        break;
                    }
                    if (projectile->weapon == VOX_DIGS_TOOL_HYDROSHOT) {
                        match->players[player].velocity_x.value_q16 +=
                            projectile->velocity_x_q16 / 3L;
                        match->players[player].velocity_y.value_q16 +=
                            projectile->velocity_y_q16 / 4L - 8192L;
                    }
                    digs_detonate_projectile(match, slot,
                                             (vox_u32)x_cell,
                                             (vox_u32)y_cell, player,
                                             hit_part);
                    detonated = 1;
                    break;
                }
            }
            if (bounced) {
                break;
            }
            if (detonated) {
                break;
            }
        }
        if (detonated) {
            continue;
        }
        projectile->age_ticks++;
        if (projectile->arming_ticks > 0U) {
            projectile->arming_ticks--;
        }
        if (projectile->fuse_ticks > 0U) {
            projectile->fuse_ticks--;
            if (projectile->fuse_ticks == 0U) {
                vox_i32 x_cell = digs_q16_to_cell(
                    projectile->position_x_q16);
                vox_i32 y_cell = digs_q16_to_cell(
                    projectile->position_y_q16);
                if (x_cell >= 0 && y_cell >= 0 &&
                    x_cell < (vox_i32)VOX_WORLD_WIDTH &&
                    y_cell < (vox_i32)VOX_WORLD_HEIGHT) {
                    digs_detonate_projectile(match, slot,
                                             (vox_u32)x_cell,
                                             (vox_u32)y_cell,
                                             VOX_DIGS_NO_PLAYER,
                                             VOX_DIGS_NO_PART);
                }
            }
        }
    }
}

static int digs_effect_material_deposits(vox_u16 material)
{
    return material == VOX_MAT_BLOOD || material == VOX_MAT_SOIL ||
           material == VOX_MAT_STONE || material == VOX_MAT_COAL ||
           material == VOX_MAT_SAND || material == VOX_MAT_BIOMASS ||
           material == VOX_MAT_FLESH;
}

static int digs_effect_cell_overlaps_player(const vox_digs_match *match,
                                            vox_i32 x, vox_i32 y)
{
    vox_i32 point_x = (x << 16) + 32768L;
    vox_i32 point_y = (y << 16) + 32768L;
    vox_u16 player;
    for (player = 0U; player < match->rules.player_count; ++player) {
        if (digs_point_hits_player(match, player, point_x, point_y, 0)) {
            return 1;
        }
    }
    return 0;
}

static void digs_deposit_effect_impact(vox_digs_match *match,
                                       const vox_digs_effect *effect,
                                       vox_i32 free_x, vox_i32 free_y,
                                       vox_i32 impact_x,
                                       vox_i32 impact_y)
{
    static const vox_i16 offsets[9][2] = {
        {0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
    };
    vox_u16 candidate;
    if (!digs_effect_material_deposits(effect->material)) {
        return;
    }
    for (candidate = 0U; candidate < 9U; ++candidate) {
        vox_i32 x = candidate == 0U ? free_x :
                    impact_x + offsets[candidate][0];
        vox_i32 y = candidate == 0U ? free_y :
                    impact_y + offsets[candidate][1];
        const vox_cell *cell;
        const vox_material_properties *properties;
        if (x < 0 || y < 0 || x >= (vox_i32)VOX_WORLD_WIDTH ||
            y >= (vox_i32)VOX_WORLD_HEIGHT ||
            digs_effect_cell_overlaps_player(match, x, y)) {
            continue;
        }
        cell = vox_world_cell(&match->world, (vox_u32)x, (vox_u32)y,
                              effect->depth);
        if (cell == 0 || cell->material != VOX_MAT_AIR) {
            continue;
        }
        if (vox_world_set(&match->world, (vox_u32)x, (vox_u32)y,
                          effect->depth, effect->material,
                          effect->material == VOX_MAT_BLOOD ?
                          37L << 16 : 80L << 16) != VOX_OK) {
            return;
        }
        properties = vox_material_get(effect->material);
        if (properties != 0 &&
            (properties->flags & VOX_MATERIAL_SOLID) != 0U) {
            (void)vox_world_set_loose(&match->world, (vox_u32)x,
                                      (vox_u32)y, effect->depth, 1U);
        }
        return;
    }
}

static void digs_step_effects(vox_digs_match *match)
{
    vox_u16 slot;
    for (slot = 0U; slot < match->rules.fx_budget; ++slot) {
        vox_digs_effect *effect = &match->effects[slot];
        vox_i32 x_cell;
        vox_i32 y_cell;
        vox_i32 previous_x;
        vox_i32 previous_y;
        vox_i32 distance;
        vox_u32 steps;
        vox_u32 step;
        int terrain_hit = 0;
        int outside = 0;
        if (!effect->active) {
            continue;
        }
        if (effect->material != VOX_MAT_SMOKE) {
            effect->velocity_y_q16 += 2048L;
        }
        previous_x = effect->position_x_q16;
        previous_y = effect->position_y_q16;
        distance = digs_distance_approx(effect->velocity_x_q16,
                                        effect->velocity_y_q16);
        steps = (vox_u32)(distance / DIGS_SWEEP_STEP_Q16);
        if ((vox_i32)(steps * (vox_u32)DIGS_SWEEP_STEP_Q16) < distance) {
            steps++;
        }
        if (steps == 0U) steps = 1U;
        x_cell = digs_q16_to_cell(previous_x);
        y_cell = digs_q16_to_cell(previous_y);
        for (step = 0U; step < steps; ++step) {
            vox_i32 next_x = effect->position_x_q16 +
                digs_div_trunc_positive(effect->velocity_x_q16, steps);
            vox_i32 next_y = effect->position_y_q16 +
                digs_div_trunc_positive(effect->velocity_y_q16, steps);
            vox_i32 next_cell_x = digs_q16_to_cell(next_x);
            vox_i32 next_cell_y = digs_q16_to_cell(next_y);
            if (next_cell_x < 0 || next_cell_y < 0 ||
                next_cell_x >= (vox_i32)VOX_WORLD_WIDTH ||
                next_cell_y >= (vox_i32)VOX_WORLD_HEIGHT) {
                outside = 1;
                break;
            }
            if (effect->material != VOX_MAT_SMOKE &&
                digs_projectile_hits_solid(&match->world,
                                           (vox_u32)next_cell_x,
                                           (vox_u32)next_cell_y)) {
                terrain_hit = 1;
                digs_deposit_effect_impact(match, effect, x_cell, y_cell,
                                           next_cell_x, next_cell_y);
                break;
            }
            effect->position_x_q16 = next_x;
            effect->position_y_q16 = next_y;
            previous_x = next_x;
            previous_y = next_y;
            x_cell = next_cell_x;
            y_cell = next_cell_y;
        }
        if (effect->ttl_ticks > 0U) {
            effect->ttl_ticks--;
        }
        if (terrain_hit || outside || effect->ttl_ticks == 0U) {
            effect->active = 0U;
            if (match->effect_count > 0U) {
                match->effect_count--;
            }
        }
    }
}

static void digs_step_bleeding(vox_digs_match *match)
{
    vox_u16 player;
    for (player = 0U; player < match->rules.player_count; ++player) {
        vox_u16 part;
        vox_u16 total_rate = 0U;
        vox_u16 damage;
        vox_u32 accumulated;
        if (!match->alive[player]) {
            continue;
        }
        for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
            total_rate = (vox_u16)(total_rate +
                match->anatomy[player][part].bleed_rate_q8);
        }
        if (total_rate == 0U) {
            match->bleed_accumulator_q8[player] = 0U;
            continue;
        }
        match->clot_ticks[player]++;
        if (match->clot_ticks[player] >= 180U) {
            match->clot_ticks[player] = 0U;
            for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
                vox_digs_anatomy_part *anatomy =
                    &match->anatomy[player][part];
                if (anatomy->bleed_rate_q8 > 0U &&
                    !(anatomy->flags & VOX_DIGS_PART_SEVERED)) {
                    anatomy->bleed_rate_q8--;
                    if (anatomy->bleed_rate_q8 == 0U) {
                        anatomy->flags = (vox_u16)(anatomy->flags &
                            (vox_u16)~VOX_DIGS_PART_BLEEDING);
                    }
                }
            }
        }
        if (match->spawn_shield_ticks[player] > 0U) {
            continue;
        }
        accumulated = (vox_u32)match->bleed_accumulator_q8[player] +
                      (vox_u32)total_rate;
        if (accumulated > 65535U) {
            match->bleed_accumulator_q8[player] = 65535U;
        } else {
            match->bleed_accumulator_q8[player] = (vox_u16)accumulated;
        }
        damage = (vox_u16)(match->bleed_accumulator_q8[player] / 256U);
        match->bleed_accumulator_q8[player] = (vox_u16)(
            match->bleed_accumulator_q8[player] % 256U);
        if (damage > 0U) {
            vox_u16 drops = damage > 8U ? 8U : damage;
            for (part = 0U; part < drops; ++part) {
                vox_u32 noise = digs_noise(match->rules.seed,
                                           match->tick, player,
                                           0xB1EEDU + part);
                digs_spawn_effect_variant(match, VOX_MAT_BLOOD,
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    ((vox_i32)(noise % 9U) - 4L) * 2048L,
                    -4096L - (vox_i32)((noise >> 7) % 5U) * 2048L,
                    (vox_u16)(28U + noise % 36U), player,
                    (vox_u16)(noise & 15U));
            }
            digs_emit_event(match, VOX_DIGS_EVENT_BLEED,
                            match->last_attacker[player], player,
                            match->last_damage_weapon[player],
                            VOX_MAT_BLOOD,
                            match->players[player].position_x.value_q16,
                            match->players[player].position_y.value_q16,
                            damage, (vox_u16)(
                                (match->last_damage_part[player] << 4) |
                                (total_rate & 15U)));
            if (damage < match->health[player]) {
                match->health[player] = (vox_u16)(match->health[player] -
                                                   damage);
            } else if (digs_last_attacker_is_recent(match, player)) {
                (void)vox_digs_record_kill(match,
                                           match->last_attacker[player],
                                           player);
            } else {
                digs_environment_defeat(match, player);
            }
        }
    }
}

static void digs_step_reactions(vox_digs_match *match)
{
    vox_u16 sample;
    for (sample = 0U; sample < DIGS_REACTION_SAMPLES; ++sample) {
        vox_u32 noise = digs_noise(match->rules.seed, match->world.tick,
                                   sample, 0xB100D5U);
        vox_u32 x = noise % VOX_WORLD_WIDTH;
        vox_u32 y = (noise >> 10) % VOX_WORLD_HEIGHT;
        vox_u32 z = (noise >> 20) % VOX_WORLD_DEPTH;
        const vox_cell *cell = vox_world_cell(&match->world, x, y, z);
        const vox_cell *below;
        if (cell == 0 || cell->material != VOX_MAT_BLOOD ||
            y + 1U >= VOX_WORLD_HEIGHT) {
            continue;
        }
        below = vox_world_cell(&match->world, x, y + 1U, z);
        if (below != 0 && below->material == VOX_MAT_LAVA) {
            (void)vox_world_set(&match->world, x, y, z,
                                VOX_MAT_SMOKE, 220L << 16);
        } else if (below != 0 && below->material == VOX_MAT_WATER &&
                   (noise & 3U) == 0U) {
            (void)vox_world_set(&match->world, x, y, z,
                                VOX_MAT_WATER, 32L << 16);
        }
    }
}

static void digs_update_lava(vox_digs_match *match)
{
    vox_u16 desired_surface;
    if (match->lava_level_q16 == 0U) {
        return;
    }
    desired_surface = (vox_u16)(DIGS_LAVA_BASIN_TOP -
        (match->lava_level_q16 *
         (DIGS_LAVA_BASIN_TOP - DIGS_SCALE(3U)) / 65535U));
    while (match->lava_surface_y > desired_surface) {
        vox_u32 x;
        vox_u32 z;
        match->lava_surface_y--;
        for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(
                    &match->world, x, match->lava_surface_y, z);
                if (cell != 0 && cell->material != VOX_MAT_BEDROCK) {
                    (void)vox_world_set(&match->world, x,
                                        match->lava_surface_y, z,
                                        VOX_MAT_LAVA, 750L << 16);
                }
            }
        }
    }
}

static void digs_apply_lava_hazards(vox_digs_match *match)
{
    vox_u16 player;
    for (player = 0U; player < match->rules.player_count; ++player) {
        vox_i32 foot_y;
        vox_i32 x_cell;
        int touching_lava = 0;
        vox_u32 z;
        if (!match->alive[player]) {
            continue;
        }
        foot_y = digs_q16_to_cell(
            match->players[player].position_y.value_q16 +
            match->players[player].half_height_q16);
        x_cell = digs_q16_to_cell(
            match->players[player].position_x.value_q16);
        if (foot_y >= (vox_i32)match->lava_surface_y) {
            touching_lava = 1;
        } else if (x_cell >= 0 && foot_y >= 0 &&
                   x_cell < (vox_i32)VOX_WORLD_WIDTH &&
                   foot_y < (vox_i32)VOX_WORLD_HEIGHT) {
            for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                const vox_cell *cell = vox_world_cell(
                    &match->world, (vox_u32)x_cell, (vox_u32)foot_y, z);
                if (cell != 0 && cell->material == VOX_MAT_LAVA) {
                    touching_lava = 1;
                    break;
                }
            }
        }
        if (touching_lava) {
            (void)vox_digs_apply_hit(match, VOX_DIGS_NO_PLAYER, player,
                                     VOX_DIGS_TOOL_CINDER_FLASK,
                                     VOX_DIGS_NO_PART, 12U,
                                     VOX_DIGS_DAMAGE_HEAT);
            if (match->alive[player]) {
                digs_spawn_effect(match, VOX_MAT_SMOKE,
                    match->players[player].position_x.value_q16,
                    match->players[player].position_y.value_q16,
                    0L, -24576L, 28U);
            }
        }
    }
}

vox_u32 vox_digs_hash(const vox_digs_match *match)
{
    vox_u32 hash = 2166136261U;
    vox_u16 i;
    if (match == 0) {
        return 0U;
    }
    hash = digs_hash_mix(hash, match->rules.seed);
    hash = digs_hash_mix(hash, match->rules.match_ticks);
    hash = digs_hash_mix(hash, match->rules.score_limit);
    hash = digs_hash_mix(hash, match->rules.lava_start_tick);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.player_count);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.bot_mask);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.map_style);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.weapon_mask);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.fx_budget);
    hash = digs_hash_mix(hash, (vox_u32)match->rules.respawn_mode);
    hash = digs_hash_mix(hash,
                         (vox_u32)match->rules.respawn_delay_ticks);
    hash = digs_hash_mix(hash, VOX_DIGS_MAP_GENERATOR_VERSION);
    hash = digs_hash_mix(hash, match->tick);
    hash = digs_hash_mix(hash, (vox_u32)match->phase);
    hash = digs_hash_mix(hash, (vox_u32)match->result_reason);
    hash = digs_hash_mix(hash, (vox_u32)match->result_draw);
    hash = digs_hash_mix(hash, (vox_u32)match->winner_player);
    hash = digs_hash_mix(hash, (vox_u32)match->speech_floor_ticks);
    hash = digs_hash_mix(hash, (vox_u32)match->speech_answer_ticks);
    hash = digs_hash_mix(hash, (vox_u32)match->speech_dry_ticks);
    hash = digs_hash_mix(hash,
                         (vox_u32)match->speech_exchange_lines);
    hash = digs_hash_mix(hash, (vox_u32)match->speech_last_line);
    hash = digs_hash_mix(hash,
                         (vox_u32)(vox_u16)match->speech_exchange_heat);
    hash = digs_hash_mix(hash, match->memory.memory_hash);
    hash = digs_hash_mix(hash, match->lava_level_q16);
    hash = digs_hash_mix(hash, (vox_u32)match->lava_surface_y);
    hash = digs_hash_mix(hash, (vox_u32)match->projectile_count);
    hash = digs_hash_mix(hash, (vox_u32)match->effect_count);
    hash = digs_hash_mix(hash, (vox_u32)match->effect_cursor);
    hash = digs_hash_mix(hash, match->terrain_hash);
    hash = digs_hash_mix(hash, vox_world_hash(&match->world));
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.gravity_q16);
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.max_speed_q16);
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.max_step_q16);
    hash = digs_hash_mix(hash, (vox_u32)match->physics_config.max_substeps);
    for (i = 0U; i < VOX_DIGS_MAX_SLOTS; ++i) {
        hash = digs_hash_mix(hash, (vox_u32)match->scores[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->alive[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->health[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->deaths[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->respawn_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->respawn_ready[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->respawn_requested[i]);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->respawn_target_x_q16[i]);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->respawn_target_y_q16[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->spawn_shield_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->player_actions[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->previous_actions[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->aim_x[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->aim_y[i]);
        hash = digs_hash_mix(hash, (vox_u32)(vox_i32)match->move_x_q15[i]);
        hash = digs_hash_mix(hash, (vox_u32)(vox_i32)match->move_y_q15[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->coyote_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->jump_buffer_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->jump_hold_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->steam_q16[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->weapon_cooldown[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->selected_weapon[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->facing_right[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->last_attacker[i]);
        hash = digs_hash_mix(hash, match->last_attacker_tick[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->last_damage_weapon[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->last_damage_part[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->rail_charge_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->rail_charging[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->weapon_charge_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->weapon_charging[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->bolt_shot_streak[i]);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->bleed_accumulator_q8[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->clot_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->buried_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->speech_stimulus[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->speech_subject[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->speech_delay[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->speech_cooldown[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->speech_urge_ticks[i]);
        hash = digs_hash_mix(hash, (vox_u32)match->speech_audience[i]);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->speech_self_stimulus[i]);
        hash = digs_hash_mix(hash, match->speech_self_tick[i]);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->speech_recent_cursor[i]);
        {
            vox_u16 slot;
            for (slot = 0U; slot < VOX_DIGS_SPEAKER_RECENT; ++slot) {
                hash = digs_hash_mix(hash,
                                     (vox_u32)match->speech_recent[i][slot]);
            }
        }
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].position_x.value_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].position_y.value_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].velocity_x.value_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].velocity_y.value_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].half_width_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->players[i].half_height_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->players[i].flags);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].anchor_x_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].anchor_y_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].length_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].tension_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].active);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].integrity);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].state);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].point_count);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].target_player);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].flags);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].hook_x_q16);
        hash = digs_hash_mix(hash, (vox_u32)match->ropes[i].hook_y_q16);
        hash = digs_hash_mix(hash,
            (vox_u32)match->ropes[i].hook_velocity_x_q16);
        hash = digs_hash_mix(hash,
            (vox_u32)match->ropes[i].hook_velocity_y_q16);
        hash = digs_hash_mix(hash,
            (vox_u32)match->ropes[i].hook_travel_q16);
        {
            vox_u16 point;
            for (point = 0U; point < VOX_DIGS_ROPE_MAX_POINTS; ++point) {
                const vox_digs_rope_point *rope_point =
                    &match->ropes[i].points[point];
                hash = digs_hash_mix(hash,
                    (vox_u32)rope_point->position_x_q16);
                hash = digs_hash_mix(hash,
                    (vox_u32)rope_point->position_y_q16);
                hash = digs_hash_mix(hash,
                    (vox_u32)rope_point->previous_x_q16);
                hash = digs_hash_mix(hash,
                    (vox_u32)rope_point->previous_y_q16);
            }
        }
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].mode);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].target);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].memory_ticks);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].state_ticks);
        hash = digs_hash_mix(hash,
                             (vox_u32)(vox_i32)match->bots[i].roam_direction);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].decision_ticks);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->bots[i].retreat_lock_ticks);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].roam_goal_x);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->bots[i].roam_goal_ticks);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].stuck_ticks);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->bots[i].breach_lock_ticks);
        hash = digs_hash_mix(hash, (vox_u32)match->bots[i].breach_ticks);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->bots[i].last_seen_x_q16);
        hash = digs_hash_mix(hash,
                             (vox_u32)match->bots[i].last_seen_y_q16);
        {
            vox_u16 part;
            for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
                const vox_digs_anatomy_part *anatomy =
                    &match->anatomy[i][part];
                hash = digs_hash_mix(hash, (vox_u32)anatomy->health);
                hash = digs_hash_mix(hash, (vox_u32)anatomy->max_health);
                hash = digs_hash_mix(hash, (vox_u32)anatomy->flags);
                hash = digs_hash_mix(hash,
                                     (vox_u32)anatomy->bleed_rate_q8);
            }
        }
    }
    for (i = 0U; i < VOX_DIGS_MAX_PAIRS; ++i) {
        const vox_digs_contract *contract = &match->contracts[i];
        vox_u16 recent;
        hash = digs_hash_mix(hash, (vox_u32)contract->tone);
        hash = digs_hash_mix(hash, (vox_u32)(vox_u16)contract->valence);
        hash = digs_hash_mix(hash, (vox_u32)contract->tone_ticks);
        hash = digs_hash_mix(hash, (vox_u32)contract->quiet_ticks);
        hash = digs_hash_mix(hash, (vox_u32)contract->last_speaker);
        hash = digs_hash_mix(hash, (vox_u32)contract->exchanges);
        hash = digs_hash_mix(hash, (vox_u32)contract->met);
        hash = digs_hash_mix(hash, (vox_u32)contract->recent_cursor);
        hash = digs_hash_mix(hash, (vox_u32)contract->last_stimulus);
        hash = digs_hash_mix(hash, (vox_u32)contract->last_actor);
        hash = digs_hash_mix(hash, contract->last_stimulus_tick);
        for (recent = 0U; recent < VOX_DIGS_RECENT_LINES; ++recent) {
            hash = digs_hash_mix(hash,
                                 (vox_u32)contract->recent_lines[recent]);
        }
    }
    for (i = 0U; i < VOX_DIGS_MAX_PROJECTILES; ++i) {
        const vox_digs_projectile *projectile = &match->projectiles[i];
        hash = digs_hash_mix(hash, (vox_u32)projectile->active);
        if (projectile->active) {
            hash = digs_hash_mix(hash, (vox_u32)projectile->position_x_q16);
            hash = digs_hash_mix(hash, (vox_u32)projectile->position_y_q16);
            hash = digs_hash_mix(hash, (vox_u32)projectile->velocity_x_q16);
            hash = digs_hash_mix(hash, (vox_u32)projectile->velocity_y_q16);
            hash = digs_hash_mix(hash,
                                 (vox_u32)projectile->launch_min_x_q16);
            hash = digs_hash_mix(hash,
                                 (vox_u32)projectile->launch_max_x_q16);
            hash = digs_hash_mix(hash,
                                 (vox_u32)projectile->launch_min_y_q16);
            hash = digs_hash_mix(hash,
                                 (vox_u32)projectile->launch_max_y_q16);
            hash = digs_hash_mix(hash, (vox_u32)projectile->owner);
            hash = digs_hash_mix(hash, (vox_u32)projectile->weapon);
            hash = digs_hash_mix(hash, (vox_u32)projectile->material);
            hash = digs_hash_mix(hash, (vox_u32)projectile->fuse_ticks);
            hash = digs_hash_mix(hash, (vox_u32)projectile->age_ticks);
            hash = digs_hash_mix(hash, (vox_u32)projectile->damage);
            hash = digs_hash_mix(hash, (vox_u32)projectile->blast_radius);
            hash = digs_hash_mix(hash, (vox_u32)projectile->owner_clear);
            hash = digs_hash_mix(hash, (vox_u32)projectile->arming_ticks);
        }
    }
    for (i = 0U; i < match->rules.fx_budget; ++i) {
        const vox_digs_effect *effect = &match->effects[i];
        hash = digs_hash_mix(hash, (vox_u32)effect->active);
        if (effect->active) {
            hash = digs_hash_mix(hash, (vox_u32)effect->position_x_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->position_y_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->velocity_x_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->velocity_y_q16);
            hash = digs_hash_mix(hash, (vox_u32)effect->material);
            hash = digs_hash_mix(hash, (vox_u32)effect->ttl_ticks);
            hash = digs_hash_mix(hash, (vox_u32)effect->variant);
            hash = digs_hash_mix(hash, (vox_u32)effect->source);
            hash = digs_hash_mix(hash, (vox_u32)effect->depth);
            hash = digs_hash_mix(hash, (vox_u32)effect->flags);
        }
    }
    return hash;
}

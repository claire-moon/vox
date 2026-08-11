/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_GAME_H
#define VOX_GAME_H

#include "vox_physics.h"
#include "vox_fluid.h"
#include "vox_rigid.h"
#include "vox_cluster.h"

/*
 * Three bots fill a four-slot match alongside a single human, which is the
 * point of the count: a solo player gets a full four-combatant deathmatch
 * without needing a second person at the keyboard.  Two humans still cap the
 * bots at two, because VOX_DIGS_MAX_SLOTS is the real ceiling.
 */
#define VOX_DIGS_MAX_BOTS 3U
#define VOX_DIGS_MAX_SLOTS 4U
#define VOX_DIGS_TICKS_PER_SECOND 60U
#define VOX_DIGS_MAP_GENERATOR_VERSION 4U

#define VOX_DIGS_ACTION_LEFT 1U
#define VOX_DIGS_ACTION_RIGHT 2U
#define VOX_DIGS_ACTION_JUMP 4U
#define VOX_DIGS_ACTION_STEAM 8U
#define VOX_DIGS_ACTION_ROPE 16U
#define VOX_DIGS_ACTION_FIRE 32U
/*
 * Speaking is an input like any other.  It has to travel the same path as
 * movement and fire, because what a miner says moves the contract state and
 * therefore the match hash -- a bark raised out of band would make a replay
 * diverge from the match it was recorded from.
 */
#define VOX_DIGS_ACTION_BARK 64U
#define VOX_DIGS_ACTION_DASH 128U
#define VOX_DIGS_ACTION_MASK (VOX_DIGS_ACTION_LEFT | VOX_DIGS_ACTION_RIGHT | \
                              VOX_DIGS_ACTION_JUMP | VOX_DIGS_ACTION_STEAM | \
                              VOX_DIGS_ACTION_ROPE | VOX_DIGS_ACTION_FIRE | \
                              VOX_DIGS_ACTION_BARK | VOX_DIGS_ACTION_DASH)

#define VOX_DIGS_MAX_HEALTH 100U
#define VOX_DIGS_RESPAWN_TICKS 180U
#define VOX_DIGS_SPAWN_SHIELD_TICKS 300U
#define VOX_DIGS_LAST_ATTACKER_TICKS 300U
/*
 * Projectiles leave a miner's muzzle inside the same fixed simulation tick
 * that their launch-volume clearance is resolved.  Keep the owner immunity
 * independent from that geometric clearance so a weapon cannot immediately
 * strike (or splash-damage) its shooter.  Forty-five ticks is 0.75 seconds
 * at the authoritative 60 Hz rate.
 */
#define VOX_DIGS_PROJECTILE_OWNER_CLEAR_TICKS 45U
#define VOX_DIGS_DASH_COOLDOWN_TICKS 45U
#define VOX_DIGS_DASH_INVULNERABILITY_TICKS 8U
#define VOX_DIGS_DASH_SPEED_Q16 (5L << 16)
#define VOX_DIGS_MAX_PROJECTILES 64U
#define VOX_DIGS_FX_RETRO 768U
#define VOX_DIGS_FX_STANDARD 1536U
#define VOX_DIGS_FX_CARNAGE 3072U
#define VOX_DIGS_MAX_EFFECTS VOX_DIGS_FX_CARNAGE
#define VOX_DIGS_MAX_EVENTS 128U
#define VOX_DIGS_ANATOMY_PART_COUNT 15U
#define VOX_DIGS_ROPE_MAX_POINTS 12U
#define VOX_DIGS_NO_PLAYER 65535U
#define VOX_DIGS_NO_PART 65535U
#define VOX_DIGS_ROPE_TARGET_TERRAIN 1U
#define VOX_DIGS_ROPE_TARGET_FIXTURE 2U
#define VOX_DIGS_ROPE_TARGET_SHIP 4U
#define VOX_DIGS_MAX_AWARDS 5U
#define VOX_DIGS_REPLAY_MAX_FRAMES 120U
#define VOX_DIGS_REPLAY_CAPTURE_STRIDE 4U
#define VOX_DIGS_REPLAY_WINDOW_RADIUS 6U
#define VOX_DIGS_REPLAY_WINDOW_DIAMETER \
    (VOX_DIGS_REPLAY_WINDOW_RADIUS * 2U + 1U)
#define VOX_DIGS_REPLAY_WINDOW_CELLS \
    (VOX_DIGS_REPLAY_WINDOW_DIAMETER * VOX_DIGS_REPLAY_WINDOW_DIAMETER)
#define VOX_DIGS_REPLAY_MAX_RIGIDS 24U
#define VOX_DIGS_REPLAY_MAX_FLUIDS 32U
#define VOX_DIGS_REPLAY_MAX_EFFECTS 24U
#define VOX_DIGS_REPLAY_MAX_EVENTS 12U
#define VOX_DIGS_DROP_SHIP_ROUTE_TICKS 360U
#define VOX_DIGS_DROPSHIP_PHASE_LAUNCH 0U
#define VOX_DIGS_DROPSHIP_PHASE_WAITING 1U
#define VOX_DIGS_DROPSHIP_PHASE_EXTRACTION 2U
#define VOX_DIGS_DROPSHIP_PHASE_DEPARTED 3U
#define VOX_DIGS_DROPSHIP_HALF_WIDTH_Q16 (5L << 16)
#define VOX_DIGS_DROPSHIP_HALF_HEIGHT_Q16 (1L << 16)
#define VOX_DIGS_DROPSHIP_BOARD_RADIUS_Q16 (6L << 16)
#define VOX_DIGS_DROPSHIP_LAUNCH_SPEED_Q16 (1L << 16)
#define VOX_DIGS_DROPSHIP_COLLISION_COOLDOWN_TICKS 12U
/* Keep the launch deck below the HUD-safe top edge while remaining above the
 * minimum authored landform surface and its fixture clearance. */
#define VOX_DIGS_DROPSHIP_CRUISE_Y_Q16 (42L << 16)
/* Keep the launch route in legal world coordinates.  Miners are staged on
 * the ship at the first simulation tick and auto-launch at the far endpoint
 * only if they have not fired for themselves. */
#define VOX_DIGS_DROPSHIP_LAUNCH_START_X_Q16 (32L << 16)
#define VOX_DIGS_DROPSHIP_LAUNCH_END_X_Q16 \
    (((vox_i32)VOX_WORLD_WIDTH - 32L) << 16)

#define VOX_DIGS_WEAPON_MELEE 1U
#define VOX_DIGS_WEAPON_PROJECTILE 2U
#define VOX_DIGS_WEAPON_EXPLOSIVE 4U
#define VOX_DIGS_WEAPON_DEPOSIT 8U
#define VOX_DIGS_WEAPON_GRAVITY 16U
#define VOX_DIGS_WEAPON_HITSCAN 32U
#define VOX_DIGS_WEAPON_PENETRATING 64U

#define VOX_DIGS_DAMAGE_BALLISTIC 1U
#define VOX_DIGS_DAMAGE_BLUNT 2U
#define VOX_DIGS_DAMAGE_EXPLOSIVE 4U
#define VOX_DIGS_DAMAGE_HEAT 8U
#define VOX_DIGS_DAMAGE_DROWNING 16U

#define VOX_DIGS_PART_VITAL 1U
#define VOX_DIGS_PART_LIMB 2U
#define VOX_DIGS_PART_SEVERED 4U
#define VOX_DIGS_PART_BLEEDING 8U
#define VOX_DIGS_PART_CAUTERIZED 16U

typedef enum vox_digs_phase {
    VOX_DIGS_SETUP = 0,
    VOX_DIGS_RUNNING = 1,
    VOX_DIGS_RESULTS = 2
} vox_digs_phase;

typedef enum vox_digs_map_style {
    VOX_DIGS_MAP_COAL_RIDGE = 0,
    VOX_DIGS_MAP_DEEPWORKS = 1,
    VOX_DIGS_MAP_FURNACE_YARD = 2,
    VOX_DIGS_MAP_COUNT = 3
} vox_digs_map_style;

typedef enum vox_digs_landform {
    VOX_DIGS_LANDFORM_ARCHIPELAGO = 0,
    VOX_DIGS_LANDFORM_CONTINENT = 1,
    VOX_DIGS_LANDFORM_TWIN_HILLS = 2,
    VOX_DIGS_LANDFORM_COUNT = 3
} vox_digs_landform;

typedef enum vox_digs_respawn_mode {
    VOX_DIGS_RESPAWN_AUTO = 0,
    VOX_DIGS_RESPAWN_ON_FIRE = 1
} vox_digs_respawn_mode;

typedef enum vox_digs_end_reason {
    VOX_DIGS_END_NONE = 0,
    VOX_DIGS_END_TIME = 1,
    VOX_DIGS_END_SCORE = 2
} vox_digs_end_reason;

typedef enum vox_digs_tool {
    VOX_DIGS_TOOL_PICK = 0,
    VOX_DIGS_TOOL_BLAST_CHARGE = 1,
    VOX_DIGS_TOOL_SMOKE_POT = 2,
    VOX_DIGS_TOOL_CINDER_FLASK = 3,
    VOX_DIGS_TOOL_PRESSURE_HOSE = 4,
    VOX_DIGS_TOOL_SLEDGE = 5,
    VOX_DIGS_TOOL_NAIL_GUN = 6,
    VOX_DIGS_TOOL_BOILER_SHOTGUN = 7,
    VOX_DIGS_TOOL_CONCUSSION_GRENADE = 8,
    VOX_DIGS_TOOL_NAIL_BOMB = 9,
    VOX_DIGS_TOOL_RAIL_GUN = 10,
    VOX_DIGS_TOOL_COUNT = 11
} vox_digs_tool;

/* Stable numeric slots with v0.0.4 gameplay names. */
#define VOX_DIGS_TOOL_PULASKI VOX_DIGS_TOOL_PICK
#define VOX_DIGS_TOOL_POPPER VOX_DIGS_TOOL_BLAST_CHARGE
#define VOX_DIGS_TOOL_SMOKER VOX_DIGS_TOOL_SMOKE_POT
#define VOX_DIGS_TOOL_HOT_RAIL VOX_DIGS_TOOL_CINDER_FLASK
#define VOX_DIGS_TOOL_HYDROSHOT VOX_DIGS_TOOL_PRESSURE_HOSE
#define VOX_DIGS_TOOL_GIANT_HAMMER VOX_DIGS_TOOL_SLEDGE
#define VOX_DIGS_TOOL_BOLT_ACTION VOX_DIGS_TOOL_NAIL_GUN
#define VOX_DIGS_TOOL_SCATTERBRAIN VOX_DIGS_TOOL_BOILER_SHOTGUN
#define VOX_DIGS_TOOL_FIRECRACKER VOX_DIGS_TOOL_CONCUSSION_GRENADE
#define VOX_DIGS_TOOL_BORE_DRILL VOX_DIGS_TOOL_NAIL_BOMB

typedef enum vox_digs_rope_state {
    VOX_DIGS_ROPE_IDLE = 0,
    VOX_DIGS_ROPE_CASTING = 1,
    VOX_DIGS_ROPE_ATTACHED = 2
} vox_digs_rope_state;

typedef enum vox_digs_anatomy_id {
    VOX_DIGS_PART_HEAD = 0,
    VOX_DIGS_PART_TORSO = 1,
    VOX_DIGS_PART_PELVIS = 2,
    VOX_DIGS_PART_LEFT_UPPER_ARM = 3,
    VOX_DIGS_PART_RIGHT_UPPER_ARM = 4,
    VOX_DIGS_PART_LEFT_FOREARM = 5,
    VOX_DIGS_PART_RIGHT_FOREARM = 6,
    VOX_DIGS_PART_LEFT_HAND = 7,
    VOX_DIGS_PART_RIGHT_HAND = 8,
    VOX_DIGS_PART_LEFT_THIGH = 9,
    VOX_DIGS_PART_RIGHT_THIGH = 10,
    VOX_DIGS_PART_LEFT_SHIN = 11,
    VOX_DIGS_PART_RIGHT_SHIN = 12,
    VOX_DIGS_PART_LEFT_FOOT = 13,
    VOX_DIGS_PART_RIGHT_FOOT = 14
} vox_digs_anatomy_id;

/*
 * The three hardcoded opponents.  A bot's archetype is derived from its
 * ordinal among the bots -- the number of bot_mask bits below its slot -- so
 * identity is stable, needs no extra rules field, and is already covered by
 * the hash through bot_mask.  When the save layer later supplies drifted
 * traits it can override the base table without disturbing this.
 */
typedef enum vox_digs_archetype {
    VOX_DIGS_ARCHETYPE_ENGINEER = 0,    /* RIVET  -- methodical, ranged */
    VOX_DIGS_ARCHETYPE_BERSERKER = 1,   /* CINDER -- closes and brawls */
    VOX_DIGS_ARCHETYPE_TRICKSTER = 2,   /* FLAMEY -- fire, traps, chaos */
    VOX_DIGS_ARCHETYPE_COUNT = 3
} vox_digs_archetype;

/*
 * Traits are 0..255 and each one maps to a decision the AI actually makes,
 * so a value can always be pointed at the behaviour it produces.
 */
typedef struct vox_digs_personality {
    vox_u16 aggression;    /* engage distance and retreat threshold */
    vox_u16 patience;      /* ticks between decisions; ambush willingness */
    vox_u16 caution;       /* weight given to hazards */
    vox_u16 grudge;        /* how long a target stays preferred */
    vox_u16 sociability;   /* bark rate and truce willingness */
    vox_u16 reserved;
} vox_digs_personality;

/*
 * How two miners currently stand with one another.
 *
 * Every unordered pair of slots carries one of these.  The story of a match
 * is what happens to these eight values, and they are what the bots read when
 * they decide who to shoot at and what to say about it.
 *
 * WARY and THAWING occupy the same band of feeling and differ only in which
 * direction it was reached from: coming down off a truce is wariness, coming
 * up out of a quarrel is a thaw.
 */
typedef enum vox_digs_tone {
    VOX_DIGS_TONE_FEUD = 0,
    VOX_DIGS_TONE_HOSTILE = 1,
    VOX_DIGS_TONE_NEEDLING = 2,
    VOX_DIGS_TONE_NEUTRAL = 3,
    VOX_DIGS_TONE_WARY = 4,
    VOX_DIGS_TONE_THAWING = 5,
    VOX_DIGS_TONE_TRUCE = 6,
    VOX_DIGS_TONE_BONDED = 7,
    VOX_DIGS_TONE_COUNT = 8
} vox_digs_tone;

/*
 * What just happened between two miners, from the point of view of whoever is
 * about to speak about it.
 *
 * Severity is expressed by *which* stimulus fires, not by scaling a number:
 * taking a leg off is its own entry rather than a large HURT.  That keeps the
 * valence table readable and means a line pool can be written per stimulus
 * without also having to cover a range of intensities.
 */
typedef enum vox_digs_stimulus {
    VOX_DIGS_STIMULUS_NONE = 0,
    VOX_DIGS_STIMULUS_FIRST_MEETING = 1,
    VOX_DIGS_STIMULUS_SPOTTED = 2,
    VOX_DIGS_STIMULUS_HURT_THEM = 3,
    VOX_DIGS_STIMULUS_HURT_BY = 4,
    VOX_DIGS_STIMULUS_NEAR_MISS = 5,
    VOX_DIGS_STIMULUS_LIMB_TAKEN = 6,
    VOX_DIGS_STIMULUS_LIMB_LOST = 7,
    VOX_DIGS_STIMULUS_KILLED_THEM = 8,
    VOX_DIGS_STIMULUS_KILLED_BY = 9,
    VOX_DIGS_STIMULUS_REVENGE = 10,
    VOX_DIGS_STIMULUS_HUMILIATED = 11,
    VOX_DIGS_STIMULUS_STREAK = 12,
    VOX_DIGS_STIMULUS_SAVED_BY = 13,
    VOX_DIGS_STIMULUS_TEAMED_UP = 14,
    VOX_DIGS_STIMULUS_BETRAYED = 15,
    VOX_DIGS_STIMULUS_TRUCE_OFFERED = 16,
    VOX_DIGS_STIMULUS_TRUCE_ACCEPTED = 17,
    VOX_DIGS_STIMULUS_TRUCE_BROKEN = 18,
    VOX_DIGS_STIMULUS_TAUNTED = 19,
    VOX_DIGS_STIMULUS_LAVA_CLOSE = 20,
    VOX_DIGS_STIMULUS_BURIED = 21,
    VOX_DIGS_STIMULUS_DOOMED = 22,
    VOX_DIGS_STIMULUS_LONG_ABSENCE = 23,
    VOX_DIGS_STIMULUS_MATCH_START = 24,
    VOX_DIGS_STIMULUS_MATCH_END = 25,
    VOX_DIGS_STIMULUS_IDLE = 26,
    /*
     * Player-authored context pools.  These are speech-selection state, not
     * new persistent identities or relationship pairs: a chronicle remains
     * format 1 and a match still has exactly four slots.
     */
    VOX_DIGS_STIMULUS_MOVE = 27,
    VOX_DIGS_STIMULUS_WEAPON = 28,
    VOX_DIGS_STIMULUS_MISS = 29,
    VOX_DIGS_STIMULUS_HIT = 30,
    VOX_DIGS_STIMULUS_NEAR_DEATH = 31,
    VOX_DIGS_STIMULUS_CAVE_IN = 32,
    VOX_DIGS_STIMULUS_GRAPPLE = 33,
    VOX_DIGS_STIMULUS_KILL = 34,
    VOX_DIGS_STIMULUS_HUMILIATION = 35,
    VOX_DIGS_STIMULUS_ESCAPE = 36,
    VOX_DIGS_STIMULUS_COUNT = 37
} vox_digs_stimulus;

/*
 * Who a line is aimed at.
 *
 * Without this every remark was addressed to exactly one miner, so three
 * bots muttering near each other produced three private conversations and
 * none of them acknowledged the others -- the "everyone in their own little
 * world" the playtest reported.
 */
typedef enum vox_digs_audience {
    VOX_DIGS_AUDIENCE_SELF = 0,   /* muttering; only close miners overhear */
    VOX_DIGS_AUDIENCE_ONE = 1,    /* aimed at the subject */
    VOX_DIGS_AUDIENCE_ALL = 2     /* said to the whole mine */
} vox_digs_audience;

/* Four slots choose two. */
#define VOX_DIGS_MAX_PAIRS 6U
/* How many lines a pair remembers, so it does not repeat itself immediately. */
/* How many lines a pair remembers, and how many a speaker remembers of its
 * own.  The pair ring alone let RIVET say the same thing to CINDER and then
 * to the player, with neither of them any the wiser. */
#define VOX_DIGS_RECENT_LINES 8U
#define VOX_DIGS_SPEAKER_RECENT 8U

typedef struct vox_digs_contract {
    vox_u16 tone;
    vox_i16 valence;          /* below zero is bad blood, above is warmth */
    vox_u16 tone_ticks;       /* dwell in the current tone */
    vox_u16 quiet_ticks;      /* since either of them last spoke to the other */
    vox_u16 last_speaker;     /* VOX_DIGS_NO_PLAYER until someone speaks */
    vox_u16 exchanges;        /* how much these two have talked this match */
    vox_u16 met;              /* have they actually laid eyes on each other */
    vox_u16 recent_lines[VOX_DIGS_RECENT_LINES];
    vox_u16 recent_cursor;
    vox_u16 last_stimulus;    /* freshest thing that happened between them */
    vox_u16 last_actor;       /* who did it; the other one is the subject */
    vox_u32 last_stimulus_tick;
} vox_digs_contract;

typedef enum vox_digs_ai_mode {
    VOX_DIGS_AI_ROAMING = 0,
    VOX_DIGS_AI_SEARCHING = 1,
    VOX_DIGS_AI_ATTACKING = 2,
    VOX_DIGS_AI_RETREATING = 3
} vox_digs_ai_mode;

typedef enum vox_digs_tunnel_state {
    VOX_DIGS_TUNNEL_NONE = 0,
    VOX_DIGS_TUNNEL_PLANNING = 1,
    VOX_DIGS_TUNNEL_EXCAVATING = 2,
    VOX_DIGS_TUNNEL_AMBUSH = 3,
    VOX_DIGS_TUNNEL_ESCAPE = 4,
    VOX_DIGS_TUNNEL_TRAP = 5,
    VOX_DIGS_TUNNEL_COLLAPSE_RISK = 6,
    VOX_DIGS_TUNNEL_DROWNING = 7,
    VOX_DIGS_TUNNEL_EXTRACTION = 8,
    VOX_DIGS_TUNNEL_RECOVERY = 9
} vox_digs_tunnel_state;

typedef enum vox_digs_event_type {
    VOX_DIGS_EVENT_NONE = 0,
    VOX_DIGS_EVENT_SPAWN = 1,
    VOX_DIGS_EVENT_SHIELD_END = 2,
    VOX_DIGS_EVENT_DAMAGE = 3,
    VOX_DIGS_EVENT_KILL = 4,
    VOX_DIGS_EVENT_WEAPON_FIRE = 5,
    VOX_DIGS_EVENT_EXPLOSION = 6,
    VOX_DIGS_EVENT_ROPE_ATTACH = 7,
    VOX_DIGS_EVENT_ROPE_DETACH = 8,
    VOX_DIGS_EVENT_ROPE_BREAK = 9,
    VOX_DIGS_EVENT_AI_STATE = 10,
    VOX_DIGS_EVENT_AI_BARK = 11,
    VOX_DIGS_EVENT_LIMB_SEVER = 12,
    VOX_DIGS_EVENT_BLEED = 13,
    VOX_DIGS_EVENT_RESPAWN_READY = 14,
    VOX_DIGS_EVENT_MATCH_END = 15,
    VOX_DIGS_EVENT_SHIELD_BLOCK = 16,
    VOX_DIGS_EVENT_ROPE_CAST = 17,
    VOX_DIGS_EVENT_ROPE_HIT = 18,
    VOX_DIGS_EVENT_RAIL_CHARGE = 19,
    VOX_DIGS_EVENT_RAIL_TRACE = 20,
    VOX_DIGS_EVENT_CRUSH = 21,
    VOX_DIGS_EVENT_DASH = 22,
    VOX_DIGS_EVENT_HEADSHOT = 23,
    VOX_DIGS_EVENT_AWARD = 24,
    VOX_DIGS_EVENT_REPLAY_SELECT = 25,
    VOX_DIGS_EVENT_SHIP_LAUNCH = 26,
    VOX_DIGS_EVENT_SHIP_COLLISION = 27,
    VOX_DIGS_EVENT_SHIP_GRAPPLE = 28,
    VOX_DIGS_EVENT_SHIP_EXTRACT = 29,
    VOX_DIGS_EVENT_SHIP_ALARM = 30,
    VOX_DIGS_EVENT_SHIP_SPLATTER = 31,
    VOX_DIGS_EVENT_CAVE_IN = 32,
    VOX_DIGS_EVENT_DEBRIS_IMPACT = 33,
    VOX_DIGS_EVENT_FIXTURE_BREAK = 34,
    VOX_DIGS_EVENT_DROWN = 35
} vox_digs_event_type;

typedef enum vox_digs_award_id {
    VOX_DIGS_AWARD_PYROMANIAC = 0,
    VOX_DIGS_AWARD_GRAVE_DIGGER = 1,
    VOX_DIGS_AWARD_HEADHUNTER = 2,
    VOX_DIGS_AWARD_CAVE_IN_ARTIST = 3,
    VOX_DIGS_AWARD_EXTRACTIONIST = 4
} vox_digs_award_id;

/*
 * A replay frame is render-only state.  It deliberately contains bounded
 * copies rather than pointers into the live match, so the results screen can
 * inspect it after the authoritative match has ended without mutating or
 * replaying the simulation. The window is the front side-view plane used by
 * the current software renderer; fluids retain their real depth.
 */
typedef struct vox_digs_replay_rigid {
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_i32 velocity_x_q16;
    vox_i32 velocity_y_q16;
    vox_i32 angle_q16;
    vox_i32 angular_velocity_q16;
    vox_i32 half_width_q16;
    vox_i32 half_height_q16;
    vox_u16 flags;
    vox_u16 reserved;
} vox_digs_replay_rigid;

typedef struct vox_digs_replay_fluid {
    vox_u16 x;
    vox_u16 y;
    vox_u16 z;
    vox_u16 material;
    vox_i32 volume_q16;
    vox_i32 temperature_q16;
    vox_i32 flow_q16;
} vox_digs_replay_fluid;

typedef struct vox_digs_replay_effect {
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_i32 velocity_x_q16;
    vox_i32 velocity_y_q16;
    vox_u16 material;
    vox_u16 ttl_ticks;
    vox_u16 variant;
    vox_u16 source;
    vox_u16 depth;
    vox_u16 flags;
} vox_digs_replay_effect;

typedef struct vox_digs_replay_event {
    vox_u32 sequence;
    vox_u32 tick;
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_u16 type;
    vox_u16 source;
    vox_u16 target;
    vox_u16 weapon;
    vox_u16 material;
    vox_u16 magnitude;
    vox_u16 variant;
    vox_u16 reserved;
} vox_digs_replay_event;

typedef struct vox_digs_replay_frame {
    vox_u32 tick;
    vox_i32 player_x_q16[VOX_DIGS_MAX_SLOTS];
    vox_i32 player_y_q16[VOX_DIGS_MAX_SLOTS];
    vox_u16 player_alive[VOX_DIGS_MAX_SLOTS];
    vox_u16 player_health[VOX_DIGS_MAX_SLOTS];
    vox_u32 fluid_hash;
    vox_u32 rigid_hash;
    vox_u32 event_sequence;
    vox_u16 terrain_origin_x;
    vox_u16 terrain_origin_y;
    vox_u16 terrain_width;
    vox_u16 terrain_height;
    vox_i32 camera_x_q16;
    vox_i32 camera_y_q16;
    vox_i32 camera_zoom_q16;
    vox_u16 terrain_material[VOX_DIGS_REPLAY_WINDOW_CELLS];
    vox_u16 rigid_count;
    vox_u16 fluid_count;
    vox_u16 effect_count;
    vox_u16 event_count;
    vox_digs_replay_rigid rigids[VOX_DIGS_REPLAY_MAX_RIGIDS];
    vox_digs_replay_fluid fluids[VOX_DIGS_REPLAY_MAX_FLUIDS];
    vox_digs_replay_effect effects[VOX_DIGS_REPLAY_MAX_EFFECTS];
    vox_digs_replay_event events[VOX_DIGS_REPLAY_MAX_EVENTS];
} vox_digs_replay_frame;

typedef struct vox_digs_replay_ledger {
    vox_u16 active;
    vox_u16 frame_count;
    vox_u16 play_cursor;
    vox_u16 capture_cursor;
    vox_u16 killer;
    vox_u16 victim;
    vox_u16 headshot;
    vox_u16 multi_kill;
    vox_u16 award_value;
    vox_u16 kill_streak;
    vox_u16 cave_in_scale;
    /* Distance in cells to the nearest selected blast, or 65535 when the
     * kill did not have a bounded recent blast context. */
    vox_u16 blast_distance;
    vox_u16 reserved;
    vox_u32 selected_tick;
    vox_u32 seed;
    vox_digs_replay_frame frames[VOX_DIGS_REPLAY_MAX_FRAMES];
} vox_digs_replay_ledger;

typedef struct vox_digs_dropship {
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_i32 previous_position_x_q16;
    vox_i32 velocity_x_q16;
    vox_u16 phase;
    vox_u16 route_ticks;
    vox_u16 launched_mask;
    vox_u16 extracted_mask;
    vox_u16 alarmed;
    vox_u16 collision_cooldown;
} vox_digs_dropship;

typedef struct vox_digs_weapon_properties {
    const char *name;
    vox_u16 cooldown_ticks;
    vox_u16 damage;
    vox_u16 blast_radius;
    vox_u16 projectile_speed_q8;
    vox_u16 fuse_ticks;
    vox_u16 flags;
    vox_u16 charge_ticks;
    vox_u16 penetration;
} vox_digs_weapon_properties;

typedef struct vox_digs_projectile {
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_i32 velocity_x_q16;
    vox_i32 velocity_y_q16;
    vox_i32 launch_min_x_q16;
    vox_i32 launch_max_x_q16;
    vox_i32 launch_min_y_q16;
    vox_i32 launch_max_y_q16;
    vox_u16 active;
    vox_u16 owner;
    vox_u16 weapon;
    vox_u16 material;
    vox_u16 fuse_ticks;
    vox_u16 age_ticks;
    vox_u16 damage;
    vox_u16 blast_radius;
    vox_u16 owner_clear;
    vox_u16 arming_ticks;
} vox_digs_projectile;

typedef struct vox_digs_effect {
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_i32 velocity_x_q16;
    vox_i32 velocity_y_q16;
    vox_u16 active;
    vox_u16 material;
    vox_u16 ttl_ticks;
    vox_u16 variant;
    vox_u16 source;
    vox_u16 depth;
    vox_u16 flags;
} vox_digs_effect;

typedef struct vox_digs_anatomy_part {
    vox_u16 health;
    vox_u16 max_health;
    vox_u16 flags;
    vox_u16 bleed_rate_q8;
} vox_digs_anatomy_part;

typedef struct vox_digs_hurtbox {
    vox_i32 offset_x_q16;
    vox_i32 offset_y_q16;
    vox_i32 half_width_q16;
    vox_i32 half_height_q16;
    vox_u16 part;
    vox_u16 reserved;
} vox_digs_hurtbox;

typedef struct vox_digs_rope_point {
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_i32 previous_x_q16;
    vox_i32 previous_y_q16;
} vox_digs_rope_point;

typedef struct vox_digs_rope {
    vox_i32 anchor_x_q16;
    vox_i32 anchor_y_q16;
    vox_i32 length_q16;
    vox_i32 tension_q16;
    vox_i32 hook_x_q16;
    vox_i32 hook_y_q16;
    vox_i32 hook_velocity_x_q16;
    vox_i32 hook_velocity_y_q16;
    vox_i32 hook_travel_q16;
    vox_digs_rope_point points[VOX_DIGS_ROPE_MAX_POINTS];
    vox_u16 active;
    vox_u16 integrity;
    vox_u16 state;
    vox_u16 point_count;
    vox_u16 target_player;
    vox_u16 flags;
    vox_u16 target_x;
    vox_u16 target_y;
    vox_u16 retarget_cursor;
    vox_u16 reserved;
} vox_digs_rope;

typedef struct vox_digs_ai_state {
    vox_u16 mode;
    vox_u16 target;
    vox_u16 memory_ticks;
    vox_u16 state_ticks;
    vox_i16 roam_direction;
    vox_u16 decision_ticks;
    /*
     * Refractory period after a retreat ends, during which the miner will not
     * retreat again.  Nothing in the simulation heals a living miner, so
     * without this a bot that once dropped below its threshold would retreat
     * for the rest of its life: the entry condition stays true forever.
     */
    vox_u16 retreat_lock_ticks;
    /*
     * Where a roaming miner is actually walking to, and how long it has left
     * to get there.  Roaming used to be a wall-to-wall ping-pong with no
     * destination, which is why bots spent three quarters of a match
     * wandering and almost never found one another.
     */
    vox_u16 roam_goal_x;
    vox_u16 roam_goal_ticks;
    /*
     * Consecutive ticks pinned against terrain that stands between this miner
     * and where it wants to be, and ticks spent boring through it.  A miner
     * that cannot walk around an obstacle digs through it instead -- which is
     * the whole of RIVET's tunneller identity and the reason a bot no longer
     * paces at a wall forever.
     */
    vox_u16 stuck_ticks;
    vox_u16 breach_lock_ticks;
    vox_u16 breach_ticks;
    vox_i32 last_seen_x_q16;
    vox_i32 last_seen_y_q16;
    vox_u16 tunnel_state;
    vox_u16 tunnel_safety_q8;
    vox_u16 collapse_risk_q8;
    vox_u16 extraction_ticks;
} vox_digs_ai_state;

typedef struct vox_digs_event {
    vox_u32 sequence;
    vox_u32 tick;
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_u16 type;
    vox_u16 source;
    vox_u16 target;
    vox_u16 weapon;
    vox_u16 material;
    vox_u16 magnitude;
    vox_u16 variant;
    vox_u16 reserved;
} vox_digs_event;

typedef struct vox_digs_rules {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 match_ticks;
    vox_u32 score_limit;
    vox_u32 lava_start_tick;
    vox_u32 seed;
    vox_u16 player_count;
    vox_u16 bot_mask;
    vox_u16 map_style;
    vox_u16 weapon_mask;
    vox_u16 fx_budget;
    vox_u16 respawn_mode;
    vox_u16 respawn_delay_ticks;
    vox_u16 reserved;
} vox_digs_rules;

typedef struct vox_digs_input {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u16 player;
    vox_u16 actions;
    vox_u16 aim_x;
    vox_u16 aim_y;
    vox_i16 move_x_q15;
    vox_i16 move_y_q15;
    vox_u16 selected_weapon;
    vox_u16 reserved;
} vox_digs_input;

/*
 * What the miners carry between matches.
 *
 * This is a hashed *input*, not part of the match: a match stays reproducible
 * from (seed + rules + snapshot), and the tests pin a canonical snapshot so
 * golden hashes do not move every time somebody plays a game.
 *
 * Everything here is keyed by identity rather than slot.  RIVET is RIVET
 * whichever slot he spawns in, and "the bots remember you" only means
 * anything if the account they keep follows the person and not the seat.
 */
#define VOX_DIGS_MEMORY_VERSION 1U
#define VOX_DIGS_IDENTITY_RIVET 0U
#define VOX_DIGS_IDENTITY_CINDER 1U
#define VOX_DIGS_IDENTITY_FLAMEY 2U
#define VOX_DIGS_IDENTITY_PLAYER 3U
#define VOX_DIGS_IDENTITY_COUNT 4U

/* How two identities left things last time. */
typedef struct vox_digs_regard {
    vox_u16 tone;
    vox_i16 valence;
    vox_u16 matches_met;
    vox_u16 kills_for;      /* the lower identity killed the higher */
    vox_u16 kills_against;
    vox_u16 truces;
    vox_u16 betrayals;
    vox_u16 reserved;
} vox_digs_regard;

typedef struct vox_digs_identity_record {
    vox_digs_personality traits;   /* drifted away from the archetype base */
    vox_u16 matches_played;
    vox_u16 wins;
    vox_u16 kills;
    vox_u16 deaths;
    vox_u16 reserved;
} vox_digs_identity_record;

typedef struct vox_digs_bot_memory {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 memory_version;
    /*
     * Both set by the port from a wall clock, and neither read by the
     * simulation -- so neither is folded into memory_hash, because that
     * digest ends up inside the authoritative match hash.
     */
    vox_u32 launch_counter;
    vox_u32 elapsed_coarse;      /* whole hours since the last launch */
    vox_digs_identity_record identities[VOX_DIGS_IDENTITY_COUNT];
    vox_digs_regard regard[VOX_DIGS_MAX_PAIRS];
    vox_u32 memory_hash;
} vox_digs_bot_memory;

typedef struct vox_digs_match {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_digs_rules rules;
    vox_world world;
    vox_fluid_world fluids;
    vox_rigid_world ragdolls;
    vox_structure_state structure;
    /* Gameplay provenance and compact material accounting for the generic
     * rigid pool.  The physics library deliberately does not know about
     * players, weapons, or terrain cells, but a detached terrain body still
     * needs deterministic damage attribution and a bounded settling policy.
     * A cooldown prevents a resting slab from dealing damage every solver
     * tick; loose_cells records the material represented by a debris body. */
    vox_u16 rigid_source[VOX_RIGID_MAX_BODIES];
    vox_u16 rigid_weapon[VOX_RIGID_MAX_BODIES];
    vox_u16 rigid_impact_cooldown[VOX_RIGID_MAX_BODIES];
    vox_u16 rigid_material[VOX_RIGID_MAX_BODIES];
    vox_u16 rigid_loose_cells[VOX_RIGID_MAX_BODIES];
    /* Cells that could not be reintroduced under the fixed settling cap.
     * This makes expiration observable and hashable rather than silent. */
    vox_u32 rigid_settle_discarded;
    vox_u32 tick;
    vox_u32 state_hash;
    vox_digs_phase phase;
    vox_u16 result_reason;
    vox_u16 result_draw;
    vox_u16 winner_player;
    vox_u16 scores[VOX_DIGS_MAX_SLOTS];
    vox_u16 alive[VOX_DIGS_MAX_SLOTS];
    vox_u16 health[VOX_DIGS_MAX_SLOTS];
    vox_u16 deaths[VOX_DIGS_MAX_SLOTS];
    vox_u16 respawn_ticks[VOX_DIGS_MAX_SLOTS];
    vox_u16 respawn_ready[VOX_DIGS_MAX_SLOTS];
    vox_u16 respawn_requested[VOX_DIGS_MAX_SLOTS];
    vox_i32 respawn_target_x_q16[VOX_DIGS_MAX_SLOTS];
    vox_i32 respawn_target_y_q16[VOX_DIGS_MAX_SLOTS];
    vox_u16 spawn_shield_ticks[VOX_DIGS_MAX_SLOTS];
    vox_u16 player_actions[VOX_DIGS_MAX_SLOTS];
    vox_u16 previous_actions[VOX_DIGS_MAX_SLOTS];
    vox_u16 aim_x[VOX_DIGS_MAX_SLOTS];
    vox_u16 aim_y[VOX_DIGS_MAX_SLOTS];
    vox_i16 move_x_q15[VOX_DIGS_MAX_SLOTS];
    vox_i16 move_y_q15[VOX_DIGS_MAX_SLOTS];
    vox_u16 coyote_ticks[VOX_DIGS_MAX_SLOTS];
    vox_u16 jump_buffer_ticks[VOX_DIGS_MAX_SLOTS];
    vox_u16 jump_hold_ticks[VOX_DIGS_MAX_SLOTS];
    vox_u16 dash_cooldown[VOX_DIGS_MAX_SLOTS];
    vox_u16 dash_invulnerability[VOX_DIGS_MAX_SLOTS];
    vox_u16 steam_q16[VOX_DIGS_MAX_SLOTS];
    vox_u16 weapon_cooldown[VOX_DIGS_MAX_SLOTS];
    vox_u16 selected_weapon[VOX_DIGS_MAX_SLOTS];
    vox_u16 facing_right[VOX_DIGS_MAX_SLOTS];
    vox_u16 last_attacker[VOX_DIGS_MAX_SLOTS];
    vox_u32 last_attacker_tick[VOX_DIGS_MAX_SLOTS];
    vox_u16 last_damage_weapon[VOX_DIGS_MAX_SLOTS];
    vox_u16 last_damage_part[VOX_DIGS_MAX_SLOTS];
    vox_u16 rail_charge_ticks[VOX_DIGS_MAX_SLOTS];
    vox_u16 rail_charging[VOX_DIGS_MAX_SLOTS];
    /* Generic held-tool presentation and release state. */
    vox_u16 weapon_charge_ticks[VOX_DIGS_MAX_SLOTS];
    vox_u16 weapon_charging[VOX_DIGS_MAX_SLOTS];
    vox_u16 bolt_shot_streak[VOX_DIGS_MAX_SLOTS];
    vox_u16 bleed_accumulator_q8[VOX_DIGS_MAX_SLOTS];
    vox_u16 clot_ticks[VOX_DIGS_MAX_SLOTS];
    /*
     * Consecutive ticks this miner has been unable to resolve out of solid
     * terrain.  Burial is a survivable emergency rather than an instant
     * death: the miner keeps acting and can dig free.  Reset the moment
     * physics resolves normally.
     */
    vox_u16 buried_ticks[VOX_DIGS_MAX_SLOTS];
    /*
     * What this miner is about to say, and how long until they say it.  The
     * delay is the whole point: a conversation is an exchange with a pause in
     * it, and the length of the pause is a character trait.
     */
    vox_u16 speech_stimulus[VOX_DIGS_MAX_SLOTS];
    vox_u16 speech_subject[VOX_DIGS_MAX_SLOTS];
    vox_u16 speech_delay[VOX_DIGS_MAX_SLOTS];
    vox_u16 speech_cooldown[VOX_DIGS_MAX_SLOTS];
    /*
     * The urge to say something: a clock that always runs down, and a dice
     * roll when it lands.  Replacing the old fixed metronome is what makes
     * the talking irregular rather than clockwork, and scaling the roll by
     * sociability is what makes one of them chattier than the others.
     */
    vox_u16 speech_urge_ticks[VOX_DIGS_MAX_SLOTS];
    vox_u16 speech_audience[VOX_DIGS_MAX_SLOTS];
    /* What this speaker has said lately, regardless of who it was said to. */
    vox_u16 speech_recent[VOX_DIGS_MAX_SLOTS][VOX_DIGS_SPEAKER_RECENT];
    vox_u16 speech_recent_cursor[VOX_DIGS_MAX_SLOTS];
    /* Open briefly after the player speaks: everyone is likelier to answer. */
    vox_u16 speech_answer_ticks;
    /* How long the mine has been silent, so it cannot stay silent forever. */
    vox_u16 speech_dry_ticks;
    /* Lines in the exchange currently running, so it can be brought to a
     * close rather than running until everybody happens to shut up. */
    vox_u16 speech_exchange_lines;
    /* The line just spoken, so a reply can be priced against how long it
     * takes to say rather than answering everything at the same speed. */
    vox_u16 speech_last_line;
    /* Which way the exchange currently running is going: below zero it is
     * turning into a row, above zero it is settling down. */
    vox_i16 speech_exchange_heat;
    /* What each miner last said, so their own words are context for their
     * next ones -- which is what lets somebody alone hold a train of
     * thought instead of firing unrelated remarks. */
    vox_u16 speech_self_stimulus[VOX_DIGS_MAX_SLOTS];
    vox_u32 speech_self_tick[VOX_DIGS_MAX_SLOTS];
    /*
     * Nobody talks over anybody.  One miner speaks at a time and the rest
     * wait their turn, so the battlefield carries a conversation rather than
     * four simultaneous monologues.
     */
    vox_u16 speech_floor_ticks;
    vox_digs_bot_memory memory;   /* what they walked in remembering */
    vox_digs_contract contracts[VOX_DIGS_MAX_PAIRS];
    vox_u16 awards[VOX_DIGS_MAX_SLOTS];
    vox_u16 award_value[VOX_DIGS_MAX_SLOTS];
    vox_digs_replay_ledger replay;
    vox_digs_dropship dropship;
    vox_u32 lava_level_q16;
    vox_u16 lava_surface_y;
    vox_u16 projectile_count;
    vox_u16 effect_count;
    vox_u16 effect_cursor;
    vox_u16 event_head;
    vox_u16 event_count;
    vox_u32 event_sequence;
    vox_u32 terrain_hash;
    vox_physics_step_config physics_config;
    vox_physics_body players[VOX_DIGS_MAX_SLOTS];
    vox_digs_rope ropes[VOX_DIGS_MAX_SLOTS];
    vox_digs_ai_state bots[VOX_DIGS_MAX_SLOTS];
    vox_digs_anatomy_part anatomy[VOX_DIGS_MAX_SLOTS]
                                        [VOX_DIGS_ANATOMY_PART_COUNT];
    vox_digs_projectile projectiles[VOX_DIGS_MAX_PROJECTILES];
    vox_digs_effect effects[VOX_DIGS_MAX_EFFECTS];
    vox_digs_event events[VOX_DIGS_MAX_EVENTS];
} vox_digs_match;

#ifdef __cplusplus
extern "C" {
#endif

void vox_digs_rules_classic(vox_digs_rules *rules);
int vox_digs_player_is_active(const vox_digs_match *match, vox_u16 player);
int vox_digs_player_is_bot(const vox_digs_match *match, vox_u16 player);
vox_u16 vox_digs_map_landform(vox_u16 map_style, vox_u32 seed);
vox_result vox_digs_generate_map(vox_world *world, vox_u16 map_style,
                                 vox_u32 seed);
/* Fill a snapshot with the canonical starting state: no history at all. */
void vox_digs_memory_init(vox_digs_bot_memory *memory);
/*
 * Recompute and store memory_hash: a digest of everything in the snapshot
 * that the simulation can actually see.  The wall-clock fields are excluded
 * on purpose -- this digest is folded into vox_digs_hash, and the match hash
 * has to be a function of the simulation alone.
 */
vox_u32 vox_digs_memory_hash(vox_digs_bot_memory *memory);
/* Which identity a slot is playing.  VOX_DIGS_IDENTITY_COUNT if invalid. */
vox_u16 vox_digs_memory_identity(const vox_digs_match *match, vox_u16 player);
/* Index into regard[] for two identities; VOX_DIGS_MAX_PAIRS if invalid. */
vox_u16 vox_digs_regard_index(vox_u16 a, vox_u16 b);

vox_result vox_digs_match_init(vox_digs_match *match,
                               const vox_digs_rules *rules);
/*
 * As vox_digs_match_init, but seeded with what the miners remember.  The
 * plain init delegates here with a canonical snapshot, which is why every
 * existing caller and the Win32 port needed no change at all.
 */
vox_result vox_digs_match_init_ex(vox_digs_match *match,
                                  const vox_digs_rules *rules,
                                  const vox_digs_bot_memory *memory);
/* The snapshot as it stands now, with drift applied.  Safe mid-match. */
vox_result vox_digs_match_export_memory(const vox_digs_match *match,
                                        vox_digs_bot_memory *memory);
vox_result vox_digs_match_step(vox_digs_match *match);
vox_result vox_digs_request_respawn(vox_digs_match *match,
                                    vox_u16 player);
vox_result vox_digs_record_kill(vox_digs_match *match, vox_u16 killer,
                                vox_u16 victim);
/* Index of the contract between two slots; VOX_DIGS_MAX_PAIRS if invalid. */
vox_u16 vox_digs_pair_index(vox_u16 a, vox_u16 b);
const vox_digs_contract *vox_digs_contract_get(const vox_digs_match *match,
                                               vox_u16 a, vox_u16 b);
const char *vox_digs_tone_name(vox_u16 tone);
const char *vox_digs_stimulus_name(vox_u16 stimulus);
/*
 * How long a line occupies the room: the time to say it plus a margin to
 * read it by.  The simulation prices replies against this and the port shows
 * the bubble for exactly this, so "they cut me off" means the same thing to
 * both of them.  Two copies of the arithmetic would drift on the first
 * tuning pass.
 */
vox_u16 vox_digs_speech_duration(vox_u16 line_id);
vox_result vox_digs_submit_input(vox_digs_match *match,
                                 const vox_digs_input *input);
vox_result vox_digs_use_tool(vox_digs_match *match, vox_u16 player,
                             vox_u16 tool, vox_u32 x, vox_u32 y, vox_u32 z);
const vox_digs_weapon_properties *vox_digs_weapon_get(vox_u16 weapon);
vox_result vox_digs_anatomy_hurtbox(vox_u16 part,
                                    vox_digs_hurtbox *hurtbox);
vox_result vox_digs_fire_weapon(vox_digs_match *match, vox_u16 player,
                                vox_u16 weapon, vox_u32 target_x,
                                vox_u32 target_y);
vox_result vox_digs_apply_damage(vox_digs_match *match, vox_u16 attacker,
                                 vox_u16 victim, vox_u16 damage);
vox_result vox_digs_apply_hit(vox_digs_match *match, vox_u16 attacker,
                              vox_u16 victim, vox_u16 weapon,
                              vox_u16 part, vox_u16 damage,
                              vox_u16 damage_flags);
vox_result vox_digs_award_note(vox_digs_match *match, vox_u16 player,
                               vox_u16 award, vox_u16 value);
/* match_init leaves the virtual hull departed.  Stage every active miner on
 * the launch ship with this call before the first interactive match tick; the
 * ground spawn retained by match_init remains the deterministic respawn
 * target. */
vox_result vox_digs_dropship_begin(vox_digs_match *match);
vox_result vox_digs_dropship_launch(vox_digs_match *match, vox_u16 player);
vox_result vox_digs_dropship_board(vox_digs_match *match, vox_u16 player);
vox_result vox_digs_dropship_grapple(vox_digs_match *match, vox_u16 player);
vox_result vox_digs_dropship_step(vox_digs_match *match);
vox_result vox_digs_replay_step(vox_digs_match *match,
                                vox_digs_replay_frame *frame);
vox_result vox_digs_bot_think(vox_digs_match *match, vox_u16 player);
/* VOX_DIGS_ARCHETYPE_COUNT for a slot that is not a bot. */
vox_u16 vox_digs_bot_archetype(const vox_digs_match *match, vox_u16 player);
const vox_digs_personality *vox_digs_personality_get(vox_u16 archetype);
const char *vox_digs_archetype_name(vox_u16 archetype);
const vox_digs_event *vox_digs_event_get(const vox_digs_match *match,
                                         vox_u16 ordinal);
vox_result vox_digs_consume_events(vox_digs_match *match, vox_u16 count);
vox_u32 vox_digs_hash(const vox_digs_match *match);

#ifdef __cplusplus
}
#endif

#endif

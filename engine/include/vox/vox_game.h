/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_GAME_H
#define VOX_GAME_H

#include "vox_physics.h"

#define VOX_DIGS_MAX_BOTS 2U
#define VOX_DIGS_MAX_SLOTS 4U
#define VOX_DIGS_TICKS_PER_SECOND 60U
#define VOX_DIGS_MAP_GENERATOR_VERSION 4U

#define VOX_DIGS_ACTION_LEFT 1U
#define VOX_DIGS_ACTION_RIGHT 2U
#define VOX_DIGS_ACTION_JUMP 4U
#define VOX_DIGS_ACTION_STEAM 8U
#define VOX_DIGS_ACTION_ROPE 16U
#define VOX_DIGS_ACTION_FIRE 32U
#define VOX_DIGS_ACTION_MASK (VOX_DIGS_ACTION_LEFT | VOX_DIGS_ACTION_RIGHT | \
                              VOX_DIGS_ACTION_JUMP | VOX_DIGS_ACTION_STEAM | \
                              VOX_DIGS_ACTION_ROPE | VOX_DIGS_ACTION_FIRE)

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
#define VOX_DIGS_MAX_PROJECTILES 64U
#define VOX_DIGS_FX_RETRO 768U
#define VOX_DIGS_FX_STANDARD 1536U
#define VOX_DIGS_FX_CARNAGE 3072U
#define VOX_DIGS_MAX_EFFECTS VOX_DIGS_FX_CARNAGE
#define VOX_DIGS_MAX_EVENTS 128U
#define VOX_DIGS_ANATOMY_PART_COUNT 15U
#define VOX_DIGS_ROPE_MAX_POINTS 12U
#define VOX_DIGS_NO_PLAYER 65535U
#define VOX_DIGS_NO_TEAM 65535U
#define VOX_DIGS_NO_PART 65535U

#define VOX_DIGS_TEAM_MINERS 0U
#define VOX_DIGS_TEAM_MACHINES 1U

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

typedef enum vox_digs_team_mode {
    VOX_DIGS_MODE_FFA = 0,
    VOX_DIGS_MODE_MINERS_VS_MACHINES = 1
} vox_digs_team_mode;

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

typedef enum vox_digs_ai_mode {
    VOX_DIGS_AI_ROAMING = 0,
    VOX_DIGS_AI_SEARCHING = 1,
    VOX_DIGS_AI_ATTACKING = 2,
    VOX_DIGS_AI_RETREATING = 3
} vox_digs_ai_mode;

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
    VOX_DIGS_EVENT_CRUSH = 21
} vox_digs_event_type;

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
} vox_digs_rope;

typedef struct vox_digs_ai_state {
    vox_u16 mode;
    vox_u16 target;
    vox_u16 memory_ticks;
    vox_u16 state_ticks;
    vox_i16 roam_direction;
    vox_u16 decision_ticks;
    vox_i32 last_seen_x_q16;
    vox_i32 last_seen_y_q16;
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
    vox_u16 team_mode;
    vox_u16 map_style;
    vox_u16 weapon_mask;
    vox_u16 fx_budget;
    vox_u16 friendly_fire;
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

typedef struct vox_digs_match {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_digs_rules rules;
    vox_world world;
    vox_u32 tick;
    vox_u32 state_hash;
    vox_digs_phase phase;
    vox_u16 result_reason;
    vox_u16 result_draw;
    vox_u16 winner_player;
    vox_u16 winner_team;
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
vox_result vox_digs_match_init(vox_digs_match *match,
                               const vox_digs_rules *rules);
vox_result vox_digs_match_step(vox_digs_match *match);
vox_result vox_digs_request_respawn(vox_digs_match *match,
                                    vox_u16 player);
vox_result vox_digs_record_kill(vox_digs_match *match, vox_u16 killer,
                                vox_u16 victim);
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
vox_result vox_digs_bot_think(vox_digs_match *match, vox_u16 player);
const vox_digs_event *vox_digs_event_get(const vox_digs_match *match,
                                         vox_u16 ordinal);
vox_result vox_digs_consume_events(vox_digs_match *match, vox_u16 count);
vox_u32 vox_digs_hash(const vox_digs_match *match);

#ifdef __cplusplus
}
#endif

#endif

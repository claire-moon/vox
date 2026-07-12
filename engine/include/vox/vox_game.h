/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_GAME_H
#define VOX_GAME_H

#include "vox_physics.h"

#define VOX_DIGS_MAX_BOTS 3U
#define VOX_DIGS_MAX_SLOTS 4U
#define VOX_DIGS_TICKS_PER_SECOND 60U
#define VOX_DIGS_MAP_GENERATOR_VERSION 1U
#define VOX_DIGS_ACTION_LEFT 1U
#define VOX_DIGS_ACTION_RIGHT 2U
#define VOX_DIGS_ACTION_JUMP 4U
#define VOX_DIGS_ACTION_STEAM 8U
#define VOX_DIGS_ACTION_MASK (VOX_DIGS_ACTION_LEFT | VOX_DIGS_ACTION_RIGHT | \
                              VOX_DIGS_ACTION_JUMP | VOX_DIGS_ACTION_STEAM)
#define VOX_DIGS_MAX_HEALTH 100U
#define VOX_DIGS_RESPAWN_TICKS 120U
#define VOX_DIGS_MAX_PROJECTILES 64U
#define VOX_DIGS_MAX_EFFECTS 192U
#define VOX_DIGS_NO_PLAYER 65535U

#define VOX_DIGS_WEAPON_MELEE 1U
#define VOX_DIGS_WEAPON_PROJECTILE 2U
#define VOX_DIGS_WEAPON_EXPLOSIVE 4U
#define VOX_DIGS_WEAPON_DEPOSIT 8U
#define VOX_DIGS_WEAPON_GRAVITY 16U

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
    VOX_DIGS_TOOL_COUNT = 10
} vox_digs_tool;

typedef struct vox_digs_weapon_properties {
    const char *name;
    vox_u16 cooldown_ticks;
    vox_u16 damage;
    vox_u16 blast_radius;
    vox_u16 projectile_speed_q8;
    vox_u16 fuse_ticks;
    vox_u16 flags;
} vox_digs_weapon_properties;

typedef struct vox_digs_projectile {
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_i32 velocity_x_q16;
    vox_i32 velocity_y_q16;
    vox_u16 active;
    vox_u16 owner;
    vox_u16 weapon;
    vox_u16 material;
    vox_u16 fuse_ticks;
    vox_u16 age_ticks;
    vox_u16 damage;
    vox_u16 blast_radius;
} vox_digs_projectile;

typedef struct vox_digs_effect {
    vox_i32 position_x_q16;
    vox_i32 position_y_q16;
    vox_i32 velocity_x_q16;
    vox_i32 velocity_y_q16;
    vox_u16 active;
    vox_u16 material;
    vox_u16 ttl_ticks;
    vox_u16 reserved;
} vox_digs_effect;

typedef struct vox_digs_rules {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 match_ticks;
    vox_u32 score_limit;
    vox_u32 lava_start_tick;
    vox_u16 bot_count;
    vox_u16 team_mode;
    vox_u16 map_style;
    vox_u16 weapon_mask;
    vox_u32 seed;
} vox_digs_rules;

typedef struct vox_digs_input {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u16 player;
    vox_u16 actions;
} vox_digs_input;

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
    vox_u16 health[VOX_DIGS_MAX_SLOTS];
    vox_u16 deaths[VOX_DIGS_MAX_SLOTS];
    vox_u16 respawn_ticks[VOX_DIGS_MAX_SLOTS];
    vox_u16 player_actions[VOX_DIGS_MAX_SLOTS];
    vox_u16 steam_q16[VOX_DIGS_MAX_SLOTS];
    vox_u16 weapon_cooldown[VOX_DIGS_MAX_SLOTS];
    vox_u16 selected_weapon[VOX_DIGS_MAX_SLOTS];
    vox_u16 facing_right[VOX_DIGS_MAX_SLOTS];
    vox_u16 last_attacker[VOX_DIGS_MAX_SLOTS];
    vox_u32 lava_level_q16;
    vox_u16 lava_surface_y;
    vox_u16 projectile_count;
    vox_u16 effect_count;
    vox_u16 effect_cursor;
    vox_u32 terrain_hash;
    vox_physics_step_config physics_config;
    vox_physics_body players[VOX_DIGS_MAX_SLOTS];
    vox_digs_projectile projectiles[VOX_DIGS_MAX_PROJECTILES];
    vox_digs_effect effects[VOX_DIGS_MAX_EFFECTS];
} vox_digs_match;

#ifdef __cplusplus
extern "C" {
#endif

void vox_digs_rules_classic(vox_digs_rules *rules);
vox_result vox_digs_generate_map(vox_world *world, vox_u16 map_style,
                                 vox_u32 seed);
vox_result vox_digs_match_init(vox_digs_match *match,
                               const vox_digs_rules *rules);
vox_result vox_digs_match_step(vox_digs_match *match);
vox_result vox_digs_record_kill(vox_digs_match *match, vox_u16 killer,
                                vox_u16 victim);
vox_result vox_digs_submit_input(vox_digs_match *match,
                                 const vox_digs_input *input);
vox_result vox_digs_use_tool(vox_digs_match *match, vox_u16 player,
                             vox_u16 tool, vox_u32 x, vox_u32 y, vox_u32 z);
const vox_digs_weapon_properties *vox_digs_weapon_get(vox_u16 weapon);
vox_result vox_digs_fire_weapon(vox_digs_match *match, vox_u16 player,
                                vox_u16 weapon, vox_u32 target_x,
                                vox_u32 target_y);
vox_result vox_digs_apply_damage(vox_digs_match *match, vox_u16 attacker,
                                 vox_u16 victim, vox_u16 damage);
vox_result vox_digs_bot_think(vox_digs_match *match, vox_u16 player);
vox_u32 vox_digs_hash(const vox_digs_match *match);

#ifdef __cplusplus
}
#endif

#endif

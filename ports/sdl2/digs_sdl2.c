/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

#include "digs_miner_art.h"
#include "vox/vox_audio.h"
#include "vox/vox_game.h"
#include "digs_lines.h"
#include "digs_chronicle.h"
#include "vox/vox_render.h"
#include "vox_sdl_ui.h"

#define DEMO_WIDTH 320U
#define DEMO_HEIGHT 200U
#define DEMO_WINDOW_WIDTH 1280
#define DEMO_WINDOW_HEIGHT 800
#define DEMO_SIM_HZ 60U
#define DEMO_MAX_CATCHUP 8U
#define DEMO_PRESENTATION_DELTA_MAX 0.25
#define DEMO_CAMERA_ZOOM_MIN 1
#define DEMO_CAMERA_ZOOM_MAX 4
#define DEMO_CAMERA_ZOOM_DEFAULT 2
#define DEMO_CAMERA_MIN_SCALE 1.0
#define DEMO_CAMERA_SAFE_TOP_PIXELS 36.0
#define DEMO_CAMERA_SAFE_BOTTOM_PIXELS 18.0
#define DEMO_CAMERA_VERTICAL_PADDING 16.0
#define DEMO_AUDIO_RATE 44100
#define DEMO_AUDIO_FRAMES 512U
#define DEMO_LOCAL_MAX 2U
#define DEMO_CONTROLLER_MAX 2U
#define DEMO_DAMAGE_POPUP_MAX 16U
#define DEMO_INPUT_SWITCH_HYSTERESIS_MS 750U
#define DEMO_CONTROLLER_ACTIVITY_MARGIN 0.08
#define DEMO_CONTROLLER_CALIBRATION_MS 750U
#define DEMO_SETTINGS_VERSION 5
#define DEMO_ROPE_AIM_RANGE 48.0
#define DEMO_NAME_CHARACTERS 12
#define DEMO_NAME_CAPACITY 13
#define DEMO_KILLFEED_MAX 6
#define DEMO_KILLFEED_TICKS 360U
#define DEMO_BANNER_TICKS 90U
#define DEMO_HIT_MARKER_TICKS 12U
#define DEMO_MINER_HIT_TICKS 8U
#define DEMO_BUBBLE_TICKS 150U
#define DEMO_MULTIKILL_WINDOW 180U
#define DEMO_BARK_COOLDOWN 180U
#define DEMO_BOT_BARK_COOLDOWN 720U
#define DEMO_GLOBAL_BARK_GAP 120U
#define DEMO_BARK_PHRASE_COUNT 50U
/* Contexts 0-5 are the ordinary moods; 6 is going into the lava. */
#define DEMO_BARK_CONTEXT_COUNT 7
#define DEMO_BARK_CONTEXT_DOOMED 6
#define DEMO_NAME_GRID_COLUMNS 8
#define DEMO_NAME_GRID_ITEMS 42
#define DEMO_FRAME_CAP_COUNT 7
#define DEMO_FRAME_CAP_DEFAULT 2
#define DEMO_SCENE_MAX_PIXELS 1048576UL
#define DEMO_RENDER_OVERLAY_CAPACITY 8192U
#define DEMO_HAPTIC_REFRESH_MS 42U
#define DEMO_HAPTIC_LEVEL_COUNT 4

#if (VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT) > DEMO_SCENE_MAX_PIXELS
#error "DIGS native scene target exceeds the protected presentation cap"
#endif

#define DEMO_SOUND_MOVE 0
#define DEMO_SOUND_SELECT 1
#define DEMO_SOUND_FIRE 2
#define DEMO_SOUND_START 3
#define DEMO_SOUND_PAUSE 4

#define DEMO_ARSENAL_FULL 0
#define DEMO_ARSENAL_MINER 1
#define DEMO_ARSENAL_POWDER 2
#define DEMO_ARSENAL_COUNT 3

/* Fixed IBM-PC/VGA 16-color values keep UI output identical on every host. */
#define DEMO_VGA_BLACK 0U, 0U, 0U
#define DEMO_VGA_BLUE 0U, 0U, 170U
#define DEMO_VGA_CYAN 0U, 170U, 170U
#define DEMO_VGA_BROWN 170U, 85U, 0U
#define DEMO_VGA_LIGHT_GRAY 170U, 170U, 170U
#define DEMO_VGA_DARK_GRAY 85U, 85U, 85U
#define DEMO_VGA_LIGHT_CYAN 85U, 255U, 255U
#define DEMO_VGA_LIGHT_RED 255U, 85U, 85U
#define DEMO_VGA_YELLOW 255U, 255U, 85U
#define DEMO_VGA_WHITE 255U, 255U, 255U

typedef enum demo_screen {
    DEMO_TITLE = 0,
    DEMO_SETUP = 1,
    DEMO_OPTIONS = 2,
    DEMO_PLAY = 3,
    DEMO_PAUSE = 4,
    DEMO_RESULTS = 5,
    DEMO_INBOX = 6,
    DEMO_LOG = 7,
    DEMO_CONTROLS = 9,
    DEMO_INPUT_OPTIONS = 11,
    DEMO_CUSTOMIZE = 12,
    DEMO_NAME_EDITOR = 13
} demo_screen;

typedef enum demo_input_preference {
    DEMO_INPUT_AUTO = 0,
    DEMO_INPUT_KEYBOARD = 1,
    DEMO_INPUT_CONTROLLER = 2
} demo_input_preference;

typedef enum demo_input_source {
    DEMO_SOURCE_KEYBOARD = 0,
    DEMO_SOURCE_CONTROLLER = 1
} demo_input_source;

typedef enum demo_rope_mode {
    DEMO_ROPE_HOLD = 0,
    DEMO_ROPE_TOGGLE = 1
} demo_rope_mode;

typedef enum demo_controller_family {
    DEMO_PAD_GENERIC = 0,
    DEMO_PAD_XBOX = 1,
    DEMO_PAD_PLAYSTATION = 2,
    DEMO_PAD_NINTENDO = 3
} demo_controller_family;

typedef enum demo_haptic_kind {
    DEMO_HAPTIC_FIRE = 0,
    DEMO_HAPTIC_RAIL = 1,
    DEMO_HAPTIC_ROPE = 2,
    DEMO_HAPTIC_HIT = 3,
    DEMO_HAPTIC_KILL = 4,
    DEMO_HAPTIC_EXPLOSION = 5,
    DEMO_HAPTIC_CRUMBLE = 6
} demo_haptic_kind;

typedef struct demo_options {
    int frame_cap_index;
    int gi_quality;
    int debug;
    int fullscreen;
    int flash_mode;
    int gore_level;
    int camera_shake;
    int damage_numbers;
    int damage_number_size;
    int damage_number_color;
    int fx_profile;
    int master_volume;
    int laptop_mode;
    int dummy_mode;
    int haptic_level;
} demo_options;

typedef struct demo_controller {
    SDL_GameController *handle;
    SDL_Joystick *joystick;
    SDL_JoystickID instance_id;
    int claimed_player;
    int raw_fallback;
    int axis_center[4];
    long calibration_total[4];
    int calibration_samples;
    int calibration_peak;
    int calibrating;
    vox_u32 calibration_stamp;
    double auto_deadzone;
    int activity_frames;
    int family;
    int zoom_axis_state;
} demo_controller;

typedef struct demo_player_input {
    int preference;
    int active_source;
    int sensitivity;
    int deadzone;
    int aim_slowdown;
    int rope_mode;
    vox_u32 switch_stamp;
    int suppress_ticks;
    double aim_direction_x;
    double aim_direction_y;
    double aim_distance;
    double aim_magnitude;
} demo_player_input;

typedef struct demo_haptic_envelope {
    vox_u16 low_peak;
    vox_u16 high_peak;
    vox_u16 total_ticks;
    vox_u16 ticks_left;
    vox_u16 last_low;
    vox_u16 last_high;
} demo_haptic_envelope;

typedef struct demo_rail_trace {
    vox_i32 start_x_q16;
    vox_i32 start_y_q16;
    vox_i32 end_x_q16;
    vox_i32 end_y_q16;
    vox_u32 until_tick;
    vox_u16 active;
    vox_u16 source;
} demo_rail_trace;

typedef struct demo_render_patch {
    vox_u32 cell_index;
    vox_cell original;
} demo_render_patch;

typedef struct demo_fixed_step_clock {
    Uint64 pending_ticks;
    Uint64 phase_units;
} demo_fixed_step_clock;

typedef struct demo_damage_popup {
    vox_i32 world_x_q16;
    vox_i32 world_y_q16;
    vox_u16 amount;
    vox_u16 ttl;
    vox_u16 target;
    vox_u16 active;
} demo_damage_popup;

typedef struct demo_hit_marker {
    vox_i32 world_x_q16;
    vox_i32 world_y_q16;
    vox_u16 ttl;
} demo_hit_marker;

typedef struct demo_killfeed_line {
    char text[48];
    vox_u16 ttl;
} demo_killfeed_line;

typedef struct demo_banner_line {
    char text[48];
    vox_u16 ttl;
} demo_banner_line;

typedef struct demo_speech_bubble {
    char text[64];
    vox_u16 ttl;
} demo_speech_bubble;

typedef struct demo_bindings {
    SDL_Scancode keyboard_left[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_right[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_jump[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_steam[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_rope[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_fire[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_previous[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_next[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_bark[DEMO_LOCAL_MAX];
    SDL_GameControllerButton pad_jump;
    SDL_GameControllerButton pad_steam;
    SDL_GameControllerButton pad_rope;
    SDL_GameControllerButton pad_fire;
    SDL_GameControllerButton pad_previous;
    SDL_GameControllerButton pad_next;
    SDL_GameControllerButton pad_bark;
} demo_bindings;

typedef struct demo_app {
    int running;
    demo_screen screen;
    int selection;
    int inbox_open;      /* -1 while listing, else the message open */
    int log_scroll;
    int bots;
    int map_style;
    int arsenal;
    vox_u32 seed;
    int local_players;
    int time_limit_index;
    int lava_index;
    int score_limit_index;
    int respawn_mode;
    int respawn_delay_index;
    char human_names[DEMO_LOCAL_MAX][DEMO_NAME_CAPACITY];
    char bot_names[VOX_DIGS_MAX_BOTS][DEMO_NAME_CAPACITY];
    char player_names[VOX_DIGS_MAX_SLOTS][DEMO_NAME_CAPACITY];
    char edit_name_backup[DEMO_NAME_CAPACITY];
    int edit_name_slot;
    int name_grid_selection;
    int selected_tool[DEMO_LOCAL_MAX];
    vox_u32 aim_world_x[DEMO_LOCAL_MAX];
    vox_u32 aim_world_y[DEMO_LOCAL_MAX];
    int mouse_x;
    int mouse_y;
    int mouse_inside;
    /*
     * Player one aims with the mouse, and that aim is re-derived from the
     * pointer every tick.  When a solo keyboard player steers with the arrow
     * keys we latch here so the pointer stops overwriting them; genuine mouse
     * movement clears it and silently takes aim back.
     */
    int keyboard_aim_active;
    int mouse_activity_x;
    int mouse_activity_y;
    int cursor_visible;
    int camera_zoom;
    double camera_world_x;
    double camera_world_y;
    double camera_velocity_x;
    double camera_velocity_y;
    double camera_scale;
    double camera_scale_velocity;
    double camera_shake_x;
    double camera_shake_y;
    double camera_trauma;
    double flash_strength;
    int flash_kind;
    double render_alpha;
    double frame_seconds;
    vox_i32 previous_player_x[VOX_DIGS_MAX_SLOTS];
    vox_i32 previous_player_y[VOX_DIGS_MAX_SLOTS];
    vox_u32 last_event_sequence;
    int binding_capture;
    int binding_player;
    int keyboard_previous_down[DEMO_LOCAL_MAX];
    int keyboard_next_down[DEMO_LOCAL_MAX];
    int rope_down[DEMO_LOCAL_MAX];
    int rope_latched[DEMO_LOCAL_MAX];
    int controller_disconnected;
    int settings_writable;
    vox_u32 controller_nav_stamp;
    int foundry;
    double measured_fps;
    vox_u32 rendered_frames;
    vox_u32 fps_stamp;
    demo_options options;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_AudioDeviceID audio_device;
    vox_audio_engine audio;
    vox_u32 audio_event_id;
    vox_u32 menu_note_stamp;
    vox_u32 menu_note_tail;
    vox_u16 menu_note_index;
    int menu_note_screen;
    demo_controller controllers[DEMO_CONTROLLER_MAX];
    demo_player_input player_input[DEMO_LOCAL_MAX];
    demo_bindings bindings;
    demo_damage_popup damage_popups[DEMO_DAMAGE_POPUP_MAX];
    demo_hit_marker hit_markers[DEMO_LOCAL_MAX];
    demo_killfeed_line killfeed[DEMO_KILLFEED_MAX];
    demo_speech_bubble bubbles[VOX_DIGS_MAX_SLOTS];
    vox_u16 miner_hit_ttl[VOX_DIGS_MAX_SLOTS];
    vox_u16 victory_bark_ttl[VOX_DIGS_MAX_SLOTS];
    vox_u16 crosshair_pulse_ttl[DEMO_LOCAL_MAX];
    vox_u16 multikill_count[DEMO_LOCAL_MAX];
    vox_u16 spree_count[DEMO_LOCAL_MAX];
    vox_u32 last_kill_tick[DEMO_LOCAL_MAX];
    vox_u32 last_bark_tick[VOX_DIGS_MAX_SLOTS];
    vox_u32 bark_sequence[VOX_DIGS_MAX_SLOTS];
    vox_u32 global_bark_tick;
    demo_banner_line banners[3];
    vox_u16 death_camera_hold;
    vox_u16 death_camera_player;
    int bark_down[DEMO_LOCAL_MAX];
    int fire_down[DEMO_LOCAL_MAX];
    demo_haptic_envelope haptic[DEMO_LOCAL_MAX];
    demo_rail_trace rail_traces[VOX_DIGS_MAX_SLOTS];
    vox_i32 rail_origin_x_q16[VOX_DIGS_MAX_SLOTS];
    vox_i32 rail_origin_y_q16[VOX_DIGS_MAX_SLOTS];
    vox_u32 scene_tick;
    int scene_valid;
    int scene_laptop;
    vox_u32 cap_cache_profile;
    vox_u32 cap_cache_us;
    vox_u32 cap_cache_mask;
    vox_u32 cap_supported_mask;
    int cap_qualified;
    vox_u16 bot_health_ttl[VOX_DIGS_MAX_SLOTS];
} demo_app;

static vox_u8 demo_pixels[DEMO_WIDTH * DEMO_HEIGHT * VOX_SOFTWARE_RGB_BYTES];
static vox_u8 demo_camera_pixels[DEMO_WIDTH * DEMO_HEIGHT *
                                 VOX_SOFTWARE_RGB_BYTES];
static vox_u8 demo_scene_pixels[VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT *
                                VOX_SOFTWARE_RGB_BYTES];
static vox_u8 demo_render_touched[VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT];
static demo_render_patch demo_render_patches[DEMO_RENDER_OVERLAY_CAPACITY];
static vox_u32 demo_render_patch_count;
static vox_digs_match demo_match;
/*
 * Everything that outlives the process.  Loaded once at startup, written
 * when a match ends, and never erased from inside a released build.
 */
static digs_chronicle demo_chronicle;
static int demo_chronicle_ready;
/*
 * The reset row has to be chosen twice.  It destroys a history that cannot
 * be recovered or replayed, and a single mis-keyed RETURN on a menu is not
 * enough intent for that.
 */
static int demo_chronicle_reset_armed;
static vox_world demo_title_world;
static vox_ui_surface demo_ui;
static vox_software_target demo_target;
static vox_software_target demo_scene_target;
static vox_software_target demo_legacy_target;
static vox_software_config demo_render_config;
static const char *demo_settings_override;

static const int demo_frame_caps[DEMO_FRAME_CAP_COUNT] = {
    15, 30, 60, 90, 120, 144, 0
};
static const char *demo_frame_names[DEMO_FRAME_CAP_COUNT] = {
    "15 LOW", "30", "60", "90", "120", "144", "UNLIMITED"
};
/*
 * Time limits, in minutes.  Zero is UNLIMITED, which is not really unlimited
 * -- digs_validate_rules refuses match_ticks == 0 and the lava ramp divides
 * by (match_ticks - lava_start_tick), so a genuinely endless match would
 * either be rejected or divide by zero.  Ninety-nine minutes is past any
 * session anyone will play and keeps every one of those invariants true.
 */
static const int demo_time_limits[6] = {1, 2, 3, 5, 10, 0};
#define DEMO_TIME_UNLIMITED_MINUTES 99
static const char *demo_time_limit_names[6] = {
    "1:00", "2:00", "3:00", "5:00", "10:00", "UNLIMITED"
};
/* When the lava starts, counted from the end.  AUTO is the old behaviour. */
static const char *demo_lava_names[5] = {
    "AUTO", "OFF", "LAST 1:00", "LAST 2:00", "LAST 5:00"
};
static const int demo_lava_lead_seconds[5] = {30, -1, 60, 120, 300};
static const char *demo_map_names[3] = {"COAL RIDGE", "DEEPWORKS", "FURNACE YARD"};
static const char *demo_gi_names[3] = {"COMPATIBILITY", "BALANCED", "SHOWCASE"};
static const char *demo_toggle_names[2] = {"OFF", "ON"};
static const char *demo_flash_names[3] = {"OFF", "REDUCED", "FULL"};
static const char *demo_gore_names[3] = {"OFF", "REDUCED", "FULL"};
static const char *demo_fx_names[3] = {"RETRO 768", "STANDARD 1536", "CARNAGE 3072"};
static const char *demo_number_color_names[2] = {"CLASSIC", "HIGH CONTRAST"};
static const char *demo_sensitivity_names[3] = {"LOW", "NORMAL", "HIGH"};
static const char *demo_deadzone_names[4] = {
    "AUTO", "SMALL", "NORMAL", "LARGE"
};
static const char *demo_slowdown_names[3] = {"OFF", "LOW", "MEDIUM"};
static const char *demo_rope_mode_names[2] = {"HOLD", "TOGGLE"};
static const char *demo_haptic_names[DEMO_HAPTIC_LEVEL_COUNT] = {
    "OFF", "LOW", "NORMAL", "HEAVY"
};
static const char *demo_arsenal_names[DEMO_ARSENAL_COUNT] = {
    "FULL WORKS", "MINER KIT", "POWDER KEG"
};
static const int demo_score_limits[4] = {0, 5, 10, 20};
static const char *demo_score_limit_names[4] = {"OFF", "5", "10", "20"};
static const int demo_respawn_delays[5] = {0, 1, 2, 3, 5};
static const char *demo_respawn_mode_names[2] = {"AUTO", "ON FIRE"};
static const char demo_name_grid[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
static const vox_u16 demo_arsenal_masks[DEMO_ARSENAL_COUNT] = {
    0x07FFU, 0x00F1U, 0x030EU
};

static demo_controller *demo_controller_for_player(demo_app *app,
                                                   int player);
static void demo_refresh_controller_claims(demo_app *app);
static int demo_save_input_settings(demo_app *app);
static SDL_Scancode *demo_keyboard_binding(demo_app *app, int player,
                                           int action);
static SDL_GameControllerButton *demo_pad_binding(demo_app *app,
                                                  int action);
static const char *demo_player_name(const demo_app *app, vox_u16 player);

static void demo_audio_lock(demo_app *app)
{
    if (app != 0 && app->audio_device != 0U) {
        SDL_LockAudioDevice(app->audio_device);
    }
}

static void demo_audio_unlock(demo_app *app)
{
    if (app != 0 && app->audio_device != 0U) {
        SDL_UnlockAudioDevice(app->audio_device);
    }
}

static void demo_audio_emit(demo_app *app, vox_u16 preset,
                            vox_u16 variant, vox_i16 pan_q15)
{
    vox_audio_event event;
    if (app == 0 || app->audio_device == 0U) {
        return;
    }
    vox_audio_event_init(&event, preset);
    event.variant = variant;
    event.pan_q15 = pan_q15;
    demo_audio_lock(app);
    event.event_id = ++app->audio_event_id;
    (void)vox_audio_emit(&app->audio, &event);
    demo_audio_unlock(app);
}

/* Keep weapon fire legible in the existing GPL synth without adding a second
 * audio backend.  The preset family supplies a distinct silhouette while the
 * stable tool-derived variant keeps repeated shots from collapsing into one
 * identical blip. */
static void demo_audio_weapon_fire(demo_app *app,
                                   const vox_digs_event *event,
                                   vox_i16 pan_q15)
{
    vox_u16 preset = VOX_AUDIO_PRESET_FIRE;
    vox_u16 variant;
    if (event == 0) return;
    switch (event->weapon) {
    case VOX_DIGS_TOOL_PULASKI:
        preset = VOX_AUDIO_PRESET_PULASKI;
        break;
    case VOX_DIGS_TOOL_POPPER:
        preset = VOX_AUDIO_PRESET_POPPER;
        break;
    case VOX_DIGS_TOOL_SMOKER:
        preset = VOX_AUDIO_PRESET_SMOKER;
        break;
    case VOX_DIGS_TOOL_HOT_RAIL:
        preset = VOX_AUDIO_PRESET_HOT_RAIL;
        break;
    case VOX_DIGS_TOOL_HYDROSHOT:
        preset = VOX_AUDIO_PRESET_HYDROSHOT;
        break;
    case VOX_DIGS_TOOL_GIANT_HAMMER:
        preset = VOX_AUDIO_PRESET_GIANT_HAMMER;
        break;
    case VOX_DIGS_TOOL_BOLT_ACTION:
        preset = VOX_AUDIO_PRESET_BOLT_ACTION;
        break;
    case VOX_DIGS_TOOL_SCATTERBRAIN:
        preset = VOX_AUDIO_PRESET_SCATTERBRAIN;
        break;
    case VOX_DIGS_TOOL_FIRECRACKER:
        preset = VOX_AUDIO_PRESET_FIRECRACKER;
        break;
    case VOX_DIGS_TOOL_BORE_DRILL:
        preset = VOX_AUDIO_PRESET_BORE_DRILL;
        break;
    case VOX_DIGS_TOOL_RAIL_GUN:
        preset = VOX_AUDIO_PRESET_KILL;
        break;
    default:
        break;
    }
    variant = (vox_u16)(event->variant + event->weapon * 29U);
    demo_audio_emit(app, preset, variant, pan_q15);
}

typedef struct demo_pronunciation {
    const char *word;
    vox_u8 count;
    vox_u8 tokens[8];
} demo_pronunciation;

#define A VOX_AUDIO_ALLOPHONE_A
#define AE VOX_AUDIO_ALLOPHONE_AE
#define AH VOX_AUDIO_ALLOPHONE_AH
#define AW VOX_AUDIO_ALLOPHONE_AW
#define AY VOX_AUDIO_ALLOPHONE_AY
#define EH VOX_AUDIO_ALLOPHONE_EH
#define ER VOX_AUDIO_ALLOPHONE_ER
#define EY VOX_AUDIO_ALLOPHONE_EY
#define IH VOX_AUDIO_ALLOPHONE_IH
#define IY VOX_AUDIO_ALLOPHONE_IY
#define OW VOX_AUDIO_ALLOPHONE_OW
#define OY VOX_AUDIO_ALLOPHONE_OY
#define UH VOX_AUDIO_ALLOPHONE_UH
#define UW VOX_AUDIO_ALLOPHONE_UW
#define B VOX_AUDIO_ALLOPHONE_B
#define CH VOX_AUDIO_ALLOPHONE_CH
#define D VOX_AUDIO_ALLOPHONE_D
#define F VOX_AUDIO_ALLOPHONE_F
#define G VOX_AUDIO_ALLOPHONE_G
#define H VOX_AUDIO_ALLOPHONE_H
#define J VOX_AUDIO_ALLOPHONE_J
#define K VOX_AUDIO_ALLOPHONE_K
#define L VOX_AUDIO_ALLOPHONE_L
#define M VOX_AUDIO_ALLOPHONE_M
#define N VOX_AUDIO_ALLOPHONE_N
#define P VOX_AUDIO_ALLOPHONE_P
#define R VOX_AUDIO_ALLOPHONE_R
#define S VOX_AUDIO_ALLOPHONE_S
#define SH VOX_AUDIO_ALLOPHONE_SH
#define T VOX_AUDIO_ALLOPHONE_T
#define TH VOX_AUDIO_ALLOPHONE_TH
#define V VOX_AUDIO_ALLOPHONE_V
#define W VOX_AUDIO_ALLOPHONE_W
#define Y VOX_AUDIO_ALLOPHONE_Y
#define Z VOX_AUDIO_ALLOPHONE_Z

static const demo_pronunciation demo_pronunciations[] = {
    {"A", 1U, {AH}}, {"AGAIN", 4U, {AH,G,EH,N}},
    {"AM", 2U, {AE,M}}, {"ANOTHER", 5U, {AH,N,AH,TH,ER}},
    {"ARE", 2U, {AH,R}}, {"BACK", 3U, {B,AE,K}},
    {"BE", 2U, {B,IY}}, {"BEARD", 4U, {B,IY,R,D}},
    {"BLAST", 5U, {B,L,AE,S,T}}, {"BOILER", 4U, {B,OY,L,ER}},
    {"BONES", 4U, {B,OW,N,Z}}, {"BREAK", 4U, {B,R,EY,K}},
    {"CART", 4U, {K,A,R,T}}, {"CHECK", 3U, {CH,EH,K}},
    {"COAL", 3U, {K,OW,L}}, {"COME", 3U, {K,AH,M}},
    {"COMPLETE", 7U, {K,AH,M,P,L,IY,T}},
    {"CRATER", 5U, {K,R,EY,T,ER}}, {"DIG", 3U, {D,IH,G}},
    {"DIGGING", 5U, {D,IH,G,IH,N}}, {"DID", 3U, {D,IH,D}},
    {"DIGS", 4U, {D,IH,G,Z}}, {"DOUBLE", 5U, {D,AH,B,AH,L}},
    {"DOWN", 3U, {D,AW,N}}, {"DRAW", 3U, {D,R,AW}},
    {"DUCK", 3U, {D,AH,K}}, {"DUST", 4U, {D,AH,S,T}},
    {"EASY", 3U, {IY,Z,IY}}, {"EAT", 2U, {IY,T}},
    {"ENDS", 4U, {EH,N,D,Z}}, {"ENOUGH", 4U, {EH,N,AH,F}},
    {"FALL", 3U, {F,AW,L}}, {"FALLS", 4U, {F,AW,L,Z}},
    {"FELT", 4U, {F,EH,L,T}}, {"FIRE", 3U, {F,AY,ER}},
    {"GET", 3U, {G,EH,T}}, {"GO", 2U, {G,OW}},
    {"GOOD", 3U, {G,UH,D}}, {"HEAR", 3U, {H,IY,R}},
    {"HELMET", 6U, {H,EH,L,M,EH,T}}, {"HERE", 3U, {H,IY,R}},
    {"HIDING", 5U, {H,AY,D,IH,N}}, {"HOLE", 3U, {H,OW,L}},
    {"HOT", 3U, {H,AH,T}}, {"HOW", 2U, {H,AW}},
    {"HURT", 3U, {H,ER,T}}, {"HURTS", 4U, {H,ER,T,S}},
    {"I", 1U, {AY}}, {"IN", 2U, {IH,N}},
    {"IRON", 4U, {AY,ER,AH,N}}, {"IS", 2U, {IH,Z}},
    {"KEEP", 3U, {K,IY,P}}, {"KILL", 3U, {K,IH,L}},
    {"KILLING", 5U, {K,IH,L,IH,N}}, {"LAVA", 4U, {L,AH,V,AH}},
    {"LONG", 4U, {L,AW,N,G}}, {"MAKE", 3U, {M,EY,K}},
    {"MEAN", 3U, {M,IY,N}}, {"MEDIC", 5U, {M,EH,D,IH,K}},
    {"MIND", 4U, {M,AY,N,D}}, {"MINE", 3U, {M,AY,N}},
    {"MINER", 4U, {M,AY,N,ER}}, {"MOVES", 4U, {M,UW,V,Z}},
    {"MULTI", 5U, {M,AH,L,T,IY}}, {"MY", 2U, {M,AY}},
    {"NEED", 3U, {N,IY,D}}, {"NO", 2U, {N,OW}},
    {"NOT", 3U, {N,AH,T}}, {"NOW", 2U, {N,AW}},
    {"ONE", 3U, {W,AH,N}}, {"OUT", 2U, {AW,T}},
    {"OW", 1U, {AW}}, {"PAY", 2U, {P,EY}},
    {"PICK", 3U, {P,IH,K}}, {"PLAN", 4U, {P,L,AE,N}},
    {"QUIET", 5U, {K,W,AY,EH,T}}, {"RUN", 3U, {R,AH,N}},
    {"SAVE", 3U, {S,EY,V}}, {"SEE", 2U, {S,IY}},
    {"SHAFT", 4U, {SH,AE,F,T}}, {"SHIFT", 4U, {SH,IH,F,T}},
    {"SHOW", 2U, {SH,OW}}, {"SMELL", 4U, {S,M,EH,L}},
    {"SPREE", 4U, {S,P,R,IY}}, {"TEA", 2U, {T,IY}},
    {"THAT", 3U, {TH,AE,T}}, {"THE", 2U, {TH,AH}},
    {"THIS", 3U, {TH,IH,S}}, {"TO", 2U, {T,UW}},
    {"TOO", 2U, {T,UW}}, {"TRACKS", 5U, {T,R,AE,K,S}},
    {"TRIPLE", 6U, {T,R,IH,P,AH,L}},
    {"TROUSERS", 6U, {T,R,AW,Z,ER,S}},
    {"TUNNEL", 5U, {T,AH,N,AH,L}}, {"UP", 2U, {AH,P}},
    {"WAS", 3U, {W,AH,Z}}, {"WE", 2U, {W,IY}},
    {"WHERE", 3U, {W,EH,R}}, {"WHO", 2U, {H,UW}},
    {"WILL", 3U, {W,IH,L}}, {"WINS", 4U, {W,IH,N,Z}},
    {"WON", 3U, {W,AH,N}}, {"WORK", 3U, {W,ER,K}},
    {"YOU", 2U, {Y,UW}}, {"YOUR", 2U, {Y,ER}},
    {"LOST", 4U, {L,AW,S,T}}
};

static const demo_pronunciation *demo_find_pronunciation(const char *word)
{
    vox_u32 index;
    vox_u32 count = (vox_u32)(sizeof(demo_pronunciations) /
                              sizeof(demo_pronunciations[0]));
    for (index = 0U; index < count; ++index) {
        if (strcmp(word, demo_pronunciations[index].word) == 0) {
            return &demo_pronunciations[index];
        }
    }
    return 0;
}

static void demo_speech_token(vox_u8 *tokens, vox_u16 *count,
                              vox_u8 token)
{
    if (*count < VOX_AUDIO_SPEECH_TOKEN_CAPACITY) {
        tokens[(*count)++] = token;
    }
}

static int demo_g2p_word(const char *word, vox_u8 *tokens,
                         vox_u16 *count)
{
    int length = (int)strlen(word);
    int index;
    vox_u16 before = *count;
    for (index = 0; index < length; ++index) {
        if (word[index] < 'A' || word[index] > 'Z') return 0;
    }
    for (index = 0; index < length; ++index) {
        int character = word[index];
        int next = index + 1 < length ? word[index + 1] : 0;
        if (character == 'C' && next == 'H') {
            demo_speech_token(tokens, count, CH); ++index;
        } else if (character == 'S' && next == 'H') {
            demo_speech_token(tokens, count, SH); ++index;
        } else if (character == 'T' && next == 'H') {
            demo_speech_token(tokens, count, TH); ++index;
        } else if (character == 'P' && next == 'H') {
            demo_speech_token(tokens, count, F); ++index;
        } else if (character == 'Q' && next == 'U') {
            demo_speech_token(tokens, count, K);
            demo_speech_token(tokens, count, W); ++index;
        } else if (character == 'A') {
            demo_speech_token(tokens, count,
                              next == 'I' || next == 'Y' ? AY : AE);
        } else if (character == 'E') {
            if (!(index + 1 == length && length > 2)) {
                demo_speech_token(tokens, count, EH);
            }
        } else if (character == 'I') demo_speech_token(tokens, count, IH);
        else if (character == 'O') demo_speech_token(tokens, count, OW);
        else if (character == 'U') demo_speech_token(tokens, count, UH);
        else if (character == 'B') demo_speech_token(tokens, count, B);
        else if (character == 'C') demo_speech_token(tokens, count,
            next == 'E' || next == 'I' || next == 'Y' ? S : K);
        else if (character == 'D') demo_speech_token(tokens, count, D);
        else if (character == 'F') demo_speech_token(tokens, count, F);
        else if (character == 'G') demo_speech_token(tokens, count,
            next == 'E' || next == 'I' || next == 'Y' ? J : G);
        else if (character == 'H') demo_speech_token(tokens, count, H);
        else if (character == 'J') demo_speech_token(tokens, count, J);
        else if (character == 'K') demo_speech_token(tokens, count, K);
        else if (character == 'L') demo_speech_token(tokens, count, L);
        else if (character == 'M') demo_speech_token(tokens, count, M);
        else if (character == 'N') demo_speech_token(tokens, count, N);
        else if (character == 'P') demo_speech_token(tokens, count, P);
        else if (character == 'Q') {
            demo_speech_token(tokens, count, K);
            demo_speech_token(tokens, count, W);
        } else if (character == 'R') demo_speech_token(tokens, count, R);
        else if (character == 'S') demo_speech_token(tokens, count, S);
        else if (character == 'T') demo_speech_token(tokens, count, T);
        else if (character == 'V') demo_speech_token(tokens, count, V);
        else if (character == 'W') demo_speech_token(tokens, count, W);
        else if (character == 'X') {
            demo_speech_token(tokens, count, K);
            demo_speech_token(tokens, count, S);
        } else if (character == 'Y') demo_speech_token(tokens, count, Y);
        else if (character == 'Z') demo_speech_token(tokens, count, Z);
    }
    return *count > before;
}

static void demo_spell_word(const char *word, vox_u8 *tokens,
                            vox_u16 *count)
{
    int index;
    for (index = 0; word[index] != '\0'; ++index) {
        int character = word[index];
        if (index > 0) demo_speech_token(tokens, count,
                                         VOX_AUDIO_ALLOPHONE_SILENCE);
        if (character == 'A') demo_speech_token(tokens, count, EY);
        else if (character == 'B') { demo_speech_token(tokens, count, B);
                                     demo_speech_token(tokens, count, IY); }
        else if (character == 'C') { demo_speech_token(tokens, count, S);
                                     demo_speech_token(tokens, count, IY); }
        else if (character == 'D') { demo_speech_token(tokens, count, D);
                                     demo_speech_token(tokens, count, IY); }
        else if (character == 'E') demo_speech_token(tokens, count, IY);
        else if (character == 'F') { demo_speech_token(tokens, count, EH);
                                     demo_speech_token(tokens, count, F); }
        else if (character == 'G') { demo_speech_token(tokens, count, J);
                                     demo_speech_token(tokens, count, IY); }
        else if (character == 'H') { demo_speech_token(tokens, count, EY);
                                     demo_speech_token(tokens, count, CH); }
        else if (character == 'I') demo_speech_token(tokens, count, AY);
        else if (character == 'J') { demo_speech_token(tokens, count, J);
                                     demo_speech_token(tokens, count, EY); }
        else if (character == 'K') { demo_speech_token(tokens, count, K);
                                     demo_speech_token(tokens, count, EY); }
        else if (character == 'L') { demo_speech_token(tokens, count, EH);
                                     demo_speech_token(tokens, count, L); }
        else if (character == 'M') { demo_speech_token(tokens, count, EH);
                                     demo_speech_token(tokens, count, M); }
        else if (character == 'N') { demo_speech_token(tokens, count, EH);
                                     demo_speech_token(tokens, count, N); }
        else if (character == 'O') demo_speech_token(tokens, count, OW);
        else if (character == 'P') { demo_speech_token(tokens, count, P);
                                     demo_speech_token(tokens, count, IY); }
        else if (character == 'Q') { demo_speech_token(tokens, count, K);
                                     demo_speech_token(tokens, count, UW); }
        else if (character == 'R') { demo_speech_token(tokens, count, A);
                                     demo_speech_token(tokens, count, R); }
        else if (character == 'S') { demo_speech_token(tokens, count, EH);
                                     demo_speech_token(tokens, count, S); }
        else if (character == 'T') { demo_speech_token(tokens, count, T);
                                     demo_speech_token(tokens, count, IY); }
        else if (character == 'U') { demo_speech_token(tokens, count, Y);
                                     demo_speech_token(tokens, count, UW); }
        else if (character == 'V') { demo_speech_token(tokens, count, V);
                                     demo_speech_token(tokens, count, IY); }
        else if (character == 'W') { demo_speech_token(tokens, count, D);
                                     demo_speech_token(tokens, count, AH);
                                     demo_speech_token(tokens, count, W); }
        else if (character == 'X') { demo_speech_token(tokens, count, EH);
                                     demo_speech_token(tokens, count, K);
                                     demo_speech_token(tokens, count, S); }
        else if (character == 'Y') { demo_speech_token(tokens, count, W);
                                     demo_speech_token(tokens, count, AY); }
        else if (character == 'Z') { demo_speech_token(tokens, count, Z);
                                     demo_speech_token(tokens, count, IY); }
        else {
            /* Digits and punctuation use a short machine-readable chirp. */
            demo_speech_token(tokens, count, character & 1 ? IH : AH);
            demo_speech_token(tokens, count, T);
        }
    }
}

static void demo_audio_speak_text(demo_app *app, const char *text_value,
                                  vox_u8 profile, vox_u8 priority,
                                  vox_i16 pan_q15)
{
    vox_u8 tokens[VOX_AUDIO_SPEECH_TOKEN_CAPACITY];
    vox_u16 count;
    int index;
    char word[16];
    int word_length;
    vox_audio_speech speech;
    if (app == 0 || app->audio_device == 0U || text_value == 0) return;
    count = 0U;
    index = 0;
    while (text_value[index] != '\0' &&
           count < VOX_AUDIO_SPEECH_TOKEN_CAPACITY) {
        const demo_pronunciation *pronunciation;
        vox_u8 token;
        word_length = 0;
        while (text_value[index] != '\0' &&
               !((text_value[index] >= 'A' && text_value[index] <= 'Z') ||
                 (text_value[index] >= '0' && text_value[index] <= '9'))) {
            ++index;
        }
        while (((text_value[index] >= 'A' && text_value[index] <= 'Z') ||
                (text_value[index] >= '0' && text_value[index] <= '9')) &&
               word_length < (int)sizeof(word) - 1) {
            word[word_length++] = text_value[index++];
        }
        word[word_length] = '\0';
        if (word_length == 0) continue;
        pronunciation = demo_find_pronunciation(word);
        if (count > 0U && count < VOX_AUDIO_SPEECH_TOKEN_CAPACITY) {
            tokens[count++] = VOX_AUDIO_ALLOPHONE_SILENCE;
        }
        if (pronunciation != 0) {
            for (token = 0U; token < pronunciation->count &&
                 count < VOX_AUDIO_SPEECH_TOKEN_CAPACITY; ++token) {
                tokens[count++] = pronunciation->tokens[token];
            }
        } else if (!demo_g2p_word(word, tokens, &count)) {
            demo_spell_word(word, tokens, &count);
        }
    }
    if (count == 0U) return;
    vox_audio_speech_init(&speech, tokens, count);
    speech.profile = profile;
    speech.priority = priority;
    speech.pan_q15 = pan_q15;
    speech.gain_q15 = (profile == VOX_AUDIO_SPEECH_DEEP ||
                       profile == VOX_AUDIO_SPEECH_CINDER) ? 15000U :
                      profile == VOX_AUDIO_SPEECH_RIVET ? 13000U : 11000U;
    demo_audio_lock(app);
    speech.event_id = ++app->audio_event_id;
    (void)vox_audio_speak(&app->audio, &speech);
    demo_audio_unlock(app);
}

#undef A
#undef AE
#undef AH
#undef AW
#undef AY
#undef EH
#undef ER
#undef EY
#undef IH
#undef IY
#undef OW
#undef OY
#undef UH
#undef UW
#undef B
#undef CH
#undef D
#undef F
#undef G
#undef H
#undef J
#undef K
#undef L
#undef M
#undef N
#undef P
#undef R
#undef S
#undef SH
#undef T
#undef TH
#undef V
#undef W
#undef Y
#undef Z

static vox_i16 demo_player_pan(vox_u16 player)
{
    long x;
    if (player >= VOX_DIGS_MAX_SLOTS) return VOX_AUDIO_PAN_CENTER;
    x = demo_match.players[player].position_x.value_q16 / 65536L;
    return (vox_i16)(x * 65534L / (long)(VOX_WORLD_WIDTH - 1U) -
                     32767L);
}

static void demo_copy_text(char *destination, size_t capacity,
                           const char *source)
{
    size_t index = 0U;
    if (destination == 0 || capacity == 0U) return;
    if (source != 0) {
        while (index + 1U < capacity && source[index] != '\0') {
            destination[index] = source[index];
            ++index;
        }
    }
    destination[index] = '\0';
}







/*
 * Which voice speaks for this slot.
 *
 * The three opponents get their own; a human keeps the deep stock voice so
 * you can tell yourself apart from them without looking.
 */
static vox_u8 demo_speech_profile(int player)
{
    if (player < 0 || (vox_u16)player >= demo_match.rules.player_count) {
        return (vox_u8)VOX_AUDIO_SPEECH_HIGH;
    }
    if (!vox_digs_player_is_bot(&demo_match, (vox_u16)player)) {
        return (vox_u8)VOX_AUDIO_SPEECH_DEEP;
    }
    switch (vox_digs_bot_archetype(&demo_match, (vox_u16)player)) {
    case VOX_DIGS_ARCHETYPE_ENGINEER:
        return (vox_u8)VOX_AUDIO_SPEECH_RIVET;
    case VOX_DIGS_ARCHETYPE_BERSERKER:
        return (vox_u8)VOX_AUDIO_SPEECH_CINDER;
    default:
        break;
    }
    return (vox_u8)VOX_AUDIO_SPEECH_FLAMEY;
}

/*
 * Speak a line the simulation already chose.
 *
 * The port no longer decides what anyone says.  It used to pick a phrase from
 * a table and, one time in four, assemble one from adjective/noun/verb lists
 * -- which is where the nonsense came from.  Selection now happens in the
 * simulation, where it is hashed and reproducible, and all that is left here
 * is substituting the name and putting it on the screen.
 */
/*
 * Put a line into readable form: the one substitution there is, %T for
 * whoever is being talked about.
 *
 * Shared rather than inlined into the speech bubble, because the inbox
 * reader has to do exactly the same thing to exactly the same lines, and a
 * message that reads "WHERE HAVE YOU BEEN, %T" is the sort of thing that
 * ships.
 */
static void demo_expand_line(vox_u16 line_id, const char *about,
                             char *out, size_t capacity)
{
    const char *source = digs_lines_text(line_id);
    size_t written = 0U;
    size_t in = 0U;
    if (out == 0 || capacity == 0U) {
        return;
    }
    if (source == 0) {
        out[0] = '\0';
        return;
    }
    if (about == 0) {
        about = "SOMEBODY";
    }
    while (source[in] != '\0' && written + 1U < capacity) {
        if (source[in] == '%' && source[in + 1] == 'T') {
            size_t copied = 0U;
            while (about[copied] != '\0' && written + 1U < capacity) {
                out[written++] = about[copied++];
            }
            in += 2U;
            continue;
        }
        out[written++] = source[in++];
    }
    out[written] = '\0';
}

static void demo_speak_line(demo_app *app, int player, int target,
                            vox_u16 line_id, int bot)
{
    const char *source = digs_lines_text(line_id);
    char phrase[96];
    if (player < 0 || player >= (int)demo_match.rules.player_count ||
        source == 0 || source[0] == '\0') {
        return;
    }
    demo_expand_line(line_id,
                     (target >= 0 &&
                      target < (int)demo_match.rules.player_count) ?
                     demo_player_name(app, (vox_u16)target) : "SOMEBODY",
                     phrase, sizeof(phrase));
    /*
     * One bubble on screen at a time.  Four miners shouting over each other
     * turned the battlefield into a noticeboard, and the conversation is
     * easier to follow -- and easier to read at this resolution -- when only
     * the newest line is up.
     */
    {
        int other;
        for (other = 0; other < (int)VOX_DIGS_MAX_SLOTS; ++other) {
            if (other != player) {
                app->bubbles[other].ttl = 0U;
            }
        }
    }
    demo_copy_text(app->bubbles[player].text,
                   sizeof(app->bubbles[player].text), phrase);
    app->bubbles[player].ttl = DEMO_BUBBLE_TICKS;
    app->last_bark_tick[player] = demo_match.tick;
    app->global_bark_tick = demo_match.tick;
    demo_audio_speak_text(app, phrase, demo_speech_profile(player),
                          bot ? VOX_AUDIO_PRIORITY_BOT_BARK :
                          VOX_AUDIO_PRIORITY_PLAYER_BARK,
                          demo_player_pan((vox_u16)player));
}

static void demo_audio_play(demo_app *app, int sound)
{
    static const vox_u16 motif[8] = {
        220U, 262U, 294U, 330U, 294U, 392U, 330U, 262U
    };
    vox_u16 preset = VOX_AUDIO_PRESET_UI_MOVE;
    if (sound == DEMO_SOUND_MOVE && app != 0 &&
        app->audio_device != 0U) {
        vox_audio_note note;
        vox_u32 now = SDL_GetTicks();
        vox_u32 sample_now;
        vox_u32 spacing;
        vox_u32 maximum_tail;
        demo_audio_lock(app);
        sample_now = vox_audio_sample_clock(&app->audio);
        spacing = vox_audio_ms_to_frames(&app->audio, 60U);
        maximum_tail = sample_now + spacing * 8U;
        if (now - app->menu_note_stamp > 1500U ||
            app->menu_note_screen != (int)app->screen) {
            app->menu_note_index = 0U;
            app->menu_note_tail = sample_now;
        }
        vox_audio_note_init(&note, motif[app->menu_note_index % 8U], 60U);
        if (app->menu_note_tail < sample_now ||
            app->menu_note_tail > maximum_tail) {
            app->menu_note_tail = sample_now;
        }
        note.delay_frames = app->menu_note_tail - sample_now;
        note.gain_q15 = 7000U;
        note.bus = VOX_AUDIO_BUS_UI;
        note.priority = VOX_AUDIO_PRIORITY_UI;
        note.event_id = ++app->audio_event_id;
        (void)vox_audio_schedule_note(&app->audio, &note);
        app->menu_note_index = (vox_u16)((app->menu_note_index + 1U) % 8U);
        app->menu_note_tail += spacing;
        app->menu_note_stamp = now;
        app->menu_note_screen = (int)app->screen;
        demo_audio_unlock(app);
        return;
    }
    if (sound == DEMO_SOUND_SELECT) {
        preset = VOX_AUDIO_PRESET_UI_ACCEPT;
    } else if (sound == DEMO_SOUND_FIRE) {
        preset = VOX_AUDIO_PRESET_FIRE;
    } else if (sound == DEMO_SOUND_START) {
        preset = VOX_AUDIO_PRESET_SPAWN;
    } else if (sound == DEMO_SOUND_PAUSE) {
        preset = VOX_AUDIO_PRESET_UI_BACK;
    }
    demo_audio_emit(app, preset, 0U, VOX_AUDIO_PAN_CENTER);
}

static void demo_audio_render_block(demo_app *app, vox_i16 *samples,
                                    vox_u32 frames)
{
    if (app == 0 || samples == 0 || frames == 0U ||
        vox_audio_render(&app->audio, samples, frames) != VOX_OK) {
        if (samples != 0) {
            memset(samples, 0, (size_t)frames *
                   VOX_AUDIO_OUTPUT_CHANNELS * sizeof(*samples));
        }
    }
}

static void SDLCALL demo_audio_callback(void *userdata, Uint8 *stream,
                                        int length)
{
    demo_app *app = (demo_app *)userdata;
    vox_u32 frames;
    if (stream == 0 || length <= 0) return;
    frames = (vox_u32)length /
             ((vox_u32)sizeof(vox_i16) * VOX_AUDIO_OUTPUT_CHANNELS);
    demo_audio_render_block(app, (vox_i16 *)stream, frames);
}

static void demo_audio_open(demo_app *app)
{
    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;
    vox_audio_config config;
    if (app == 0 || SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        return;
    }
    memset(&desired, 0, sizeof(desired));
    memset(&obtained, 0, sizeof(obtained));
    desired.freq = DEMO_AUDIO_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = VOX_AUDIO_OUTPUT_CHANNELS;
    desired.samples = 512U;
    desired.callback = demo_audio_callback;
    desired.userdata = app;
    app->audio_device = SDL_OpenAudioDevice(0, 0, &desired, &obtained,
                                             SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (app->audio_device == 0U) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    vox_audio_config_init(&config, (vox_u32)obtained.freq, 0xD1655EEDU);
    config.master_gain_q15 = (vox_u16)((vox_u32)
        app->options.master_volume * VOX_AUDIO_GAIN_MAX / 10U);
    if (obtained.format != AUDIO_S16SYS || obtained.channels != 2U ||
        vox_audio_init_ex(&app->audio, &config) != VOX_OK) {
        SDL_CloseAudioDevice(app->audio_device);
        app->audio_device = 0U;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    SDL_PauseAudioDevice(app->audio_device, 0);
}

static void demo_audio_close(demo_app *app)
{
    if (app != 0 && app->audio_device != 0U) {
        SDL_PauseAudioDevice(app->audio_device, 1);
        demo_audio_lock(app);
        vox_audio_stop_all(&app->audio);
        demo_audio_unlock(app);
        SDL_CloseAudioDevice(app->audio_device);
        app->audio_device = 0U;
    }
}

static int demo_q16_to_screen(vox_i32 value, vox_u32 screen_size,
                              vox_u32 world_size)
{
    vox_i32 value_q8 = value / 256;
    return (int)((long)value_q8 * (long)screen_size /
                 (long)(world_size * 256U));
}

static int demo_weapon_is_allowed(vox_u16 mask, int weapon)
{
    if (weapon < 0 || weapon >= (int)VOX_DIGS_TOOL_COUNT) {
        return 0;
    }
    return (mask & (vox_u16)(1U << (unsigned int)weapon)) != 0U;
}

static int demo_first_weapon(vox_u16 mask)
{
    int weapon;
    for (weapon = 0; weapon < (int)VOX_DIGS_TOOL_COUNT; ++weapon) {
        if (demo_weapon_is_allowed(mask, weapon)) {
            return weapon;
        }
    }
    return VOX_DIGS_TOOL_PICK;
}

static void demo_cycle_weapon(demo_app *app, int player, int direction)
{
    int step;
    if (player < 0 || player >= (int)DEMO_LOCAL_MAX) {
        return;
    }
    for (step = 1; step <= (int)VOX_DIGS_TOOL_COUNT; ++step) {
        int weapon = app->selected_tool[player] + direction * step;
        while (weapon < 0) {
            weapon += (int)VOX_DIGS_TOOL_COUNT;
        }
        weapon %= (int)VOX_DIGS_TOOL_COUNT;
        if (demo_weapon_is_allowed(demo_match.rules.weapon_mask, weapon)) {
            app->selected_tool[player] = weapon;
            return;
        }
    }
}

static int demo_key_weapon(SDL_Keycode key)
{
    if (key >= SDLK_1 && key <= SDLK_9) {
        return (int)(key - SDLK_1);
    }
    if (key == SDLK_0) {
        return 9;
    }
    if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
        return VOX_DIGS_TOOL_RAIL_GUN;
    }
    return -1;
}

static void demo_bindings_default(demo_app *app)
{
    app->bindings.keyboard_left[0] = SDL_SCANCODE_A;
    app->bindings.keyboard_right[0] = SDL_SCANCODE_D;
    app->bindings.keyboard_jump[0] = SDL_SCANCODE_SPACE;
    app->bindings.keyboard_steam[0] = SDL_SCANCODE_LSHIFT;
    /* P1 ropes with the right mouse button.  A keyboard binding remains
     * available through Controls for players who deliberately want one. */
    app->bindings.keyboard_rope[0] = SDL_SCANCODE_UNKNOWN;
    app->bindings.keyboard_fire[0] = SDL_SCANCODE_E;
    app->bindings.keyboard_previous[0] = SDL_SCANCODE_Z;
    app->bindings.keyboard_next[0] = SDL_SCANCODE_X;
    app->bindings.keyboard_bark[0] = SDL_SCANCODE_C;
    app->bindings.keyboard_left[1] = SDL_SCANCODE_LEFT;
    app->bindings.keyboard_right[1] = SDL_SCANCODE_RIGHT;
    app->bindings.keyboard_jump[1] = SDL_SCANCODE_UP;
    app->bindings.keyboard_steam[1] = SDL_SCANCODE_RSHIFT;
    app->bindings.keyboard_rope[1] = SDL_SCANCODE_SLASH;
    app->bindings.keyboard_fire[1] = SDL_SCANCODE_RCTRL;
    app->bindings.keyboard_previous[1] = SDL_SCANCODE_COMMA;
    app->bindings.keyboard_next[1] = SDL_SCANCODE_PERIOD;
    app->bindings.keyboard_bark[1] = SDL_SCANCODE_M;
    app->bindings.pad_jump = SDL_CONTROLLER_BUTTON_A;
    app->bindings.pad_steam = SDL_CONTROLLER_BUTTON_X;
    app->bindings.pad_rope = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    app->bindings.pad_fire = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    app->bindings.pad_previous = SDL_CONTROLLER_BUTTON_Y;
    app->bindings.pad_next = SDL_CONTROLLER_BUTTON_B;
    app->bindings.pad_bark = SDL_CONTROLLER_BUTTON_RIGHTSTICK;
}

static int demo_controller_connected(const demo_controller *controller)
{
    return controller != 0 && controller->joystick != 0;
}

static int demo_ascii_lower(int character)
{
    if (character >= 'A' && character <= 'Z') return character + 32;
    return character;
}

static int demo_text_contains_ci(const char *text_value,
                                 const char *needle)
{
    int start;
    if (text_value == 0 || needle == 0 || needle[0] == '\0') return 0;
    for (start = 0; text_value[start] != '\0'; ++start) {
        int offset = 0;
        while (needle[offset] != '\0' && text_value[start + offset] != '\0' &&
               demo_ascii_lower((unsigned char)text_value[start + offset]) ==
               demo_ascii_lower((unsigned char)needle[offset])) {
            ++offset;
        }
        if (needle[offset] == '\0') return 1;
    }
    return 0;
}

static int demo_controller_family_from_name(const char *name)
{
    if (demo_text_contains_ci(name, "nintendo") ||
        demo_text_contains_ci(name, "switch") ||
        demo_text_contains_ci(name, "joy-con")) return DEMO_PAD_NINTENDO;
    if (demo_text_contains_ci(name, "playstation") ||
        demo_text_contains_ci(name, "dualshock") ||
        demo_text_contains_ci(name, "dualsense") ||
        demo_text_contains_ci(name, "sony")) return DEMO_PAD_PLAYSTATION;
    if (demo_text_contains_ci(name, "xbox") ||
        demo_text_contains_ci(name, "xinput") ||
        demo_text_contains_ci(name, "microsoft")) return DEMO_PAD_XBOX;
    return DEMO_PAD_GENERIC;
}

static int demo_controller_detect_family(SDL_GameController *handle,
                                         SDL_Joystick *joystick)
{
    const char *name = handle != 0 ? SDL_GameControllerName(handle) :
                                    SDL_JoystickName(joystick);
#if SDL_VERSION_ATLEAST(2, 0, 12)
    if (handle != 0) {
        SDL_GameControllerType type = SDL_GameControllerGetType(handle);
        if (type == SDL_CONTROLLER_TYPE_PS3 ||
            type == SDL_CONTROLLER_TYPE_PS4
#if SDL_VERSION_ATLEAST(2, 0, 14)
            || type == SDL_CONTROLLER_TYPE_PS5
#endif
            ) return DEMO_PAD_PLAYSTATION;
        /* Joy-Con enum values arrived after the older SDL2 headers still
         * shipped by supported Linux distributions.  The name fallback below
         * recognizes them there, while Switch Pro remains a direct type match.
         */
        if (type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO) {
            return DEMO_PAD_NINTENDO;
        }
        if (type == SDL_CONTROLLER_TYPE_XBOX360 ||
            type == SDL_CONTROLLER_TYPE_XBOXONE) return DEMO_PAD_XBOX;
    }
#endif
    return demo_controller_family_from_name(name);
}

static const char *demo_pad_button_label(int family,
                                         SDL_GameControllerButton button)
{
    if (family == DEMO_PAD_NINTENDO) {
        if (button == SDL_CONTROLLER_BUTTON_A) return "B";
        if (button == SDL_CONTROLLER_BUTTON_B) return "A";
        if (button == SDL_CONTROLLER_BUTTON_X) return "Y";
        if (button == SDL_CONTROLLER_BUTTON_Y) return "X";
        if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) return "L";
        if (button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) return "R";
        if (button == SDL_CONTROLLER_BUTTON_BACK) return "-";
        if (button == SDL_CONTROLLER_BUTTON_START) return "+";
        if (button == SDL_CONTROLLER_BUTTON_LEFTSTICK) return "LS";
        if (button == SDL_CONTROLLER_BUTTON_RIGHTSTICK) return "RS";
    } else if (family == DEMO_PAD_PLAYSTATION) {
        if (button == SDL_CONTROLLER_BUTTON_A) return "CROSS";
        if (button == SDL_CONTROLLER_BUTTON_B) return "CIRCLE";
        if (button == SDL_CONTROLLER_BUTTON_X) return "SQUARE";
        if (button == SDL_CONTROLLER_BUTTON_Y) return "TRIANGLE";
        if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) return "L1";
        if (button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) return "R1";
        if (button == SDL_CONTROLLER_BUTTON_BACK) return "SHARE";
        if (button == SDL_CONTROLLER_BUTTON_START) return "OPTIONS";
        if (button == SDL_CONTROLLER_BUTTON_LEFTSTICK) return "L3";
        if (button == SDL_CONTROLLER_BUTTON_RIGHTSTICK) return "R3";
    } else if (family == DEMO_PAD_XBOX) {
        if (button == SDL_CONTROLLER_BUTTON_A) return "A";
        if (button == SDL_CONTROLLER_BUTTON_B) return "B";
        if (button == SDL_CONTROLLER_BUTTON_X) return "X";
        if (button == SDL_CONTROLLER_BUTTON_Y) return "Y";
        if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) return "LB";
        if (button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) return "RB";
        if (button == SDL_CONTROLLER_BUTTON_BACK) return "VIEW";
        if (button == SDL_CONTROLLER_BUTTON_START) return "MENU";
        if (button == SDL_CONTROLLER_BUTTON_LEFTSTICK) return "L3";
        if (button == SDL_CONTROLLER_BUTTON_RIGHTSTICK) return "R3";
    } else {
        if (button == SDL_CONTROLLER_BUTTON_A) return "B1";
        if (button == SDL_CONTROLLER_BUTTON_B) return "B2";
        if (button == SDL_CONTROLLER_BUTTON_X) return "B3";
        if (button == SDL_CONTROLLER_BUTTON_Y) return "B4";
        if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) return "B5";
        if (button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) return "B6";
        if (button == SDL_CONTROLLER_BUTTON_BACK) return "SELECT";
        if (button == SDL_CONTROLLER_BUTTON_START) return "START";
        if (button == SDL_CONTROLLER_BUTTON_LEFTSTICK) return "B9";
        if (button == SDL_CONTROLLER_BUTTON_RIGHTSTICK) return "B10";
    }
    return "PAD";
}

static SDL_GameControllerButton demo_pad_accept_button(int family)
{
    return family == DEMO_PAD_NINTENDO ? SDL_CONTROLLER_BUTTON_B :
                                        SDL_CONTROLLER_BUTTON_A;
}

static SDL_GameControllerButton demo_pad_back_button(int family)
{
    return family == DEMO_PAD_NINTENDO ? SDL_CONTROLLER_BUTTON_A :
                                        SDL_CONTROLLER_BUTTON_B;
}

static int demo_prompt_family(demo_app *app)
{
    int player;
    int slot;
    for (player = 0; player < app->local_players; ++player) {
        if (app->player_input[player].active_source ==
            DEMO_SOURCE_CONTROLLER) {
            demo_controller *controller = demo_controller_for_player(app,
                                                                      player);
            if (controller != 0) return controller->family;
        }
    }
    for (slot = 0; slot < (int)DEMO_CONTROLLER_MAX; ++slot) {
        if (demo_controller_connected(&app->controllers[slot])) {
            return app->controllers[slot].family;
        }
    }
    return DEMO_PAD_GENERIC;
}

static void demo_options_defaults(demo_app *app)
{
    memset(&app->options, 0, sizeof(app->options));
    app->options.frame_cap_index = DEMO_FRAME_CAP_DEFAULT;
    app->options.gi_quality = VOX_GI_BALANCED;
    app->options.flash_mode = 2;
    app->options.gore_level = 2;
    app->options.camera_shake = 1;
    app->options.damage_numbers = 1;
    app->options.damage_number_size = 0;
    app->options.damage_number_color = 0;
    app->options.fx_profile = 1;
    app->options.master_volume = 8;
    app->options.laptop_mode = 0;
    app->options.dummy_mode = 0;
    app->options.haptic_level = 2;
    app->cap_supported_mask = ((vox_u32)1U << DEMO_FRAME_CAP_COUNT) - 1U;
    app->cap_qualified = 0;
}

static void demo_input_defaults(demo_app *app)
{
    int player;
    for (player = 0; player < (int)DEMO_LOCAL_MAX; ++player) {
        app->player_input[player].preference = DEMO_INPUT_AUTO;
        app->player_input[player].active_source = DEMO_SOURCE_KEYBOARD;
        app->player_input[player].sensitivity = 1;
        app->player_input[player].deadzone = 0;
        app->player_input[player].aim_slowdown = 1;
        app->player_input[player].rope_mode = DEMO_ROPE_HOLD;
        app->player_input[player].switch_stamp = 0U;
        app->player_input[player].suppress_ticks = 0;
        app->player_input[player].aim_direction_x = player == 0 ? 1.0 : -1.0;
        app->player_input[player].aim_direction_y = 0.0;
        app->player_input[player].aim_distance = 24.0;
        app->player_input[player].aim_magnitude = 1.0;
    }
}

static int demo_name_character_allowed(char character)
{
    return (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9') ||
           character == ' ' || character == '-' || character == '_';
}

static void demo_sanitize_name(char *name, const char *fallback)
{
    int read_index;
    int write_index;
    if (name == 0) return;
    write_index = 0;
    for (read_index = 0; name[read_index] != '\0' &&
         write_index < DEMO_NAME_CHARACTERS; ++read_index) {
        char character = name[read_index];
        if (character >= 'a' && character <= 'z') {
            character = (char)(character - 'a' + 'A');
        }
        if (demo_name_character_allowed(character)) {
            name[write_index++] = character;
        }
    }
    while (write_index > 0 && name[write_index - 1] == ' ') {
        --write_index;
    }
    name[write_index] = '\0';
    if (write_index == 0 && fallback != 0) {
        demo_copy_text(name, DEMO_NAME_CAPACITY, fallback);
    }
}

static void demo_refresh_roster(demo_app *app)
{
    int slot;
    int bot;
    for (slot = 0; slot < (int)VOX_DIGS_MAX_SLOTS; ++slot) {
        app->player_names[slot][0] = '\0';
    }
    for (slot = 0; slot < app->local_players &&
         slot < (int)DEMO_LOCAL_MAX; ++slot) {
        strcpy(app->player_names[slot], app->human_names[slot]);
    }
    for (bot = 0; bot < app->bots && bot < (int)VOX_DIGS_MAX_BOTS; ++bot) {
        slot = app->local_players + bot;
        if (slot < (int)VOX_DIGS_MAX_SLOTS) {
            strcpy(app->player_names[slot], app->bot_names[bot]);
        }
    }
}

static void demo_match_settings_defaults(demo_app *app)
{
    strcpy(app->human_names[0], "MINER 1");
    strcpy(app->human_names[1], "MINER 2");
    strcpy(app->bot_names[0], "RIVET");
    strcpy(app->bot_names[1], "CINDER");
    strcpy(app->bot_names[2], "FLAMEY");
    app->time_limit_index = 1;
    app->lava_index = 0;
    app->score_limit_index = 0;
    app->respawn_mode = 0;
    app->respawn_delay_index = 3;
    app->edit_name_slot = -1;
    demo_refresh_roster(app);
}

static int demo_input_settings_path(char *path, int capacity,
                                    const char *suffix)
{
    char *base;
    size_t required;
    if (path == 0 || capacity <= 0 || suffix == 0) return 0;
    if (demo_settings_override != 0) {
        required = strlen(demo_settings_override) + 1U;
        if (required > (size_t)capacity) return 0;
        strcpy(path, demo_settings_override);
        return 1;
    }
    base = SDL_GetPrefPath("Pinnacle Point Development", "DIGS");
    if (base == 0) return 0;
    required = strlen(base) + strlen(suffix) + 1U;
    if (required > (size_t)capacity) {
        SDL_free(base);
        return 0;
    }
    sprintf(path, "%s%s", base, suffix);
    SDL_free(base);
    return 1;
}

/*
 * Where the chronicle lives, next to settings.cfg.  The same override that
 * lets the tests point settings somewhere harmless points this too, so a
 * self-test never writes over a real player's history.
 */
static int demo_chronicle_path(char *path, int capacity)
{
    return demo_input_settings_path(path, capacity, "chronicle.dat");
}

/*
 * Take everything this match taught them and put it away.
 *
 * Called once when the match ends.  Export first -- the drift and the pair
 * accounts are computed there -- then write, atomically, so a crash halfway
 * through leaves the previous history intact rather than half of two.
 */
static void demo_chronicle_commit(void)
{
    char path[1032];
    if (!demo_chronicle_ready) {
        return;
    }
    if (vox_digs_match_export_memory(&demo_match,
                                     &demo_chronicle.memory) != VOX_OK) {
        return;
    }
    if (demo_chronicle.matches_recorded < 65535U) {
        demo_chronicle.matches_recorded++;
    }
    if (demo_chronicle_path(path, (int)sizeof(path))) {
        (void)digs_chronicle_save(&demo_chronicle, path);
    }
}


static void demo_validate_input_settings(demo_app *app)
{
    int player;
    int invalid_binding = 0;
    for (player = 0; player < (int)DEMO_LOCAL_MAX; ++player) {
        demo_player_input *input = &app->player_input[player];
        if (input->preference < DEMO_INPUT_AUTO ||
            input->preference > DEMO_INPUT_CONTROLLER) {
            input->preference = DEMO_INPUT_AUTO;
        }
        if (input->sensitivity < 0 || input->sensitivity > 2) {
            input->sensitivity = 1;
        }
        if (input->deadzone < 0 || input->deadzone > 3) {
            input->deadzone = 0;
        }
        if (input->aim_slowdown < 0 || input->aim_slowdown > 2) {
            input->aim_slowdown = 1;
        }
        if (input->rope_mode < DEMO_ROPE_HOLD ||
            input->rope_mode > DEMO_ROPE_TOGGLE) {
            input->rope_mode = DEMO_ROPE_HOLD;
        }
        input->active_source = input->preference == DEMO_INPUT_CONTROLLER ?
                               DEMO_SOURCE_CONTROLLER : DEMO_SOURCE_KEYBOARD;
        {
            int action;
            for (action = 0; action < 9; ++action) {
                SDL_Scancode *binding = demo_keyboard_binding(app, player,
                                                               action);
                if (binding == 0 || *binding <= SDL_SCANCODE_UNKNOWN ||
                    *binding >= SDL_NUM_SCANCODES) {
                    invalid_binding = 1;
                }
            }
        }
    }
    {
        int action;
        for (action = 2; action < 9; ++action) {
            SDL_GameControllerButton *binding = demo_pad_binding(app,
                                                                  action);
            if (binding == 0 ||
                *binding <= SDL_CONTROLLER_BUTTON_INVALID ||
                *binding >= SDL_CONTROLLER_BUTTON_MAX) {
                invalid_binding = 1;
            }
        }
    }
    if (invalid_binding) demo_bindings_default(app);
    if (app->options.master_volume < 0 ||
        app->options.master_volume > 10) {
        app->options.master_volume = 8;
    }
    if (app->options.frame_cap_index < 0 ||
        app->options.frame_cap_index >= DEMO_FRAME_CAP_COUNT) {
        app->options.frame_cap_index = DEMO_FRAME_CAP_DEFAULT;
    }
    if (app->options.gi_quality > (int)VOX_GI_SHOWCASE) {
        app->options.gi_quality = VOX_GI_BALANCED;
    }
    if (app->options.flash_mode < 0 || app->options.flash_mode > 2) {
        app->options.flash_mode = 2;
    }
    if (app->options.gore_level < 0 || app->options.gore_level > 2) {
        app->options.gore_level = 2;
    }
    if (app->options.fx_profile < 0 || app->options.fx_profile > 2) {
        app->options.fx_profile = 1;
    }
    app->options.debug = app->options.debug != 0;
    app->options.fullscreen = app->options.fullscreen != 0;
    app->options.camera_shake = app->options.camera_shake != 0;
    app->options.damage_numbers = app->options.damage_numbers != 0;
    app->options.damage_number_size = app->options.damage_number_size != 0;
    app->options.damage_number_color = app->options.damage_number_color != 0;
    app->options.laptop_mode = app->options.laptop_mode != 0;
    app->options.dummy_mode = app->options.dummy_mode != 0;
    if (app->options.haptic_level < 0 ||
        app->options.haptic_level >= DEMO_HAPTIC_LEVEL_COUNT) {
        app->options.haptic_level = 2;
    }
    if (app->cap_cache_us > 10000000U ||
        (app->cap_cache_mask & ~(((vox_u32)1U <<
                                  DEMO_FRAME_CAP_COUNT) - 1U)) != 0U) {
        app->cap_cache_profile = 0U;
        app->cap_cache_us = 0U;
        app->cap_cache_mask = 0U;
    }
    if (app->time_limit_index < 0 || app->time_limit_index > 5) {
        app->time_limit_index = 1;
    }
    if (app->lava_index < 0 || app->lava_index > 4) {
        app->lava_index = 0;
    }
    if (app->score_limit_index < 0 || app->score_limit_index > 3) {
        app->score_limit_index = 0;
    }
    if (app->respawn_mode < 0 || app->respawn_mode > 1) {
        app->respawn_mode = 0;
    }
    if (app->respawn_delay_index < 0 ||
        app->respawn_delay_index > 4) {
        app->respawn_delay_index = 3;
    }
    demo_sanitize_name(app->human_names[0], "MINER 1");
    demo_sanitize_name(app->human_names[1], "MINER 2");
    demo_refresh_roster(app);
}

static int demo_load_input_settings(demo_app *app)
{
    char path[1024];
    char line[128];
    FILE *file;
    demo_player_input saved_input[DEMO_LOCAL_MAX];
    demo_options saved_options;
    demo_bindings saved_bindings;
    char saved_human_names[DEMO_LOCAL_MAX][DEMO_NAME_CAPACITY];
    int saved_time_limit_index;
    int saved_lava_index;
    int saved_score_limit_index;
    int saved_respawn_mode;
    int saved_respawn_delay_index;
    vox_u32 saved_cap_profile;
    vox_u32 saved_cap_us;
    vox_u32 saved_cap_mask;
    int version = 0;
    if (!demo_input_settings_path(path, (int)sizeof(path), "settings.cfg")) {
        return 0;
    }
    file = fopen(path, "r");
    if (file == 0) return 0;
    memcpy(saved_input, app->player_input, sizeof(saved_input));
    saved_options = app->options;
    saved_bindings = app->bindings;
    memcpy(saved_human_names, app->human_names,
           sizeof(saved_human_names));
    saved_time_limit_index = app->time_limit_index;
    saved_lava_index = app->lava_index;
    saved_score_limit_index = app->score_limit_index;
    saved_respawn_mode = app->respawn_mode;
    saved_respawn_delay_index = app->respawn_delay_index;
    saved_cap_profile = app->cap_cache_profile;
    saved_cap_us = app->cap_cache_us;
    saved_cap_mask = app->cap_cache_mask;
    while (fgets(line, (int)sizeof(line), file) != 0) {
        int value;
        unsigned long unsigned_value;
        if (sscanf(line, "DIGS_SETTINGS=%d", &value) == 1 ||
            sscanf(line, "DIGS_INPUT_SETTINGS=%d", &value) == 1) {
            version = value;
        } else if (sscanf(line, "P1_MODE=%d", &value) == 1) {
            app->player_input[0].preference = value;
        } else if (sscanf(line, "P1_SENSITIVITY=%d", &value) == 1) {
            app->player_input[0].sensitivity = value;
        } else if (sscanf(line, "P1_DEADZONE=%d", &value) == 1) {
            app->player_input[0].deadzone = value;
        } else if (sscanf(line, "P1_SLOWDOWN=%d", &value) == 1) {
            app->player_input[0].aim_slowdown = value;
        } else if (sscanf(line, "P1_ROPE_MODE=%d", &value) == 1) {
            app->player_input[0].rope_mode = value;
        } else if (sscanf(line, "P2_MODE=%d", &value) == 1) {
            app->player_input[1].preference = value;
        } else if (sscanf(line, "P2_SENSITIVITY=%d", &value) == 1) {
            app->player_input[1].sensitivity = value;
        } else if (sscanf(line, "P2_DEADZONE=%d", &value) == 1) {
            app->player_input[1].deadzone = value;
        } else if (sscanf(line, "P2_SLOWDOWN=%d", &value) == 1) {
            app->player_input[1].aim_slowdown = value;
        } else if (sscanf(line, "P2_ROPE_MODE=%d", &value) == 1) {
            app->player_input[1].rope_mode = value;
        } else if (sscanf(line, "MASTER_VOLUME=%d", &value) == 1) {
            app->options.master_volume = value;
        } else if (sscanf(line, "FRAME_CAP_INDEX=%d", &value) == 1) {
            app->options.frame_cap_index = value;
        } else if (sscanf(line, "GI_QUALITY=%d", &value) == 1) {
            app->options.gi_quality = value;
        } else if (sscanf(line, "DEBUG=%d", &value) == 1) {
            app->options.debug = value;
        } else if (sscanf(line, "FULLSCREEN=%d", &value) == 1) {
            app->options.fullscreen = value;
        } else if (sscanf(line, "FLASH_MODE=%d", &value) == 1) {
            app->options.flash_mode = value;
        } else if (sscanf(line, "GORE_LEVEL=%d", &value) == 1) {
            app->options.gore_level = value;
        } else if (sscanf(line, "CAMERA_SHAKE=%d", &value) == 1) {
            app->options.camera_shake = value;
        } else if (sscanf(line, "DAMAGE_NUMBERS=%d", &value) == 1) {
            app->options.damage_numbers = value;
        } else if (sscanf(line, "DAMAGE_NUMBER_SIZE=%d", &value) == 1) {
            app->options.damage_number_size = value;
        } else if (sscanf(line, "DAMAGE_NUMBER_COLOR=%d", &value) == 1) {
            app->options.damage_number_color = value;
        } else if (sscanf(line, "FX_PROFILE=%d", &value) == 1) {
            app->options.fx_profile = value;
        } else if (sscanf(line, "LAPTOP_MODE=%d", &value) == 1) {
            app->options.laptop_mode = value;
        } else if (sscanf(line, "DUMMY_MODE=%d", &value) == 1) {
            app->options.dummy_mode = value;
        } else if (sscanf(line, "HAPTIC_LEVEL=%d", &value) == 1) {
            app->options.haptic_level = value;
        } else if (sscanf(line, "CAP_PROFILE=%lu", &unsigned_value) == 1) {
            app->cap_cache_profile = (vox_u32)unsigned_value;
        } else if (sscanf(line, "CAP_FRAME_US=%lu", &unsigned_value) == 1) {
            app->cap_cache_us = (vox_u32)unsigned_value;
        } else if (sscanf(line, "CAP_SUPPORTED_MASK=%lu",
                          &unsigned_value) == 1) {
            app->cap_cache_mask = (vox_u32)unsigned_value;
        } else if (sscanf(line, "TIME_LIMIT_INDEX=%d", &value) == 1) {
            app->time_limit_index = value;
        } else if (sscanf(line, "LAVA_INDEX=%d", &value) == 1) {
            app->lava_index = value;
        } else if (sscanf(line, "MATCH_MINUTES=%d", &value) == 1) {
            /* Schema 4 and earlier stored the minutes directly. */
            app->time_limit_index = value == 3 ? 2 : 1;
        } else if (sscanf(line, "SCORE_LIMIT_INDEX=%d", &value) == 1) {
            app->score_limit_index = value;
        } else if (sscanf(line, "RESPAWN_MODE=%d", &value) == 1) {
            app->respawn_mode = value;
        } else if (sscanf(line, "RESPAWN_DELAY_INDEX=%d", &value) == 1) {
            app->respawn_delay_index = value;
        } else if (strncmp(line, "P1_NAME=", 8U) == 0) {
            demo_copy_text(app->human_names[0], DEMO_NAME_CAPACITY,
                           line + 8);
        } else if (strncmp(line, "P2_NAME=", 8U) == 0) {
            demo_copy_text(app->human_names[1], DEMO_NAME_CAPACITY,
                           line + 8);
        } else if (sscanf(line, "P1_BARK=%d", &value) == 1) {
            app->bindings.keyboard_bark[0] = (SDL_Scancode)value;
        } else if (sscanf(line, "P2_BARK=%d", &value) == 1) {
            app->bindings.keyboard_bark[1] = (SDL_Scancode)value;
        } else if (sscanf(line, "PAD_BARK=%d", &value) == 1) {
            app->bindings.pad_bark = (SDL_GameControllerButton)value;
        } else if (sscanf(line, "P1_KEY_LEFT=%d", &value) == 1) {
            app->bindings.keyboard_left[0] = (SDL_Scancode)value;
        } else if (sscanf(line, "P1_KEY_RIGHT=%d", &value) == 1) {
            app->bindings.keyboard_right[0] = (SDL_Scancode)value;
        } else if (sscanf(line, "P1_KEY_JUMP=%d", &value) == 1) {
            app->bindings.keyboard_jump[0] = (SDL_Scancode)value;
        } else if (sscanf(line, "P1_KEY_STEAM=%d", &value) == 1) {
            app->bindings.keyboard_steam[0] = (SDL_Scancode)value;
        } else if (sscanf(line, "P1_KEY_ROPE=%d", &value) == 1) {
            app->bindings.keyboard_rope[0] = (SDL_Scancode)value;
        } else if (sscanf(line, "P1_KEY_FIRE=%d", &value) == 1) {
            app->bindings.keyboard_fire[0] = (SDL_Scancode)value;
        } else if (sscanf(line, "P1_KEY_PREV=%d", &value) == 1) {
            app->bindings.keyboard_previous[0] = (SDL_Scancode)value;
        } else if (sscanf(line, "P1_KEY_NEXT=%d", &value) == 1) {
            app->bindings.keyboard_next[0] = (SDL_Scancode)value;
        } else if (sscanf(line, "P2_KEY_LEFT=%d", &value) == 1) {
            app->bindings.keyboard_left[1] = (SDL_Scancode)value;
        } else if (sscanf(line, "P2_KEY_RIGHT=%d", &value) == 1) {
            app->bindings.keyboard_right[1] = (SDL_Scancode)value;
        } else if (sscanf(line, "P2_KEY_JUMP=%d", &value) == 1) {
            app->bindings.keyboard_jump[1] = (SDL_Scancode)value;
        } else if (sscanf(line, "P2_KEY_STEAM=%d", &value) == 1) {
            app->bindings.keyboard_steam[1] = (SDL_Scancode)value;
        } else if (sscanf(line, "P2_KEY_ROPE=%d", &value) == 1) {
            app->bindings.keyboard_rope[1] = (SDL_Scancode)value;
        } else if (sscanf(line, "P2_KEY_FIRE=%d", &value) == 1) {
            app->bindings.keyboard_fire[1] = (SDL_Scancode)value;
        } else if (sscanf(line, "P2_KEY_PREV=%d", &value) == 1) {
            app->bindings.keyboard_previous[1] = (SDL_Scancode)value;
        } else if (sscanf(line, "P2_KEY_NEXT=%d", &value) == 1) {
            app->bindings.keyboard_next[1] = (SDL_Scancode)value;
        } else if (sscanf(line, "PAD_JUMP=%d", &value) == 1) {
            app->bindings.pad_jump = (SDL_GameControllerButton)value;
        } else if (sscanf(line, "PAD_STEAM=%d", &value) == 1) {
            app->bindings.pad_steam = (SDL_GameControllerButton)value;
        } else if (sscanf(line, "PAD_ROPE=%d", &value) == 1) {
            app->bindings.pad_rope = (SDL_GameControllerButton)value;
        } else if (sscanf(line, "PAD_FIRE=%d", &value) == 1) {
            app->bindings.pad_fire = (SDL_GameControllerButton)value;
        } else if (sscanf(line, "PAD_PREV=%d", &value) == 1) {
            app->bindings.pad_previous = (SDL_GameControllerButton)value;
        } else if (sscanf(line, "PAD_NEXT=%d", &value) == 1) {
            app->bindings.pad_next = (SDL_GameControllerButton)value;
        }
    }
    (void)fclose(file);
    /*
     * Every schema this build knows how to read, not just the current one.
     * Bumping DEMO_SETTINGS_VERSION without adding the outgoing number here
     * silently discards the settings of everybody upgrading -- which is what
     * happened to schema four the moment five was introduced.
     */
    if (version != 1 && version != 2 && version != 3 && version != 4 &&
        version != DEMO_SETTINGS_VERSION) {
        memcpy(app->player_input, saved_input, sizeof(saved_input));
        app->options = saved_options;
        app->bindings = saved_bindings;
        memcpy(app->human_names, saved_human_names,
               sizeof(saved_human_names));
        app->time_limit_index = saved_time_limit_index;
        app->lava_index = saved_lava_index;
        app->score_limit_index = saved_score_limit_index;
        app->respawn_mode = saved_respawn_mode;
        app->respawn_delay_index = saved_respawn_delay_index;
        app->cap_cache_profile = saved_cap_profile;
        app->cap_cache_us = saved_cap_us;
        app->cap_cache_mask = saved_cap_mask;
        demo_refresh_roster(app);
        if (version > DEMO_SETTINGS_VERSION) {
            app->settings_writable = 0;
        }
        return 0;
    }
    /* v0.0.4 moved the default P1 rope control from Q to the mouse's right
     * button.  Old default configs must not silently keep the retired key. */
    if (version < DEMO_SETTINGS_VERSION &&
        app->bindings.keyboard_rope[0] == SDL_SCANCODE_Q) {
        app->bindings.keyboard_rope[0] = SDL_SCANCODE_UNKNOWN;
    }
    demo_validate_input_settings(app);
    return 1;
}

static int demo_save_input_settings(demo_app *app)
{
    char path[1024];
    char temporary[1032];
    char backup[1032];
    FILE *file;
    int player;
    if (!app->settings_writable ||
        !demo_input_settings_path(path, (int)sizeof(path), "settings.cfg")) {
        return 0;
    }
    if (strlen(path) + 5U >= sizeof(temporary)) return 0;
    sprintf(temporary, "%s.tmp", path);
    file = fopen(temporary, "w");
    if (file == 0) return 0;
    if (fprintf(file, "DIGS_SETTINGS=%d\n",
                DEMO_SETTINGS_VERSION) < 0) {
        (void)fclose(file);
        (void)remove(temporary);
        return 0;
    }
    for (player = 0; player < (int)DEMO_LOCAL_MAX; ++player) {
        const demo_player_input *input = &app->player_input[player];
        if (fprintf(file,
                    "P%d_MODE=%d\nP%d_SENSITIVITY=%d\n"
                    "P%d_DEADZONE=%d\nP%d_SLOWDOWN=%d\n"
                    "P%d_ROPE_MODE=%d\n",
                    player + 1, input->preference,
                    player + 1, input->sensitivity,
                    player + 1, input->deadzone,
                    player + 1, input->aim_slowdown,
                    player + 1, input->rope_mode) < 0) {
            (void)fclose(file);
            (void)remove(temporary);
            return 0;
        }
    }
    if (fprintf(file,
                "P1_KEY_LEFT=%d\nP1_KEY_RIGHT=%d\nP1_KEY_JUMP=%d\n"
                "P1_KEY_STEAM=%d\nP1_KEY_ROPE=%d\nP1_KEY_FIRE=%d\n"
                "P1_KEY_PREV=%d\nP1_KEY_NEXT=%d\n"
                "P2_KEY_LEFT=%d\nP2_KEY_RIGHT=%d\nP2_KEY_JUMP=%d\n"
                "P2_KEY_STEAM=%d\nP2_KEY_ROPE=%d\nP2_KEY_FIRE=%d\n"
                "P2_KEY_PREV=%d\nP2_KEY_NEXT=%d\n"
                "PAD_JUMP=%d\nPAD_STEAM=%d\nPAD_ROPE=%d\n"
                "PAD_FIRE=%d\nPAD_PREV=%d\nPAD_NEXT=%d\n",
                (int)app->bindings.keyboard_left[0],
                (int)app->bindings.keyboard_right[0],
                (int)app->bindings.keyboard_jump[0],
                (int)app->bindings.keyboard_steam[0],
                (int)app->bindings.keyboard_rope[0],
                (int)app->bindings.keyboard_fire[0],
                (int)app->bindings.keyboard_previous[0],
                (int)app->bindings.keyboard_next[0],
                (int)app->bindings.keyboard_left[1],
                (int)app->bindings.keyboard_right[1],
                (int)app->bindings.keyboard_jump[1],
                (int)app->bindings.keyboard_steam[1],
                (int)app->bindings.keyboard_rope[1],
                (int)app->bindings.keyboard_fire[1],
                (int)app->bindings.keyboard_previous[1],
                (int)app->bindings.keyboard_next[1],
                (int)app->bindings.pad_jump,
                (int)app->bindings.pad_steam,
                (int)app->bindings.pad_rope,
                (int)app->bindings.pad_fire,
                (int)app->bindings.pad_previous,
                (int)app->bindings.pad_next) < 0) {
        (void)fclose(file);
        (void)remove(temporary);
        return 0;
    }
    if (fprintf(file,
                "FRAME_CAP_INDEX=%d\nGI_QUALITY=%d\nDEBUG=%d\n"
                "FULLSCREEN=%d\nFLASH_MODE=%d\nGORE_LEVEL=%d\n"
                "CAMERA_SHAKE=%d\nDAMAGE_NUMBERS=%d\n"
                "DAMAGE_NUMBER_SIZE=%d\nDAMAGE_NUMBER_COLOR=%d\n"
                "FX_PROFILE=%d\nMASTER_VOLUME=%d\nLAPTOP_MODE=%d\n"
                "DUMMY_MODE=%d\nHAPTIC_LEVEL=%d\nTIME_LIMIT_INDEX=%d\n"
                "CAP_PROFILE=%lu\nCAP_FRAME_US=%lu\n"
                "CAP_SUPPORTED_MASK=%lu\n"
                "LAVA_INDEX=%d\nSCORE_LIMIT_INDEX=%d\nRESPAWN_MODE=%d\n"
                "RESPAWN_DELAY_INDEX=%d\nP1_NAME=%s\nP2_NAME=%s\n"
                "P1_BARK=%d\nP2_BARK=%d\nPAD_BARK=%d\n",
                app->options.frame_cap_index, app->options.gi_quality,
                app->options.debug, app->options.fullscreen,
                app->options.flash_mode, app->options.gore_level,
                app->options.camera_shake, app->options.damage_numbers,
                app->options.damage_number_size,
                app->options.damage_number_color, app->options.fx_profile,
                app->options.master_volume, app->options.laptop_mode,
                app->options.dummy_mode, app->options.haptic_level,
                app->time_limit_index,
                (unsigned long)app->cap_cache_profile,
                (unsigned long)app->cap_cache_us,
                (unsigned long)app->cap_cache_mask,
                app->lava_index, app->score_limit_index,
                app->respawn_mode,
                app->respawn_delay_index, app->human_names[0],
                app->human_names[1],
                (int)app->bindings.keyboard_bark[0],
                (int)app->bindings.keyboard_bark[1],
                (int)app->bindings.pad_bark) < 0) {
        (void)fclose(file);
        (void)remove(temporary);
        return 0;
    }
    if (fclose(file) != 0) {
        (void)remove(temporary);
        return 0;
    }
    if (rename(temporary, path) != 0) {
        /* ISO C rename replaces atomically on POSIX. Older Windows CRTs do
         * not. Move the old file aside so a failed replacement can restore
         * it instead of deleting the last readable preferences. */
        if (strlen(path) + 5U >= sizeof(backup)) {
            (void)remove(temporary);
            return 0;
        }
        sprintf(backup, "%s.bak", path);
        (void)remove(backup);
        if (rename(path, backup) != 0 || rename(temporary, path) != 0) {
            (void)rename(backup, path);
            (void)remove(temporary);
            return 0;
        }
        (void)remove(backup);
    }
    return 1;
}

static void demo_refresh_controller_claims(demo_app *app)
{
    int slots[DEMO_CONTROLLER_MAX];
    int total = 0;
    int index;
    int player;
    for (index = 0; index < (int)DEMO_CONTROLLER_MAX; ++index) {
        app->controllers[index].claimed_player = -1;
        if (demo_controller_connected(&app->controllers[index])) {
            slots[total++] = index;
        }
    }
    if (total == 0) {
        for (player = 0; player < (int)DEMO_LOCAL_MAX; ++player) {
            if (app->player_input[player].preference == DEMO_INPUT_AUTO) {
                app->player_input[player].active_source =
                    DEMO_SOURCE_KEYBOARD;
            }
        }
        return;
    }
    if (app->local_players < 2) {
        if (app->player_input[0].preference != DEMO_INPUT_KEYBOARD) {
            app->controllers[slots[0]].claimed_player = 0;
        }
    } else if (total == 1) {
        int claim = -1;
        if (app->player_input[0].preference == DEMO_INPUT_CONTROLLER &&
            app->player_input[1].preference != DEMO_INPUT_CONTROLLER) {
            claim = 0;
        } else if (app->player_input[1].preference ==
                   DEMO_INPUT_CONTROLLER) {
            claim = 1;
        } else if (app->player_input[1].preference == DEMO_INPUT_KEYBOARD &&
                   app->player_input[0].preference != DEMO_INPUT_KEYBOARD) {
            claim = 0;
        } else if (app->player_input[1].preference != DEMO_INPUT_KEYBOARD) {
            claim = 1;
        }
        app->controllers[slots[0]].claimed_player = claim;
    } else {
        int next_slot = 0;
        for (player = 0; player < app->local_players &&
             player < (int)DEMO_LOCAL_MAX; ++player) {
            if (app->player_input[player].preference != DEMO_INPUT_KEYBOARD &&
                next_slot < total) {
                app->controllers[slots[next_slot]].claimed_player = player;
                ++next_slot;
            }
        }
    }
    for (player = 0; player < (int)DEMO_LOCAL_MAX; ++player) {
        int has_controller = 0;
        for (index = 0; index < total; ++index) {
            if (app->controllers[slots[index]].claimed_player == player) {
                has_controller = 1;
            }
        }
        if (!has_controller &&
            app->player_input[player].preference == DEMO_INPUT_AUTO) {
            app->player_input[player].active_source = DEMO_SOURCE_KEYBOARD;
        }
    }
}

static int demo_open_controller(demo_app *app, int device_index)
{
    SDL_GameController *controller = 0;
    SDL_Joystick *joystick;
    SDL_JoystickID instance;
    int slot;
    int raw_fallback = 0;
    if (SDL_IsGameController(device_index)) {
        controller = SDL_GameControllerOpen(device_index);
        if (controller == 0) return 0;
        joystick = SDL_GameControllerGetJoystick(controller);
    } else {
        joystick = SDL_JoystickOpen(device_index);
        raw_fallback = 1;
        if (joystick == 0) return 0;
    }
    instance = SDL_JoystickInstanceID(joystick);
    /* SDL can queue DEVICEADDED while startup enumeration is opening the
     * same pad.  Never let one physical controller occupy both local slots. */
    for (slot = 0; slot < (int)DEMO_CONTROLLER_MAX; ++slot) {
        if (demo_controller_connected(&app->controllers[slot]) &&
            app->controllers[slot].instance_id == instance) {
            if (controller != 0) SDL_GameControllerClose(controller);
            else SDL_JoystickClose(joystick);
            return 1;
        }
    }
    for (slot = 0; slot < (int)DEMO_CONTROLLER_MAX; ++slot) {
        if (!demo_controller_connected(&app->controllers[slot])) {
            int axis;
            int axis_count = SDL_JoystickNumAxes(joystick);
            app->controllers[slot].handle = controller;
            app->controllers[slot].joystick = joystick;
            app->controllers[slot].instance_id = instance;
            app->controllers[slot].claimed_player = -1;
            app->controllers[slot].raw_fallback = raw_fallback;
            app->controllers[slot].family =
                demo_controller_detect_family(controller, joystick);
            app->controllers[slot].zoom_axis_state = 0;
            app->controllers[slot].auto_deadzone = 0.20;
            app->controllers[slot].activity_frames = 0;
            app->controllers[slot].calibrating = 1;
            app->controllers[slot].calibration_stamp = SDL_GetTicks();
            app->controllers[slot].calibration_samples = 0;
            app->controllers[slot].calibration_peak = 0;
            for (axis = 0; axis < 4; ++axis) {
                app->controllers[slot].axis_center[axis] =
                    raw_fallback && axis < axis_count ?
                    (int)SDL_JoystickGetAxis(joystick, axis) : 0;
                app->controllers[slot].calibration_total[axis] = 0L;
            }
            demo_refresh_controller_claims(app);
            return 1;
        }
    }
    if (controller != 0) SDL_GameControllerClose(controller);
    else SDL_JoystickClose(joystick);
    return 0;
}

static void demo_close_controller(demo_app *app, SDL_JoystickID instance)
{
    int slot;
    for (slot = 0; slot < (int)DEMO_CONTROLLER_MAX; ++slot) {
        if (demo_controller_connected(&app->controllers[slot]) &&
            app->controllers[slot].instance_id == instance) {
            int claimed_player = app->controllers[slot].claimed_player;
            if (app->controllers[slot].raw_fallback) {
                SDL_JoystickClose(app->controllers[slot].joystick);
            } else {
                SDL_GameControllerClose(app->controllers[slot].handle);
            }
            app->controllers[slot].handle = 0;
            app->controllers[slot].joystick = 0;
            app->controllers[slot].instance_id = -1;
            app->controllers[slot].claimed_player = -1;
            if (claimed_player >= 0 && claimed_player <
                (int)DEMO_LOCAL_MAX) {
                memset(&app->haptic[claimed_player], 0,
                       sizeof(app->haptic[claimed_player]));
            }
            if (app->screen == DEMO_PLAY && claimed_player >= 0 &&
                claimed_player < app->local_players &&
                claimed_player < (int)DEMO_LOCAL_MAX) {
                demo_player_input *input =
                    &app->player_input[claimed_player];
                if (input->preference == DEMO_INPUT_CONTROLLER) {
                    app->screen = DEMO_PAUSE;
                    app->selection = 0;
                    app->controller_disconnected = 1;
                } else {
                    input->active_source = DEMO_SOURCE_KEYBOARD;
                    input->suppress_ticks = 1;
                }
            }
        }
    }
    demo_refresh_controller_claims(app);
}

static demo_controller *demo_controller_by_instance(
                                      demo_app *app,
                                      SDL_JoystickID instance)
{
    int slot;
    for (slot = 0; slot < (int)DEMO_CONTROLLER_MAX; ++slot) {
        if (demo_controller_connected(&app->controllers[slot]) &&
            app->controllers[slot].instance_id == instance) {
            return &app->controllers[slot];
        }
    }
    return 0;
}

static void demo_close_controllers(demo_app *app)
{
    int slot;
    for (slot = 0; slot < (int)DEMO_CONTROLLER_MAX; ++slot) {
        if (demo_controller_connected(&app->controllers[slot])) {
            if (app->controllers[slot].raw_fallback) {
                SDL_JoystickClose(app->controllers[slot].joystick);
            } else {
                SDL_GameControllerClose(app->controllers[slot].handle);
            }
            app->controllers[slot].handle = 0;
            app->controllers[slot].joystick = 0;
        }
        app->controllers[slot].instance_id = -1;
        app->controllers[slot].claimed_player = -1;
    }
}

static Sint16 demo_controller_axis(const demo_controller *controller,
                                   SDL_GameControllerAxis axis)
{
    int raw_axis;
    int value;
    if (!demo_controller_connected(controller)) return 0;
    if (!controller->raw_fallback) {
        return SDL_GameControllerGetAxis(controller->handle, axis);
    }
    raw_axis = -1;
    if (axis == SDL_CONTROLLER_AXIS_LEFTX) raw_axis = 0;
    else if (axis == SDL_CONTROLLER_AXIS_LEFTY) raw_axis = 1;
    else if (axis == SDL_CONTROLLER_AXIS_RIGHTX) raw_axis = 2;
    else if (axis == SDL_CONTROLLER_AXIS_RIGHTY) raw_axis = 3;
    else if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) raw_axis = 4;
    else if (axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) raw_axis = 5;
    if (raw_axis < 0 || raw_axis >= SDL_JoystickNumAxes(controller->joystick)) {
        return 0;
    }
    value = (int)SDL_JoystickGetAxis(controller->joystick, raw_axis);
    if (raw_axis < 4) value -= controller->axis_center[raw_axis];
    if (value < -32767) value = -32767;
    if (value > 32767) value = 32767;
    return (Sint16)value;
}

static int demo_controller_button(const demo_controller *controller,
                                  SDL_GameControllerButton button)
{
    int raw_button = -1;
    Uint8 hat;
    if (!demo_controller_connected(controller)) return 0;
    if (!controller->raw_fallback) {
        return SDL_GameControllerGetButton(controller->handle, button) != 0;
    }
    if (button == SDL_CONTROLLER_BUTTON_A) raw_button = 0;
    else if (button == SDL_CONTROLLER_BUTTON_B) raw_button = 1;
    else if (button == SDL_CONTROLLER_BUTTON_X) raw_button = 2;
    else if (button == SDL_CONTROLLER_BUTTON_Y) raw_button = 3;
    else if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) raw_button = 4;
    else if (button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) raw_button = 5;
    else if (button == SDL_CONTROLLER_BUTTON_BACK) raw_button = 6;
    else if (button == SDL_CONTROLLER_BUTTON_START) raw_button = 7;
    else if (button == SDL_CONTROLLER_BUTTON_LEFTSTICK) raw_button = 8;
    else if (button == SDL_CONTROLLER_BUTTON_RIGHTSTICK) raw_button = 9;
    if (raw_button >= 0 &&
        raw_button < SDL_JoystickNumButtons(controller->joystick)) {
        return SDL_JoystickGetButton(controller->joystick, raw_button) != 0;
    }
    if (SDL_JoystickNumHats(controller->joystick) <= 0) return 0;
    hat = SDL_JoystickGetHat(controller->joystick, 0);
    if (button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
        return (hat & SDL_HAT_LEFT) != 0;
    }
    if (button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
        return (hat & SDL_HAT_RIGHT) != 0;
    }
    if (button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
        return (hat & SDL_HAT_UP) != 0;
    }
    if (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
        return (hat & SDL_HAT_DOWN) != 0;
    }
    return 0;
}

static double demo_sqrt(double value)
{
    double root;
    int iteration;
    if (value <= 0.0) return 0.0;
    root = value > 1.0 ? value : 1.0;
    for (iteration = 0; iteration < 8; ++iteration) {
        root = 0.5 * (root + value / root);
    }
    return root;
}

static double demo_input_deadzone(const demo_player_input *input,
                                  const demo_controller *controller)
{
    if (input->deadzone == 1) return 0.12;
    if (input->deadzone == 2) return 0.20;
    if (input->deadzone == 3) return 0.30;
    if (controller != 0 && controller->auto_deadzone >= 0.12 &&
        controller->auto_deadzone <= 0.35) {
        return controller->auto_deadzone;
    }
    return 0.20;
}

static int demo_radial_response(Sint16 raw_x, Sint16 raw_y,
                                double deadzone, int sensitivity,
                                double *output_x, double *output_y,
                                double *output_magnitude)
{
    double x = (double)raw_x / 32767.0;
    double y = (double)raw_y / 32767.0;
    double magnitude = demo_sqrt(x * x + y * y);
    double scaled;
    if (magnitude <= deadzone || magnitude <= 0.0001) {
        *output_x = 0.0;
        *output_y = 0.0;
        *output_magnitude = 0.0;
        return 0;
    }
    if (magnitude > 0.95) magnitude = 0.95;
    scaled = (magnitude - deadzone) / (0.95 - deadzone);
    if (scaled < 0.0) scaled = 0.0;
    if (scaled > 1.0) scaled = 1.0;
    if (sensitivity <= 0) {
        scaled = scaled * scaled;
    } else if (sensitivity == 1) {
        scaled = scaled * (0.35 + scaled * 0.65);
    }
    *output_x = x / demo_sqrt(x * x + y * y);
    *output_y = y / demo_sqrt(x * x + y * y);
    *output_magnitude = scaled;
    return 1;
}

static void demo_reset_input_edges(demo_app *app, int player)
{
    int slot;
    if (player < 0 || player >= (int)DEMO_LOCAL_MAX) return;
    app->keyboard_previous_down[player] = 0;
    app->keyboard_next_down[player] = 0;
    app->player_input[player].suppress_ticks = 1;
    for (slot = 0; slot < (int)DEMO_CONTROLLER_MAX; ++slot) {
        if (app->controllers[slot].claimed_player == player) {
            app->controllers[slot].activity_frames = 0;
        }
    }
}

static void demo_change_zoom(demo_app *app, int direction)
{
    int previous;
    if (app == 0 || direction == 0) return;
    previous = app->camera_zoom;
    app->camera_zoom += direction > 0 ? 1 : -1;
    if (app->camera_zoom < DEMO_CAMERA_ZOOM_MIN) {
        app->camera_zoom = DEMO_CAMERA_ZOOM_MIN;
    } else if (app->camera_zoom > DEMO_CAMERA_ZOOM_MAX) {
        app->camera_zoom = DEMO_CAMERA_ZOOM_MAX;
    }
    if (app->camera_zoom != previous) {
        demo_audio_emit(app, VOX_AUDIO_PRESET_ZOOM_CLICK,
                        0U, VOX_AUDIO_PAN_CENTER);
    }
}

static int demo_controller_zoom_step(demo_controller *controller,
                                     Sint16 modifier, Sint16 vertical)
{
    int direction = 0;
    if (controller == 0) return 0;
    if (modifier <= 16000) {
        controller->zoom_axis_state = 0;
        return 0;
    }
    if (vertical > -10000 && vertical < 10000) {
        controller->zoom_axis_state = 0;
    } else if (vertical <= -20000 && controller->zoom_axis_state >= 0) {
        controller->zoom_axis_state = -1;
        direction = 1;
    } else if (vertical >= 20000 && controller->zoom_axis_state <= 0) {
        controller->zoom_axis_state = 1;
        direction = -1;
    }
    return direction;
}

static int demo_activate_source(demo_app *app, int player, int source,
                                int deliberate)
{
    demo_player_input *input;
    vox_u32 now;
    if (player < 0 || player >= app->local_players ||
        player >= (int)DEMO_LOCAL_MAX) return 0;
    input = &app->player_input[player];
    if (input->preference == DEMO_INPUT_KEYBOARD &&
        source != DEMO_SOURCE_KEYBOARD) return 0;
    if (input->preference == DEMO_INPUT_CONTROLLER &&
        source != DEMO_SOURCE_CONTROLLER) return 0;
    if (input->active_source == source) return 0;
    if (input->preference != DEMO_INPUT_AUTO) return 0;
    now = SDL_GetTicks();
    if (!deliberate && now - input->switch_stamp <
                       DEMO_INPUT_SWITCH_HYSTERESIS_MS) return 0;
    input->active_source = source;
    input->switch_stamp = now;
    demo_reset_input_edges(app, player);
    return 1;
}

static void demo_begin_controller_calibration(demo_app *app)
{
    int slot;
    int axis;
    for (slot = 0; slot < (int)DEMO_CONTROLLER_MAX; ++slot) {
        demo_controller *controller = &app->controllers[slot];
        if (!demo_controller_connected(controller)) continue;
        controller->calibrating = 1;
        controller->calibration_stamp = SDL_GetTicks();
        controller->calibration_samples = 0;
        controller->calibration_peak = 0;
        for (axis = 0; axis < 4; ++axis) {
            controller->calibration_total[axis] = 0L;
        }
    }
}

static void demo_update_controller_calibration(demo_controller *controller)
{
    int axis;
    if (!demo_controller_connected(controller) || !controller->calibrating) {
        return;
    }
    for (axis = 0; axis < 4; ++axis) {
        int raw;
        int difference;
        if (axis >= SDL_JoystickNumAxes(controller->joystick)) continue;
        raw = (int)SDL_JoystickGetAxis(controller->joystick, axis);
        difference = raw - controller->axis_center[axis];
        if (difference < 0) difference = -difference;
        controller->calibration_total[axis] += raw;
        if (difference > controller->calibration_peak) {
            controller->calibration_peak = difference;
        }
    }
    ++controller->calibration_samples;
    if (SDL_GetTicks() - controller->calibration_stamp >=
        DEMO_CONTROLLER_CALIBRATION_MS) {
        double measured;
        if (controller->raw_fallback && controller->calibration_samples > 0) {
            for (axis = 0; axis < 4; ++axis) {
                controller->axis_center[axis] = (int)(
                    controller->calibration_total[axis] /
                    (long)controller->calibration_samples);
            }
        }
        measured = (double)controller->calibration_peak / 32767.0 + 0.06;
        if (measured < 0.18) measured = 0.18;
        if (measured > 0.35) measured = 0.35;
        controller->auto_deadzone = measured;
        controller->calibrating = 0;
    }
}

static void demo_load_controller_mappings(void)
{
    const char *environment = getenv("DIGS_GAMECONTROLLERDB");
    const char *relative = "share/digs/controllers/gamecontrollerdb.txt";
    char path[1024];
    char *base;
    FILE *file;
    if (environment != 0 && *environment != '\0') {
        (void)SDL_GameControllerAddMappingsFromFile(environment);
        return;
    }
    base = SDL_GetBasePath();
    if (base != 0 && strlen(base) + strlen(relative) + 1U < sizeof(path)) {
        sprintf(path, "%s%s", base, relative);
        file = fopen(path, "r");
        if (file != 0) {
            (void)fclose(file);
            (void)SDL_GameControllerAddMappingsFromFile(path);
            SDL_free(base);
            return;
        }
    }
    if (base != 0) SDL_free(base);
    (void)SDL_GameControllerAddMappingsFromFile(
        "share/digs/controllers/gamecontrollerdb.txt");
}

static void demo_prepare_targets(void)
{
    demo_ui.pixels = demo_pixels;
    demo_ui.width = DEMO_WIDTH;
    demo_ui.height = DEMO_HEIGHT;
    demo_ui.stride = DEMO_WIDTH * VOX_SOFTWARE_RGB_BYTES;
    demo_target.abi_version = VOX_ABI_VERSION;
    demo_target.struct_size = (vox_u32)sizeof(demo_target);
    demo_target.width = DEMO_WIDTH;
    demo_target.height = DEMO_HEIGHT;
    demo_target.stride = demo_ui.stride;
    demo_target.pixels = demo_pixels;
    demo_scene_target.abi_version = VOX_ABI_VERSION;
    demo_scene_target.struct_size = (vox_u32)sizeof(demo_scene_target);
    demo_scene_target.width = VOX_WORLD_WIDTH;
    demo_scene_target.height = VOX_WORLD_HEIGHT;
    demo_scene_target.stride = VOX_WORLD_WIDTH * VOX_SOFTWARE_RGB_BYTES;
    demo_scene_target.pixels = demo_scene_pixels;
    demo_legacy_target.abi_version = VOX_ABI_VERSION;
    demo_legacy_target.struct_size = (vox_u32)sizeof(demo_legacy_target);
    demo_legacy_target.width = DEMO_WIDTH;
    demo_legacy_target.height = DEMO_HEIGHT;
    demo_legacy_target.stride = DEMO_WIDTH * VOX_SOFTWARE_RGB_BYTES;
    demo_legacy_target.pixels = demo_camera_pixels;
    vox_software_config_default(&demo_render_config);
}

static void demo_build_title_world(void)
{
    vox_u32 start_x = VOX_WORLD_WIDTH * 19U / 64U;
    vox_u32 end_x = VOX_WORLD_WIDTH * 27U / 64U;
    vox_u32 lava_y = VOX_WORLD_HEIGHT * 11U / 32U;
    vox_u32 blast_x = VOX_WORLD_WIDTH * 91U / 256U;
    vox_u32 blast_y = VOX_WORLD_HEIGHT * 54U / 160U;
    vox_u32 blast_radius = VOX_WORLD_WIDTH / 64U;
    vox_u32 x;
    vox_u32 tick;
    if (blast_radius == 0U) blast_radius = 1U;
    (void)vox_digs_generate_map(&demo_title_world, VOX_DIGS_MAP_FURNACE_YARD,
                                0xD1655EEDU);
    for (x = start_x; x < end_x; ++x) {
        (void)vox_world_set(&demo_title_world, x, lava_y,
                            VOX_WORLD_DEPTH - 1U, VOX_MAT_LAVA, 700L << 16);
    }
    (void)vox_world_blast(&demo_title_world, blast_x, blast_y, 0U,
                          blast_radius, 700L << 16);
    for (tick = 0U; tick < 4U; ++tick) {
        (void)vox_world_step(&demo_title_world, 0);
    }
}

static vox_u32 demo_cap_mask_from_frame_us(vox_u32 frame_us)
{
    vox_u32 mask = (vox_u32)1U << (DEMO_FRAME_CAP_COUNT - 1);
    int index;
    for (index = 0; index < DEMO_FRAME_CAP_COUNT - 1; ++index) {
        vox_u32 budget = 850000U / (vox_u32)demo_frame_caps[index];
        if (frame_us <= budget) mask |= (vox_u32)1U << index;
    }
    return mask;
}

static vox_u32 demo_cap_hash_text(vox_u32 hash, const char *text_value)
{
    int index;
    if (text_value == 0) return hash;
    for (index = 0; text_value[index] != '\0'; ++index) {
        hash ^= (vox_u32)(unsigned char)text_value[index];
        hash *= 16777619U;
    }
    return hash;
}

static vox_u32 demo_cap_profile(demo_app *app)
{
    SDL_RendererInfo info;
    char values[96];
    vox_u32 hash = 2166136261U;
    memset(&info, 0, sizeof(info));
    if (app->renderer != 0) (void)SDL_GetRendererInfo(app->renderer, &info);
    hash = demo_cap_hash_text(hash, info.name == 0 ? "SOFTWARE" : info.name);
    hash = demo_cap_hash_text(hash, SDL_GetPlatform());
    sprintf(values, "%u:%u:%d:%d:%d:%d", (unsigned int)VOX_WORLD_WIDTH,
            (unsigned int)VOX_WORLD_HEIGHT, app->options.gi_quality,
            app->options.laptop_mode, SDL_GetCPUCount(), SDL_GetSystemRAM());
    hash = demo_cap_hash_text(hash, values);
    return hash == 0U ? 1U : hash;
}

static int demo_next_supported_cap(const demo_app *app, int current,
                                   int direction)
{
    int step;
    int index = current;
    for (step = 0; step < DEMO_FRAME_CAP_COUNT; ++step) {
        index += direction < 0 ? -1 : 1;
        if (index < 0) index = DEMO_FRAME_CAP_COUNT - 1;
        if (index >= DEMO_FRAME_CAP_COUNT) index = 0;
        if ((app->cap_supported_mask & ((vox_u32)1U << index)) != 0U) {
            return index;
        }
    }
    return DEMO_FRAME_CAP_COUNT - 1;
}

static void demo_qualify_caps(demo_app *app, int force)
{
    vox_u32 profile;
    vox_u32 samples[7];
    vox_software_view view;
    Uint64 frequency;
    int sample;
    int left;
    int right;
    profile = demo_cap_profile(app);
    if (!force && app->cap_cache_profile == profile &&
        app->cap_cache_us > 0U &&
        (app->cap_cache_mask & 1U) != 0U &&
        (app->cap_cache_mask &
         ((vox_u32)1U << (DEMO_FRAME_CAP_COUNT - 1))) != 0U) {
        app->cap_supported_mask = app->cap_cache_mask;
        app->cap_qualified = 1;
        return;
    }
    vox_software_view_full(&view);
    demo_render_config.gi_quality = app->options.laptop_mode ?
        VOX_GI_COMPATIBILITY : (vox_u16)app->options.gi_quality;
    (void)vox_software_render_view_ex(&demo_title_world, &demo_target,
                                      &demo_render_config, &view);
    frequency = SDL_GetPerformanceFrequency();
    for (sample = 0; sample < 7; ++sample) {
        Uint64 started = SDL_GetPerformanceCounter();
        Uint64 stopped;
        (void)vox_software_render_view_ex(&demo_title_world, &demo_target,
                                          &demo_render_config, &view);
        stopped = SDL_GetPerformanceCounter();
        if (frequency == 0U || stopped <= started) samples[sample] = 1000000U;
        else samples[sample] = (vox_u32)(((stopped - started) * 1000000U +
                                          frequency - 1U) / frequency);
    }
    for (left = 1; left < 7; ++left) {
        vox_u32 value = samples[left];
        right = left;
        while (right > 0 && samples[right - 1] > value) {
            samples[right] = samples[right - 1];
            --right;
        }
        samples[right] = value;
    }
    app->cap_cache_profile = profile;
    app->cap_cache_us = samples[3];
    app->cap_cache_mask = demo_cap_mask_from_frame_us(samples[3]);
    app->cap_supported_mask = app->cap_cache_mask;
    app->cap_qualified = 1;
    if ((app->cap_supported_mask &
         ((vox_u32)1U << app->options.frame_cap_index)) == 0U) {
        int fallback = app->options.frame_cap_index;
        while (fallback >= 0 &&
               (app->cap_supported_mask &
                ((vox_u32)1U << fallback)) == 0U) {
            --fallback;
        }
        app->options.frame_cap_index = fallback >= 0 ? fallback :
            DEMO_FRAME_CAP_COUNT - 1;
    }
    app->scene_valid = 0;
    (void)demo_save_input_settings(app);
}

static void demo_dark_panel(int x, int y, int width, int height)
{
    vox_ui_rect(&demo_ui, x, y, width, height, DEMO_VGA_BLACK);
    vox_ui_frame(&demo_ui, x, y, width, height, DEMO_VGA_BROWN);
}

/*
 * Where the rows a screen just drew actually are.
 *
 * The mouse handlers were all guarded by screen == DEMO_PLAY, so no menu had
 * ever been clickable.  Rather than write a rectangle table per screen and
 * keep it in step with the drawing by hand -- which is how those things drift
 * -- each row registers itself as it is drawn.  The registry is rebuilt every
 * frame, so it cannot describe a layout that is no longer on screen.
 */
#define DEMO_MENU_ROW_MAX 32
#define DEMO_ROW_ACTION 0
#define DEMO_ROW_VALUE 1

typedef struct demo_menu_row {
    int x;
    int y;
    int width;
    int height;
    int index;
    int kind;
} demo_menu_row;

static demo_menu_row demo_menu_rows[DEMO_MENU_ROW_MAX];
static int demo_menu_row_count;

static void demo_rows_reset(void)
{
    demo_menu_row_count = 0;
}

static void demo_row_register(int x, int y, int width, int height, int index,
                              int kind)
{
    demo_menu_row *row;
    if (demo_menu_row_count >= DEMO_MENU_ROW_MAX || index < 0) {
        return;
    }
    row = &demo_menu_rows[demo_menu_row_count++];
    row->x = x;
    row->y = y;
    row->width = width;
    row->height = height;
    row->index = index;
    row->kind = kind;
}

/* Which row the pointer is over, or -1.  Nearest match wins on overlap. */
static const demo_menu_row *demo_row_at(int x, int y)
{
    int i;
    for (i = 0; i < demo_menu_row_count; ++i) {
        const demo_menu_row *row = &demo_menu_rows[i];
        if (x >= row->x && x < row->x + row->width &&
            y >= row->y && y < row->y + row->height) {
            return row;
        }
    }
    return 0;
}

static void demo_menu_item(int y, const char *label, int index,
                           int selection)
{
    int selected = index >= 0 && index == selection;
    demo_row_register(82, y - 2, 156, 11, index, DEMO_ROW_ACTION);
    if (selected) {
        vox_ui_rect(&demo_ui, 82, y - 2, 156, 11, DEMO_VGA_BLUE);
        vox_ui_frame(&demo_ui, 82, y - 2, 156, 11,
                     DEMO_VGA_LIGHT_CYAN);
        vox_ui_text_center_shadow(&demo_ui, 160, y, 1, label,
                                  DEMO_VGA_YELLOW);
    } else {
        vox_ui_text_center_shadow(&demo_ui, 160, y, 1, label,
                                  DEMO_VGA_LIGHT_GRAY);
    }
}

/* Whose name goes on a message, by identity rather than by slot. */
static const char *demo_identity_name(const demo_app *app, vox_u16 identity)
{
    if (identity == VOX_DIGS_IDENTITY_PLAYER) {
        return app->player_names[0][0] != '\0' ? app->player_names[0] : "YOU";
    }
    if (identity < (vox_u16)VOX_DIGS_MAX_BOTS) {
        return app->bot_names[identity];
    }
    return "THE MINE";
}

/*
 * INBOX: what the miners wrote while you were away.
 *
 * A list until you open one, then the message itself.  There is no reply --
 * the point is that they have been getting on with it without you.
 */
static void demo_draw_inbox(demo_app *app)
{
    vox_u16 count = demo_chronicle.inbox_count;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(28, 8, 264, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 16, 1, "INBOX",
                              DEMO_VGA_YELLOW);
    if (count == 0U) {
        vox_ui_text_center(&demo_ui, 160, 92, 1, "NOTHING WAITING",
                           DEMO_VGA_DARK_GRAY);
        vox_ui_text_center(&demo_ui, 160, 106, 1,
                           "THEY WRITE WHEN THEY HAVE SOMETHING TO SAY",
                           DEMO_VGA_DARK_GRAY);
        vox_ui_text_center(&demo_ui, 160, 174, 1, "ESC  BACK",
                           DEMO_VGA_DARK_GRAY);
        return;
    }
    if (app->inbox_open < 0) {
        vox_u16 i;
        for (i = 0U; i < count && i < DIGS_INBOX_CAPACITY; ++i) {
            const digs_inbox_message *message = &demo_chronicle.inbox[i];
            char row[48];
            sprintf(row, "%s%s", demo_identity_name(app, message->from),
                    message->unread ? "   NEW" : "");
            demo_menu_item(34 + (int)i * 14, row, (int)i,
                           app->selection);
        }
        vox_ui_text_center(&demo_ui, 160, 174, 1,
                           "UP DOWN  ENTER READ  ESC BACK",
                           DEMO_VGA_DARK_GRAY);
    } else {
        const digs_inbox_message *message =
            &demo_chronicle.inbox[app->inbox_open];
        const char *you = demo_identity_name(app,
                                             (vox_u16)VOX_DIGS_IDENTITY_PLAYER);
        int y = 44;
        vox_u16 i;
        char header[48];
        sprintf(header, "FROM %s", demo_identity_name(app, message->from));
        vox_ui_text(&demo_ui, 40, 30, 1, header, DEMO_VGA_LIGHT_CYAN);
        vox_ui_rect(&demo_ui, 40, 40, 240, 1, DEMO_VGA_DARK_GRAY);
        for (i = 0U; i < message->line_count && i < DIGS_INBOX_LINES; ++i) {
            char text[128];
            demo_expand_line(message->lines[i], you, text, sizeof(text));
            y += vox_ui_text_wrap(&demo_ui, 40, y, 240, 3, 1, text,
                                  DEMO_VGA_LIGHT_GRAY) + 6;
        }
        vox_ui_text_center(&demo_ui, 160, 174, 1, "ESC  BACK TO INBOX",
                           DEMO_VGA_DARK_GRAY);
    }
}

/*
 * LOG: everything anyone has said, oldest first.
 *
 * No filtering and no summary -- it is a record, and the value of a record
 * is that it is complete and you can scroll back through it.
 */
static void demo_draw_log(demo_app *app)
{
    vox_u16 total = demo_chronicle.log_count;
    const char *you =
        demo_identity_name(app, (vox_u16)VOX_DIGS_IDENTITY_PLAYER);
    int rows = 13;
    int i;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(20, 8, 280, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 16, 1, "LOG", DEMO_VGA_YELLOW);
    if (total == 0U) {
        vox_ui_text_center(&demo_ui, 160, 96, 1, "NOTHING SAID YET",
                           DEMO_VGA_DARK_GRAY);
        vox_ui_text_center(&demo_ui, 160, 174, 1, "ESC  BACK",
                           DEMO_VGA_DARK_GRAY);
        return;
    }
    if (app->log_scroll > (int)total - rows) {
        app->log_scroll = (int)total - rows;
    }
    if (app->log_scroll < 0) {
        app->log_scroll = 0;
    }
    for (i = 0; i < rows; ++i) {
        int index = app->log_scroll + i;
        const digs_log_entry *entry;
        char text[128];
        char row[160];
        if (index >= (int)total) {
            break;
        }
        entry = digs_chronicle_log_at(&demo_chronicle, (vox_u16)index);
        if (entry == 0) {
            continue;
        }
        demo_expand_line(entry->line,
                         demo_identity_name(app, entry->target), text,
                         sizeof(text));
        sprintf(row, "%s: %s", demo_identity_name(app, entry->speaker), text);
        /* Derived, not guessed: the panel is 280 wide from x=26. */
        if ((int)strlen(row) > (320 - 26 - 20) / VOX_UI_DOS_ADVANCE) {
            row[(320 - 26 - 20) / VOX_UI_DOS_ADVANCE] = '\0';
        }
        vox_ui_text(&demo_ui, 26, 30 + i * 10, 1, row,
                    entry->speaker == VOX_DIGS_IDENTITY_PLAYER ?
                    255U : 170U,
                    entry->speaker == VOX_DIGS_IDENTITY_PLAYER ?
                    255U : 170U,
                    entry->speaker == VOX_DIGS_IDENTITY_PLAYER ?
                    255U : 170U);
    }
    (void)you;
    {
        char footer[64];
        sprintf(footer, "UP DOWN SCROLL   %d-%d OF %u   ESC BACK",
                app->log_scroll + 1,
                app->log_scroll + rows < (int)total ?
                app->log_scroll + rows : (int)total,
                (unsigned int)total);
        vox_ui_text_center(&demo_ui, 160, 174, 1, footer,
                           DEMO_VGA_DARK_GRAY);
    }
}

static void demo_handle_inbox_key(demo_app *app, SDL_Keycode key)
{
    vox_u16 count = demo_chronicle.inbox_count;
    if (key == SDLK_ESCAPE) {
        if (app->inbox_open >= 0) {
            app->inbox_open = -1;
        } else {
            app->screen = DEMO_TITLE;
            app->selection = 1;
        }
        demo_audio_play(app, DEMO_SOUND_SELECT);
        return;
    }
    if (app->inbox_open >= 0 || count == 0U) {
        return;
    }
    if (key == SDLK_UP) {
        app->selection = (app->selection + (int)count - 1) % (int)count;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % (int)count;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        app->inbox_open = app->selection;
        /*
         * Reading it is what marks it read, and the count on the title
         * follows immediately -- so the chronicle is written now rather than
         * at some later checkpoint that a crash could eat.
         */
        if (digs_chronicle_mark_read(&demo_chronicle,
                                     (vox_u16)app->selection)) {
            char path[1032];
            if (demo_chronicle_path(path, (int)sizeof(path))) {
                (void)digs_chronicle_save(&demo_chronicle, path);
            }
        }
        demo_audio_play(app, DEMO_SOUND_SELECT);
    }
}

static void demo_handle_log_key(demo_app *app, SDL_Keycode key)
{
    if (key == SDLK_ESCAPE) {
        app->screen = DEMO_TITLE;
        app->selection = 2;
        demo_audio_play(app, DEMO_SOUND_SELECT);
    } else if (key == SDLK_UP) {
        app->log_scroll--;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->log_scroll++;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_PAGEUP) {
        app->log_scroll -= 13;
    } else if (key == SDLK_PAGEDOWN) {
        app->log_scroll += 13;
    }
}

static void demo_draw_title(demo_app *app)
{
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(62, 10, 196, 180);
    vox_ui_text_center_shadow(&demo_ui, 160, 24, 3, "DIGS",
                              DEMO_VGA_YELLOW);
    vox_ui_rect(&demo_ui, 82, 65, 156, 1, DEMO_VGA_DARK_GRAY);
    vox_ui_rect(&demo_ui, 82, 66, 156, 1, DEMO_VGA_BROWN);
    {
        char inbox_label[24];
        vox_u16 unread = digs_chronicle_unread(&demo_chronicle);
        if (unread > 0U) {
            sprintf(inbox_label, "INBOX (%u)", (unsigned int)unread);
        } else {
            strcpy(inbox_label, "INBOX");
        }
        demo_menu_item(73, "START MATCH", 0, app->selection);
        demo_menu_item(87, inbox_label, 1, app->selection);
        demo_menu_item(101, "LOG", 2, app->selection);
        demo_menu_item(115, "FOUNDRY LAB", 3, app->selection);
        demo_menu_item(129, "CONTROLS", 4, app->selection);
        demo_menu_item(143, "OPTIONS", 5, app->selection);
        demo_menu_item(157, "QUIT", 6, app->selection);
    }
}

static void demo_value_line(int y, const char *label, const char *value,
                            int index, int selection)
{
    int selected = index >= 0 && index == selection;
    demo_row_register(44, y - 2, 232, 11, index, DEMO_ROW_VALUE);
    if (selected) {
        vox_ui_rect(&demo_ui, 44, y - 2, 232, 11, DEMO_VGA_BLUE);
        vox_ui_frame(&demo_ui, 44, y - 2, 232, 11,
                     DEMO_VGA_LIGHT_CYAN);
    }
    vox_ui_text(&demo_ui, 50, y, 1, label, DEMO_VGA_LIGHT_GRAY);
    vox_ui_text(&demo_ui, 170, y, 1, value,
                selected ? 255U : 170U, selected ? 255U : 170U,
                selected ? 85U : 170U);
}

static void demo_draw_setup(demo_app *app)
{
    char value[64];
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(28, 8, 264, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 16, 1, "MATCH SETUP",
                              DEMO_VGA_YELLOW);
    sprintf(value, "%d", app->local_players);
    demo_value_line(43, "LOCAL PLAYERS", value, 0, app->selection);
    sprintf(value, "%d", app->bots);
    demo_value_line(57, "BOTS", value, 1, app->selection);
    demo_value_line(71, "MAP", demo_map_names[app->map_style], 2, app->selection);
    sprintf(value, "%08lX", (unsigned long)app->seed);
    demo_value_line(85, "SEED", value, 3, app->selection);
    demo_value_line(99, "ARSENAL", demo_arsenal_names[app->arsenal], 4, app->selection);
    demo_menu_item(117, "CUSTOMIZE GAME", 5, app->selection);
    demo_menu_item(131, "START MATCH", 6, app->selection);
    demo_menu_item(145, "BACK", 7, app->selection);
    vox_ui_text_center(&demo_ui, 160, 180, 1,
                       "ARROWS CHANGE  ENTER SELECTS", DEMO_VGA_DARK_GRAY);
}

static void demo_draw_customize(demo_app *app)
{
    char value[32];
    int player;
    int active_players = app->local_players + app->bots;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(28, 8, 264, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 16, 1, "CUSTOMIZE GAME",
                              DEMO_VGA_YELLOW);
    for (player = 0; player < (int)VOX_DIGS_MAX_SLOTS; ++player) {
        char label[12];
        sprintf(label, "PLAYER %d", player + 1);
        demo_value_line(30 + player * 12, label,
                        player < active_players ?
                        app->player_names[player] : "EMPTY", player, app->selection);
    }
    demo_value_line(82, "TIME LIMIT",
                    demo_time_limit_names[app->time_limit_index], 4, app->selection);
    demo_value_line(95, "LAVA RISES",
                    demo_lava_names[app->lava_index], 5, app->selection);
    demo_value_line(108, "SCORE LIMIT",
                    demo_score_limit_names[app->score_limit_index], 6, app->selection);
    demo_value_line(121, "RESPAWN",
                    demo_respawn_mode_names[app->respawn_mode], 7, app->selection);
    sprintf(value, "%d SEC",
            demo_respawn_delays[app->respawn_delay_index]);
    demo_value_line(134, "SPAWN DELAY", value, 8, app->selection);
    demo_menu_item(151, "RESTORE DEFAULTS", 9, app->selection);
    demo_menu_item(166, "BACK", 10, app->selection);
    vox_ui_text_center(&demo_ui, 160, 180, 1,
                       "ENTER EDITS NAMES  ARROWS CHANGE",
                       DEMO_VGA_DARK_GRAY);
}

static const char *demo_name_grid_label(int item, char *label)
{
    if (item >= 0 && item < 38) {
        label[0] = demo_name_grid[item];
        label[1] = '\0';
        return label;
    }
    if (item == 38) return "SPACE";
    if (item == 39) return "DEL";
    if (item == 40) return "CLEAR";
    return "DONE";
}

static void demo_draw_name_editor(demo_app *app)
{
    char heading[24];
    int item;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(18, 8, 284, 184);
    sprintf(heading, "PLAYER %d NAME", app->edit_name_slot + 1);
    vox_ui_text_center_shadow(&demo_ui, 160, 16, 1, heading,
                              DEMO_VGA_YELLOW);
    vox_ui_rect(&demo_ui, 72, 37, 176, 17, DEMO_VGA_BLACK);
    vox_ui_frame(&demo_ui, 72, 37, 176, 17, DEMO_VGA_LIGHT_CYAN);
    vox_ui_text_center(&demo_ui, 160, 42, 1,
                       app->player_names[app->edit_name_slot],
                       DEMO_VGA_WHITE);
    for (item = 0; item < DEMO_NAME_GRID_ITEMS; ++item) {
        int column = item % DEMO_NAME_GRID_COLUMNS;
        int row = item / DEMO_NAME_GRID_COLUMNS;
        int x = 28 + column * 34;
        int y = 66 + row * 17;
        char character[2];
        const char *label = demo_name_grid_label(item, character);
        if (item == app->name_grid_selection) {
            vox_ui_rect(&demo_ui, x - 3, y - 3, 32, 12,
                        DEMO_VGA_BLUE);
            vox_ui_frame(&demo_ui, x - 3, y - 3, 32, 12,
                         DEMO_VGA_LIGHT_CYAN);
        }
        vox_ui_text_center(&demo_ui, x + 12, y, 1, label,
                           item == app->name_grid_selection ? 255U : 170U,
                           item == app->name_grid_selection ? 255U : 170U,
                           item == app->name_grid_selection ? 85U : 170U);
    }
    vox_ui_text_center(&demo_ui, 160, 178, 1,
                       "TYPE OR USE GRID  ESC CANCELS",
                       DEMO_VGA_DARK_GRAY);
}

static void demo_draw_options(demo_app *app)
{
    char volume[16];
    char cap_value[32];
    int page;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(28, 8, 264, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 16, 1, "OPTIONS",
                              DEMO_VGA_YELLOW);
    sprintf(volume, "%d%%", app->options.master_volume * 10);
    if ((app->cap_supported_mask & ((vox_u32)1U <<
         app->options.frame_cap_index)) == 0U) {
        /*
         * "UNSUPPORTED" spelled out ran past the panel once the glyph
         * advance widened.  The value column has 19 characters at x=170
         * before it reaches the frame.
         */
        sprintf(cap_value, "%s N/A",
                demo_frame_names[app->options.frame_cap_index]);
    } else sprintf(cap_value, "%s",
                   demo_frame_names[app->options.frame_cap_index]);
    page = app->selection < 8 ? 0 : 1;
    if (page == 0) {
        demo_value_line(36, "FRAME CAP", cap_value, 0, app->selection);
        demo_value_line(51, "LIGHTFIELD",
                        demo_gi_names[app->options.gi_quality], 1, app->selection);
        demo_value_line(66, "MASTER VOLUME", volume, 2, app->selection);
        demo_value_line(81, "LAPTOP MODE",
                        demo_toggle_names[app->options.laptop_mode], 3, app->selection);
        demo_value_line(96, "HAPTICS",
                        demo_haptic_names[app->options.haptic_level], 4, app->selection);
        demo_value_line(111, "FX PROFILE",
                        demo_fx_names[app->options.fx_profile], 5, app->selection);
        demo_value_line(126, "FLASHES",
                        demo_flash_names[app->options.flash_mode], 6, app->selection);
        demo_value_line(141, "GORE",
                        demo_gore_names[app->options.gore_level], 7, app->selection);
        vox_ui_text_center(&demo_ui, 160, 174, 1,
                           "UP DOWN  PAGE 1/2", DEMO_VGA_DARK_GRAY);
    } else {
        demo_value_line(36, "CAMERA SHAKE",
                        demo_toggle_names[app->options.camera_shake], 8, app->selection);
        demo_value_line(51, "DAMAGE NUMBERS",
                        demo_toggle_names[app->options.damage_numbers], 9, app->selection);
        demo_value_line(66, "NUMBER SIZE",
                        app->options.damage_number_size ? "LARGE" : "SMALL", 10, app->selection);
        demo_value_line(81, "NUMBER COLOR",
                        demo_number_color_names[
                            app->options.damage_number_color], 11, app->selection);
        demo_value_line(96, "FULLSCREEN",
                        demo_toggle_names[app->options.fullscreen], 12, app->selection);
        demo_value_line(111, "DUMMY MODE",
                        demo_toggle_names[app->options.dummy_mode], 13, app->selection);
        demo_menu_item(126, demo_chronicle_reset_armed ?
                       "PRESS AGAIN TO ERASE" :
                       "RESET MINER MEMORY", 14, app->selection);
        demo_menu_item(140, "INPUT & CONTROLLER", 15, app->selection);
        demo_menu_item(154, "BACK", 16, app->selection);
        vox_ui_text_center(&demo_ui, 160, 174, 1,
                           "UP DOWN  PAGE 2/2  F8 CAPS",
                           DEMO_VGA_DARK_GRAY);
    }
}

static void demo_input_mode_value(demo_app *app, int player,
                                  char *value)
{
    const demo_player_input *input = &app->player_input[player];
    demo_controller *controller = demo_controller_for_player(app, player);
    if (input->preference == DEMO_INPUT_KEYBOARD) {
        sprintf(value, "%s", player == 0 ? "KEYBOARD+MOUSE" : "KEYBOARD");
    } else if (input->preference == DEMO_INPUT_CONTROLLER) {
        if (controller == 0) sprintf(value, "PAD LOST");
        else sprintf(value, "CONTROLLER %d",
                     (int)(controller - app->controllers) + 1);
    } else if (input->active_source == DEMO_SOURCE_CONTROLLER &&
               controller != 0) {
        sprintf(value, "AUTO PAD %d",
                (int)(controller - app->controllers) + 1);
    } else {
        sprintf(value, "%s", player == 0 ? "AUTO KBD+MOUSE" : "AUTO KBD");
    }
}

static void demo_draw_input_options(demo_app *app)
{
    char p1_mode[24];
    char p2_mode[24];
    int calibrating = 0;
    int page;
    int slot;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(28, 8, 264, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 16, 1,
                              "INPUT & CONTROLLER", DEMO_VGA_YELLOW);
    demo_input_mode_value(app, 0, p1_mode);
    demo_input_mode_value(app, 1, p2_mode);
    for (slot = 0; slot < (int)DEMO_CONTROLLER_MAX; ++slot) {
        if (app->controllers[slot].calibrating) calibrating = 1;
    }
    page = app->selection < 7 ? 0 : 1;
    if (page == 0) {
        demo_value_line(36, "P1 MODE", p1_mode, 0, app->selection);
        demo_value_line(51, "P1 SENSITIVITY",
                        demo_sensitivity_names[
                            app->player_input[0].sensitivity], 1, app->selection);
        demo_value_line(66, "P1 DEADZONE",
                        demo_deadzone_names[app->player_input[0].deadzone], 2, app->selection);
        demo_value_line(81, "P1 AIM SLOW",
                        demo_slowdown_names[
                            app->player_input[0].aim_slowdown], 3, app->selection);
        demo_value_line(96, "P1 ROPE",
                        demo_rope_mode_names[
                            app->player_input[0].rope_mode], 4, app->selection);
        demo_value_line(111, "P2 MODE", p2_mode, 5, app->selection);
        demo_value_line(126, "P2 SENSITIVITY",
                        demo_sensitivity_names[
                            app->player_input[1].sensitivity], 6, app->selection);
        vox_ui_text_center(&demo_ui, 160, 174, 1,
                           "UP DOWN  PAGE 1/2", DEMO_VGA_DARK_GRAY);
    } else {
        demo_value_line(36, "P2 DEADZONE",
                        demo_deadzone_names[app->player_input[1].deadzone], 7, app->selection);
        demo_value_line(51, "P2 AIM SLOW",
                        demo_slowdown_names[
                            app->player_input[1].aim_slowdown], 8, app->selection);
        demo_value_line(66, "P2 ROPE",
                        demo_rope_mode_names[
                            app->player_input[1].rope_mode], 9, app->selection);
        demo_value_line(81, "CALIBRATE PADS",
                        calibrating ? "KEEP STICKS STILL" : "START", 10, app->selection);
        demo_menu_item(101, "RESTORE INPUT DEFAULTS", 11, app->selection);
        demo_menu_item(117, "BACK", 12, app->selection);
        vox_ui_text_center(&demo_ui, 160, 174, 1,
                           "UP DOWN  PAGE 2/2", DEMO_VGA_DARK_GRAY);
    }
}


static void demo_short_label(char *destination, int capacity,
                             const char *source, int max_characters)
{
    int length;
    if (destination == 0 || capacity <= 0) {
        return;
    }
    if (source == 0) {
        destination[0] = '\0';
        return;
    }
    length = 0;
    while (source[length] != '\0' && length < max_characters &&
           length + 1 < capacity) {
        destination[length] = source[length];
        ++length;
    }
    destination[length] = '\0';
}


static const char *demo_scancode_label(SDL_Scancode code)
{
    const char *name = SDL_GetScancodeName(code);
    return name == 0 || *name == '\0' ? "UNBOUND" : name;
}

static SDL_Scancode *demo_keyboard_binding(demo_app *app, int player,
                                           int action)
{
    if (player < 0 || player >= (int)DEMO_LOCAL_MAX) return 0;
    if (action == 0) return &app->bindings.keyboard_left[player];
    if (action == 1) return &app->bindings.keyboard_right[player];
    if (action == 2) return &app->bindings.keyboard_jump[player];
    if (action == 3) return &app->bindings.keyboard_steam[player];
    if (action == 4) return &app->bindings.keyboard_rope[player];
    if (action == 5) return &app->bindings.keyboard_fire[player];
    if (action == 6) return &app->bindings.keyboard_previous[player];
    if (action == 7) return &app->bindings.keyboard_next[player];
    if (action == 8) return &app->bindings.keyboard_bark[player];
    return 0;
}

static SDL_GameControllerButton *demo_pad_binding(demo_app *app, int action)
{
    if (action == 2) return &app->bindings.pad_jump;
    if (action == 3) return &app->bindings.pad_steam;
    if (action == 4) return &app->bindings.pad_rope;
    if (action == 5) return &app->bindings.pad_fire;
    if (action == 6) return &app->bindings.pad_previous;
    if (action == 7) return &app->bindings.pad_next;
    if (action == 8) return &app->bindings.pad_bark;
    return 0;
}

static void demo_assign_keyboard_binding(demo_app *app, int player,
                                         int action, SDL_Scancode code)
{
    SDL_Scancode *destination;
    int other;
    destination = demo_keyboard_binding(app, player, action);
    if (destination == 0 || code == SDL_SCANCODE_UNKNOWN) return;
    for (other = 0; other < 9; ++other) {
        SDL_Scancode *candidate = demo_keyboard_binding(app, player, other);
        if (candidate != 0 && candidate != destination &&
            *candidate == code) {
            *candidate = *destination;
        }
    }
    *destination = code;
}

static void demo_assign_pad_binding(demo_app *app, int action,
                                    SDL_GameControllerButton button)
{
    SDL_GameControllerButton *destination;
    int other;
    destination = demo_pad_binding(app, action);
    if (destination == 0 || button == SDL_CONTROLLER_BUTTON_INVALID) return;
    for (other = 2; other < 9; ++other) {
        SDL_GameControllerButton *candidate = demo_pad_binding(app, other);
        if (candidate != 0 && candidate != destination &&
            *candidate == button) {
            *candidate = *destination;
        }
    }
    *destination = button;
}

static void demo_draw_controls(demo_app *app)
{
    static const char *actions[9] = {
        "MOVE LEFT", "MOVE RIGHT", "JUMP", "STEAM", "ROPE", "FIRE",
        "PREV WEAPON", "NEXT WEAPON", "BARK"
    };
    char line[80];
    int action;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(18, 8, 284, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 16, 1, "CONTROLS",
                              DEMO_VGA_YELLOW);
    demo_value_line(36, "DEVICE",
        app->binding_player == 0 ? "P1 KEYBOARD" :
        (app->binding_player == 1 ? "P2 KEYBOARD" : "CONTROLLER"), 0, app->selection);
    for (action = 0; action < 9; ++action) {
        const char *binding_name;
        if (app->binding_player < 2) {
            SDL_Scancode *binding = demo_keyboard_binding(
                app, app->binding_player, action);
            if (app->binding_player == 0 && action == 4 && binding != 0 &&
                *binding == SDL_SCANCODE_UNKNOWN) {
                binding_name = "RMB (MOUSE)";
            } else {
                binding_name = binding == 0 ? "UNBOUND" :
                               demo_scancode_label(*binding);
            }
        } else if (action < 2) {
            binding_name = "STICK OR DPAD";
        } else {
            SDL_GameControllerButton *binding = demo_pad_binding(app, action);
            binding_name = binding == 0 ? "UNBOUND" :
                demo_pad_button_label(demo_prompt_family(app), *binding);
        }
        sprintf(line, "%s", binding_name);
        demo_value_line(44 + action * 11, actions[action], line, action + 1, app->selection);
    }
    demo_menu_item(148, "RESTORE DEFAULTS", 10, app->selection);
    demo_menu_item(163, "BACK", 11, app->selection);
    vox_ui_text_center(&demo_ui, 160, 180, 1,
        app->binding_capture ? "PRESS A NEW KEY  ESC CANCELS" :
        "ENTER REBINDS  LEFT RIGHT DEVICE", DEMO_VGA_LIGHT_CYAN);
}

static vox_i32 demo_material_temperature(vox_u16 material)
{
    if (material == VOX_MAT_LAVA) {
        return 700L << 16;
    }
    if (material == VOX_MAT_SMOKE || material == VOX_MAT_FIREDAMP) {
        return 180L << 16;
    }
    if (material == VOX_MAT_FLESH || material == VOX_MAT_BLOOD) {
        return 37L << 16;
    }
    return 20L << 16;
}

static void demo_render_overlay_begin(void)
{
    demo_render_patch_count = 0U;
}

static void demo_render_overlay_restore(void)
{
    vox_u32 patch;
    for (patch = 0U; patch < demo_render_patch_count; ++patch) {
        vox_u32 cell_index = demo_render_patches[patch].cell_index;
        vox_u32 plane_index = cell_index %
            (VOX_WORLD_WIDTH * VOX_WORLD_HEIGHT);
        demo_match.world.cells[cell_index] =
            demo_render_patches[patch].original;
        demo_render_touched[plane_index] = 0U;
    }
    demo_render_patch_count = 0U;
}

static void demo_render_voxel(int x, int y, vox_u16 material)
{
    vox_u32 plane_index;
    vox_u32 cell_index;
    vox_cell *cell;
    if (x < 0 || y < 0 || x >= (int)VOX_WORLD_WIDTH ||
        y >= (int)VOX_WORLD_HEIGHT || material == VOX_MAT_AIR) {
        return;
    }
    plane_index = (vox_u32)y * VOX_WORLD_WIDTH + (vox_u32)x;
    cell_index = (VOX_WORLD_DEPTH - 1U) * VOX_WORLD_WIDTH *
                 VOX_WORLD_HEIGHT + plane_index;
    cell = &demo_match.world.cells[cell_index];
    if (demo_render_touched[plane_index] == 0U) {
        if (demo_render_patch_count >= DEMO_RENDER_OVERLAY_CAPACITY) return;
        demo_render_touched[plane_index] = 1U;
        demo_render_patches[demo_render_patch_count].cell_index = cell_index;
        demo_render_patches[demo_render_patch_count].original = *cell;
        ++demo_render_patch_count;
    }
    cell->material = material;
    cell->flags = 0U;
    cell->temperature_q16 = demo_material_temperature(material);
    cell->damage_q16 = 0L;
}

static void demo_miner_overlay_plot(void *context, int x, int y,
                                    vox_u16 material)
{
    (void)context;
    demo_render_voxel(x, y, material);
}

static void demo_voxelize_miner(const demo_app *app, vox_u16 player)
{
    static const vox_u16 coats[VOX_DIGS_MAX_SLOTS] = {
        VOX_MAT_METAL, VOX_MAT_BIOMASS, VOX_MAT_COAL, VOX_MAT_STONE
    };
    const vox_physics_body *body = &demo_match.players[player];
    digs_miner_pose pose;
    int x = demo_q16_to_screen(body->position_x.value_q16,
                               VOX_WORLD_WIDTH, VOX_WORLD_WIDTH);
    int y = demo_q16_to_screen(body->position_y.value_q16,
                               VOX_WORLD_HEIGHT, VOX_WORLD_HEIGHT);
    vox_u16 part;
    digs_miner_pose_default(&pose);
    pose.coat_material = app->miner_hit_ttl[player] > 0U ?
                         VOX_MAT_BLOOD : coats[player];
    pose.facing_right = demo_match.facing_right[player];
    pose.steam_pack = 1U;
    pose.steam_thrusting =
        (demo_match.player_actions[player] & VOX_DIGS_ACTION_STEAM) != 0U &&
        demo_match.steam_q16[player] > 0U;
    pose.steam_variant = (vox_u16)((demo_match.tick + player * 17U) & 7U);
    for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
        if ((demo_match.anatomy[player][part].flags &
             VOX_DIGS_PART_SEVERED) != 0U) {
            pose.severed_mask |= (vox_u32)1U << part;
        }
    }
    (void)digs_miner_plot(x, y, &pose, demo_miner_overlay_plot, 0);
}

static void demo_voxelize_line(int x0, int y0, int x1, int y1,
                               vox_u16 material)
{
    int dx;
    int dy;
    int sx;
    int sy;
    int error;
    dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    dy = y1 >= y0 ? y1 - y0 : y0 - y1;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    error = dx - dy;
    for (;;) {
        demo_render_voxel(x0, y0, material);
        if (x0 == x1 && y0 == y1) break;
        {
            int twice = error * 2;
            if (twice > -dy) {
                error -= dy;
                x0 += sx;
            }
            if (twice < dx) {
                error += dx;
                y0 += sy;
            }
        }
    }
}

static void demo_voxelize_rope(vox_u16 player)
{
    const vox_digs_rope *rope = &demo_match.ropes[player];
    int previous_x;
    int previous_y;
    vox_u16 point;
    if (!rope->active && rope->state == VOX_DIGS_ROPE_IDLE) return;
    previous_x = (int)(demo_match.players[player].position_x.value_q16 /
                       65536L);
    previous_y = (int)(demo_match.players[player].position_y.value_q16 /
                       65536L);
    if (rope->state == VOX_DIGS_ROPE_CASTING) {
        demo_voxelize_line(previous_x, previous_y,
            (int)(rope->hook_x_q16 / 65536L),
            (int)(rope->hook_y_q16 / 65536L), VOX_MAT_METAL);
        return;
    }
    for (point = 0U; point < rope->point_count &&
         point < VOX_DIGS_ROPE_MAX_POINTS; ++point) {
        int x = (int)(rope->points[point].position_x_q16 / 65536L);
        int y = (int)(rope->points[point].position_y_q16 / 65536L);
        demo_voxelize_line(previous_x, previous_y, x, y, VOX_MAT_METAL);
        previous_x = x;
        previous_y = y;
    }
    demo_voxelize_line(previous_x, previous_y,
        (int)(rope->anchor_x_q16 / 65536L),
        (int)(rope->anchor_y_q16 / 65536L), VOX_MAT_METAL);
}

static void demo_voxelize_rail_traces(demo_app *app)
{
    vox_u16 player;
    for (player = 0U; player < VOX_DIGS_MAX_SLOTS; ++player) {
        demo_rail_trace *trace = &app->rail_traces[player];
        if (trace->active && demo_match.tick <= trace->until_tick) {
            demo_voxelize_line((int)(trace->start_x_q16 / 65536L),
                (int)(trace->start_y_q16 / 65536L),
                (int)(trace->end_x_q16 / 65536L),
                (int)(trace->end_y_q16 / 65536L), VOX_MAT_LAVA);
        } else if (trace->active) trace->active = 0U;
    }
}

static void demo_voxelize_lava_horizon(void)
{
    vox_u32 x;
    vox_u32 y;
    vox_u32 wave_tick;
    if (demo_match.lava_surface_y >= VOX_WORLD_HEIGHT) {
        return;
    }
    y = (vox_u32)demo_match.lava_surface_y;
    wave_tick = demo_match.tick / 8U;
    for (x = 0U; x < VOX_WORLD_WIDTH; ++x) {
        demo_render_voxel((int)x, (int)y, VOX_MAT_LAVA);
        if (y + 1U < VOX_WORLD_HEIGHT) {
            demo_render_voxel((int)x, (int)(y + 1U), VOX_MAT_LAVA);
        }
        if (y > 0U && ((x / 4U + wave_tick) % 13U) == 0U) {
            demo_render_voxel((int)x, (int)(y - 1U), VOX_MAT_LAVA);
        }
    }
}

static void demo_build_render_world(demo_app *app)
{
    vox_u16 player;
    vox_u16 index;
    /* The simulation treats every cell at and below lava_surface_y as lethal.
     * Give that exact boundary a top-layer voxel horizon so bedrock or deep
     * terrain cannot visually hide the rising hazard. */
    demo_voxelize_lava_horizon();
    for (player = 0U; player < VOX_DIGS_MAX_SLOTS; ++player) {
        if (demo_match.alive[player]) {
            demo_voxelize_rope(player);
            demo_voxelize_miner(app, player);
        }
    }
    demo_voxelize_rail_traces(app);
    for (index = 0U; index < VOX_DIGS_MAX_PROJECTILES; ++index) {
        const vox_digs_projectile *projectile = &demo_match.projectiles[index];
        if (projectile->active) {
            int x = demo_q16_to_screen(projectile->position_x_q16,
                                       VOX_WORLD_WIDTH, VOX_WORLD_WIDTH);
            int y = demo_q16_to_screen(projectile->position_y_q16,
                                       VOX_WORLD_HEIGHT, VOX_WORLD_HEIGHT);
            vox_u16 material = projectile->material == VOX_MAT_AIR ?
                               VOX_MAT_METAL : projectile->material;
            demo_render_voxel(x, y, material);
        }
    }
    for (index = 0U; index < demo_match.rules.fx_budget; ++index) {
        const vox_digs_effect *effect = &demo_match.effects[index];
        if (effect->active) {
            int x = demo_q16_to_screen(effect->position_x_q16,
                                       VOX_WORLD_WIDTH, VOX_WORLD_WIDTH);
            int y = demo_q16_to_screen(effect->position_y_q16,
                                       VOX_WORLD_HEIGHT, VOX_WORLD_HEIGHT);
            int gore = effect->material == VOX_MAT_FLESH ||
                       effect->material == VOX_MAT_BLOOD;
            if (!gore || app->options.gore_level == 2 ||
                (app->options.gore_level == 1 && (index % 3U) == 0U)) {
                demo_render_voxel(x, y, effect->material);
            }
        }
    }
}

static int demo_normalize_camera_zoom(demo_app *app)
{
    if (app->camera_zoom < DEMO_CAMERA_ZOOM_MIN ||
        app->camera_zoom > DEMO_CAMERA_ZOOM_MAX) {
        app->camera_zoom = DEMO_CAMERA_ZOOM_DEFAULT;
    }
    return app->camera_zoom;
}

static void demo_constrain_camera(demo_app *app)
{
    double half_width;
    double half_height;
    double minimum_x;
    double maximum_x;
    double minimum_y;
    double maximum_y;
    double presented_x;
    double presented_y;
    if (app->camera_scale < DEMO_CAMERA_MIN_SCALE) {
        app->camera_scale = DEMO_CAMERA_MIN_SCALE;
        app->camera_scale_velocity = 0.0;
    }
    half_width = (double)VOX_WORLD_WIDTH /
                 (app->camera_scale * 2.0);
    half_height = (double)VOX_WORLD_HEIGHT /
                  (app->camera_scale * 2.0);
    minimum_x = half_width;
    maximum_x = (double)VOX_WORLD_WIDTH - half_width;
    minimum_y = half_height;
    maximum_y = (double)VOX_WORLD_HEIGHT - half_height;
    if (app->camera_world_x < minimum_x) {
        app->camera_world_x = minimum_x;
        if (app->camera_velocity_x < 0.0) app->camera_velocity_x = 0.0;
    } else if (app->camera_world_x > maximum_x) {
        app->camera_world_x = maximum_x;
        if (app->camera_velocity_x > 0.0) app->camera_velocity_x = 0.0;
    }
    if (app->camera_world_y < minimum_y) {
        app->camera_world_y = minimum_y;
        if (app->camera_velocity_y < 0.0) app->camera_velocity_y = 0.0;
    } else if (app->camera_world_y > maximum_y) {
        app->camera_world_y = maximum_y;
        if (app->camera_velocity_y > 0.0) app->camera_velocity_y = 0.0;
    }
    /* Clamp presentation shake as part of the authoritative camera transform.
     * Mouse and controller aim consume this same adjusted value. */
    presented_x = app->camera_world_x + app->camera_shake_x;
    presented_y = app->camera_world_y + app->camera_shake_y;
    if (presented_x < minimum_x) {
        app->camera_shake_x = minimum_x - app->camera_world_x;
    } else if (presented_x > maximum_x) {
        app->camera_shake_x = maximum_x - app->camera_world_x;
    }
    if (presented_y < minimum_y) {
        app->camera_shake_y = minimum_y - app->camera_world_y;
    } else if (presented_y > maximum_y) {
        app->camera_shake_y = maximum_y - app->camera_world_y;
    }
}

static void demo_camera_exterior_pixel(vox_u8 *destination, int source_y,
                                       int source_height)
{
    long world_y;
    vox_u32 shade_y;
    if (source_y <= 0) {
        world_y = 0L;
    } else if (source_y >= source_height) {
        world_y = (long)VOX_WORLD_HEIGHT - 1L;
    } else {
        world_y = (long)source_y * (long)VOX_WORLD_HEIGHT /
                  (long)source_height;
    }
    if (demo_match.lava_surface_y < VOX_WORLD_HEIGHT &&
        world_y >= (long)demo_match.lava_surface_y) {
        destination[0] = 255U;
        destination[1] = 98U;
        destination[2] = 8U;
        return;
    }
    shade_y = (vox_u32)world_y;
    destination[0] = (vox_u8)(16U + shade_y * 18U / VOX_WORLD_HEIGHT);
    destination[1] = (vox_u8)(24U + shade_y * 22U / VOX_WORLD_HEIGHT);
    destination[2] = (vox_u8)(42U + shade_y * 28U / VOX_WORLD_HEIGHT);
}

static void demo_update_player_camera(demo_app *app)
{
    double minimum_x;
    double maximum_x;
    double minimum_y;
    double maximum_y;
    double target_x;
    double target_y;
    double target_scale;
    double target_sum_x;
    double fit_x;
    double fit_y;
    double dt;
    double damping;
    double shake_x;
    double shake_y;
    int active;
    int alive_local;
    int player;
    int user_zoom = demo_normalize_camera_zoom(app);
    minimum_x = (double)VOX_WORLD_WIDTH;
    maximum_x = 0.0;
    minimum_y = (double)VOX_WORLD_HEIGHT;
    maximum_y = 0.0;
    active = 0;
    alive_local = 0;
    for (player = 0; player < app->local_players; ++player) {
        if (demo_match.alive[player]) ++alive_local;
    }
    target_sum_x = 0.0;
    for (player = 0; player < app->local_players; ++player) {
        int hold_dead = alive_local == 0 && !demo_match.alive[player] &&
                        app->death_camera_hold > 0U &&
                        app->death_camera_player == (vox_u16)player;
        int pending_spawn = !demo_match.alive[player] && !hold_dead &&
                            alive_local == 0;
        if (demo_match.alive[player] || hold_dead || pending_spawn) {
            double previous_x = (double)app->previous_player_x[player] /
                                65536.0;
            double previous_y = (double)app->previous_player_y[player] /
                                65536.0;
            double current_x = pending_spawn ?
                (double)demo_match.respawn_target_x_q16[player] / 65536.0 :
                (double)demo_match.players[player].position_x.value_q16 /
                65536.0;
            double current_y = pending_spawn ?
                (double)demo_match.respawn_target_y_q16[player] / 65536.0 :
                (double)demo_match.players[player].position_y.value_q16 /
                65536.0;
            double interpolated_x = previous_x +
                                    (current_x - previous_x) *
                                    app->render_alpha;
            double interpolated_y = previous_y +
                                    (current_y - previous_y) *
                                    app->render_alpha;
            double look_x = pending_spawn ? 0.0 :
                (double)demo_match.players[player].velocity_x.value_q16 /
                65536.0 * 2.5;
            if (interpolated_x < minimum_x) minimum_x = interpolated_x;
            if (interpolated_x > maximum_x) maximum_x = interpolated_x;
            if (interpolated_y < minimum_y) minimum_y = interpolated_y;
            if (interpolated_y > maximum_y) maximum_y = interpolated_y;
            target_sum_x += interpolated_x + look_x;
            ++active;
        }
    }
    if (active == 0) {
        target_x = app->camera_world_x;
        target_y = app->camera_world_y;
        minimum_x = maximum_x = target_x;
        minimum_y = maximum_y = target_y;
    } else {
        target_x = target_sum_x / (double)active;
        target_y = (minimum_y + maximum_y) * 0.5 - 5.0;
    }
    fit_x = (double)VOX_WORLD_WIDTH /
            ((maximum_x - minimum_x) + 54.0);
    fit_y = ((double)DEMO_HEIGHT - DEMO_CAMERA_SAFE_TOP_PIXELS -
             DEMO_CAMERA_SAFE_BOTTOM_PIXELS) *
            (double)VOX_WORLD_HEIGHT /
            ((double)DEMO_HEIGHT *
             ((maximum_y - minimum_y) +
              DEMO_CAMERA_VERTICAL_PADDING));
    target_scale = fit_x < fit_y ? fit_x : fit_y;
    if (target_scale > (double)user_zoom) target_scale = (double)user_zoom;
    if (target_scale < DEMO_CAMERA_MIN_SCALE) {
        target_scale = DEMO_CAMERA_MIN_SCALE;
    }
    if (app->camera_trauma > 0.0) {
        target_scale *= 1.0 + app->camera_trauma * 0.06;
    }
    dt = app->frame_seconds;
    if (dt <= 0.0) dt = 1.0 / 60.0;
    if (dt > 0.05) dt = 0.05;
    damping = 1.0 / (1.0 + 10.0 * dt);
    app->camera_velocity_x += (target_x - app->camera_world_x) *
                              30.0 * dt;
    app->camera_velocity_y += (target_y - app->camera_world_y) *
                              30.0 * dt;
    app->camera_velocity_x *= damping;
    app->camera_velocity_y *= damping;
    app->camera_world_x += app->camera_velocity_x * dt;
    app->camera_world_y += app->camera_velocity_y * dt;
    app->camera_scale_velocity += (target_scale - app->camera_scale) *
                                  26.0 * dt;
    app->camera_scale_velocity *= damping;
    app->camera_scale += app->camera_scale_velocity * dt;
    if (app->camera_scale < DEMO_CAMERA_MIN_SCALE) {
        app->camera_scale = DEMO_CAMERA_MIN_SCALE;
    }
    if (app->camera_scale > 4.25) app->camera_scale = 4.25;
    shake_x = 0.0;
    shake_y = 0.0;
    if (app->options.camera_shake && app->camera_trauma > 0.0) {
        vox_u32 noise = demo_match.tick * 1664525U +
                        app->last_event_sequence * 1013904223U;
        shake_x = ((double)((int)((noise >> 16) & 255U) - 128) / 128.0) *
                  app->camera_trauma * 2.0;
        shake_y = ((double)((int)((noise >> 24) & 255U) - 128) / 128.0) *
                  app->camera_trauma * 1.5;
    }
    app->camera_shake_x = shake_x;
    app->camera_shake_y = shake_y;
    demo_constrain_camera(app);
    app->camera_trauma -= dt * 1.8;
    if (app->camera_trauma < 0.0) app->camera_trauma = 0.0;
}

static void demo_camera_view(const demo_app *app,
                             vox_software_view *view)
{
    vox_i32 world_width_q16 = (vox_i32)(VOX_WORLD_WIDTH << 16);
    vox_i32 world_height_q16 = (vox_i32)(VOX_WORLD_HEIGHT << 16);
    double scale = app->camera_scale;
    double center_x = app->camera_world_x + app->camera_shake_x;
    double center_y = app->camera_world_y + app->camera_shake_y;
    if (scale < DEMO_CAMERA_MIN_SCALE) scale = DEMO_CAMERA_MIN_SCALE;
    view->abi_version = VOX_ABI_VERSION;
    view->struct_size = (vox_u32)sizeof(*view);
    view->width_q16 = (vox_i32)((double)world_width_q16 / scale + 0.5);
    view->height_q16 = (vox_i32)((double)world_height_q16 / scale + 0.5);
    if (view->width_q16 > world_width_q16) {
        view->width_q16 = world_width_q16;
    }
    if (view->height_q16 > world_height_q16) {
        view->height_q16 = world_height_q16;
    }
    view->origin_x_q16 = (vox_i32)(center_x * 65536.0 + 0.5) -
                         view->width_q16 / 2;
    view->origin_y_q16 = (vox_i32)(center_y * 65536.0 + 0.5) -
                         view->height_q16 / 2;
    if (view->origin_x_q16 < 0L) view->origin_x_q16 = 0L;
    if (view->origin_y_q16 < 0L) view->origin_y_q16 = 0L;
    if (view->origin_x_q16 > world_width_q16 - view->width_q16) {
        view->origin_x_q16 = world_width_q16 - view->width_q16;
    }
    if (view->origin_y_q16 > world_height_q16 - view->height_q16) {
        view->origin_y_q16 = world_height_q16 - view->height_q16;
    }
}

static void demo_resample_player_camera(const demo_app *app,
                                        const vox_u8 *source_pixels,
                                        int source_width,
                                        int source_height)
{
    int center_x = (int)((app->camera_world_x + app->camera_shake_x) *
                         (double)source_width /
                         (double)VOX_WORLD_WIDTH);
    int center_y = (int)((app->camera_world_y + app->camera_shake_y) *
                         (double)source_height /
                         (double)VOX_WORLD_HEIGHT);
    int x;
    int y;
    for (y = 0; y < (int)DEMO_HEIGHT; ++y) {
        for (x = 0; x < (int)DEMO_WIDTH; ++x) {
            int source_x = center_x + (int)((double)(x -
                           (int)DEMO_WIDTH / 2) *
                           ((double)source_width /
                            (double)VOX_WORLD_WIDTH) /
                           app->camera_scale);
            int source_y = center_y + (int)((double)(y -
                           (int)DEMO_HEIGHT / 2) *
                           ((double)source_height /
                            (double)VOX_WORLD_HEIGHT) /
                           app->camera_scale);
            vox_u8 *destination = &demo_pixels[(y * (int)DEMO_WIDTH + x) *
                                               VOX_SOFTWARE_RGB_BYTES];
            if (source_x >= 0 && source_y >= 0 &&
                source_x < source_width && source_y < source_height) {
                const vox_u8 *source = &source_pixels[
                    (source_y * source_width + source_x) *
                    VOX_SOFTWARE_RGB_BYTES];
                destination[0] = source[0];
                destination[1] = source[1];
                destination[2] = source[2];
            } else {
                demo_camera_exterior_pixel(destination, source_y,
                                           source_height);
            }
        }
    }
}

/* The resampling wrapper remains useful for deterministic camera geometry
 * tests. Gameplay renders the Q16.16 camera view directly, so it never scans
 * or shades the off-screen world. */
static void demo_apply_player_camera(demo_app *app,
                                     const vox_u8 *source_pixels,
                                     int source_width, int source_height)
{
    demo_update_player_camera(app);
    demo_resample_player_camera(app, source_pixels,
                                source_width, source_height);
}

static void demo_mouse_world(demo_app *app, vox_u32 *world_x,
                             vox_u32 *world_y)
{
    double offset_x = (double)(app->mouse_x - (int)DEMO_WIDTH / 2) /
                      (app->camera_scale *
                       ((double)DEMO_WIDTH / (double)VOX_WORLD_WIDTH));
    double offset_y = (double)(app->mouse_y - (int)DEMO_HEIGHT / 2) /
                      (app->camera_scale *
                       ((double)DEMO_HEIGHT / (double)VOX_WORLD_HEIGHT));
    long x = (long)(app->camera_world_x + app->camera_shake_x + offset_x);
    long y = (long)(app->camera_world_y + app->camera_shake_y + offset_y);
    if (x < 0L) x = 0L;
    if (y < 0L) y = 0L;
    if (x >= (long)VOX_WORLD_WIDTH) x = (long)VOX_WORLD_WIDTH - 1L;
    if (y >= (long)VOX_WORLD_HEIGHT) y = (long)VOX_WORLD_HEIGHT - 1L;
    *world_x = (vox_u32)x;
    *world_y = (vox_u32)y;
}

static void demo_draw_hud(demo_app *app)
{
    char text[96];
    char weapon_label[28];
    char p2_weapon_label[28];
    const vox_digs_weapon_properties *weapon =
        vox_digs_weapon_get((vox_u16)app->selected_tool[0]);
    vox_u32 remaining = demo_match.tick < demo_match.rules.match_ticks ?
                        demo_match.rules.match_ticks - demo_match.tick : 0U;
    vox_u32 seconds = (remaining + VOX_DIGS_TICKS_PER_SECOND - 1U) /
                      VOX_DIGS_TICKS_PER_SECOND;
    int health_width = (int)((vox_u32)demo_match.health[0] * 42U /
                             VOX_DIGS_MAX_HEALTH);
    int steam_width = (int)((vox_u32)demo_match.steam_q16[0] * 40U / 65535U);
    vox_ui_rect(&demo_ui, 3, 3, 166, 29, DEMO_VGA_BLACK);
    vox_ui_frame(&demo_ui, 3, 3, 166, 29, DEMO_VGA_BROWN);
    sprintf(text, "%s K%u D%u HP%u",
            app->player_names[0],
            (unsigned int)demo_match.scores[0],
            (unsigned int)demo_match.deaths[0],
            (unsigned int)demo_match.health[0]);
    vox_ui_text(&demo_ui, 7, 7, 1, text, DEMO_VGA_WHITE);
    vox_ui_text(&demo_ui, 7, 18, 1, "HP", DEMO_VGA_LIGHT_RED);
    vox_ui_rect(&demo_ui, 22, 18, 44, 5, DEMO_VGA_BROWN);
    vox_ui_rect(&demo_ui, 23, 19, health_width, 3, DEMO_VGA_LIGHT_RED);
    vox_ui_text(&demo_ui, 73, 18, 1, "STEAM", DEMO_VGA_LIGHT_CYAN);
    vox_ui_rect(&demo_ui, 112, 18, 42, 5, DEMO_VGA_BLUE);
    vox_ui_rect(&demo_ui, 113, 19, steam_width, 3, DEMO_VGA_CYAN);
    vox_ui_rect(&demo_ui, 174, 3, 143, 29, DEMO_VGA_BLACK);
    vox_ui_frame(&demo_ui, 174, 3, 143, 29, DEMO_VGA_BROWN);
    demo_short_label(weapon_label, (int)sizeof(weapon_label),
                     weapon == 0 ? "UNKNOWN" : weapon->name, 25);
    vox_ui_text(&demo_ui, 179, 7, 1,
                weapon_label,
                DEMO_VGA_YELLOW);
    if (weapon != 0 && demo_match.weapon_charging[0] != 0U) {
        vox_u32 maximum = weapon->charge_ticks > 0U ?
                          weapon->charge_ticks : 60U;
        vox_u32 charge = (vox_u32)demo_match.weapon_charge_ticks[0] * 100U /
                         maximum;
        if (charge > 100U) charge = 100U;
        sprintf(text, "CHARGE %lu%%", (unsigned long)charge);
    } else if (app->selected_tool[0] == VOX_DIGS_TOOL_RAIL_GUN &&
               weapon != 0 && weapon->charge_ticks > 0U) {
        vox_u32 charge = (vox_u32)demo_match.rail_charge_ticks[0] * 100U /
                         weapon->charge_ticks;
        if (charge > 100U) charge = 100U;
        sprintf(text, "RAIL %lu%%", (unsigned long)charge);
    } else sprintf(text, "CD %u  LMB FIRE",
                   (unsigned int)demo_match.weapon_cooldown[0]);
    vox_ui_text(&demo_ui, 179, 15, 1, text, DEMO_VGA_LIGHT_GRAY);
    vox_ui_text(&demo_ui, 179, 23, 1, "1-0  WHEEL  ZOOM",
                DEMO_VGA_DARK_GRAY);
    if (app->local_players > 1) {
        const vox_digs_weapon_properties *p2_weapon =
            vox_digs_weapon_get((vox_u16)app->selected_tool[1]);
        int p2_health = (int)((vox_u32)demo_match.health[1] * 56U /
                              VOX_DIGS_MAX_HEALTH);
        vox_ui_rect(&demo_ui, 3, 35, 166, 21, DEMO_VGA_BLACK);
        vox_ui_frame(&demo_ui, 3, 35, 166, 21, DEMO_VGA_CYAN);
        sprintf(text, "%s K%u D%u HP%u", app->player_names[1],
                (unsigned int)demo_match.scores[1],
                (unsigned int)demo_match.deaths[1],
                (unsigned int)demo_match.health[1]);
        vox_ui_text(&demo_ui, 7, 39, 1, text, DEMO_VGA_LIGHT_CYAN);
        vox_ui_rect(&demo_ui, 105, 40, 58, 5, DEMO_VGA_BLUE);
        vox_ui_rect(&demo_ui, 106, 41, p2_health, 3, DEMO_VGA_LIGHT_RED);
        vox_ui_rect(&demo_ui, 174, 35, 143, 21, DEMO_VGA_BLACK);
        vox_ui_frame(&demo_ui, 174, 35, 143, 21, DEMO_VGA_CYAN);
        demo_short_label(p2_weapon_label, (int)sizeof(p2_weapon_label),
                         p2_weapon == 0 ? "UNKNOWN" : p2_weapon->name,
                         25);
        vox_ui_text(&demo_ui, 179, 39, 1,
                    p2_weapon_label,
                    DEMO_VGA_LIGHT_CYAN);
        vox_ui_text(&demo_ui, 179, 48, 1, "PAD 2 OR P2 KEYS",
                    DEMO_VGA_DARK_GRAY);
    }
    {
        int timer_y = app->options.debug ? 153 : 178;
        vox_u8 red = 170U;
        vox_u8 green = 255U;
        vox_u8 blue = 255U;
        if (demo_match.tick >= demo_match.rules.lava_start_tick) {
            red = 255U;
            green = 255U;
            blue = 85U;
        }
        if (seconds <= 10U) {
            red = 255U;
            green = 85U;
            blue = 85U;
        }
        vox_ui_rect(&demo_ui, 3, timer_y, 74, 19, DEMO_VGA_BLACK);
        vox_ui_frame(&demo_ui, 3, timer_y, 74, 19,
                     red, green, blue);
        sprintf(text, "TIME %lu:%02lu", (unsigned long)(seconds / 60U),
                (unsigned long)(seconds % 60U));
        vox_ui_text(&demo_ui, 8, timer_y + 6, 1, text,
                    red, green, blue);
    }
}

static void demo_draw_notifications(demo_app *app)
{
    int line;
    char text[48];
    int banner_y = app->local_players > 1 ? 60 : 38;
    int killfeed_y = banner_y + 13;
    /* Four concise entries reserve the lower screen for player barks and
     * respawn prompts instead of allowing a six-line feed to stack on them. */
    for (line = 0; line < 4; ++line) {
        if (app->killfeed[line].ttl > 0U) {
            char compact[24];
            int width;
            int y = killfeed_y + line * 9;
            int x;
            demo_short_label(compact, (int)sizeof(compact),
                             app->killfeed[line].text, 22);
            width = vox_ui_text_width(compact, 1);
            x = (int)DEMO_WIDTH - width - 5;
            if (x < 4) x = 4;
            vox_ui_rect(&demo_ui, x - 2, y - 1, width + 4, 9,
                        DEMO_VGA_BLACK);
            vox_ui_frame(&demo_ui, x - 2, y - 1, width + 4, 9,
                         DEMO_VGA_DARK_GRAY);
            vox_ui_text_shadow(&demo_ui, x, y, 1, compact,
                               DEMO_VGA_LIGHT_GRAY);
        }
    }
    for (line = 2; line >= 0; --line) {
        if (app->banners[line].ttl > 0U) {
            char compact[30];
            int width;
            int x;
            demo_short_label(compact, (int)sizeof(compact),
                             app->banners[line].text, 28);
            width = vox_ui_text_width(compact, 1);
            x = (int)DEMO_WIDTH / 2 - width / 2;
            if (x < 4) x = 4;
            if (x + width + 4 > (int)DEMO_WIDTH) {
                x = (int)DEMO_WIDTH - width - 4;
            }
            vox_ui_rect(&demo_ui, x - 3, banner_y - 2, width + 6, 11,
                        DEMO_VGA_BLACK);
            vox_ui_frame(&demo_ui, x - 3, banner_y - 2, width + 6, 11,
                         DEMO_VGA_LIGHT_GRAY);
            vox_ui_text_shadow(&demo_ui, x, banner_y, 1, compact,
                               line == 0 ? 170U : 255U,
                               line == 0 ? 170U : 255U,
                               line == 0 ? 170U : 85U);
            break;
        }
    }
    for (line = 0; line < app->local_players; ++line) {
        if (!demo_match.alive[line]) {
            int y = 132 + line * 11;
            int width;
            if (demo_match.respawn_ready[line]) {
                if (demo_match.rules.respawn_mode ==
                    VOX_DIGS_RESPAWN_ON_FIRE) {
                    sprintf(text, "%s  PRESS FIRE",
                            app->player_names[line]);
                } else {
                    sprintf(text, "%s  DEPLOYING",
                            app->player_names[line]);
                }
            } else {
                vox_u32 tenths = ((vox_u32)demo_match.respawn_ticks[line] *
                    10U + VOX_DIGS_TICKS_PER_SECOND - 1U) /
                    VOX_DIGS_TICKS_PER_SECOND;
                sprintf(text, "%s  RESPAWN %lu.%lu",
                        app->player_names[line],
                        (unsigned long)(tenths / 10U),
                        (unsigned long)(tenths % 10U));
            }
            width = vox_ui_text_width(text, 1);
            vox_ui_rect(&demo_ui, 160 - width / 2 - 3, y - 2,
                        width + 6, 11, DEMO_VGA_BLACK);
            vox_ui_frame(&demo_ui, 160 - width / 2 - 3, y - 2,
                         width + 6, 11, DEMO_VGA_LIGHT_CYAN);
            vox_ui_text_center_shadow(&demo_ui, 160, y, 1, text,
                                      DEMO_VGA_LIGHT_CYAN);
        }
    }
}

static void demo_draw_debug(demo_app *app)
{
    char text[128];
    const vox_cell *cell;
    vox_u32 world_x;
    vox_u32 world_y;
    if (!app->options.debug) {
        return;
    }
    demo_mouse_world(app, &world_x, &world_y);
    if (world_x >= VOX_WORLD_WIDTH) {
        world_x = VOX_WORLD_WIDTH - 1U;
    }
    if (world_y >= VOX_WORLD_HEIGHT) {
        world_y = VOX_WORLD_HEIGHT - 1U;
    }
    cell = vox_world_cell(&demo_match.world, world_x, world_y,
                          VOX_WORLD_DEPTH - 1U);
    vox_ui_rect(&demo_ui, 3, 174, 314, 23, DEMO_VGA_BLACK);
    vox_ui_frame(&demo_ui, 3, 174, 314, 23, DEMO_VGA_DARK_GRAY);
    sprintf(text, "FPS %lu CAP %s T%lu P%u FX%u L%u H%08lX",
            (unsigned long)(app->measured_fps + 0.5),
            demo_frame_names[app->options.frame_cap_index],
            (unsigned long)demo_match.tick,
            (unsigned int)demo_match.projectile_count,
            (unsigned int)demo_match.effect_count,
            (unsigned int)demo_match.lava_surface_y,
            (unsigned long)demo_match.state_hash);
    vox_ui_text(&demo_ui, 7, 178, 1, text, DEMO_VGA_LIGHT_CYAN);
    sprintf(text, "CELLS %lu AWAKE %lu TARGET %lu,%lu MAT %u",
            (unsigned long)demo_match.world.occupied_cells,
            (unsigned long)demo_match.world.awake_cells,
            (unsigned long)world_x, (unsigned long)world_y,
            cell == 0 ? 0U : (unsigned int)cell->material);
    vox_ui_text(&demo_ui, 7, 187, 1, text, DEMO_VGA_LIGHT_GRAY);
}

static void demo_world_to_screen(demo_app *app, vox_i32 world_x_q16,
                                 vox_i32 world_y_q16, int *screen_x,
                                 int *screen_y)
{
    double world_x = (double)world_x_q16 / 65536.0;
    double world_y = (double)world_y_q16 / 65536.0;
    *screen_x = (int)DEMO_WIDTH / 2 +
        (int)((world_x - app->camera_world_x - app->camera_shake_x) *
              app->camera_scale *
              (double)DEMO_WIDTH / (double)VOX_WORLD_WIDTH);
    *screen_y = (int)DEMO_HEIGHT / 2 +
        (int)((world_y - app->camera_world_y - app->camera_shake_y) *
              app->camera_scale *
              (double)DEMO_HEIGHT / (double)VOX_WORLD_HEIGHT);
}

/* Four L-shaped corners read cleanly against busy voxels without hiding the
 * exact point of aim.  The outer bloom follows weapon reach, aim distance and
 * hit confirmation; an inner frame closes toward the point while a held tool
 * charges.  It is shared by the hardware-mouse and controller paths so the
 * player never sees two competing aiming systems. */
static void demo_draw_crosshair(demo_app *app, int player, int x, int y)
{
    const vox_digs_weapon_properties *weapon;
    long body_x;
    long body_y;
    long delta_x;
    long delta_y;
    int distance;
    int radius;
    int charge_radius;
    int charge_total;
    int charge_ticks;
    int leg;
    int pulse;
    vox_u8 red;
    vox_u8 green;
    vox_u8 blue;
    if (app == 0 || player < 0 || player >= app->local_players) return;
    weapon = vox_digs_weapon_get((vox_u16)app->selected_tool[player]);
    body_x = demo_match.players[player].position_x.value_q16 / 65536L;
    body_y = demo_match.players[player].position_y.value_q16 / 65536L;
    delta_x = (long)app->aim_world_x[player] - body_x;
    delta_y = (long)app->aim_world_y[player] - body_y;
    distance = (int)demo_sqrt((double)(delta_x * delta_x +
                                       delta_y * delta_y));
    pulse = (int)app->crosshair_pulse_ttl[player];
    radius = 3 + distance / 24 + (pulse + 2) / 5;
    if (weapon != 0) radius += (int)weapon->blast_radius / 3;
    if (radius < 3) radius = 3;
    if (radius > 12) radius = 12;
    leg = radius / 2 + 2;
    red = player == 0 ? 255U : 85U;
    green = 255U;
    blue = player == 0 ? 85U : 255U;
    vox_ui_rect(&demo_ui, x - radius, y - radius, leg, 1,
                red, green, blue);
    vox_ui_rect(&demo_ui, x - radius, y - radius, 1, leg,
                red, green, blue);
    vox_ui_rect(&demo_ui, x + radius - leg + 1, y - radius, leg, 1,
                red, green, blue);
    vox_ui_rect(&demo_ui, x + radius, y - radius, 1, leg,
                red, green, blue);
    vox_ui_rect(&demo_ui, x - radius, y + radius, leg, 1,
                red, green, blue);
    vox_ui_rect(&demo_ui, x - radius, y + radius - leg + 1, 1, leg,
                red, green, blue);
    vox_ui_rect(&demo_ui, x + radius - leg + 1, y + radius, leg, 1,
                red, green, blue);
    vox_ui_rect(&demo_ui, x + radius, y + radius - leg + 1, 1, leg,
                red, green, blue);
    if (demo_match.weapon_charging[player] != 0U) {
        charge_total = weapon != 0 && weapon->charge_ticks > 0U ?
                       (int)weapon->charge_ticks : 60;
        charge_ticks = (int)demo_match.weapon_charge_ticks[player];
        if (charge_ticks < 0) charge_ticks = 0;
        if (charge_ticks > charge_total) charge_ticks = charge_total;
        charge_radius = radius - (radius - 2) * charge_ticks / charge_total;
        if (charge_radius < 2) charge_radius = 2;
        leg = charge_radius > 3 ? 3 : 2;
        vox_ui_rect(&demo_ui, x - charge_radius, y - charge_radius,
                    leg, 1, DEMO_VGA_WHITE);
        vox_ui_rect(&demo_ui, x - charge_radius, y - charge_radius,
                    1, leg, DEMO_VGA_WHITE);
        vox_ui_rect(&demo_ui, x + charge_radius - leg + 1,
                    y - charge_radius, leg, 1, DEMO_VGA_WHITE);
        vox_ui_rect(&demo_ui, x + charge_radius, y - charge_radius,
                    1, leg, DEMO_VGA_WHITE);
        vox_ui_rect(&demo_ui, x - charge_radius, y + charge_radius,
                    leg, 1, DEMO_VGA_WHITE);
        vox_ui_rect(&demo_ui, x - charge_radius,
                    y + charge_radius - leg + 1, 1, leg,
                    DEMO_VGA_WHITE);
        vox_ui_rect(&demo_ui, x + charge_radius - leg + 1,
                    y + charge_radius, leg, 1, DEMO_VGA_WHITE);
        vox_ui_rect(&demo_ui, x + charge_radius,
                    y + charge_radius - leg + 1, 1, leg,
                    DEMO_VGA_WHITE);
    }
    vox_ui_rect(&demo_ui, x, y, 1, 1, DEMO_VGA_WHITE);
}

static void demo_draw_world_feedback(demo_app *app)
{
    static const char *ai_names[4] = {
        "ROAM", "SEARCH", "ATTACK", "RETREAT"
    };
    int player;
    int slot;
    int hide_bubbles = 0;
    for (player = 0; player < app->local_players; ++player) {
        if (!demo_match.alive[player]) hide_bubbles = 1;
    }
    for (player = 0; player < (int)demo_match.rules.player_count; ++player) {
        int x;
        int y;
        int name_x;
        int name_width;
        int name_y;
        demo_world_to_screen(app,
            demo_match.players[player].position_x.value_q16,
            demo_match.players[player].position_y.value_q16, &x, &y);
        if (demo_match.spawn_shield_ticks[player] > 0U &&
            ((demo_match.tick / 4U) & 1U) == 0U) {
            vox_ui_frame(&demo_ui, x - 6, y - 12, 13, 19,
                         DEMO_VGA_LIGHT_CYAN);
        }
        if (player >= app->local_players &&
            app->bot_health_ttl[player] > 0U && demo_match.alive[player]) {
            int width = (int)((vox_u32)demo_match.health[player] * 18U /
                              VOX_DIGS_MAX_HEALTH);
            vox_ui_rect(&demo_ui, x - 10, y - 15, 20, 4, DEMO_VGA_BLACK);
            vox_ui_rect(&demo_ui, x - 9, y - 14, width, 2,
                        DEMO_VGA_LIGHT_RED);
        }
        if (demo_match.alive[player]) {
            name_width = vox_ui_text_width(app->player_names[player], 1);
            name_x = x;
            if (name_x - name_width / 2 < 2) {
                name_x = 2 + name_width / 2;
            }
            if (name_x + name_width / 2 >= (int)DEMO_WIDTH - 2) {
                name_x = (int)DEMO_WIDTH - 3 - name_width / 2;
            }
            name_y = y < 75 ? y + 10 : y - 17;
            vox_ui_text_center_shadow(&demo_ui, name_x, name_y, 1,
                                      app->player_names[player],
                                      player < app->local_players ?
                                      255U : 85U,
                                      player < app->local_players ?
                                      255U : 255U,
                                      player < app->local_players ?
                                      255U : 255U);
        }
        if (!hide_bubbles && app->bubbles[player].ttl > 0U) {
            int bubble_x = x - 49;
            int bubble_y = y < 130 ? y + 13 : y - 41;
            int bubble_min_y = app->local_players > 1 ? 110 : 98;
            int bubble_max_y = app->options.debug ? 132 : 156;
            if (bubble_x < 2) bubble_x = 2;
            if (bubble_x > (int)DEMO_WIDTH - 102) {
                bubble_x = (int)DEMO_WIDTH - 102;
            }
            if (bubble_y < bubble_min_y) bubble_y = bubble_min_y;
            if (bubble_y > bubble_max_y) {
                bubble_y = bubble_max_y;
            }
            vox_ui_rect(&demo_ui, bubble_x, bubble_y, 100, 18,
                        DEMO_VGA_BLACK);
            vox_ui_frame(&demo_ui, bubble_x, bubble_y, 100, 18,
                         DEMO_VGA_LIGHT_GRAY);
            (void)vox_ui_text_wrap(&demo_ui, bubble_x + 4,
                                   bubble_y + 3, 92, 2, 1,
                                   app->bubbles[player].text,
                                   255U, 255U, 255U);
        }
        if (app->options.debug) {
            /*
             * With F1 held down, label every miner with the tool it is
             * actually holding.  Without this the archetype weapon
             * preferences can only be read off a histogram after the fact,
             * which makes them impossible to judge while playing.
             */
            const vox_digs_weapon_properties *held =
                vox_digs_weapon_get(demo_match.selected_weapon[player]);
            if (held != 0) {
                vox_ui_text_center(&demo_ui, x, y + 10, 1, held->name,
                                   DEMO_VGA_YELLOW);
            }
            if (vox_digs_player_is_bot(&demo_match, (vox_u16)player)) {
                vox_u16 mode = demo_match.bots[player].mode;
                vox_u16 archetype = vox_digs_bot_archetype(&demo_match,
                                                           (vox_u16)player);
                const char *name = mode < 4U ? ai_names[mode] : "AI";
                const char *who = vox_digs_archetype_name(archetype);
                char label[48];
                sprintf(label, "%s %s", who != 0 ? who : "BOT", name);
                vox_ui_text_center(&demo_ui, x, y + 18, 1, label,
                                   DEMO_VGA_LIGHT_CYAN);
            }
        }
    }
    for (player = 0; app->screen == DEMO_PLAY &&
         player < app->local_players; ++player) {
        int x;
        int y;
        if (player == 0 && app->mouse_inside &&
            app->player_input[0].active_source == DEMO_SOURCE_KEYBOARD) {
            continue;
        }
        demo_world_to_screen(app,
            (vox_i32)app->aim_world_x[player] << 16,
            (vox_i32)app->aim_world_y[player] << 16, &x, &y);
        demo_draw_crosshair(app, player, x, y);
    }
    for (player = 0; player < app->local_players; ++player) {
        const demo_hit_marker *marker = &app->hit_markers[player];
        if (marker->ttl > 0U) {
            int x;
            int y;
            demo_world_to_screen(app, marker->world_x_q16,
                                 marker->world_y_q16, &x, &y);
            vox_ui_rect(&demo_ui, x - 4, y, 9, 1, DEMO_VGA_WHITE);
            vox_ui_rect(&demo_ui, x, y - 4, 1, 9, DEMO_VGA_WHITE);
            vox_ui_rect(&demo_ui, x - 2, y - 2, 5, 5,
                        DEMO_VGA_LIGHT_RED);
            vox_ui_rect(&demo_ui, x, y, 1, 1, DEMO_VGA_WHITE);
        }
    }
    for (slot = 0; slot < (int)DEMO_DAMAGE_POPUP_MAX; ++slot) {
        const demo_damage_popup *popup = &app->damage_popups[slot];
        if (popup->active) {
            char amount[16];
            int x;
            int y;
            int scale = app->options.damage_number_size ? 2 : 1;
            vox_u8 red;
            vox_u8 green;
            vox_u8 blue;
            sprintf(amount, "%u", (unsigned int)popup->amount);
            demo_world_to_screen(app, popup->world_x_q16,
                                 popup->world_y_q16, &x, &y);
            if (app->options.damage_number_color) {
                red = 255U;
                green = 255U;
                blue = popup->target < (vox_u16)app->local_players ?
                       255U : 85U;
            } else {
                red = 255U;
                green = popup->target < (vox_u16)app->local_players ?
                        85U : 255U;
                blue = 85U;
            }
            vox_ui_text_center_shadow(&demo_ui, x, y, scale, amount,
                                      red, green, blue);
        }
    }
}

static void demo_apply_flash(demo_app *app)
{
    vox_u32 pixel;
    double strength;
    int target_red;
    int target_green;
    int target_blue;
    if (app->flash_strength <= 0.0) return;
    strength = app->flash_strength;
    if (strength > 0.85) strength = 0.85;
    target_red = 255;
    target_green = app->flash_kind == 1 ? 0 : 255;
    target_blue = app->flash_kind == 1 ? 0 : 255;
    for (pixel = 0U; pixel < DEMO_WIDTH * DEMO_HEIGHT; ++pixel) {
        vox_u8 *destination = &demo_pixels[pixel * VOX_SOFTWARE_RGB_BYTES];
        destination[0] = (vox_u8)((double)destination[0] +
            ((double)target_red - destination[0]) * strength);
        destination[1] = (vox_u8)((double)destination[1] +
            ((double)target_green - destination[1]) * strength);
        destination[2] = (vox_u8)((double)destination[2] +
            ((double)target_blue - destination[2]) * strength);
    }
    app->flash_strength -= app->frame_seconds * 3.8;
    if (app->flash_strength < 0.0) app->flash_strength = 0.0;
}

static void demo_draw_play(demo_app *app)
{
    vox_software_view view;
    vox_result render_status;
    demo_update_player_camera(app);
    demo_camera_view(app, &view);
    /* Laptop Mode changes only presentation cost. The authoritative world,
     * effects, inputs and 60 Hz simulation remain byte-for-byte identical. */
    demo_render_config.gi_quality = app->options.laptop_mode ?
        VOX_GI_COMPATIBILITY : (vox_u16)app->options.gi_quality;
    demo_render_overlay_begin();
    demo_build_render_world(app);
    render_status = vox_software_render_view_ex(&demo_match.world,
        &demo_target, &demo_render_config, &view);
    demo_render_overlay_restore();
    if (render_status != VOX_OK) {
        /* A validated camera should never reach this path. Keep presentation
         * readable instead of exposing uninitialized or stale pixels. */
        memset(demo_pixels, 0, sizeof(demo_pixels));
    }
    app->scene_tick = demo_match.tick;
    app->scene_laptop = app->options.laptop_mode;
    app->scene_valid = render_status == VOX_OK;
    /* Keep the mouse player's authoritative aim synchronized to the camera
     * transform that is actually being presented this frame. */
    if (app->screen == DEMO_PLAY && app->mouse_inside &&
        app->player_input[0].active_source == DEMO_SOURCE_KEYBOARD) {
        demo_mouse_world(app, &app->aim_world_x[0], &app->aim_world_y[0]);
    }
    if (app->screen == DEMO_PLAY && app->mouse_inside &&
        app->player_input[0].active_source == DEMO_SOURCE_KEYBOARD) {
        demo_draw_crosshair(app, 0, app->mouse_x, app->mouse_y);
    }
    demo_draw_world_feedback(app);
    demo_draw_hud(app);
    demo_draw_notifications(app);
    demo_draw_debug(app);
    if (app->screen == DEMO_PAUSE) {
        demo_dark_panel(88, 65, 144, 70);
        vox_ui_text_center_shadow(&demo_ui, 160, 76, 1, "PAUSED",
                                  DEMO_VGA_YELLOW);
        if (app->controller_disconnected) {
            vox_ui_text_center(&demo_ui, 160, 94, 1,
                               "CONTROLLER LOST  RECONNECT OR RECLAIM",
                               DEMO_VGA_LIGHT_RED);
        }
        demo_menu_item(105, "RESUME", 0, app->selection);
        demo_menu_item(120, "EXIT TO TITLE", 1, app->selection);
    }
    demo_apply_flash(app);
}

static void demo_draw_results(demo_app *app)
{
    char line[64];
    int player;
    demo_draw_play(app);
    demo_dark_panel(55, 36, 210, 132);
    vox_ui_text_center_shadow(&demo_ui, 160, 46, 1, "MATCH RESULTS",
                              DEMO_VGA_YELLOW);
    if (demo_match.result_draw) {
        strcpy(line, "DRAW");
    } else if (demo_match.winner_player < VOX_DIGS_MAX_SLOTS) {
        sprintf(line, "%s WINS",
                app->player_names[demo_match.winner_player]);
    } else {
        strcpy(line, "SHIFT COMPLETE");
    }
    vox_ui_text_center(&demo_ui, 160, 68, 1, line, DEMO_VGA_WHITE);
    for (player = 0; player < (int)demo_match.rules.player_count; ++player) {
        sprintf(line, "%s  K%u D%u", app->player_names[player],
                (unsigned int)demo_match.scores[player],
                (unsigned int)demo_match.deaths[player]);
        vox_ui_text_center(&demo_ui, 160, 84 + player * 11, 1, line,
                           player == (int)demo_match.winner_player ?
                           255U : 170U,
                           player == (int)demo_match.winner_player ?
                           255U : 170U,
                           player == (int)demo_match.winner_player ?
                           85U : 170U);
    }
    sprintf(line, "STATE HASH %08lX", (unsigned long)demo_match.state_hash);
    vox_ui_text_center(&demo_ui, 160, 132, 1, line,
                       DEMO_VGA_LIGHT_CYAN);
    vox_ui_text_center(&demo_ui, 160, 150, 1,
                       "ENTER RETURNS TO TITLE", DEMO_VGA_YELLOW);
}

static void demo_render(demo_app *app)
{
    /*
     * Rebuilt every frame from what actually gets drawn, so the registry can
     * never describe a layout that is no longer on screen.
     */
    demo_rows_reset();
    if (app->screen == DEMO_TITLE) {
        demo_draw_title(app);
    } else if (app->screen == DEMO_SETUP) {
        demo_draw_setup(app);
    } else if (app->screen == DEMO_CUSTOMIZE) {
        demo_draw_customize(app);
    } else if (app->screen == DEMO_NAME_EDITOR) {
        demo_draw_name_editor(app);
    } else if (app->screen == DEMO_OPTIONS) {
        demo_draw_options(app);
    } else if (app->screen == DEMO_INBOX) {
        demo_draw_inbox(app);
    } else if (app->screen == DEMO_LOG) {
        demo_draw_log(app);
    } else if (app->screen == DEMO_CONTROLS) {
        demo_draw_controls(app);
    } else if (app->screen == DEMO_INPUT_OPTIONS) {
        demo_draw_input_options(app);
    } else if (app->screen == DEMO_RESULTS) {
        demo_draw_results(app);
    } else {
        demo_draw_play(app);
    }
}

static vox_u32 demo_clamp_world_coordinate(long value, vox_u32 limit)
{
    if (limit == 0U || value < 0L) return 0U;
    if ((vox_u32)value >= limit) return limit - 1U;
    return (vox_u32)value;
}

static void demo_prepare_foundry_world(void)
{
    vox_u32 scale_x = VOX_WORLD_WIDTH / 256U;
    vox_u32 scale_y = VOX_WORLD_HEIGHT / 160U;
    long player_x = demo_match.players[0].position_x.value_q16 / 65536L;
    long player_y = demo_match.players[0].position_y.value_q16 / 65536L;
    vox_u32 sand_left;
    vox_u32 sand_right;
    vox_u32 sand_y;
    vox_u32 chamber_left;
    vox_u32 chamber_right;
    vox_u32 chamber_top;
    vox_u32 chamber_bottom;
    vox_u32 gas_left;
    vox_u32 gas_right;
    vox_u32 gas_top;
    vox_u32 gas_bottom;
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    if (scale_x == 0U) scale_x = 1U;
    if (scale_y == 0U) scale_y = 1U;
    sand_left = demo_clamp_world_coordinate(
        player_x - (long)(16U * scale_x), VOX_WORLD_WIDTH);
    sand_right = demo_clamp_world_coordinate(
        player_x - (long)(9U * scale_x), VOX_WORLD_WIDTH);
    sand_y = demo_clamp_world_coordinate(
        player_y - (long)(10U * scale_y), VOX_WORLD_HEIGHT - 1U);
    chamber_left = demo_clamp_world_coordinate(
        player_x + (long)(8U * scale_x), VOX_WORLD_WIDTH);
    chamber_right = demo_clamp_world_coordinate(
        player_x + (long)(18U * scale_x), VOX_WORLD_WIDTH);
    chamber_top = demo_clamp_world_coordinate(
        player_y - (long)(12U * scale_y), VOX_WORLD_HEIGHT);
    chamber_bottom = demo_clamp_world_coordinate(
        player_y - (long)(3U * scale_y), VOX_WORLD_HEIGHT);
    gas_left = demo_clamp_world_coordinate(
        player_x + (long)(11U * scale_x), VOX_WORLD_WIDTH);
    gas_right = demo_clamp_world_coordinate(
        player_x + (long)(15U * scale_x), VOX_WORLD_WIDTH);
    gas_top = demo_clamp_world_coordinate(
        player_y - (long)(9U * scale_y), VOX_WORLD_HEIGHT);
    gas_bottom = demo_clamp_world_coordinate(
        player_y - (long)(5U * scale_y), VOX_WORLD_HEIGHT);
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (x = sand_left; x <= sand_right; ++x) {
            (void)vox_world_set(&demo_match.world, x, sand_y, z,
                                VOX_MAT_SAND, 20L << 16);
            (void)vox_world_set(&demo_match.world, x, sand_y + 1U, z,
                                VOX_MAT_SAND, 20L << 16);
        }
        for (y = chamber_top; y <= chamber_bottom; ++y) {
            for (x = chamber_left; x <= chamber_right; ++x) {
                int boundary = y == chamber_top || y == chamber_bottom ||
                               x == chamber_left || x == chamber_right;
                (void)vox_world_set(&demo_match.world, x, y, z,
                                    boundary ? VOX_MAT_METAL : VOX_MAT_AIR,
                                    20L << 16);
            }
        }
    }
    for (y = gas_top; y <= gas_bottom; ++y) {
        for (x = gas_left; x <= gas_right; ++x) {
            (void)vox_world_set(&demo_match.world, x, y,
                                VOX_WORLD_DEPTH - 1U,
                                VOX_MAT_FIREDAMP, 20L << 16);
        }
    }
}

static int demo_start_match(demo_app *app, int foundry)
{
    vox_digs_rules rules;
    vox_u16 player;
    demo_refresh_roster(app);
    vox_digs_rules_classic(&rules);
    rules.player_count = (vox_u16)(foundry ? 1 :
                         app->local_players + app->bots);
    rules.bot_mask = 0U;
    for (player = (vox_u16)(foundry ? 1 : app->local_players);
         player < rules.player_count; ++player) {
        rules.bot_mask = (vox_u16)(rules.bot_mask |
                                   (vox_u16)(1U << player));
    }
    rules.fx_budget = app->options.fx_profile == 0 ? VOX_DIGS_FX_RETRO :
                      (app->options.fx_profile == 2 ? VOX_DIGS_FX_CARNAGE :
                       VOX_DIGS_FX_STANDARD);
    rules.map_style = (vox_u16)app->map_style;
    rules.weapon_mask = foundry ? demo_arsenal_masks[DEMO_ARSENAL_FULL] :
                        demo_arsenal_masks[app->arsenal];
    rules.seed = app->seed;
    if (foundry) {
        rules.match_ticks = 10U * 60U * VOX_DIGS_TICKS_PER_SECOND;
        rules.lava_start_tick = rules.match_ticks - 60U;
        rules.score_limit = 100U;
    } else {
        /*
         * UNLIMITED is ninety-nine minutes, not infinity.  Zero match_ticks
         * is rejected by digs_validate_rules and the lava ramp divides by
         * (match_ticks - lava_start_tick), so a genuinely endless match
         * would either fail validation or divide by zero.  Ninety-nine
         * minutes is past any session anyone will sit through.
         */
        int minutes = demo_time_limits[app->time_limit_index];
        vox_u32 lead;
        if (minutes <= 0) {
            minutes = DEMO_TIME_UNLIMITED_MINUTES;
        }
        rules.match_ticks = (vox_u32)minutes * 60U *
                            VOX_DIGS_TICKS_PER_SECOND;
        if (demo_lava_lead_seconds[app->lava_index] < 0) {
            /*
             * Lava OFF still has to start, because lava_start_tick must be
             * below match_ticks.  One tick before the end means it never
             * gets off the floor.
             */
            lead = 1U;
        } else {
            lead = (vox_u32)demo_lava_lead_seconds[app->lava_index] *
                   VOX_DIGS_TICKS_PER_SECOND;
            if (lead >= rules.match_ticks) {
                lead = rules.match_ticks - 1U;
            }
        }
        rules.lava_start_tick = rules.match_ticks - lead;
        rules.score_limit = (vox_u32)
                            demo_score_limits[app->score_limit_index];
        rules.respawn_mode = (vox_u16)app->respawn_mode;
        rules.respawn_delay_ticks = (vox_u16)
            (demo_respawn_delays[app->respawn_delay_index] *
             (int)VOX_DIGS_TICKS_PER_SECOND);
    }
    if (vox_digs_match_init_ex(&demo_match, &rules,
                               demo_chronicle_ready ?
                               &demo_chronicle.memory : 0) != VOX_OK) {
        return 0;
    }
    if (foundry) {
        demo_prepare_foundry_world();
        demo_match.state_hash = vox_digs_hash(&demo_match);
    }
    app->foundry = foundry;
    (void)demo_normalize_camera_zoom(app);
    app->selected_tool[0] = demo_first_weapon(rules.weapon_mask);
    app->selected_tool[1] = app->selected_tool[0];
    app->last_event_sequence = 0U;
    memset(app->damage_popups, 0, sizeof(app->damage_popups));
    memset(app->hit_markers, 0, sizeof(app->hit_markers));
    memset(app->killfeed, 0, sizeof(app->killfeed));
    memset(app->bubbles, 0, sizeof(app->bubbles));
    memset(app->miner_hit_ttl, 0, sizeof(app->miner_hit_ttl));
    memset(app->victory_bark_ttl, 0, sizeof(app->victory_bark_ttl));
    memset(app->crosshair_pulse_ttl, 0,
           sizeof(app->crosshair_pulse_ttl));
    memset(app->multikill_count, 0, sizeof(app->multikill_count));
    memset(app->spree_count, 0, sizeof(app->spree_count));
    memset(app->last_kill_tick, 0, sizeof(app->last_kill_tick));
    memset(app->last_bark_tick, 0, sizeof(app->last_bark_tick));
    memset(app->bark_sequence, 0, sizeof(app->bark_sequence));
    memset(app->rope_down, 0, sizeof(app->rope_down));
    memset(app->rope_latched, 0, sizeof(app->rope_latched));
    memset(app->haptic, 0, sizeof(app->haptic));
    memset(app->rail_traces, 0, sizeof(app->rail_traces));
    memset(app->rail_origin_x_q16, 0, sizeof(app->rail_origin_x_q16));
    memset(app->rail_origin_y_q16, 0, sizeof(app->rail_origin_y_q16));
    app->scene_valid = 0;
    app->global_bark_tick = 0U;
    memset(app->banners, 0, sizeof(app->banners));
    app->death_camera_hold = 0U;
    app->death_camera_player = VOX_DIGS_NO_PLAYER;
    app->camera_velocity_x = 0.0;
    app->camera_velocity_y = 0.0;
    app->camera_scale_velocity = 0.0;
    app->camera_trauma = 0.0;
    app->camera_world_x = (double)demo_match.players[0].
                          position_x.value_q16 / 65536.0;
    app->camera_world_y = (double)demo_match.players[0].
                          position_y.value_q16 / 65536.0;
    app->camera_scale = (double)app->camera_zoom;
    for (player = 0U; player < VOX_DIGS_MAX_SLOTS; ++player) {
        app->previous_player_x[player] =
            demo_match.players[player].position_x.value_q16;
        app->previous_player_y[player] =
            demo_match.players[player].position_y.value_q16;
        if (player < DEMO_LOCAL_MAX) {
            vox_i32 aim_x = demo_match.players[player].position_x.value_q16 /
                             65536L + (player == 0U ? 28L : -28L);
            vox_i32 aim_y = demo_match.players[player].position_y.value_q16 /
                             65536L;
            if (aim_x < 0L) aim_x = 0L;
            if (aim_x >= (vox_i32)VOX_WORLD_WIDTH) {
                aim_x = (vox_i32)VOX_WORLD_WIDTH - 1L;
            }
            if (aim_y < 0L) aim_y = 0L;
            if (aim_y >= (vox_i32)VOX_WORLD_HEIGHT) {
                aim_y = (vox_i32)VOX_WORLD_HEIGHT - 1L;
            }
            app->aim_world_x[player] = (vox_u32)aim_x;
            app->aim_world_y[player] = (vox_u32)aim_y;
            app->player_input[player].aim_direction_x =
                player == 0U ? 1.0 : -1.0;
            app->player_input[player].aim_direction_y = 0.0;
            app->player_input[player].aim_distance = 24.0;
            app->player_input[player].aim_magnitude = 1.0;
            app->player_input[player].suppress_ticks = 0;
        }
    }
    app->screen = DEMO_PLAY;
    app->selection = 0;
    demo_audio_play(app, DEMO_SOUND_START);
    demo_audio_speak_text(app, "GET TO WORK!", VOX_AUDIO_SPEECH_DEEP,
                          VOX_AUDIO_PRIORITY_ANNOUNCER,
                          VOX_AUDIO_PAN_CENTER);
    return 1;
}

static demo_controller *demo_controller_for_player(demo_app *app,
                                                   int player)
{
    int index;
    for (index = 0; index < (int)DEMO_CONTROLLER_MAX; ++index) {
        if (demo_controller_connected(&app->controllers[index]) &&
            app->controllers[index].claimed_player == player) {
            return &app->controllers[index];
        }
    }
    return 0;
}

static void demo_clamp_aim(demo_app *app, int player)
{
    if (app->aim_world_x[player] >= VOX_WORLD_WIDTH) {
        app->aim_world_x[player] = VOX_WORLD_WIDTH - 1U;
    }
    if (app->aim_world_y[player] >= VOX_WORLD_HEIGHT) {
        app->aim_world_y[player] = VOX_WORLD_HEIGHT - 1U;
    }
}

static double demo_weapon_aim_range(demo_app *app, int player,
                                    int rope_held)
{
    const vox_digs_weapon_properties *properties;
    if (rope_held) return DEMO_ROPE_AIM_RANGE;
    properties = vox_digs_weapon_get((vox_u16)app->selected_tool[player]);
    if (properties != 0 && (properties->flags & VOX_DIGS_WEAPON_MELEE)) {
        return 8.0;
    }
    return 24.0;
}

static int demo_aim_line_clear(int start_x, int start_y,
                               int end_x, int end_y)
{
    int delta_x = end_x - start_x;
    int delta_y = end_y - start_y;
    int absolute_x = delta_x < 0 ? -delta_x : delta_x;
    int absolute_y = delta_y < 0 ? -delta_y : delta_y;
    int steps = absolute_x > absolute_y ? absolute_x : absolute_y;
    int step;
    if (steps <= 1) return 1;
    for (step = 2; step < steps; ++step) {
        int x = start_x + delta_x * step / steps;
        int y = start_y + delta_y * step / steps;
        const vox_cell *cell;
        if (x < 0 || y < 0 || x >= (int)VOX_WORLD_WIDTH ||
            y >= (int)VOX_WORLD_HEIGHT) return 0;
        cell = vox_world_cell(&demo_match.world, (vox_u32)x, (vox_u32)y,
                              VOX_WORLD_DEPTH - 1U);
        if (cell != 0 && cell->material != VOX_MAT_AIR) return 0;
    }
    return 1;
}

static int demo_aim_near_visible_target(int player, double direction_x,
                                        double direction_y, double reach)
{
    int body_x = (int)(demo_match.players[player].position_x.value_q16 /
                       65536L);
    int body_y = (int)(demo_match.players[player].position_y.value_q16 /
                       65536L);
    int target;
    for (target = 0; target < (int)demo_match.rules.player_count; ++target) {
        double delta_x;
        double delta_y;
        double forward;
        double lateral;
        int target_x;
        int target_y;
        if (target == player || !demo_match.alive[target]) continue;
        target_x = (int)(demo_match.players[target].position_x.value_q16 /
                         65536L);
        target_y = (int)(demo_match.players[target].position_y.value_q16 /
                         65536L);
        delta_x = (double)(target_x - body_x);
        delta_y = (double)(target_y - body_y);
        forward = delta_x * direction_x + delta_y * direction_y;
        lateral = delta_x * direction_y - delta_y * direction_x;
        if (lateral < 0.0) lateral = -lateral;
        if (forward > 0.0 && forward <= reach + 4.0 && lateral <= 3.5 &&
            demo_aim_line_clear(body_x, body_y, target_x, target_y)) {
            return 1;
        }
    }
    return 0;
}

static void demo_update_controller_aim(demo_app *app, int player,
                                       demo_controller *controller,
                                       Sint16 raw_x, Sint16 raw_y,
                                       int rope_held)
{
    demo_player_input *input = &app->player_input[player];
    double desired_x;
    double desired_y;
    double magnitude;
    double reach = demo_weapon_aim_range(app, player, rope_held);
    double smoothing = input->sensitivity == 0 ? 0.24 :
                       (input->sensitivity == 1 ? 0.38 : 0.58);
    int active = demo_radial_response(raw_x, raw_y,
        demo_input_deadzone(input, controller), input->sensitivity,
        &desired_x, &desired_y, &magnitude);
    if (active) {
        double length;
        if (input->aim_slowdown > 0 &&
            demo_aim_near_visible_target(player, desired_x, desired_y,
                                         reach)) {
            smoothing *= input->aim_slowdown == 1 ? 0.65 : 0.45;
        }
        input->aim_direction_x +=
            (desired_x - input->aim_direction_x) * smoothing;
        input->aim_direction_y +=
            (desired_y - input->aim_direction_y) * smoothing;
        length = demo_sqrt(input->aim_direction_x * input->aim_direction_x +
                           input->aim_direction_y * input->aim_direction_y);
        if (length > 0.0001) {
            input->aim_direction_x /= length;
            input->aim_direction_y /= length;
        }
        input->aim_magnitude = magnitude;
    }
    input->aim_distance = reach * input->aim_magnitude;
    {
        long body_x = demo_match.players[player].position_x.value_q16 /
                      65536L;
        long body_y = demo_match.players[player].position_y.value_q16 /
                      65536L;
        long world_x = body_x + (long)(input->aim_direction_x *
                                       input->aim_distance);
        long world_y = body_y + (long)(input->aim_direction_y *
                                       input->aim_distance);
        if (world_x < 0L) world_x = 0L;
        if (world_y < 0L) world_y = 0L;
        app->aim_world_x[player] = (vox_u32)world_x;
        app->aim_world_y[player] = (vox_u32)world_y;
    }
}

static void demo_submit_human_input(demo_app *app)
{
    const vox_u8 *keys = SDL_GetKeyboardState(0);
    Uint32 mouse_buttons = SDL_GetMouseState(0, 0);
    int player;
    for (player = 0; player < app->local_players; ++player) {
        vox_digs_input input;
        demo_controller *controller = demo_controller_for_player(app, player);
        Sint16 move_x = 0;
        Sint16 move_y = 0;
        int fire = 0;
        int bark = 0;
        int previous_down = 0;
        int next_down = 0;
        int physical_rope = 0;
        int use_controller;
        SDL_Scancode *left = demo_keyboard_binding(app, player, 0);
        SDL_Scancode *right = demo_keyboard_binding(app, player, 1);
        SDL_Scancode *jump = demo_keyboard_binding(app, player, 2);
        SDL_Scancode *steam = demo_keyboard_binding(app, player, 3);
        SDL_Scancode *rope = demo_keyboard_binding(app, player, 4);
        SDL_Scancode *fire_key = demo_keyboard_binding(app, player, 5);
        SDL_Scancode *previous_key = demo_keyboard_binding(app, player, 6);
        SDL_Scancode *next_key = demo_keyboard_binding(app, player, 7);
        SDL_Scancode *bark_key = demo_keyboard_binding(app, player, 8);
        if (controller != 0) {
            Sint16 activity_axes[4];
            double activity = 0.0;
            int axis;
            demo_update_controller_calibration(controller);
            activity_axes[0] = demo_controller_axis(
                controller, SDL_CONTROLLER_AXIS_LEFTX);
            activity_axes[1] = demo_controller_axis(
                controller, SDL_CONTROLLER_AXIS_LEFTY);
            activity_axes[2] = demo_controller_axis(
                controller, SDL_CONTROLLER_AXIS_RIGHTX);
            activity_axes[3] = demo_controller_axis(
                controller, SDL_CONTROLLER_AXIS_RIGHTY);
            for (axis = 0; axis < 4; axis += 2) {
                double x = (double)activity_axes[axis] / 32767.0;
                double y = (double)activity_axes[axis + 1] / 32767.0;
                double magnitude = demo_sqrt(x * x + y * y);
                if (magnitude > activity) activity = magnitude;
            }
            if (demo_controller_axis(controller,
                    SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000 ||
                demo_controller_axis(controller,
                    SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) {
                activity = 1.0;
            }
            if (!controller->calibrating &&
                activity > demo_input_deadzone(&app->player_input[player],
                                                controller) +
                           DEMO_CONTROLLER_ACTIVITY_MARGIN) {
                ++controller->activity_frames;
                if (controller->activity_frames >= 2) {
                    (void)demo_activate_source(app, player,
                                               DEMO_SOURCE_CONTROLLER, 0);
                }
            } else {
                controller->activity_frames = 0;
            }
        }
        use_controller = app->player_input[player].active_source ==
                         DEMO_SOURCE_CONTROLLER;
        memset(&input, 0, sizeof(input));
        input.abi_version = VOX_ABI_VERSION;
        input.struct_size = (vox_u32)sizeof(input);
        input.player = (vox_u16)player;
        if (!use_controller) {
            previous_down = previous_key != 0 && keys[*previous_key];
            next_down = next_key != 0 && keys[*next_key];
            if (player == 0 && app->mouse_inside &&
                !app->keyboard_aim_active) {
                demo_mouse_world(app, &app->aim_world_x[0],
                                 &app->aim_world_y[0]);
            }
            if (left != 0 && keys[*left]) move_x = -32767;
            if (right != 0 && keys[*right]) move_x = 32767;
            if (jump != 0 && keys[*jump]) {
                input.actions = (vox_u16)(input.actions |
                                          VOX_DIGS_ACTION_JUMP);
            }
            if (steam != 0 && keys[*steam]) {
                input.actions = (vox_u16)(input.actions |
                                          VOX_DIGS_ACTION_STEAM);
            }
            if (rope != 0 && *rope != SDL_SCANCODE_UNKNOWN &&
                keys[*rope]) physical_rope = 1;
            if (fire_key != 0 && keys[*fire_key]) fire = 1;
            if (player == 0 && app->mouse_inside) {
                /*
                 * Left fires, middle throws the rope, right runs the steam
                 * pack.  Rope moved off the right button so the two
                 * continuous-hold actions sit under the two large buttons
                 * and the rope gets its own.
                 */
                if ((mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0U) {
                    fire = 1;
                }
                if ((mouse_buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0U) {
                    physical_rope = 1;
                }
                if ((mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0U) {
                    input.actions = (vox_u16)(input.actions |
                                              VOX_DIGS_ACTION_STEAM);
                }
            }
            if (bark_key != 0 && keys[*bark_key]) bark = 1;
            if (player == 0) {
                if (keys[SDL_SCANCODE_W]) move_y = -32767;
                if (keys[SDL_SCANCODE_S]) move_y = 32767;
                /*
                 * Arrow keys are player two's movement, so they only steer
                 * player one's aim when nobody else is using them.  That
                 * keeps two-player controls exactly as they were while
                 * giving the common solo case full keyboard play.
                 */
                if (app->local_players == 1) {
                    int aimed = 0;
                    /*
                     * Solo play leaves player two's whole right-hand cluster
                     * free, so it becomes a complete keyboard scheme: left
                     * hand moves and jumps, right hand aims, fires, thrusts,
                     * and ropes. Nothing here is reachable with two locals.
                     */
                    if (keys[SDL_SCANCODE_RCTRL] ||
                        keys[SDL_SCANCODE_RETURN] ||
                        keys[SDL_SCANCODE_KP_ENTER]) {
                        fire = 1;
                    }
                    if (keys[SDL_SCANCODE_RSHIFT]) {
                        input.actions = (vox_u16)(input.actions |
                                                  VOX_DIGS_ACTION_STEAM);
                    }
                    if (keys[SDL_SCANCODE_SLASH] ||
                        keys[SDL_SCANCODE_RALT]) {
                        physical_rope = 1;
                    }
                    if (keys[SDL_SCANCODE_UP] &&
                        app->aim_world_y[0] > 0U) {
                        --app->aim_world_y[0];
                        aimed = 1;
                    }
                    if (keys[SDL_SCANCODE_DOWN]) {
                        ++app->aim_world_y[0];
                        aimed = 1;
                    }
                    if (keys[SDL_SCANCODE_LEFT] &&
                        app->aim_world_x[0] > 0U) {
                        --app->aim_world_x[0];
                        aimed = 1;
                    }
                    if (keys[SDL_SCANCODE_RIGHT]) {
                        ++app->aim_world_x[0];
                        aimed = 1;
                    }
                    if (aimed) {
                        app->keyboard_aim_active = 1;
                    }
                }
            } else {
                if (keys[SDL_SCANCODE_UP]) move_y = -32767;
                if (keys[SDL_SCANCODE_DOWN]) move_y = 32767;
                if (keys[SDL_SCANCODE_I] &&
                    app->aim_world_y[player] > 0U) {
                    --app->aim_world_y[player];
                }
                if (keys[SDL_SCANCODE_K]) ++app->aim_world_y[player];
                if (keys[SDL_SCANCODE_J] &&
                    app->aim_world_x[player] > 0U) {
                    --app->aim_world_x[player];
                }
                if (keys[SDL_SCANCODE_L]) ++app->aim_world_x[player];
            }
        } else if (controller != 0) {
            double direction_x;
            double direction_y;
            double magnitude;
            Sint16 left_x = demo_controller_axis(
                controller, SDL_CONTROLLER_AXIS_LEFTX);
            Sint16 left_y = demo_controller_axis(
                controller, SDL_CONTROLLER_AXIS_LEFTY);
            Sint16 aim_x = demo_controller_axis(
                controller, SDL_CONTROLLER_AXIS_RIGHTX);
            Sint16 aim_y = demo_controller_axis(
                controller, SDL_CONTROLLER_AXIS_RIGHTY);
            Sint16 zoom_modifier = demo_controller_axis(
                controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            int zoom_direction = demo_controller_zoom_step(
                controller, zoom_modifier, aim_y);
            int rope_held;
            if (zoom_direction != 0) demo_change_zoom(app, zoom_direction);
            physical_rope = demo_controller_button(
                controller, app->bindings.pad_rope);
            rope_held = app->player_input[player].rope_mode ==
                        DEMO_ROPE_TOGGLE ? app->rope_latched[player] :
                        physical_rope;
            if (demo_radial_response(left_x, left_y,
                demo_input_deadzone(&app->player_input[player], controller),
                app->player_input[player].sensitivity,
                &direction_x, &direction_y, &magnitude)) {
                move_x = (Sint16)(direction_x * magnitude * 32767.0);
                move_y = (Sint16)(direction_y * magnitude * 32767.0);
            }
            if (demo_controller_button(controller,
                                       SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
                move_x = -32767;
            } else if (demo_controller_button(
                           controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
                move_x = 32767;
            }
            if (demo_controller_button(controller,
                                       SDL_CONTROLLER_BUTTON_DPAD_UP)) {
                move_y = -32767;
            } else if (demo_controller_button(
                           controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
                move_y = 32767;
            }
            if (demo_controller_button(controller, app->bindings.pad_jump)) {
                input.actions = (vox_u16)(input.actions |
                                          VOX_DIGS_ACTION_JUMP);
            }
            if (demo_controller_button(controller, app->bindings.pad_steam)) {
                input.actions = (vox_u16)(input.actions |
                                          VOX_DIGS_ACTION_STEAM);
            }
            if (demo_controller_button(controller, app->bindings.pad_fire) ||
                demo_controller_axis(controller,
                    SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) {
                fire = 1;
            }
            if (demo_controller_button(controller,
                                       app->bindings.pad_bark)) bark = 1;
            if (zoom_modifier <= 16000) {
                demo_update_controller_aim(app, player, controller,
                                           aim_x, aim_y, rope_held);
            }
        }
        if (app->player_input[player].suppress_ticks > 0) {
            input.actions = 0U;
            move_x = 0;
            move_y = 0;
            fire = 0;
            bark = 0;
            previous_down = 0;
            next_down = 0;
            app->rope_down[player] = physical_rope;
            --app->player_input[player].suppress_ticks;
        } else if (app->player_input[player].rope_mode == DEMO_ROPE_TOGGLE) {
            if (physical_rope && !app->rope_down[player]) {
                app->rope_latched[player] = !app->rope_latched[player];
            }
            app->rope_down[player] = physical_rope;
        } else {
            app->rope_latched[player] = 0;
            app->rope_down[player] = physical_rope;
        }
        if ((app->player_input[player].rope_mode == DEMO_ROPE_TOGGLE &&
             app->rope_latched[player]) ||
            (app->player_input[player].rope_mode == DEMO_ROPE_HOLD &&
             physical_rope)) {
            input.actions = (vox_u16)(input.actions |
                                      VOX_DIGS_ACTION_ROPE);
        }
        if (move_x < 0) {
            input.actions = (vox_u16)(input.actions | VOX_DIGS_ACTION_LEFT);
        } else if (move_x > 0) {
            input.actions = (vox_u16)(input.actions | VOX_DIGS_ACTION_RIGHT);
        }
        input.move_x_q15 = (vox_i16)move_x;
        input.move_y_q15 = (vox_i16)move_y;
        demo_clamp_aim(app, player);
        input.aim_x = (vox_u16)app->aim_world_x[player];
        input.aim_y = (vox_u16)app->aim_world_y[player];
        input.selected_weapon = (vox_u16)app->selected_tool[player];
        input.reserved = 0U;
        if (fire && demo_match.alive[player]) {
            input.actions = (vox_u16)(input.actions |
                                      VOX_DIGS_ACTION_FIRE);
        }
        /*
         * Keep holding jump after the hop tops out and the steampack takes
         * over, so nobody has to discover a separate control to get height.
         * A dedicated steam binding still works, and still works on the
         * ground, which this deliberately does not.
         *
         * This is an input binding and lives here rather than in the
         * simulation: making it a movement rule applied it to bots too, and
         * a bot holds JUMP as its reflex for being blocked and latches that
         * for a whole decision window.  All three of them flew constantly,
         * which cost sixteen extra unattributed deaths over a 10800-tick
         * soak.  Bots reach for the steampack where they mean it instead.
         */
        if ((input.actions & VOX_DIGS_ACTION_JUMP) != 0U &&
            demo_match.alive[player] &&
            demo_match.jump_hold_ticks[player] == 0U &&
            (demo_match.players[player].flags &
             VOX_PHYSICS_BODY_GROUNDED) == 0U) {
            input.actions = (vox_u16)(input.actions |
                                      VOX_DIGS_ACTION_STEAM);
        }
        /*
         * Speaking goes through the input stream like everything else,
         * because what gets said moves contract state and therefore the
         * match hash -- a bark raised straight into the port would make a
         * replay diverge from the match it was recorded from.
         *
         * This has to be set BEFORE the submit.  It was not, so the bit was
         * written onto an input that had already been handed over and the
         * bark button did nothing at all.
         */
        if (bark && !app->bark_down[player] && demo_match.alive[player]) {
            input.actions = (vox_u16)(input.actions | VOX_DIGS_ACTION_BARK);
        }
        (void)vox_digs_submit_input(&demo_match, &input);
        if (player < (int)DEMO_LOCAL_MAX) {
            if (previous_down && !app->keyboard_previous_down[player]) {
                demo_cycle_weapon(app, player, -1);
            }
            if (next_down && !app->keyboard_next_down[player]) {
                demo_cycle_weapon(app, player, 1);
            }
            app->keyboard_previous_down[player] = previous_down;
            app->keyboard_next_down[player] = next_down;
        }
        app->bark_down[player] = bark;
        if (!demo_match.alive[player] && fire && !app->fire_down[player]) {
            (void)vox_digs_request_respawn(&demo_match, (vox_u16)player);
        }
        app->fire_down[player] = fire;
    }
}

static int demo_event_is_local(const demo_app *app,
                               const vox_digs_event *event)
{
    return (event->source < (vox_u16)app->local_players) ||
           (event->target < (vox_u16)app->local_players);
}

static void demo_add_damage_popup(demo_app *app,
                                  const vox_digs_event *event)
{
    int slot;
    int free_slot;
    if (!app->options.damage_numbers || !demo_event_is_local(app, event)) {
        return;
    }
    free_slot = -1;
    for (slot = 0; slot < (int)DEMO_DAMAGE_POPUP_MAX; ++slot) {
        demo_damage_popup *popup = &app->damage_popups[slot];
        if (popup->active && popup->target == event->target &&
            popup->ttl > 84U) {
            popup->amount = (vox_u16)(popup->amount + event->magnitude);
            popup->ttl = 90U;
            popup->world_x_q16 = event->position_x_q16;
            popup->world_y_q16 = event->position_y_q16;
            return;
        }
        if (!popup->active && free_slot < 0) free_slot = slot;
    }
    if (free_slot < 0) free_slot = (int)(event->sequence %
                                          DEMO_DAMAGE_POPUP_MAX);
    app->damage_popups[free_slot].active = 1U;
    app->damage_popups[free_slot].amount = event->magnitude;
    app->damage_popups[free_slot].ttl = 90U;
    app->damage_popups[free_slot].target = event->target;
    app->damage_popups[free_slot].world_x_q16 = event->position_x_q16;
    app->damage_popups[free_slot].world_y_q16 = event->position_y_q16;
}

static void demo_haptic_impulse(demo_app *app, vox_u16 player,
                                int kind, vox_u16 magnitude,
                                vox_u16 locality_q15)
{
    demo_haptic_envelope *envelope;
    vox_u32 low = 9000U;
    vox_u32 high = 16000U;
    vox_u16 ticks = 4U;
    if (app == 0 || app->options.haptic_level == 0 ||
        player >= (vox_u16)app->local_players || locality_q15 == 0U) return;
    if (kind == DEMO_HAPTIC_FIRE) {
        low = 7000U; high = 19000U; ticks = 3U;
    } else if (kind == DEMO_HAPTIC_RAIL) {
        low = 46000U; high = 65535U; ticks = 12U;
    } else if (kind == DEMO_HAPTIC_ROPE) {
        low = 10000U; high = 21000U; ticks = 4U;
    } else if (kind == DEMO_HAPTIC_HIT) {
        low = 11000U + (vox_u32)magnitude * 350U;
        high = 9000U + (vox_u32)magnitude * 440U;
        ticks = magnitude > 50U ? 7U : 4U;
    } else if (kind == DEMO_HAPTIC_KILL) {
        low = 60000U; high = 52000U; ticks = 14U;
    } else if (kind == DEMO_HAPTIC_EXPLOSION) {
        low = 38000U + (vox_u32)magnitude * 140U;
        high = 28000U + (vox_u32)magnitude * 180U;
        ticks = 10U;
    } else if (kind == DEMO_HAPTIC_CRUMBLE) {
        low = 18000U + (vox_u32)magnitude * 90U;
        high = 5000U; ticks = 6U;
    }
    if (low > 65535U) low = 65535U;
    if (high > 65535U) high = 65535U;
    low = low * locality_q15 / 32767U;
    high = high * locality_q15 / 32767U;
    envelope = &app->haptic[player];
    if (low > envelope->low_peak) envelope->low_peak = (vox_u16)low;
    if (high > envelope->high_peak) envelope->high_peak = (vox_u16)high;
    if (ticks > envelope->ticks_left) envelope->ticks_left = ticks;
    if (ticks > envelope->total_ticks) envelope->total_ticks = ticks;
}

static vox_u16 demo_haptic_locality(const vox_digs_event *event,
                                    vox_u16 player)
{
    long event_x;
    long event_y;
    long player_x;
    long player_y;
    long dx;
    long dy;
    long distance;
    if (event == 0 || player >= demo_match.rules.player_count) return 0U;
    if (event->source == player || event->target == player) return 32767U;
    event_x = event->position_x_q16 / 65536L;
    event_y = event->position_y_q16 / 65536L;
    player_x = demo_match.players[player].position_x.value_q16 / 65536L;
    player_y = demo_match.players[player].position_y.value_q16 / 65536L;
    dx = event_x - player_x;
    dy = event_y - player_y;
    if (dx < 0L) dx = -dx;
    if (dy < 0L) dy = -dy;
    distance = dx + dy;
    if (distance >= 128L) return 0U;
    return (vox_u16)((128L - distance) * 32767L / 128L);
}

static void demo_haptic_world(demo_app *app, const vox_digs_event *event,
                              int kind)
{
    vox_u16 player;
    for (player = 0U; player < (vox_u16)app->local_players; ++player) {
        demo_haptic_impulse(app, player, kind, event->magnitude,
                            demo_haptic_locality(event, player));
    }
}

static void demo_haptic_mix_sample(const demo_haptic_envelope *envelope,
                                   int level, vox_u16 *low_out,
                                   vox_u16 *high_out)
{
    static const vox_u16 gains[DEMO_HAPTIC_LEVEL_COUNT] = {
        0U, 14745U, 24575U, 32767U
    };
    vox_u32 low = 0U;
    vox_u32 high = 0U;
    if (envelope != 0 && level > 0 &&
        level < DEMO_HAPTIC_LEVEL_COUNT && envelope->ticks_left > 0U &&
        envelope->total_ticks > 0U) {
        vox_u32 gain = gains[level];
        low = (vox_u32)envelope->low_peak * envelope->ticks_left /
              envelope->total_ticks;
        high = (vox_u32)envelope->high_peak * envelope->ticks_left /
               envelope->total_ticks;
        low = low * gain / 32767U;
        high = high * gain / 32767U;
    }
    *low_out = (vox_u16)low;
    *high_out = (vox_u16)high;
}

static void demo_haptics_tick(demo_app *app)
{
    int player;
    for (player = 0; player < app->local_players &&
         player < (int)DEMO_LOCAL_MAX; ++player) {
        demo_haptic_envelope *envelope = &app->haptic[player];
        demo_controller *controller = demo_controller_for_player(app, player);
        vox_u16 low;
        vox_u16 high;
        demo_haptic_mix_sample(envelope, app->options.haptic_level,
                               &low, &high);
        if (app->options.haptic_level > 0 && envelope->ticks_left > 0U &&
            envelope->total_ticks > 0U) {
            --envelope->ticks_left;
        }
        if (controller != 0 &&
            (low != envelope->last_low || high != envelope->last_high)) {
            if (controller->raw_fallback) {
                (void)SDL_JoystickRumble(controller->joystick,
                    low, high, DEMO_HAPTIC_REFRESH_MS);
            } else {
                (void)SDL_GameControllerRumble(controller->handle,
                    low, high, DEMO_HAPTIC_REFRESH_MS);
            }
        }
        envelope->last_low = low;
        envelope->last_high = high;
        if (envelope->ticks_left == 0U) {
            envelope->low_peak = 0U;
            envelope->high_peak = 0U;
            envelope->total_ticks = 0U;
        }
    }
}

static const char *demo_player_name(const demo_app *app, vox_u16 player)
{
    if (player >= VOX_DIGS_MAX_SLOTS ||
        app->player_names[player][0] == '\0') return "THE MINE";
    return app->player_names[player];
}

static void demo_add_killfeed(demo_app *app, const char *text_value)
{
    int line;
    for (line = DEMO_KILLFEED_MAX - 1; line > 0; --line) {
        app->killfeed[line] = app->killfeed[line - 1];
    }
    demo_copy_text(app->killfeed[0].text,
                   sizeof(app->killfeed[0].text), text_value);
    app->killfeed[0].ttl = DEMO_KILLFEED_TICKS;
}

static void demo_set_banner(demo_app *app, const char *text_value,
                            int announce)
{
    int slot;
    for (slot = 0; slot < 3; ++slot) {
        if (app->banners[slot].ttl == 0U) break;
    }
    if (slot == 3) {
        app->banners[0] = app->banners[1];
        app->banners[1] = app->banners[2];
        slot = 2;
    }
    demo_copy_text(app->banners[slot].text,
                   sizeof(app->banners[slot].text), text_value);
    app->banners[slot].ttl = DEMO_BANNER_TICKS;
    if (announce) {
        demo_audio_speak_text(app, text_value, VOX_AUDIO_SPEECH_DEEP,
                              VOX_AUDIO_PRIORITY_ANNOUNCER,
                              VOX_AUDIO_PAN_CENTER);
    }
}

static void demo_register_kill(demo_app *app,
                               const vox_digs_event *event)
{
    char line[48];
    int source_local = event->source < (vox_u16)app->local_players;
    int target_local = event->target < (vox_u16)app->local_players;
    if (event->source < demo_match.rules.player_count &&
        event->source != event->target) {
        sprintf(line, "%s > %s", demo_player_name(app, event->source),
                demo_player_name(app, event->target));
    } else if (event->target < demo_match.rules.player_count) {
        sprintf(line, "%s LOST TO THE MINE",
                demo_player_name(app, event->target));
    } else {
        strcpy(line, "THE MINE CLAIMED ANOTHER");
    }
    demo_add_killfeed(app, line);
    if (event->target < DEMO_LOCAL_MAX) {
        app->spree_count[event->target] = 0U;
        app->multikill_count[event->target] = 0U;
    }
    if (source_local && event->source != event->target &&
        event->source < DEMO_LOCAL_MAX) {
        vox_u16 source = event->source;
        if (app->last_kill_tick[source] != 0U &&
            event->tick - app->last_kill_tick[source] <=
            DEMO_MULTIKILL_WINDOW) {
            ++app->multikill_count[source];
        } else {
            app->multikill_count[source] = 1U;
        }
        app->last_kill_tick[source] = event->tick;
        ++app->spree_count[source];
        if (target_local) {
            sprintf(line, "%s KILLED %s",
                    demo_player_name(app, event->source),
                    demo_player_name(app, event->target));
        } else {
            sprintf(line, "YOU KILLED %s",
                    demo_player_name(app, event->target));
        }
        demo_set_banner(app, line, 0);
        if (app->multikill_count[source] == 2U) {
            demo_set_banner(app, "DOUBLE KILL!", 1);
        } else if (app->multikill_count[source] == 3U) {
            demo_set_banner(app, "TRIPLE KILL!", 1);
        } else if (app->multikill_count[source] >= 4U) {
            demo_set_banner(app, "MULTI KILL!", 1);
        }
        if (app->spree_count[source] == 5U) {
            demo_set_banner(app, "KILLING SPREE!", 1);
        }
        demo_audio_emit(app, VOX_AUDIO_PRESET_KILL_CONFIRM,
                        event->variant, VOX_AUDIO_PAN_CENTER);
    } else if (target_local) {
        if (event->source < demo_match.rules.player_count &&
            event->source != event->target) {
            sprintf(line, "KILLED BY %s",
                    demo_player_name(app, event->source));
        } else {
            strcpy(line, "THE MINE GOT YOU");
        }
        demo_set_banner(app, line, 0);
    }
    if (target_local) {
        app->death_camera_hold = 72U;
        app->death_camera_player = event->target;
    }
}

static void demo_process_events(demo_app *app)
{
    vox_u16 ordinal;
    for (ordinal = 0U; ordinal < demo_match.event_count; ++ordinal) {
        const vox_digs_event *event = vox_digs_event_get(&demo_match, ordinal);
        vox_i16 pan;
        if (event == 0 || event->sequence <= app->last_event_sequence) {
            continue;
        }
        app->last_event_sequence = event->sequence;
        pan = (vox_i16)((long)(event->position_x_q16 / 65536L) * 65534L /
                        (long)(VOX_WORLD_WIDTH - 1U) - 32767L);
        if (event->type == VOX_DIGS_EVENT_DAMAGE) {
            demo_add_damage_popup(app, event);
            if (event->target < VOX_DIGS_MAX_SLOTS) {
                app->bot_health_ttl[event->target] = 180U;
            }
            demo_audio_emit(app, VOX_AUDIO_PRESET_HIT,
                            event->variant, pan);
            if (event->magnitude > 0U &&
                event->target < VOX_DIGS_MAX_SLOTS) {
                app->miner_hit_ttl[event->target] = DEMO_MINER_HIT_TICKS;
            }
            if (event->magnitude > 0U &&
                event->source < (vox_u16)app->local_players &&
                event->source != event->target) {
                app->hit_markers[event->source].world_x_q16 =
                    event->position_x_q16;
                app->hit_markers[event->source].world_y_q16 =
                    event->position_y_q16;
                app->hit_markers[event->source].ttl =
                    DEMO_HIT_MARKER_TICKS;
                app->crosshair_pulse_ttl[event->source] =
                    DEMO_HIT_MARKER_TICKS;
                demo_audio_emit(app, VOX_AUDIO_PRESET_HIT_CONFIRM,
                                event->variant, VOX_AUDIO_PAN_CENTER);
            }
            if (event->target < (vox_u16)app->local_players) {
                app->camera_trauma += (double)event->magnitude / 180.0;
                app->flash_kind = 1;
                app->flash_strength = app->options.flash_mode == 2 ? 0.48 :
                                      (app->options.flash_mode == 1 ? 0.20 : 0.0);
                demo_haptic_impulse(app, event->target, DEMO_HAPTIC_HIT,
                                    event->magnitude, 32767U);
            }
        } else if (event->type == VOX_DIGS_EVENT_EXPLOSION) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_EXPLOSION,
                            event->variant, pan);
            if (demo_event_is_local(app, event)) {
                app->camera_trauma += 0.55;
                app->flash_kind = 2;
                app->flash_strength = app->options.flash_mode == 2 ? 0.62 :
                                      (app->options.flash_mode == 1 ? 0.24 : 0.0);
            }
            demo_haptic_world(app, event, DEMO_HAPTIC_EXPLOSION);
        } else if (event->type == VOX_DIGS_EVENT_KILL) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_KILL,
                            event->variant, pan);
            if (demo_event_is_local(app, event)) {
                app->camera_trauma += 0.9;
                app->flash_kind = 1;
                app->flash_strength = app->options.flash_mode == 2 ? 0.75 :
                                      (app->options.flash_mode == 1 ? 0.30 : 0.0);
            }
            demo_haptic_world(app, event, DEMO_HAPTIC_KILL);
            if (event->source < demo_match.rules.player_count &&
                event->source != event->target) {
                app->victory_bark_ttl[event->source] = 180U;
            }
            if (event->target < DEMO_LOCAL_MAX) {
                app->rope_latched[event->target] = 0;
                app->rope_down[event->target] = 0;
            }
            demo_register_kill(app, event);
        } else if (event->type == VOX_DIGS_EVENT_SPAWN) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_SPAWN,
                            event->variant, pan);
        } else if (event->type == VOX_DIGS_EVENT_ROPE_ATTACH) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_ROPE_ATTACH,
                            event->variant, pan);
            if (event->source < (vox_u16)app->local_players) {
                demo_haptic_impulse(app, event->source, DEMO_HAPTIC_ROPE,
                                    event->magnitude, 32767U);
            }
        } else if (event->type == VOX_DIGS_EVENT_ROPE_DETACH ||
                   event->type == VOX_DIGS_EVENT_ROPE_BREAK) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_ROPE_BREAK,
                            event->variant, pan);
            if (event->source < DEMO_LOCAL_MAX) {
                app->rope_latched[event->source] = 0;
            }
        } else if (event->type == VOX_DIGS_EVENT_ROPE_CAST ||
                   event->type == VOX_DIGS_EVENT_ROPE_HIT) {
            if (event->source < (vox_u16)app->local_players) {
                demo_haptic_impulse(app, event->source, DEMO_HAPTIC_ROPE,
                                    event->magnitude, 32767U);
            }
        } else if (event->type == VOX_DIGS_EVENT_SHIELD_BLOCK) {
            demo_haptic_world(app, event, DEMO_HAPTIC_HIT);
        } else if (event->type == VOX_DIGS_EVENT_RAIL_CHARGE) {
            if (event->source < (vox_u16)app->local_players) {
                demo_haptic_impulse(app, event->source, DEMO_HAPTIC_RAIL,
                                    event->magnitude, 32767U);
            }
        } else if (event->type == VOX_DIGS_EVENT_RAIL_TRACE) {
            if (event->source < VOX_DIGS_MAX_SLOTS) {
                demo_rail_trace *trace = &app->rail_traces[event->source];
                trace->start_x_q16 = app->rail_origin_x_q16[event->source];
                trace->start_y_q16 = app->rail_origin_y_q16[event->source];
                trace->end_x_q16 = event->position_x_q16;
                trace->end_y_q16 = event->position_y_q16;
                trace->until_tick = demo_match.tick + 4U;
                trace->active = 1U;
                trace->source = event->source;
            }
            demo_haptic_world(app, event, DEMO_HAPTIC_RAIL);
        } else if (event->type == VOX_DIGS_EVENT_CRUSH) {
            demo_haptic_world(app, event, DEMO_HAPTIC_CRUMBLE);
        } else if (event->type == VOX_DIGS_EVENT_AI_BARK) {
            digs_chronicle_log(&demo_chronicle,
                vox_digs_memory_identity(&demo_match, event->source),
                vox_digs_memory_identity(&demo_match, event->target),
                event->variant);
            demo_speak_line(app, (int)event->source,
                            event->target < demo_match.rules.player_count ?
                            (int)event->target : -1, event->variant,
                            vox_digs_player_is_bot(&demo_match,
                                                   event->source));
        } else if (event->type == VOX_DIGS_EVENT_MATCH_END) {
            demo_chronicle_commit();
            if (demo_match.result_draw) {
                demo_set_banner(app, "DRAW!", 1);
            } else if (demo_match.winner_player <
                       (vox_u16)app->local_players) {
                demo_set_banner(app, "SHIFT WON!", 1);
            } else {
                demo_set_banner(app, "SHIFT LOST!", 1);
            }
        } else if (event->type == VOX_DIGS_EVENT_WEAPON_FIRE) {
            demo_audio_weapon_fire(app, event, pan);
            if (event->source < VOX_DIGS_MAX_SLOTS) {
                app->rail_origin_x_q16[event->source] =
                    event->position_x_q16;
                app->rail_origin_y_q16[event->source] =
                    event->position_y_q16;
            }
            if (event->source < (vox_u16)app->local_players) {
                demo_haptic_impulse(app, event->source,
                    event->weapon == VOX_DIGS_TOOL_RAIL_GUN ?
                    DEMO_HAPTIC_RAIL : DEMO_HAPTIC_FIRE,
                    event->magnitude, 32767U);
            }
        }
        if (app->camera_trauma > 1.0) app->camera_trauma = 1.0;
    }
}

static void demo_update_ambience(demo_app *app)
{
    int row;
    int column;
    vox_u32 depth;
    vox_u32 water = 0U;
    vox_u32 lava = 0U;
    long water_x = 0L;
    long lava_x = 0L;
    double span_x;
    double span_y;
    if (app->audio_device == 0U || (demo_match.tick % 12U) != 0U) return;
    span_x = (double)VOX_WORLD_WIDTH / app->camera_scale;
    span_y = (double)VOX_WORLD_HEIGHT / app->camera_scale;
    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 8; ++column) {
            long x = (long)(app->camera_world_x - span_x * 0.5 +
                     span_x * ((double)column + 0.5) / 8.0);
            long y = (long)(app->camera_world_y - span_y * 0.5 +
                     span_y * ((double)row + 0.5) / 4.0);
            if (x < 0L) x = 0L;
            if (y < 0L) y = 0L;
            if (x >= (long)VOX_WORLD_WIDTH) x = VOX_WORLD_WIDTH - 1L;
            if (y >= (long)VOX_WORLD_HEIGHT) y = VOX_WORLD_HEIGHT - 1L;
            for (depth = 0U; depth < VOX_WORLD_DEPTH; ++depth) {
                const vox_cell *cell = vox_world_cell(&demo_match.world,
                    (vox_u32)x, (vox_u32)y, depth);
                if (cell != 0 && cell->material == VOX_MAT_WATER) {
                    ++water;
                    water_x += x;
                } else if (cell != 0 && cell->material == VOX_MAT_LAVA) {
                    ++lava;
                    lava_x += x;
                }
            }
        }
    }
    demo_audio_lock(app);
    (void)vox_audio_set_ambience(&app->audio, VOX_AUDIO_AMBIENCE_WIND,
                                  1800U, VOX_AUDIO_PAN_CENTER);
    (void)vox_audio_set_ambience(&app->audio, VOX_AUDIO_AMBIENCE_WATER,
        (vox_u16)(water > 20U ? 6000U : water * 300U),
        water == 0U ? VOX_AUDIO_PAN_CENTER :
        (vox_i16)((water_x / (long)water) * 65534L /
                  (long)(VOX_WORLD_WIDTH - 1U) - 32767L));
    (void)vox_audio_set_ambience(&app->audio, VOX_AUDIO_AMBIENCE_LAVA,
        (vox_u16)(lava > 20U ? 8000U : lava * 400U),
        lava == 0U ? VOX_AUDIO_PAN_CENTER :
        (vox_i16)((lava_x / (long)lava) * 65534L /
                  (long)(VOX_WORLD_WIDTH - 1U) - 32767L));
    demo_audio_unlock(app);
}

static void demo_tick_presentation(demo_app *app)
{
    int slot;
    for (slot = 0; slot < (int)DEMO_DAMAGE_POPUP_MAX; ++slot) {
        if (app->damage_popups[slot].active) {
            if (app->damage_popups[slot].ttl > 0U) {
                --app->damage_popups[slot].ttl;
                app->damage_popups[slot].world_y_q16 -= 9000L;
            } else {
                app->damage_popups[slot].active = 0U;
            }
        }
    }
    for (slot = 0; slot < (int)VOX_DIGS_MAX_SLOTS; ++slot) {
        if (app->bot_health_ttl[slot] > 0U) {
            --app->bot_health_ttl[slot];
        }
        if (app->miner_hit_ttl[slot] > 0U) {
            --app->miner_hit_ttl[slot];
        }
        if (app->victory_bark_ttl[slot] > 0U) {
            --app->victory_bark_ttl[slot];
        }
        if (app->bubbles[slot].ttl > 0U) {
            --app->bubbles[slot].ttl;
        }
    }
    for (slot = 0; slot < (int)DEMO_LOCAL_MAX; ++slot) {
        if (app->hit_markers[slot].ttl > 0U) {
            --app->hit_markers[slot].ttl;
        }
        if (app->crosshair_pulse_ttl[slot] > 0U) {
            --app->crosshair_pulse_ttl[slot];
        }
        if (app->multikill_count[slot] > 0U &&
            demo_match.tick - app->last_kill_tick[slot] >
            DEMO_MULTIKILL_WINDOW) {
            app->multikill_count[slot] = 0U;
        }
    }
    for (slot = 0; slot < DEMO_KILLFEED_MAX; ++slot) {
        if (app->killfeed[slot].ttl > 0U) --app->killfeed[slot].ttl;
    }
    for (slot = 0; slot < 3; ++slot) {
        if (app->banners[slot].ttl > 0U) --app->banners[slot].ttl;
    }
    if (app->death_camera_hold > 0U) --app->death_camera_hold;
    demo_update_ambience(app);
}

static void demo_tick(demo_app *app)
{
    if (app->screen != DEMO_PLAY) {
        return;
    }
    {
        vox_u16 player;
        for (player = 0U; player < VOX_DIGS_MAX_SLOTS; ++player) {
            app->previous_player_x[player] =
                demo_match.players[player].position_x.value_q16;
            app->previous_player_y[player] =
                demo_match.players[player].position_y.value_q16;
        }
    }
    demo_submit_human_input(app);
    if (vox_digs_match_step(&demo_match) != VOX_OK) {
        app->screen = DEMO_RESULTS;
        return;
    }
    demo_process_events(app);
    demo_haptics_tick(app);
    demo_tick_presentation(app);
    if (demo_match.phase == VOX_DIGS_RESULTS) {
        app->screen = DEMO_RESULTS;
    }
}

static void demo_apply_fullscreen(demo_app *app)
{
    Uint32 flags = app->options.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0U;
    (void)SDL_SetWindowFullscreen(app->window, flags);
}

static void demo_set_mouse_logical(demo_app *app, int logical_x,
                                   int logical_y)
{
    if (logical_x < 0) {
        logical_x = 0;
    } else if (logical_x >= (int)DEMO_WIDTH) {
        logical_x = (int)DEMO_WIDTH - 1;
    }
    if (logical_y < 0) {
        logical_y = 0;
    } else if (logical_y >= (int)DEMO_HEIGHT) {
        logical_y = (int)DEMO_HEIGHT - 1;
    }
    app->mouse_x = logical_x;
    app->mouse_y = logical_y;
}

static int demo_sync_hardware_mouse(demo_app *app)
{
    int window_x;
    int window_y;
    int window_width;
    int window_height;
    int output_width;
    int output_height;
    int view_width;
    int view_height;
    int view_x;
    int view_y;
    int logical_x;
    int logical_y;
    double scale_x;
    double scale_y;
    double scale;
    double output_x;
    double output_y;
    (void)SDL_GetMouseState(&window_x, &window_y);
    SDL_GetWindowSize(app->window, &window_width, &window_height);
    if (window_width <= 0 || window_height <= 0) {
        app->mouse_inside = 0;
        return 0;
    }
    if (SDL_GetRendererOutputSize(app->renderer, &output_width,
                                  &output_height) != 0 ||
        output_width <= 0 || output_height <= 0) {
        output_width = window_width;
        output_height = window_height;
    }
    output_x = (double)window_x * (double)output_width /
               (double)window_width;
    output_y = (double)window_y * (double)output_height /
               (double)window_height;
    scale_x = (double)output_width / (double)DEMO_WIDTH;
    scale_y = (double)output_height / (double)DEMO_HEIGHT;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale >= 1.0) {
        scale = (double)(int)scale;
    }
    view_width = (int)((double)DEMO_WIDTH * scale);
    view_height = (int)((double)DEMO_HEIGHT * scale);
    if (view_width <= 0 || view_height <= 0) {
        app->mouse_inside = 0;
        return 0;
    }
    view_x = (output_width - view_width) / 2;
    view_y = (output_height - view_height) / 2;
    app->mouse_inside = output_x >= (double)view_x &&
                        output_y >= (double)view_y &&
                        output_x < (double)(view_x + view_width) &&
                        output_y < (double)(view_y + view_height);
    logical_x = (int)((output_x - (double)view_x) / scale);
    logical_y = (int)((output_y - (double)view_y) / scale);
    demo_set_mouse_logical(app, logical_x, logical_y);
    return app->mouse_inside;
}

static void demo_update_cursor_visibility(demo_app *app)
{
    int visible = app->screen == DEMO_PLAY ? 0 : 1;
    if (visible != app->cursor_visible) {
        (void)SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
        app->cursor_visible = visible;
    }
}

static void demo_handle_title_key(demo_app *app, SDL_Keycode key)
{
    if (key == SDLK_UP) {
        app->selection = (app->selection + 6) % 7;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 7;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, DEMO_SOUND_SELECT);
        if (app->selection == 0) {
            app->screen = DEMO_SETUP;
            app->selection = 0;
        } else if (app->selection == 1) {
            app->screen = DEMO_INBOX;
            app->selection = 0;
            app->inbox_open = -1;
        } else if (app->selection == 2) {
            app->screen = DEMO_LOG;
            app->selection = 0;
            app->log_scroll = 0;
        } else if (app->selection == 3) {
            (void)demo_start_match(app, 1);
        } else if (app->selection == 4) {
            app->screen = DEMO_CONTROLS;
            app->selection = 0;
        } else if (app->selection == 5) {
            app->screen = DEMO_OPTIONS;
            app->selection = 0;
        } else {
            app->running = 0;
        }
    }
}

static void demo_handle_setup_key(demo_app *app, SDL_Keycode key)
{
    int direction = key == SDLK_LEFT ? -1 : (key == SDLK_RIGHT ? 1 : 0);
    if (key == SDLK_ESCAPE) {
        app->screen = DEMO_TITLE;
        app->selection = 0;
    } else if (key == SDLK_UP) {
        app->selection = (app->selection + 7) % 8;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 8;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (direction != 0) {
        demo_audio_play(app, DEMO_SOUND_MOVE);
        if (app->selection == 0) {
            app->local_players = app->local_players == 1 ? 2 : 1;
            if (app->local_players + app->bots > 4) {
                app->bots = 4 - app->local_players;
            }
            demo_refresh_controller_claims(app);
            demo_refresh_roster(app);
        } else if (app->selection == 1) {
            /* Four slots, so zero through three bots. */
            app->bots = (app->bots + direction + (int)VOX_DIGS_MAX_BOTS + 1) %
                        ((int)VOX_DIGS_MAX_BOTS + 1);
            if (app->local_players + app->bots > (int)VOX_DIGS_MAX_SLOTS) {
                app->bots = (int)VOX_DIGS_MAX_SLOTS - app->local_players;
            }
            demo_refresh_roster(app);
        } else if (app->selection == 2) {
            app->map_style = (app->map_style + direction + 3) % 3;
        } else if (app->selection == 3) {
            app->seed += direction > 0 ? 1U : (vox_u32)-1;
        } else if (app->selection == 4) {
            app->arsenal = (app->arsenal + direction +
                            DEMO_ARSENAL_COUNT) % DEMO_ARSENAL_COUNT;
        }
    } else if (key == SDLK_r && app->selection == 3) {
        app->seed = app->seed * 1664525U + 1013904223U;
        demo_audio_play(app, DEMO_SOUND_SELECT);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, DEMO_SOUND_SELECT);
        if (app->selection == 5) {
            app->screen = DEMO_CUSTOMIZE;
            app->selection = 0;
        } else if (app->selection == 6) {
            (void)demo_start_match(app, 0);
        } else if (app->selection == 7) {
            app->screen = DEMO_TITLE;
            app->selection = 0;
        }
    }
}

static void demo_name_append(demo_app *app, char character)
{
    char *name;
    size_t length;
    if (app->edit_name_slot < 0 ||
        app->edit_name_slot >= (int)VOX_DIGS_MAX_SLOTS) return;
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (!demo_name_character_allowed(character)) return;
    name = app->player_names[app->edit_name_slot];
    length = strlen(name);
    if (length >= DEMO_NAME_CHARACTERS) return;
    name[length] = character;
    name[length + 1U] = '\0';
}

static void demo_name_delete(demo_app *app)
{
    char *name;
    size_t length;
    if (app->edit_name_slot < 0 ||
        app->edit_name_slot >= (int)VOX_DIGS_MAX_SLOTS) return;
    name = app->player_names[app->edit_name_slot];
    length = strlen(name);
    if (length > 0U) name[length - 1U] = '\0';
}

static void demo_finish_name_editor(demo_app *app, int accept)
{
    int slot = app->edit_name_slot;
    if (slot < 0 || slot >= (int)VOX_DIGS_MAX_SLOTS) return;
    if (accept) {
        static const char *human_fallbacks[DEMO_LOCAL_MAX] = {
            "MINER 1", "MINER 2"
        };
        static const char *bot_fallbacks[VOX_DIGS_MAX_BOTS] = {
            "RIVET", "CINDER", "FLAMEY"
        };
        const char *fallback = "MINER";
        if (slot < app->local_players && slot < (int)DEMO_LOCAL_MAX) {
            fallback = human_fallbacks[slot];
        } else if (slot >= app->local_players &&
                   slot - app->local_players <
                   (int)VOX_DIGS_MAX_BOTS) {
            fallback = bot_fallbacks[slot - app->local_players];
        }
        demo_sanitize_name(app->player_names[slot], fallback);
        if (slot < app->local_players && slot < (int)DEMO_LOCAL_MAX) {
            strcpy(app->human_names[slot], app->player_names[slot]);
        } else if (slot >= app->local_players &&
                   slot - app->local_players < (int)VOX_DIGS_MAX_BOTS) {
            strcpy(app->bot_names[slot - app->local_players],
                   app->player_names[slot]);
        }
        (void)demo_save_input_settings(app);
    } else {
        strcpy(app->player_names[slot], app->edit_name_backup);
    }
    SDL_StopTextInput();
    app->edit_name_slot = -1;
    app->screen = DEMO_CUSTOMIZE;
    app->selection = slot;
}

static void demo_open_name_editor(demo_app *app, int slot)
{
    if (slot < 0 || slot >= (int)VOX_DIGS_MAX_SLOTS) return;
    app->edit_name_slot = slot;
    strcpy(app->edit_name_backup, app->player_names[slot]);
    app->name_grid_selection = 0;
    app->screen = DEMO_NAME_EDITOR;
    SDL_StartTextInput();
}

static void demo_handle_customize_key(demo_app *app, SDL_Keycode key)
{
    int direction = key == SDLK_LEFT ? -1 : (key == SDLK_RIGHT ? 1 : 0);
    if (key == SDLK_ESCAPE) {
        app->screen = DEMO_SETUP;
        app->selection = 7;
    } else if (key == SDLK_UP) {
        app->selection = (app->selection + 10) % 11;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 11;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (direction != 0) {
        if (app->selection == 4) {
            app->time_limit_index =
                (app->time_limit_index + direction + 6) % 6;
        } else if (app->selection == 5) {
            app->lava_index = (app->lava_index + direction + 5) % 5;
        } else if (app->selection == 6) {
            app->score_limit_index =
                (app->score_limit_index + direction + 4) % 4;
        } else if (app->selection == 7) {
            app->respawn_mode = 1 - app->respawn_mode;
        } else if (app->selection == 8) {
            app->respawn_delay_index =
                (app->respawn_delay_index + direction + 5) % 5;
        }
        (void)demo_save_input_settings(app);
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, DEMO_SOUND_SELECT);
        if (app->selection >= 0 &&
            app->selection < app->local_players + app->bots) {
            demo_open_name_editor(app, app->selection);
        } else if (app->selection == 9) {
            demo_match_settings_defaults(app);
            (void)demo_save_input_settings(app);
        } else if (app->selection == 10) {
            app->screen = DEMO_SETUP;
            app->selection = 7;
        }
    }
}

static void demo_handle_name_editor_key(demo_app *app, SDL_Keycode key)
{
    if (key == SDLK_ESCAPE) {
        demo_finish_name_editor(app, 0);
    } else if (key == SDLK_BACKSPACE) {
        demo_name_delete(app);
    } else if (key == SDLK_LEFT) {
        app->name_grid_selection =
            (app->name_grid_selection + DEMO_NAME_GRID_ITEMS - 1) %
            DEMO_NAME_GRID_ITEMS;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_RIGHT) {
        app->name_grid_selection =
            (app->name_grid_selection + 1) % DEMO_NAME_GRID_ITEMS;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_UP) {
        app->name_grid_selection =
            (app->name_grid_selection + DEMO_NAME_GRID_ITEMS -
             DEMO_NAME_GRID_COLUMNS) % DEMO_NAME_GRID_ITEMS;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->name_grid_selection =
            (app->name_grid_selection + DEMO_NAME_GRID_COLUMNS) %
            DEMO_NAME_GRID_ITEMS;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        int item = app->name_grid_selection;
        if (item < 38) demo_name_append(app, demo_name_grid[item]);
        else if (item == 38) demo_name_append(app, ' ');
        else if (item == 39) demo_name_delete(app);
        else if (item == 40) {
            app->player_names[app->edit_name_slot][0] = '\0';
        } else demo_finish_name_editor(app, 1);
        demo_audio_play(app, DEMO_SOUND_SELECT);
    } else if (key == SDLK_SPACE) {
        demo_name_append(app, ' ');
    }
}

static void demo_handle_options_key(demo_app *app, SDL_Keycode key)
{
    int direction = key == SDLK_LEFT ? -1 : (key == SDLK_RIGHT ? 1 : 0);
    int change = direction == 0 ? 1 : direction;
    if (key == SDLK_F8) {
        demo_qualify_caps(app, 1);
        demo_audio_play(app, DEMO_SOUND_SELECT);
    } else if (key == SDLK_ESCAPE) {
        app->screen = DEMO_TITLE;
        app->selection = 0;
    } else if (key == SDLK_UP) {
        app->selection = (app->selection + 16) % 17;
        demo_chronicle_reset_armed = 0;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 17;
        demo_chronicle_reset_armed = 0;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (direction != 0 || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, direction == 0 ? DEMO_SOUND_SELECT :
                        DEMO_SOUND_MOVE);
        if (app->selection == 0) {
            app->options.frame_cap_index = demo_next_supported_cap(
                app, app->options.frame_cap_index, change);
        } else if (app->selection == 1) {
            app->options.gi_quality =
                (app->options.gi_quality + change + 3) % 3;
            app->scene_valid = 0;
            demo_qualify_caps(app, 0);
        } else if (app->selection == 2) {
            app->options.master_volume =
                (app->options.master_volume + change + 11) % 11;
            if (app->audio_device != 0U) {
                demo_audio_lock(app);
                (void)vox_audio_set_master_gain(&app->audio,
                    (vox_u16)((vox_u32)app->options.master_volume *
                              VOX_AUDIO_GAIN_MAX / 10U));
                demo_audio_unlock(app);
            }
        } else if (app->selection == 3) {
            app->options.laptop_mode = !app->options.laptop_mode;
            app->scene_valid = 0;
            demo_qualify_caps(app, 0);
        } else if (app->selection == 4) {
            app->options.haptic_level =
                (app->options.haptic_level + change +
                 DEMO_HAPTIC_LEVEL_COUNT) % DEMO_HAPTIC_LEVEL_COUNT;
        } else if (app->selection == 5) {
            app->options.fx_profile =
                (app->options.fx_profile + change + 3) % 3;
        } else if (app->selection == 6) {
            app->options.flash_mode =
                (app->options.flash_mode + change + 3) % 3;
        } else if (app->selection == 7) {
            app->options.gore_level =
                (app->options.gore_level + change + 3) % 3;
        } else if (app->selection == 8) {
            app->options.camera_shake = !app->options.camera_shake;
        } else if (app->selection == 9) {
            app->options.damage_numbers = !app->options.damage_numbers;
        } else if (app->selection == 10) {
            app->options.damage_number_size =
                !app->options.damage_number_size;
        } else if (app->selection == 11) {
            app->options.damage_number_color =
                !app->options.damage_number_color;
        } else if (app->selection == 12) {
            app->options.fullscreen = !app->options.fullscreen;
            demo_apply_fullscreen(app);
        } else if (app->selection == 13) {
            app->options.dummy_mode = !app->options.dummy_mode;
        } else if (app->selection == 14 && direction == 0) {
            /*
             * Erase everything the miners remember.
             *
             * The lead's notes ask for no reset in the shipped game, so that
             * the history is unrepeatable and each player's is their own.
             * This is here because it was asked for directly afterwards; the
             * two-step is the compromise, because the thing being destroyed
             * cannot be recovered and a single mis-keyed RETURN is not
             * enough intent for that.
             */
            if (!demo_chronicle_reset_armed) {
                demo_chronicle_reset_armed = 1;
            } else {
                char path[1032];
                demo_chronicle_reset_armed = 0;
                digs_chronicle_reset(&demo_chronicle);
                demo_chronicle_ready = 1;
                if (demo_chronicle_path(path, (int)sizeof(path))) {
                    (void)digs_chronicle_save(&demo_chronicle, path);
                }
                demo_set_banner(app, "THEY HAVE FORGOTTEN YOU", 0);
            }
        } else if (app->selection == 15 && direction == 0) {
            app->screen = DEMO_INPUT_OPTIONS;
            app->selection = 0;
        } else if (app->selection == 16 && direction == 0) {
            app->screen = DEMO_TITLE;
            app->selection = 0;
        }
        (void)demo_save_input_settings(app);
    }
}

static void demo_handle_input_options_key(demo_app *app, SDL_Keycode key)
{
    int direction = key == SDLK_LEFT ? -1 : (key == SDLK_RIGHT ? 1 : 0);
    int change = direction == 0 ? 1 : direction;
    int player = app->selection >= 5 && app->selection <= 9 ? 1 : 0;
    int field = app->selection - player * 5;
    if (key == SDLK_ESCAPE) {
        app->screen = DEMO_OPTIONS;
        app->selection = 14;
    } else if (key == SDLK_UP) {
        app->selection = (app->selection + 12) % 13;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 13;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (direction != 0 || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, direction == 0 ? DEMO_SOUND_SELECT :
                        DEMO_SOUND_MOVE);
        if (app->selection <= 9) {
            demo_player_input *input = &app->player_input[player];
            if (field == 0) {
                input->preference = (input->preference + change + 3) % 3;
                if (input->preference == DEMO_INPUT_KEYBOARD) {
                    input->active_source = DEMO_SOURCE_KEYBOARD;
                } else if (input->preference == DEMO_INPUT_CONTROLLER) {
                    input->active_source = DEMO_SOURCE_CONTROLLER;
                }
                demo_reset_input_edges(app, player);
                demo_refresh_controller_claims(app);
            } else if (field == 1) {
                input->sensitivity = (input->sensitivity + change + 3) % 3;
            } else if (field == 2) {
                input->deadzone = (input->deadzone + change + 4) % 4;
            } else if (field == 3) {
                input->aim_slowdown =
                    (input->aim_slowdown + change + 3) % 3;
            } else if (field == 4) {
                input->rope_mode =
                    (input->rope_mode + change + 2) % 2;
                app->rope_latched[player] = 0;
                app->rope_down[player] = 0;
            }
            (void)demo_save_input_settings(app);
        } else if (app->selection == 10 && direction == 0) {
            demo_begin_controller_calibration(app);
        } else if (app->selection == 11 && direction == 0) {
            demo_input_defaults(app);
            demo_refresh_controller_claims(app);
            (void)demo_save_input_settings(app);
        } else if (app->selection == 12 && direction == 0) {
            app->screen = DEMO_OPTIONS;
            app->selection = 14;
        }
    }
}

static void demo_handle_controls_key(demo_app *app, SDL_Keycode key,
                                     SDL_Scancode scancode)
{
    if (app->binding_capture) {
        if (key == SDLK_ESCAPE) {
            app->binding_capture = 0;
        } else if (app->binding_player < 2) {
            demo_assign_keyboard_binding(app, app->binding_player,
                                         app->binding_capture - 1,
                                         scancode);
            app->binding_capture = 0;
            (void)demo_save_input_settings(app);
            demo_audio_play(app, DEMO_SOUND_SELECT);
        }
        return;
    }
    if (key == SDLK_ESCAPE) {
        app->screen = DEMO_TITLE;
        app->selection = 0;
    } else if (key == SDLK_UP) {
        app->selection = (app->selection + 11) % 12;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 12;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (app->selection == 0 &&
               (key == SDLK_LEFT || key == SDLK_RIGHT)) {
        int direction = key == SDLK_LEFT ? -1 : 1;
        app->binding_player = (app->binding_player + direction + 3) % 3;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (app->selection >= 1 && app->selection <= 9) {
            int action = app->selection - 1;
            if (app->binding_player < 2 || action >= 2) {
                app->binding_capture = action + 1;
                demo_audio_play(app, DEMO_SOUND_SELECT);
            }
        } else if (app->selection == 10) {
            demo_bindings_default(app);
            (void)demo_save_input_settings(app);
            demo_audio_play(app, DEMO_SOUND_SELECT);
        } else if (app->selection == 11) {
            app->screen = DEMO_TITLE;
            app->selection = 0;
        }
    }
}

static void demo_handle_key(demo_app *app, SDL_Keycode key,
                            SDL_Scancode scancode)
{
    int weapon_key = demo_key_weapon(key);
    if (app->screen == DEMO_CONTROLS && app->binding_capture) {
        demo_handle_controls_key(app, key, scancode);
        return;
    }
    if (key == SDLK_F1) {
        app->options.debug = !app->options.debug;
    } else if (key == SDLK_F11) {
        app->options.fullscreen = !app->options.fullscreen;
        demo_apply_fullscreen(app);
    } else if (app->screen == DEMO_TITLE) {
        demo_handle_title_key(app, key);
    } else if (app->screen == DEMO_SETUP) {
        demo_handle_setup_key(app, key);
    } else if (app->screen == DEMO_CUSTOMIZE) {
        demo_handle_customize_key(app, key);
    } else if (app->screen == DEMO_NAME_EDITOR) {
        demo_handle_name_editor_key(app, key);
    } else if (app->screen == DEMO_OPTIONS) {
        demo_handle_options_key(app, key);
    } else if (app->screen == DEMO_INPUT_OPTIONS) {
        demo_handle_input_options_key(app, key);
    } else if (app->screen == DEMO_CONTROLS) {
        demo_handle_controls_key(app, key, scancode);
    } else if (app->screen == DEMO_INBOX) {
        demo_handle_inbox_key(app, key);
    } else if (app->screen == DEMO_LOG) {
        demo_handle_log_key(app, key);
    } else if (app->screen == DEMO_PLAY && key == SDLK_ESCAPE) {
        app->screen = DEMO_PAUSE;
        app->selection = 0;
        demo_audio_play(app, DEMO_SOUND_PAUSE);
    } else if (app->screen == DEMO_PAUSE) {
        if (key == SDLK_UP || key == SDLK_DOWN) {
            app->selection = 1 - app->selection;
            demo_audio_play(app, DEMO_SOUND_MOVE);
        } else if (key == SDLK_ESCAPE ||
                   ((key == SDLK_RETURN || key == SDLK_KP_ENTER) &&
                    app->selection == 0)) {
            app->screen = DEMO_PLAY;
            app->controller_disconnected = 0;
            demo_audio_play(app, DEMO_SOUND_SELECT);
        } else if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) &&
                   app->selection == 1) {
            app->screen = DEMO_TITLE;
            app->selection = 0;
            demo_audio_play(app, DEMO_SOUND_SELECT);
        }
    } else if (app->screen == DEMO_RESULTS &&
               (key == SDLK_RETURN || key == SDLK_KP_ENTER ||
                key == SDLK_ESCAPE)) {
        strcpy(app->bot_names[0], "RIVET");
        strcpy(app->bot_names[1], "CINDER");
        strcpy(app->bot_names[2], "FLAMEY");
        demo_refresh_roster(app);
        app->screen = DEMO_TITLE;
        app->selection = 0;
        demo_audio_play(app, DEMO_SOUND_SELECT);
    } else if (app->screen == DEMO_PLAY && weapon_key >= 0) {
        if (demo_weapon_is_allowed(demo_match.rules.weapon_mask,
                                   weapon_key)) {
            app->selected_tool[0] = weapon_key;
            demo_audio_play(app, DEMO_SOUND_MOVE);
        }
    } else if (app->screen == DEMO_PLAY && key == SDLK_r) {
        (void)demo_start_match(app, app->foundry);
    }
}

static int demo_keyboard_player_for_scancode(demo_app *app,
                                             SDL_Scancode scancode)
{
    int player;
    int action;
    for (player = 0; player < app->local_players &&
         player < (int)DEMO_LOCAL_MAX; ++player) {
        for (action = 0; action < 9; ++action) {
            SDL_Scancode *binding = demo_keyboard_binding(app, player,
                                                          action);
            if (binding != 0 && *binding == scancode) return player;
        }
    }
    if (scancode == SDL_SCANCODE_W || scancode == SDL_SCANCODE_S ||
        (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_0)) {
        return 0;
    }
    if (app->local_players > 1 &&
        (scancode == SDL_SCANCODE_I || scancode == SDL_SCANCODE_J ||
         scancode == SDL_SCANCODE_K || scancode == SDL_SCANCODE_L)) {
        return 1;
    }
    return -1;
}

static SDL_GameControllerButton demo_raw_button(int raw_button)
{
    if (raw_button == 0) return SDL_CONTROLLER_BUTTON_A;
    if (raw_button == 1) return SDL_CONTROLLER_BUTTON_B;
    if (raw_button == 2) return SDL_CONTROLLER_BUTTON_X;
    if (raw_button == 3) return SDL_CONTROLLER_BUTTON_Y;
    if (raw_button == 4) return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    if (raw_button == 5) return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    if (raw_button == 6) return SDL_CONTROLLER_BUTTON_BACK;
    if (raw_button == 7) return SDL_CONTROLLER_BUTTON_START;
    if (raw_button == 8) return SDL_CONTROLLER_BUTTON_LEFTSTICK;
    if (raw_button == 9) return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    return SDL_CONTROLLER_BUTTON_INVALID;
}

static void demo_handle_controller_button(demo_app *app,
                                          demo_controller *controller,
                                          SDL_GameControllerButton button)
{
    int player = controller == 0 ? -1 : controller->claimed_player;
    int changed = 0;
    if (button == SDL_CONTROLLER_BUTTON_INVALID) return;
    if (app->screen == DEMO_CONTROLS && app->binding_capture &&
        app->binding_player == 2) {
        demo_assign_pad_binding(app, app->binding_capture - 1, button);
        app->binding_capture = 0;
        (void)demo_save_input_settings(app);
        demo_audio_play(app, DEMO_SOUND_SELECT);
        return;
    }
    if (app->screen == DEMO_PLAY && controller != 0) {
        if (button == SDL_CONTROLLER_BUTTON_START) {
            demo_handle_key(app, SDLK_ESCAPE, SDL_SCANCODE_UNKNOWN);
            return;
        }
        if (player >= 0 && player < app->local_players &&
            player < (int)DEMO_LOCAL_MAX) {
            changed = demo_activate_source(app, player,
                                           DEMO_SOURCE_CONTROLLER, 1);
            if (app->player_input[player].active_source !=
                DEMO_SOURCE_CONTROLLER || changed) return;
            if (button == app->bindings.pad_previous) {
                demo_cycle_weapon(app, player, -1);
            } else if (button == app->bindings.pad_next) {
                demo_cycle_weapon(app, player, 1);
            }
        }
        return;
    }
    {
        SDL_Keycode key = SDLK_UNKNOWN;
        int family = controller == 0 ? DEMO_PAD_GENERIC : controller->family;
        if (button == SDL_CONTROLLER_BUTTON_DPAD_UP) key = SDLK_UP;
        else if (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) key = SDLK_DOWN;
        else if (button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) key = SDLK_LEFT;
        else if (button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) key = SDLK_RIGHT;
        else if (button == demo_pad_accept_button(family) ||
                 button == SDL_CONTROLLER_BUTTON_START) key = SDLK_RETURN;
        else if (button == demo_pad_back_button(family)) key = SDLK_ESCAPE;
        if (key != SDLK_UNKNOWN) {
            demo_handle_key(app, key, SDL_SCANCODE_UNKNOWN);
        }
    }
}

static void demo_handle_controller_navigation_axis(demo_app *app,
                                                   int axis, Sint16 value)
{
    SDL_Keycode key = SDLK_UNKNOWN;
    if (app->screen == DEMO_PLAY ||
        SDL_GetTicks() < app->controller_nav_stamp) return;
    if (axis == SDL_CONTROLLER_AXIS_LEFTX && value < -24000) key = SDLK_LEFT;
    else if (axis == SDL_CONTROLLER_AXIS_LEFTX && value > 24000) {
        key = SDLK_RIGHT;
    } else if (axis == SDL_CONTROLLER_AXIS_LEFTY && value < -24000) {
        key = SDLK_UP;
    } else if (axis == SDL_CONTROLLER_AXIS_LEFTY && value > 24000) {
        key = SDLK_DOWN;
    }
    if (key != SDLK_UNKNOWN) {
        app->controller_nav_stamp = SDL_GetTicks() + 180U;
        demo_handle_key(app, key, SDL_SCANCODE_UNKNOWN);
    }
}

static void demo_handle_event(demo_app *app, const SDL_Event *event)
{
    if (event->type == SDL_QUIT) {
        app->running = 0;
    } else if (event->type == SDL_TEXTINPUT &&
               app->screen == DEMO_NAME_EDITOR) {
        int index;
        for (index = 0; event->text.text[index] != '\0'; ++index) {
            demo_name_append(app, event->text.text[index]);
        }
    } else if (event->type == SDL_MOUSEMOTION) {
        int old_x = app->mouse_activity_x;
        int old_y = app->mouse_activity_y;
        int delta_x;
        int delta_y;
        /* The renderer filters event coordinates already, so query the raw
         * window-relative hardware pointer and transform that exactly once. */
        (void)demo_sync_hardware_mouse(app);
        delta_x = app->mouse_x - old_x;
        delta_y = app->mouse_y - old_y;
        if (delta_x < 0) delta_x = -delta_x;
        if (delta_y < 0) delta_y = -delta_y;
        if (app->screen == DEMO_PLAY && (delta_x > 2 || delta_y > 2)) {
            (void)demo_activate_source(app, 0, DEMO_SOURCE_KEYBOARD, 0);
            app->mouse_activity_x = app->mouse_x;
            app->mouse_activity_y = app->mouse_y;
            app->keyboard_aim_active = 0;
        } else if (app->screen != DEMO_PLAY &&
                   (delta_x > 1 || delta_y > 1)) {
            /*
             * Hovering moves the selection, so the mouse and the keyboard
             * are pointing at the same row rather than fighting over two.
             * Deliberately outside demo_activate_source: this is menu
             * navigation, not a claim on a player slot, and routing it
             * through the arbitration would take the pad off a player who
             * merely brushed the mouse.
             */
            const demo_menu_row *row = demo_row_at(app->mouse_x,
                                                   app->mouse_y);
            app->mouse_activity_x = app->mouse_x;
            app->mouse_activity_y = app->mouse_y;
            if (row != 0 && row->index != app->selection) {
                app->selection = row->index;
                demo_audio_play(app, DEMO_SOUND_MOVE);
            }
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN &&
               app->screen != DEMO_PLAY &&
               (event->button.button == SDL_BUTTON_LEFT ||
                event->button.button == SDL_BUTTON_RIGHT)) {
        /*
         * A click is the key press the row would have taken.  Routing it
         * through the same handler means every screen gets a mouse for free
         * and no screen can disagree with itself about what a row does.
         */
        const demo_menu_row *row;
        (void)demo_sync_hardware_mouse(app);
        row = demo_row_at(app->mouse_x, app->mouse_y);
        if (row != 0) {
            app->selection = row->index;
            if (row->kind == DEMO_ROW_VALUE) {
                demo_handle_key(app,
                                event->button.button == SDL_BUTTON_RIGHT ?
                                SDLK_LEFT : SDLK_RIGHT, SDL_SCANCODE_UNKNOWN);
            } else {
                demo_handle_key(app, SDLK_RETURN, SDL_SCANCODE_UNKNOWN);
            }
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN &&
               event->button.button == SDL_BUTTON_LEFT &&
               app->screen == DEMO_PLAY) {
        if (demo_sync_hardware_mouse(app)) {
            int changed = demo_activate_source(app, 0,
                                               DEMO_SOURCE_KEYBOARD, 1);
            app->keyboard_aim_active = 0;
            if (!changed && app->player_input[0].active_source ==
                            DEMO_SOURCE_KEYBOARD) {
                demo_mouse_world(app, &app->aim_world_x[0],
                                 &app->aim_world_y[0]);
            }
        }
    } else if (event->type == SDL_MOUSEWHEEL && app->screen == DEMO_PLAY) {
        if (event->wheel.y != 0) {
            int direction = event->wheel.y > 0 ? 1 : -1;
            int changed = demo_activate_source(app, 0,
                                               DEMO_SOURCE_KEYBOARD, 1);
            if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                direction = -direction;
            }
            if (changed || app->player_input[0].active_source !=
                           DEMO_SOURCE_KEYBOARD) {
                return;
            } else if ((SDL_GetModState() & KMOD_SHIFT) != 0) {
                demo_cycle_weapon(app, 0, direction);
                demo_audio_play(app, DEMO_SOUND_MOVE);
            } else demo_change_zoom(app, direction);
        }
    } else if (event->type == SDL_CONTROLLERDEVICEADDED ||
               event->type == SDL_JOYDEVICEADDED) {
        (void)demo_open_controller(app, event->cdevice.which);
    } else if (event->type == SDL_CONTROLLERDEVICEREMOVED ||
               event->type == SDL_JOYDEVICEREMOVED) {
        demo_close_controller(app, event->cdevice.which);
    } else if (event->type == SDL_CONTROLLERAXISMOTION) {
        demo_handle_controller_navigation_axis(app, event->caxis.axis,
                                               event->caxis.value);
    } else if (event->type == SDL_JOYAXISMOTION) {
        demo_controller *controller = demo_controller_by_instance(
            app, event->jaxis.which);
        if (controller != 0 && controller->raw_fallback &&
            event->jaxis.axis <= 1) {
            demo_handle_controller_navigation_axis(
                app, event->jaxis.axis == 0 ? SDL_CONTROLLER_AXIS_LEFTX :
                                              SDL_CONTROLLER_AXIS_LEFTY,
                event->jaxis.value);
        }
    } else if (event->type == SDL_CONTROLLERBUTTONDOWN) {
        demo_controller *controller = demo_controller_by_instance(
            app, event->cbutton.which);
        SDL_GameControllerButton button =
            (SDL_GameControllerButton)event->cbutton.button;
        demo_handle_controller_button(app, controller, button);
    } else if (event->type == SDL_JOYBUTTONDOWN) {
        demo_controller *controller = demo_controller_by_instance(
            app, event->jbutton.which);
        if (controller != 0 && controller->raw_fallback) {
            demo_handle_controller_button(app, controller,
                                          demo_raw_button(
                                              event->jbutton.button));
        }
    } else if (event->type == SDL_JOYHATMOTION) {
        demo_controller *controller = demo_controller_by_instance(
            app, event->jhat.which);
        if (controller != 0 && controller->raw_fallback) {
            SDL_GameControllerButton button = SDL_CONTROLLER_BUTTON_INVALID;
            if (event->jhat.value & SDL_HAT_UP) {
                button = SDL_CONTROLLER_BUTTON_DPAD_UP;
            } else if (event->jhat.value & SDL_HAT_DOWN) {
                button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
            } else if (event->jhat.value & SDL_HAT_LEFT) {
                button = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
            } else if (event->jhat.value & SDL_HAT_RIGHT) {
                button = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
            }
            demo_handle_controller_button(app, controller, button);
        }
    } else if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        if (app->screen == DEMO_PLAY &&
            event->key.keysym.sym != SDLK_ESCAPE) {
            int player = demo_keyboard_player_for_scancode(
                app, event->key.keysym.scancode);
            if (player >= 0) {
                int changed = demo_activate_source(
                    app, player, DEMO_SOURCE_KEYBOARD, 1);
                if (changed || app->player_input[player].active_source !=
                               DEMO_SOURCE_KEYBOARD) return;
            }
        }
        demo_handle_key(app, event->key.keysym.sym,
                        event->key.keysym.scancode);
    }
}

static int demo_write_ppm(const char *path)
{
    FILE *file = fopen(path, "wb");
    if (file == 0) {
        return 0;
    }
    if (fprintf(file, "P6\n%u %u\n255\n", (unsigned int)DEMO_WIDTH,
                (unsigned int)DEMO_HEIGHT) < 0 ||
        fwrite(demo_pixels, 1U, sizeof(demo_pixels), file) !=
            sizeof(demo_pixels)) {
        (void)fclose(file);
        return 0;
    }
    (void)fclose(file);
    return 1;
}


/*
 * Frame capture for headless visual review.
 *
 * The renderer, the camera, and the PPM writer are all already headless --
 * demo_smoke_test proves it by producing one frame with no window. This
 * turns that into a filmstrip: run a scripted scenario and write a frame
 * every few ticks, so terrain, effects, and physics can be inspected as
 * images rather than inferred from counters. Deterministic and windowless,
 * so it runs anywhere the tests do.
 */
static void demo_capture_setup_app(demo_app *app)
{
    memset(app, 0, sizeof(*app));
    app->bots = 0;
    app->local_players = 1;
    app->arsenal = DEMO_ARSENAL_FULL;
    app->options.gi_quality = VOX_GI_BALANCED;
    app->options.gore_level = 2;
    app->options.fx_profile = 1;
    app->screen = DEMO_PLAY;
    app->mouse_x = 160;
    app->mouse_y = 100;
    app->mouse_activity_x = 160;
    app->mouse_activity_y = 100;
    app->cursor_visible = -1;
    app->camera_zoom = DEMO_CAMERA_ZOOM_DEFAULT;
    app->camera_scale = (double)DEMO_CAMERA_ZOOM_DEFAULT;
    app->frame_seconds = 1.0 / 60.0;
    app->render_alpha = 1.0;
    app->measured_fps = 60.0;
}

static void demo_capture_frame(demo_app *app, const char *directory,
                               vox_u32 index)
{
    char path[512];
    vox_u16 slot;
    for (slot = 0U; slot < VOX_DIGS_MAX_SLOTS; ++slot) {
        app->previous_player_x[slot] =
            demo_match.players[slot].position_x.value_q16;
        app->previous_player_y[slot] =
            demo_match.players[slot].position_y.value_q16;
    }
    app->camera_world_x = (double)demo_match.players[0].
                          position_x.value_q16 / 65536.0;
    app->camera_world_y = (double)demo_match.players[0].
                          position_y.value_q16 / 65536.0;
    demo_update_player_camera(app);
    demo_render(app);
    sprintf(path, "%s/frame_%04lu.ppm", directory, (unsigned long)index);
    (void)demo_write_ppm(path);
}

static int demo_capture(const char *scenario, const char *directory,
                        vox_u32 frames, vox_u32 interval)
{
    demo_app app;
    vox_digs_rules rules;
    vox_u32 frame;
    vox_u32 tick = 0U;
    vox_i32 origin_x;
    vox_i32 origin_y;
    int wide = strcmp(scenario, "blast-wide") == 0;
    int narrow = strcmp(scenario, "blast-narrow") == 0;
    int smoke = strcmp(scenario, "smoke") == 0;
    int overhang = strcmp(scenario, "overhang") == 0;
    if (!wide && !narrow && !smoke && !overhang) {
        fprintf(stderr,
                "capture scenarios: blast-wide blast-narrow overhang smoke\n");
        return 2;
    }
    demo_capture_setup_app(&app);
    demo_prepare_targets();
    vox_digs_rules_classic(&rules);
    rules.player_count = 1U;
    rules.bot_mask = 0U;
    rules.score_limit = 0U;
    rules.match_ticks = 20000U;
    rules.lava_start_tick = 19000U;
    rules.map_style = VOX_DIGS_MAP_COAL_RIDGE;
    rules.weapon_mask = demo_arsenal_masks[DEMO_ARSENAL_FULL];
    rules.seed = 0x564F5831U;
    if (vox_digs_match_init(&demo_match, &rules) != VOX_OK) {
        return 3;
    }
    demo_match.spawn_shield_ticks[0] = 0U;
    origin_x = demo_match.players[0].position_x.value_q16 >> 16;
    origin_y = demo_match.players[0].position_y.value_q16 >> 16;
    /* Bury the miner's own column so the camera frames the test terrain. */
    if (wide || narrow || overhang) {
        vox_i32 half = wide ? 20L : (overhang ? 14L : 3L);
        vox_i32 x;
        vox_i32 y;
        vox_i32 top = origin_y + 6L;
        vox_i32 bottom = origin_y + 10L;
        /* Solid overburden above, so there is something to bring down. */
        for (y = origin_y - 24L; y <= bottom; ++y) {
            for (x = origin_x - 34L; x <= origin_x + 34L; ++x) {
                vox_u32 z;
                if (x < 0L || y < 0L) continue;
                for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                    (void)vox_world_set(&demo_match.world, (vox_u32)x,
                                        (vox_u32)y, z, VOX_MAT_SOIL,
                                        20L << 16);
                }
            }
        }
        for (y = origin_y - 40L; y < origin_y - 24L; ++y) {
            for (x = origin_x - 34L; x <= origin_x + 34L; ++x) {
                vox_u32 z;
                if (x < 0L || y < 0L) continue;
                for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
                    (void)vox_world_set(&demo_match.world, (vox_u32)x,
                                        (vox_u32)y, z, VOX_MAT_AIR,
                                        20L << 16);
                }
            }
        }
        (void)vox_world_sleep_all(&demo_match.world);
        demo_match.players[0].position_y.value_q16 =
            (vox_i32)((origin_y + 8L) << 16);
        demo_capture_frame(&app, directory, 0U);
        /* Blast the void open the way a real weapon would. */
        for (x = origin_x - half; x <= origin_x + half; x += 6L) {
            if (x < 0L) continue;
            (void)vox_world_blast(&demo_match.world, (vox_u32)x,
                                  (vox_u32)top, 0U, 6U, 300L << 16);
        }
    } else {
        demo_capture_frame(&app, directory, 0U);
        (void)vox_digs_fire_weapon(&demo_match, 0U, VOX_DIGS_TOOL_SMOKER,
                                   (vox_u32)(origin_x + 10L),
                                   (vox_u32)origin_y);
    }
    for (frame = 1U; frame < frames; ++frame) {
        vox_u32 step;
        for (step = 0U; step < interval; ++step) {
            if (vox_digs_match_step(&demo_match) != VOX_OK) {
                return 4;
            }
            tick++;
        }
        demo_capture_frame(&app, directory, frame);
    }
    printf("capture scenario=%s frames=%lu interval=%lu ticks=%lu "
           "awake=%lu occupied=%lu hash=%08lx dir=%s\n",
           scenario, (unsigned long)frames, (unsigned long)interval,
           (unsigned long)tick,
           (unsigned long)demo_match.world.awake_cells,
           (unsigned long)demo_match.world.occupied_cells,
           (unsigned long)demo_match.state_hash, directory);
    return 0;
}

static int demo_smoke_test(const char *path)
{
    static const vox_u16 scripted_weapons[8] = {
        VOX_DIGS_TOOL_NAIL_GUN, VOX_DIGS_TOOL_BOILER_SHOTGUN,
        VOX_DIGS_TOOL_SMOKE_POT, VOX_DIGS_TOOL_CINDER_FLASK,
        VOX_DIGS_TOOL_PRESSURE_HOSE, VOX_DIGS_TOOL_BLAST_CHARGE,
        VOX_DIGS_TOOL_CONCUSSION_GRENADE, VOX_DIGS_TOOL_NAIL_BOMB
    };
    demo_app app;
    vox_digs_rules rules;
    vox_u32 tick;
    vox_u16 script_index = 0U;
    vox_u16 fired_mask = 0U;
    vox_u16 max_projectiles = 0U;
    vox_u16 max_effects = 0U;
    vox_u16 min_lava_y;
    vox_u16 damage_seen = 0U;
    vox_u16 death_total = 0U;
    memset(&app, 0, sizeof(app));
    app.bots = 1;
    app.local_players = 1;
    app.map_style = VOX_DIGS_MAP_FURNACE_YARD;
    app.arsenal = DEMO_ARSENAL_FULL;
    app.seed = 0xD1655EEDU;
    app.options.gi_quality = VOX_GI_BALANCED;
    app.options.gore_level = 2;
    app.options.fx_profile = 1;
    demo_prepare_targets();
    vox_digs_rules_classic(&rules);
    rules.match_ticks = 1200U;
    rules.score_limit = 100U;
    rules.lava_start_tick = 600U;
    rules.player_count = 2U;
    rules.bot_mask = 2U;
    rules.map_style = VOX_DIGS_MAP_FURNACE_YARD;
    rules.weapon_mask = demo_arsenal_masks[DEMO_ARSENAL_FULL];
    rules.seed = app.seed;
    if (vox_digs_match_init(&demo_match, &rules) != VOX_OK) {
        return 2;
    }
    min_lava_y = demo_match.lava_surface_y;
    for (tick = 0U; tick < 750U; ++tick) {
        vox_digs_input input;
        memset(&input, 0, sizeof(input));
        input.abi_version = VOX_ABI_VERSION;
        input.struct_size = (vox_u32)sizeof(input);
        input.player = 0U;
        input.actions = tick < 120U ? VOX_DIGS_ACTION_RIGHT :
                        (tick < 200U ? (VOX_DIGS_ACTION_RIGHT |
                                        VOX_DIGS_ACTION_STEAM) : 0U);
        input.move_x_q15 = (input.actions & VOX_DIGS_ACTION_RIGHT) ?
                           32767 : 0;
        if (demo_match.alive[0]) {
            (void)vox_digs_submit_input(&demo_match, &input);
        }
        if (tick == 5U && demo_match.alive[1] &&
            vox_digs_apply_damage(&demo_match, 0U, 1U, 15U) == VOX_OK) {
            damage_seen = 1U;
        }
        if (script_index < 8U && demo_match.alive[0]) {
            vox_u32 target_x;
            vox_u32 target_y;
            vox_u16 weapon = scripted_weapons[script_index];
            vox_u16 target_player = demo_match.alive[1] ? 1U : 0U;
            target_x = (vox_u32)demo_q16_to_screen(
                demo_match.players[target_player].position_x.value_q16,
                VOX_WORLD_WIDTH, VOX_WORLD_WIDTH);
            target_y = (vox_u32)demo_q16_to_screen(
                demo_match.players[target_player].position_y.value_q16,
                VOX_WORLD_HEIGHT, VOX_WORLD_HEIGHT);
            if (vox_digs_fire_weapon(&demo_match, 0U, weapon,
                                     target_x, target_y) == VOX_OK) {
                fired_mask = (vox_u16)(fired_mask |
                              (vox_u16)(1U << weapon));
                app.selected_tool[0] = weapon;
                script_index++;
            }
        }
        if (demo_match.projectile_count > max_projectiles) {
            max_projectiles = demo_match.projectile_count;
        }
        if (vox_digs_match_step(&demo_match) != VOX_OK) {
            return 3;
        }
        if (demo_match.projectile_count > max_projectiles) {
            max_projectiles = demo_match.projectile_count;
        }
        if (demo_match.effect_count > max_effects) {
            max_effects = demo_match.effect_count;
        }
        if (demo_match.lava_surface_y < min_lava_y) {
            min_lava_y = demo_match.lava_surface_y;
        }
        if (demo_match.health[0] < VOX_DIGS_MAX_HEALTH ||
            demo_match.health[1] < VOX_DIGS_MAX_HEALTH) {
            damage_seen = 1U;
        }
        death_total = (vox_u16)(demo_match.deaths[0] +
                                 demo_match.deaths[1]);
    }
    app.screen = DEMO_PLAY;
    app.mouse_x = 160;
    app.mouse_y = 100;
    app.mouse_activity_x = 160;
    app.mouse_activity_y = 100;
    app.cursor_visible = -1;
    app.camera_zoom = DEMO_CAMERA_ZOOM_DEFAULT;
    app.camera_scale = (double)DEMO_CAMERA_ZOOM_DEFAULT;
    app.frame_seconds = 1.0 / 60.0;
    app.render_alpha = 1.0;
    app.camera_world_x = (double)demo_match.players[0].
                         position_x.value_q16 / 65536.0;
    app.camera_world_y = (double)demo_match.players[0].
                         position_y.value_q16 / 65536.0;
    for (tick = 0U; tick < VOX_DIGS_MAX_SLOTS; ++tick) {
        app.previous_player_x[tick] =
            demo_match.players[tick].position_x.value_q16;
        app.previous_player_y[tick] =
            demo_match.players[tick].position_y.value_q16;
    }
    app.options.debug = 1;
    app.measured_fps = 60.0;
    demo_render(&app);
    if (!demo_write_ppm(path)) {
        return 5;
    }
    printf("DIGS demo smoke tick=%lu hash=%08lx frame=%08lx "
           "fired=%03x projectiles=%u effects=%u damage=%u deaths=%u "
           "lava_y=%u path=%s\n",
           (unsigned long)demo_match.tick,
           (unsigned long)demo_match.state_hash,
           (unsigned long)vox_software_hash(&demo_target),
           (unsigned int)fired_mask, (unsigned int)max_projectiles,
           (unsigned int)max_effects, (unsigned int)damage_seen,
           (unsigned int)death_total, (unsigned int)min_lava_y, path);
    if ((fired_mask & 0x00C0U) == 0U ||
        (fired_mask & 0x001EU) == 0U ||
        (fired_mask & 0x0302U) == 0U || max_projectiles == 0U ||
        max_effects == 0U || !damage_seen ||
        min_lava_y >= VOX_WORLD_HEIGHT - 2U) {
        fprintf(stderr, "smoke acceptance evidence incomplete\n");
        return 5;
    }
    return 0;
}

static int demo_benchmark(vox_u32 frames)
{
    vox_u16 quality;
    demo_prepare_targets();
    demo_build_title_world();
    printf("VOX software lightfield benchmark: %lu frames per tier\n",
           (unsigned long)frames);
    for (quality = VOX_GI_COMPATIBILITY;
         quality <= VOX_GI_SHOWCASE; ++quality) {
        clock_t started;
        clock_t stopped;
        double seconds;
        double milliseconds;
        double fps;
        vox_u32 frame;
        demo_render_config.gi_quality = quality;
        (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                     &demo_render_config);
        started = clock();
        for (frame = 0U; frame < frames; ++frame) {
            if (vox_software_render_ex(&demo_title_world, &demo_target,
                                       &demo_render_config) != VOX_OK) {
                return 6;
            }
        }
        stopped = clock();
        seconds = (double)(stopped - started) / (double)CLOCKS_PER_SEC;
        if (seconds <= 0.0) {
            seconds = 1.0 / (double)CLOCKS_PER_SEC;
        }
        milliseconds = seconds * 1000.0 / (double)frames;
        fps = (double)frames / seconds;
        printf("  %-13s %8.3f ms/frame  %9.1f fps  frame=%08lx\n",
               demo_gi_names[quality], milliseconds, fps,
               (unsigned long)vox_software_hash(&demo_target));
    }
    return 0;
}

static void demo_fixed_step_clock_init(demo_fixed_step_clock *clock)
{
    clock->pending_ticks = 0U;
    clock->phase_units = 0U;
}

static void demo_fixed_step_add_elapsed(demo_fixed_step_clock *clock,
                                        Uint64 elapsed_units,
                                        Uint64 frequency)
{
    Uint64 whole_seconds;
    Uint64 remainder;
    vox_u32 phase_pass;
    if (frequency == 0U) return;
    whole_seconds = elapsed_units / frequency;
    remainder = elapsed_units % frequency;
    clock->pending_ticks += whole_seconds * (Uint64)DEMO_SIM_HZ;
    /* Add remainder * DEMO_SIM_HZ without multiplying two timer-sized
     * values. phase_units remains less than frequency, so this modular
     * addition cannot overflow even on a 64-bit performance counter. */
    for (phase_pass = 0U; phase_pass < DEMO_SIM_HZ; ++phase_pass) {
        if (clock->phase_units >= frequency - remainder) {
            clock->phase_units -= frequency - remainder;
            ++clock->pending_ticks;
        } else {
            clock->phase_units += remainder;
        }
    }
}

static vox_u32 demo_fixed_step_service(demo_fixed_step_clock *clock,
                                       vox_u32 maximum_ticks)
{
    vox_u32 serviced;
    if (clock->pending_ticks < (Uint64)maximum_ticks) {
        serviced = (vox_u32)clock->pending_ticks;
    } else {
        serviced = maximum_ticks;
    }
    clock->pending_ticks -= (Uint64)serviced;
    return serviced;
}

static int demo_fixed_step_can_present(const demo_fixed_step_clock *clock)
{
    return clock->pending_ticks == 0U;
}

static double demo_fixed_step_alpha(const demo_fixed_step_clock *clock,
                                    Uint64 frequency)
{
    if (frequency == 0U || clock->pending_ticks != 0U) return 0.0;
    return (double)clock->phase_units / (double)frequency;
}

static int demo_fixed_step_self_test(void)
{
    demo_fixed_step_clock clock;
    vox_digs_rules rules;
    const Uint64 frequency = 1000000U;
    vox_u32 simulation_ticks = 0U;
    vox_u32 batches = 0U;
    vox_u32 skipped_presentations = 0U;
    vox_u32 serviced;
    vox_u32 tick_index;
    vox_digs_rules_classic(&rules);
    rules.player_count = 2U;
    rules.bot_mask = 2U;
    rules.match_ticks = 600U;
    rules.lava_start_tick = 300U;
    rules.seed = 0x46535450U;
    if (vox_digs_match_init(&demo_match, &rules) != VOX_OK) {
        fprintf(stderr, "fixed-step self-test: match init failed\n");
        return 1;
    }
    demo_fixed_step_clock_init(&clock);
    /* 250 ms represents fifteen authoritative 60 Hz ticks. This is well
     * beyond the eight-tick/133 ms catch-up budget for one event pump. */
    demo_fixed_step_add_elapsed(&clock, 250000U, frequency);
    while (!demo_fixed_step_can_present(&clock)) {
        serviced = demo_fixed_step_service(&clock, DEMO_MAX_CATCHUP);
        for (tick_index = 0U; tick_index < serviced; ++tick_index) {
            if (vox_digs_match_step(&demo_match) != VOX_OK) {
                fprintf(stderr,
                        "fixed-step self-test: match step failed at %lu\n",
                        (unsigned long)simulation_ticks);
                return 1;
            }
            ++simulation_ticks;
        }
        ++batches;
        if (!demo_fixed_step_can_present(&clock)) {
            ++skipped_presentations;
        }
        if (serviced == 0U || batches > 16U) {
            fprintf(stderr, "fixed-step self-test: recovery stalled\n");
            return 1;
        }
    }
    if (simulation_ticks != 15U || batches != 2U ||
        skipped_presentations != 1U || clock.pending_ticks != 0U ||
        clock.phase_units != 0U || demo_match.tick != 15U ||
        demo_fixed_step_alpha(&clock, frequency) != 0.0) {
        fprintf(stderr,
                "fixed-step self-test: debt was lost ticks=%lu batches=%lu "
                "skipped=%lu pending=%lu phase=%lu match_tick=%lu\n",
                (unsigned long)simulation_ticks, (unsigned long)batches,
                (unsigned long)skipped_presentations,
                (unsigned long)clock.pending_ticks,
                (unsigned long)clock.phase_units,
                (unsigned long)demo_match.tick);
        return 1;
    }
    /* Fractional counter time must survive separate event-pump samples. */
    demo_fixed_step_add_elapsed(&clock, 16666U, frequency);
    if (demo_fixed_step_service(&clock, DEMO_MAX_CATCHUP) != 0U) {
        fprintf(stderr, "fixed-step self-test: early fractional tick\n");
        return 1;
    }
    demo_fixed_step_add_elapsed(&clock, 1U, frequency);
    if (demo_fixed_step_service(&clock, DEMO_MAX_CATCHUP) != 1U ||
        clock.phase_units != 20U || !demo_fixed_step_can_present(&clock)) {
        fprintf(stderr, "fixed-step self-test: fractional debt was lost\n");
        return 1;
    }
    printf("DIGS fixed-step self-test passed stall_ms=250 ticks=%lu "
           "match_tick=%lu batches=%lu skipped_presentations=%lu "
           "fractional_phase=%lu\n",
           (unsigned long)simulation_ticks,
           (unsigned long)demo_match.tick, (unsigned long)batches,
           (unsigned long)skipped_presentations,
           (unsigned long)clock.phase_units);
    return 0;
}

static int demo_compare_u32(const void *left, const void *right)
{
    const vox_u32 left_value = *(const vox_u32 *)left;
    const vox_u32 right_value = *(const vox_u32 *)right;
    if (left_value < right_value) return -1;
    if (left_value > right_value) return 1;
    return 0;
}

static int demo_performance_self_test(vox_u32 ticks, int qualify_named_bench)
{
    static const vox_u16 explosive_tools[3] = {
        VOX_DIGS_TOOL_FIRECRACKER,
        VOX_DIGS_TOOL_FIRECRACKER,
        VOX_DIGS_TOOL_FIRECRACKER
    };
    vox_digs_rules rules;
    vox_u32 *samples = 0;
    Uint64 frequency = 0U;
    Uint64 total_us = 0U;
    vox_u32 tick;
    vox_u32 p95_index;
    vox_u32 average_us;
    vox_u32 p95_us;
    vox_u32 maximum_us;
    vox_u32 fired = 0U;
    vox_u32 explosions = 0U;
    vox_u32 crushes = 0U;
    vox_u32 last_event_sequence = 0U;
    vox_u16 max_effects = 0U;
    vox_u32 max_awake = 0U;
    int status = 0;
    if (qualify_named_bench) {
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_TIMER) != 0) {
            fprintf(stderr,
                    "performance self-test: SDL timer unavailable: %s\n",
                    SDL_GetError());
            return 1;
        }
        frequency = SDL_GetPerformanceFrequency();
        if (frequency == 0U) {
            fprintf(stderr,
                    "performance self-test: high-resolution timer failed\n");
            SDL_Quit();
            return 2;
        }
        samples = (vox_u32 *)malloc((size_t)ticks * sizeof(*samples));
        if (samples == 0) {
            fprintf(stderr,
                    "performance self-test: sample allocation failed\n");
            SDL_Quit();
            return 3;
        }
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
    rules.seed = 0x50303033U;
    if (vox_digs_match_init(&demo_match, &rules) != VOX_OK) {
        fprintf(stderr, "performance self-test: match init failed\n");
        if (qualify_named_bench) {
            free(samples);
            SDL_Quit();
        }
        return 4;
    }
    for (tick = 0U; tick < ticks; ++tick) {
        vox_u16 player;
        if ((tick % 90U) == 0U) {
            vox_u32 blast_x = VOX_WORLD_WIDTH / 5U +
                (tick / 90U * 67U) % (VOX_WORLD_WIDTH * 3U / 5U);
            vox_u32 blast_y = VOX_WORLD_HEIGHT * 3U / 5U;
            (void)vox_world_blast(&demo_match.world, blast_x, blast_y, 0U,
                                  7U, 700L << 16);
        }
        for (player = 0U; player < 2U; ++player) {
            vox_digs_input input;
            vox_u16 target = (vox_u16)(player + 2U);
            vox_u16 weapon = explosive_tools[
                (tick / 120U + player) % 3U];
            long aim_x = demo_match.players[target].position_x.value_q16 /
                         65536L;
            long aim_y = demo_match.players[target].position_y.value_q16 /
                         65536L;
            memset(&input, 0, sizeof(input));
            input.abi_version = VOX_ABI_VERSION;
            input.struct_size = (vox_u32)sizeof(input);
            input.player = player;
            input.actions = VOX_DIGS_ACTION_FIRE;
            /* Firecracker is a release weapon.  This deterministic load
             * stream intentionally charges then releases it instead of
             * pretending a held input is a completed throw. */
            if ((tick % 72U) >= 52U) {
                input.actions = 0U;
            }
            if (((tick / 75U) + player) & 1U) {
                input.actions = (vox_u16)(input.actions |
                                          VOX_DIGS_ACTION_LEFT);
                input.move_x_q15 = -24576;
            } else {
                input.actions = (vox_u16)(input.actions |
                                          VOX_DIGS_ACTION_RIGHT);
                input.move_x_q15 = 24576;
            }
            if ((tick % 100U) < 24U) {
                input.actions = (vox_u16)(input.actions |
                                          VOX_DIGS_ACTION_STEAM);
                input.move_y_q15 = -16384;
            }
            if (aim_x < 0L) aim_x = 0L;
            if (aim_y < 0L) aim_y = 0L;
            if (aim_x >= (long)VOX_WORLD_WIDTH) {
                aim_x = (long)VOX_WORLD_WIDTH - 1L;
            }
            if (aim_y >= (long)VOX_WORLD_HEIGHT) {
                aim_y = (long)VOX_WORLD_HEIGHT - 1L;
            }
            input.aim_x = (vox_u16)aim_x;
            input.aim_y = (vox_u16)aim_y;
            input.selected_weapon = weapon;
            (void)vox_digs_submit_input(&demo_match, &input);
        }
        if (qualify_named_bench) {
            Uint64 started = SDL_GetPerformanceCounter();
            Uint64 stopped;
            Uint64 elapsed;
            if (vox_digs_match_step(&demo_match) != VOX_OK) {
                fprintf(stderr,
                        "performance self-test: match step failed at %lu\n",
                        (unsigned long)tick);
                status = 5;
                break;
            }
            stopped = SDL_GetPerformanceCounter();
            elapsed = stopped - started;
            samples[tick] = (vox_u32)((elapsed * 1000000U + frequency - 1U) /
                                      frequency);
            total_us += samples[tick];
        } else if (vox_digs_match_step(&demo_match) != VOX_OK) {
            fprintf(stderr,
                    "load self-test: match step failed at %lu\n",
                    (unsigned long)tick);
            status = 5;
            break;
        }
        if (demo_match.effect_count > max_effects) {
            max_effects = demo_match.effect_count;
        }
        if (demo_match.world.awake_cells > max_awake) {
            max_awake = demo_match.world.awake_cells;
        }
        {
            vox_u16 event_index;
            for (event_index = 0U; event_index < demo_match.event_count;
                 ++event_index) {
                const vox_digs_event *event =
                    vox_digs_event_get(&demo_match, event_index);
                if (event == 0 || event->sequence <= last_event_sequence) {
                    continue;
                }
                last_event_sequence = event->sequence;
                if (event->type == VOX_DIGS_EVENT_WEAPON_FIRE) ++fired;
                else if (event->type == VOX_DIGS_EVENT_EXPLOSION) {
                    ++explosions;
                } else if (event->type == VOX_DIGS_EVENT_CRUSH) ++crushes;
            }
        }
    }
    if (status == 0) {
        if (qualify_named_bench) {
            qsort(samples, (size_t)ticks, sizeof(*samples), demo_compare_u32);
            p95_index = (ticks * 95U + 99U) / 100U;
            if (p95_index > 0U) --p95_index;
            if (p95_index >= ticks) p95_index = ticks - 1U;
            average_us = (vox_u32)((total_us + ticks / 2U) / ticks);
            p95_us = samples[p95_index];
            maximum_us = samples[ticks - 1U];
            printf("DIGS named-bench performance qualification ticks=%lu "
                   "slots=4 local=2 bots=2 avg=%.3fms p95=%.3fms "
                   "max=%.3fms fired=%lu explosions=%lu crushes=%lu "
                   "effects=%u awake=%lu hash=%08lx\n",
                   (unsigned long)ticks, (double)average_us / 1000.0,
                   (double)p95_us / 1000.0,
                   (double)maximum_us / 1000.0,
                   (unsigned long)fired, (unsigned long)explosions,
                   (unsigned long)crushes, (unsigned int)max_effects,
                   (unsigned long)max_awake,
                   (unsigned long)demo_match.state_hash);
        } else {
            printf("DIGS deterministic load self-test ticks=%lu slots=4 "
                   "local=2 bots=2 fired=%lu explosions=%lu crushes=%lu "
                   "effects=%u awake=%lu hash=%08lx\n",
                   (unsigned long)ticks, (unsigned long)fired,
                   (unsigned long)explosions, (unsigned long)crushes,
                   (unsigned int)max_effects, (unsigned long)max_awake,
                   (unsigned long)demo_match.state_hash);
        }
        if (fired == 0U || explosions == 0U || max_effects == 0U ||
            max_awake == 0U) {
            fprintf(stderr,
                    "load self-test: explosive/collapse load missing\n");
            status = 6;
        } else if (ticks == 600U &&
                   (fired != 43U || explosions != 16U || crushes != 1U ||
                    max_effects != 474U || max_awake != 10754U ||
                    demo_match.state_hash != (vox_u32)0xFC2E1A6DUL)) {
            fprintf(stderr,
                    "load self-test: canonical 600-tick activity/hash "
                    "mismatch\n");
            status = 8;
        } else if (qualify_named_bench &&
                   (average_us > 5000U || p95_us > 8000U ||
                    maximum_us > 16667U)) {
            fprintf(stderr,
                    "performance self-test: named-bench RFC limits exceeded "
                    "(avg<=5ms p95<=8ms max<=16.67ms)\n");
            status = 7;
        }
    }
    if (qualify_named_bench) {
        free(samples);
        SDL_Quit();
    }
    return status;
}

static int demo_input_self_test(void)
{
    demo_app app;
    demo_player_input input;
    double x;
    double y;
    double magnitude;
    double unit_error;
    memset(&app, 0, sizeof(app));
    memset(&input, 0, sizeof(input));
    demo_input_defaults(&app);
    input.deadzone = 2;
    if (demo_radial_response(3000, 3000, 0.20, 1,
                             &x, &y, &magnitude)) {
        fprintf(stderr, "input self-test: deadzone leak\n");
        return 1;
    }
    if (!demo_radial_response(32767, 32767, 0.20, 1,
                              &x, &y, &magnitude)) {
        fprintf(stderr, "input self-test: full diagonal rejected\n");
        return 2;
    }
    unit_error = x * x + y * y - 1.0;
    if (unit_error < 0.0) unit_error = -unit_error;
    if (unit_error > 0.01 || magnitude < 0.99 || magnitude > 1.0) {
        fprintf(stderr, "input self-test: radial normalization failed\n");
        return 3;
    }
    if (!demo_radial_response(9000, 0, 0.20, 1,
                              &x, &y, &magnitude) || magnitude >= 0.20) {
        fprintf(stderr, "input self-test: precision response failed\n");
        return 4;
    }
    app.selected_tool[0] = VOX_DIGS_TOOL_PICK;
    if (demo_weapon_aim_range(&app, 0, 0) != 8.0 ||
        demo_weapon_aim_range(&app, 0, 1) != DEMO_ROPE_AIM_RANGE) {
        fprintf(stderr, "input self-test: tool ranges failed\n");
        return 5;
    }
    app.selected_tool[0] = VOX_DIGS_TOOL_NAIL_GUN;
    if (demo_weapon_aim_range(&app, 0, 0) != 24.0) {
        fprintf(stderr, "input self-test: projectile range failed\n");
        return 6;
    }
    if (demo_input_deadzone(&input, 0) != 0.20) {
        fprintf(stderr, "input self-test: deadzone preset failed\n");
        return 7;
    }
    app.local_players = 2;
    app.controllers[0].joystick = (SDL_Joystick *)(void *)&app;
    app.controllers[0].claimed_player = -1;
    demo_refresh_controller_claims(&app);
    if (app.controllers[0].claimed_player != 1) {
        fprintf(stderr, "input self-test: one-pad P2 claim failed\n");
        return 8;
    }
    app.player_input[0].preference = DEMO_INPUT_CONTROLLER;
    app.player_input[1].preference = DEMO_INPUT_KEYBOARD;
    demo_refresh_controller_claims(&app);
    if (app.controllers[0].claimed_player != 0) {
        fprintf(stderr, "input self-test: explicit pad claim failed\n");
        return 9;
    }
    demo_input_defaults(&app);
    app.local_players = 2;
    app.controllers[1].joystick = (SDL_Joystick *)(void *)&input;
    app.controllers[1].claimed_player = -1;
    demo_refresh_controller_claims(&app);
    if (app.controllers[0].claimed_player != 0 ||
        app.controllers[1].claimed_player != 1) {
        fprintf(stderr, "input self-test: exclusive two-pad claims failed\n");
        return 10;
    }
    if (!demo_activate_source(&app, 0, DEMO_SOURCE_CONTROLLER, 1) ||
        app.player_input[0].active_source != DEMO_SOURCE_CONTROLLER ||
        app.player_input[0].suppress_ticks != 1) {
        fprintf(stderr, "input self-test: AUTO source switch failed\n");
        return 11;
    }
    app.player_input[0].preference = DEMO_INPUT_KEYBOARD;
    app.player_input[0].active_source = DEMO_SOURCE_KEYBOARD;
    if (demo_activate_source(&app, 0, DEMO_SOURCE_CONTROLLER, 1)) {
        fprintf(stderr, "input self-test: locked source was stolen\n");
        return 12;
    }
    memset(&app.controllers[0], 0, sizeof(app.controllers[0]));
    if (demo_controller_zoom_step(&app.controllers[0], 20000, -24000) != 1 ||
        demo_controller_zoom_step(&app.controllers[0], 20000, -24000) != 0 ||
        demo_controller_zoom_step(&app.controllers[0], 20000, 0) != 0 ||
        demo_controller_zoom_step(&app.controllers[0], 20000, 24000) != -1 ||
        demo_controller_zoom_step(&app.controllers[0], 0, 24000) != 0) {
        fprintf(stderr, "input self-test: controller zoom debounce failed\n");
        return 13;
    }
    if (demo_controller_family_from_name("Nintendo Switch Pro") !=
            DEMO_PAD_NINTENDO ||
        demo_controller_family_from_name("Sony DualSense") !=
            DEMO_PAD_PLAYSTATION ||
        demo_controller_family_from_name("Xbox Wireless") !=
            DEMO_PAD_XBOX ||
        strcmp(demo_pad_button_label(DEMO_PAD_NINTENDO,
                                     SDL_CONTROLLER_BUTTON_A), "B") != 0 ||
        strcmp(demo_pad_button_label(DEMO_PAD_PLAYSTATION,
                                     SDL_CONTROLLER_BUTTON_A), "CROSS") != 0 ||
        strcmp(demo_pad_button_label(DEMO_PAD_XBOX,
                                     SDL_CONTROLLER_BUTTON_A), "A") != 0 ||
        demo_pad_accept_button(DEMO_PAD_NINTENDO) !=
            SDL_CONTROLLER_BUTTON_B ||
        demo_pad_back_button(DEMO_PAD_NINTENDO) !=
            SDL_CONTROLLER_BUTTON_A ||
        demo_pad_accept_button(DEMO_PAD_XBOX) != SDL_CONTROLLER_BUTTON_A ||
        demo_pad_back_button(DEMO_PAD_PLAYSTATION) !=
            SDL_CONTROLLER_BUTTON_B) {
        fprintf(stderr, "input self-test: controller family labels failed\n");
        return 14;
    }
    printf("DIGS input self-test passed\n");
    return 0;
}

static int demo_cap_self_test(void)
{
    demo_app app;
    vox_u32 mask_slow = demo_cap_mask_from_frame_us(60000U);
    vox_u32 mask_recovery = demo_cap_mask_from_frame_us(50000U);
    vox_u32 mask_mid = demo_cap_mask_from_frame_us(14000U);
    memset(&app, 0, sizeof(app));
    if (mask_slow != ((vox_u32)1U << (DEMO_FRAME_CAP_COUNT - 1)) ||
        (mask_recovery & ((vox_u32)1U << 0)) == 0U ||
        (mask_recovery & ((vox_u32)1U << 1)) != 0U ||
        (mask_mid & ((vox_u32)1U << 2)) == 0U ||
        (mask_mid & ((vox_u32)1U << 3)) != 0U) {
        fprintf(stderr, "cap self-test: qualification thresholds failed\n");
        return 1;
    }
    app.cap_supported_mask = ((vox_u32)1U << 0) |
                             ((vox_u32)1U << 2) |
                             ((vox_u32)1U << 6);
    if (demo_next_supported_cap(&app, 0, 1) != 2 ||
        demo_next_supported_cap(&app, 2, 1) != 6 ||
        demo_next_supported_cap(&app, 0, -1) != 6 ||
        demo_cap_hash_text(2166136261U, "RENDERER") ==
        demo_cap_hash_text(2166136261U, "renderer")) {
        fprintf(stderr, "cap self-test: gating/profile helper failed\n");
        return 2;
    }
    printf("DIGS cap qualification self-test passed slow=%02lx "
           "recovery=%02lx mid=%02lx\n",
           (unsigned long)mask_slow, (unsigned long)mask_recovery,
           (unsigned long)mask_mid);
    return 0;
}

static int demo_haptic_self_test(void)
{
    demo_app app;
    demo_haptic_envelope full;
    demo_haptic_envelope distant;
    vox_digs_event event;
    vox_u16 low[DEMO_HAPTIC_LEVEL_COUNT];
    vox_u16 high[DEMO_HAPTIC_LEVEL_COUNT];
    vox_u16 near_locality;
    vox_u16 far_locality;
    int level;
    memset(&app, 0, sizeof(app));
    app.local_players = 2;
    app.options.haptic_level = 0;
    demo_haptic_impulse(&app, 0U, DEMO_HAPTIC_EXPLOSION, 1000U, 32767U);
    if (app.haptic[0].ticks_left != 0U) {
        fprintf(stderr, "haptic self-test: Off accepted an impulse\n");
        return 1;
    }
    app.options.haptic_level = 3;
    demo_haptic_impulse(&app, 0U, DEMO_HAPTIC_EXPLOSION, 1000U, 32767U);
    demo_haptic_impulse(&app, 0U, DEMO_HAPTIC_KILL, 100U, 32767U);
    if (app.haptic[0].low_peak != 65535U ||
        app.haptic[0].high_peak != 65535U ||
        app.haptic[0].ticks_left != 14U ||
        app.haptic[1].ticks_left != 0U) {
        fprintf(stderr,
                "haptic self-test: overlap saturation/isolation failed\n");
        return 2;
    }
    full = app.haptic[0];
    for (level = 0; level < DEMO_HAPTIC_LEVEL_COUNT; ++level) {
        demo_haptic_mix_sample(&full, level, &low[level], &high[level]);
    }
    if (low[0] != 0U || high[0] != 0U ||
        (vox_u32)low[1] + high[1] >= (vox_u32)low[2] + high[2] ||
        (vox_u32)low[2] + high[2] >= (vox_u32)low[3] + high[3]) {
        fprintf(stderr, "haptic self-test: intensity ladder failed\n");
        return 3;
    }
    memset(&event, 0, sizeof(event));
    memset(&demo_match, 0, sizeof(demo_match));
    demo_match.rules.player_count = 2U;
    event.source = VOX_DIGS_NO_PLAYER;
    event.target = VOX_DIGS_NO_PLAYER;
    event.position_x_q16 = 0L;
    event.position_y_q16 = 0L;
    demo_match.players[0].position_x.value_q16 = 0L;
    demo_match.players[0].position_y.value_q16 = 0L;
    demo_match.players[1].position_x.value_q16 = 64L << 16;
    demo_match.players[1].position_y.value_q16 = 0L;
    near_locality = demo_haptic_locality(&event, 0U);
    far_locality = demo_haptic_locality(&event, 1U);
    memset(&app.haptic[0], 0, sizeof(app.haptic[0]));
    demo_haptic_impulse(&app, 0U, DEMO_HAPTIC_CRUMBLE, 40U,
                        near_locality);
    distant = app.haptic[0];
    memset(&app.haptic[0], 0, sizeof(app.haptic[0]));
    demo_haptic_impulse(&app, 0U, DEMO_HAPTIC_CRUMBLE, 40U,
                        far_locality);
    if (near_locality != 32767U || far_locality >= near_locality ||
        app.haptic[0].low_peak >= distant.low_peak) {
        fprintf(stderr, "haptic self-test: distance attenuation failed\n");
        return 4;
    }
    /* No controller is installed in this pure test. Sampling and advancing
     * the envelope must still be safe and deterministic. */
    {
        vox_u16 before = app.haptic[0].ticks_left;
        demo_haptics_tick(&app);
        if (before == 0U || app.haptic[0].ticks_left + 1U != before) {
            fprintf(stderr, "haptic self-test: no-device path failed\n");
            return 5;
        }
    }
    printf("DIGS haptic self-test passed off=%u low=%u normal=%u "
           "heavy=%u near=%u far=%u\n",
           (unsigned int)(low[0] + high[0]),
           (unsigned int)(low[1] + high[1]),
           (unsigned int)(low[2] + high[2]),
           (unsigned int)(low[3] + high[3]),
           (unsigned int)near_locality, (unsigned int)far_locality);
    return 0;
}

static int demo_audio_cadence_self_test(void)
{
    demo_app app;
    vox_audio_config config;
    vox_audio_event event;
    vox_i16 samples[512U * VOX_AUDIO_OUTPUT_CHANNELS];
    static const vox_u32 blocks[4] = {73U, 127U, 256U, 440U};
    vox_u32 expected = 0U;
    vox_u32 block;
    unsigned long energy = 0UL;
    memset(&app, 0, sizeof(app));
    vox_audio_config_init(&config, DEMO_AUDIO_RATE, 0xC0DEC0DEU);
    if (vox_audio_init_ex(&app.audio, &config) != VOX_OK) return 1;
    vox_audio_event_init(&event, VOX_AUDIO_PRESET_EXPLOSION);
    event.event_id = 1U;
    if (vox_audio_emit(&app.audio, &event) != VOX_OK) return 2;
    for (block = 0U; block < 4U; ++block) {
        vox_u32 sample;
        demo_audio_render_block(&app, samples, blocks[block]);
        expected += blocks[block];
        if (vox_audio_sample_clock(&app.audio) != expected) {
            fprintf(stderr, "audio cadence self-test: clock drift\n");
            return 3;
        }
        for (sample = 0U;
             sample < blocks[block] * VOX_AUDIO_OUTPUT_CHANNELS; ++sample) {
            long value = samples[sample];
            if (value < 0L) value = -value;
            energy += (unsigned long)value;
        }
    }
    if (energy == 0UL) {
        fprintf(stderr, "audio cadence self-test: silent callback stream\n");
        return 4;
    }
    printf("DIGS audio cadence self-test passed frames=%lu energy=%lu\n",
           (unsigned long)expected, energy);
    return 0;
}

static int demo_bark_self_test(void)
{
    demo_app app;
    vox_u8 tokens[VOX_AUDIO_SPEECH_TOKEN_CAPACITY];
    vox_u16 count = 0U;
    int context;
    int phrase;
    memset(&app, 0, sizeof(app));
    memset(&demo_match, 0, sizeof(demo_match));
    vox_world_init(&demo_match.world);
    demo_match.rules.seed = 0xB4A4C123U;
    demo_match.rules.player_count = 2U;
    demo_match.lava_surface_y = VOX_WORLD_HEIGHT;
    demo_match.alive[0] = 1U;
    demo_match.alive[1] = 1U;
    demo_match.health[0] = VOX_DIGS_MAX_HEALTH;
    demo_match.health[1] = VOX_DIGS_MAX_HEALTH;
    demo_match.selected_weapon[0] = VOX_DIGS_TOOL_PICK;
    demo_match.selected_weapon[1] = VOX_DIGS_TOOL_NAIL_GUN;
    strcpy(app.player_names[0], "RIVET");
    strcpy(app.player_names[1], "CINDER");
    for (context = 0; context < (int)DIGS_VOICE_COUNT; ++context) {
        for (phrase = 1; phrase < (int)VOX_DIGS_STIMULUS_COUNT; ++phrase) {
            digs_line_pool pool = digs_lines_pool((vox_u16)context,
                (vox_u16)VOX_DIGS_TONE_NEUTRAL, (vox_u16)phrase);
            if (pool.count == 0U ||
                digs_lines_text(pool.first)[0] == '\0') {
                fprintf(stderr,
                    "bark self-test: no line for voice=%d stimulus=%d\n",
                    context, phrase);
                return 1;
            }
        }
    }
    if (!demo_g2p_word("RIVETING", tokens, &count) || count == 0U) {
        fprintf(stderr, "bark self-test: unknown-word G2P failed\n");
        return 2;
    }
    count = 0U;
    demo_spell_word("7", tokens, &count);
    if (count == 0U) {
        fprintf(stderr, "bark self-test: spelling fallback failed\n");
        return 3;
    }
    /*
     * Cooldowns and repeat suppression moved into the simulation, where they
     * are hashed and covered by the headless tests.  What is left for the
     * port to prove is that it renders what it is handed: the name goes in,
     * and nothing is assembled here any more.
     */
    demo_match.tick = 10U;
    strcpy(app.player_names[1], "CINDER");
    {
        digs_line_pool pool = digs_lines_pool((vox_u16)DIGS_VOICE_RIVET,
            (vox_u16)VOX_DIGS_TONE_NEUTRAL,
            (vox_u16)VOX_DIGS_STIMULUS_FIRST_MEETING);
        vox_u16 id;
        int substituted = 0;
        if (pool.count == 0U) {
            fprintf(stderr, "bark self-test: no first-meeting lines\n");
            return 4;
        }
        for (id = 0U; id < pool.count; ++id) {
            const char *raw = digs_lines_text((vox_u16)(pool.first + id));
            if (strstr(raw, "%T") == 0) {
                continue;
            }
            app.bubbles[0].text[0] = '\0';
            demo_speak_line(&app, 0, 1, (vox_u16)(pool.first + id), 1);
            if (strstr(app.bubbles[0].text, "%T") != 0) {
                fprintf(stderr, "bark self-test: %%T was not substituted\n");
                return 5;
            }
            if (strstr(app.bubbles[0].text, "CINDER") == 0) {
                fprintf(stderr, "bark self-test: name did not reach the "
                                "bubble\n");
                return 6;
            }
            substituted = 1;
            break;
        }
        if (!substituted) {
            fprintf(stderr, "bark self-test: no line carried a name slot\n");
            return 7;
        }
    }
    printf("DIGS bark self-test passed lines=%u g2p=%u\n",
           (unsigned int)digs_lines_total(), (unsigned int)count);
    return 0;
}

static int demo_write_settings_fixture(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    int written;
    if (file == 0) return 0;
    written = fputs(text, file) != EOF;
    if (fclose(file) != 0) written = 0;
    return written;
}

static int demo_settings_first_line_equals(const char *path,
                                           const char *expected)
{
    FILE *file = fopen(path, "r");
    char line[64];
    int matched;
    if (file == 0) return 0;
    matched = fgets(line, (int)sizeof(line), file) != 0 &&
              strcmp(line, expected) == 0;
    if (fclose(file) != 0) return 0;
    return matched;
}

static void demo_settings_test_defaults(demo_app *app)
{
    memset(app, 0, sizeof(*app));
    app->settings_writable = 1;
    app->local_players = 1;
    app->bots = 1;
    demo_options_defaults(app);
    demo_match_settings_defaults(app);
    demo_input_defaults(app);
    demo_bindings_default(app);
}

static int demo_settings_self_test(const char *path)
{
    demo_app app;
    char temporary[1032];
    char backup[1032];
    int status = 0;

    if (path == 0 || path[0] == '\0' || strlen(path) + 5U >= 1024U) {
        fprintf(stderr, "settings self-test requires a short path\n");
        return 1;
    }
    sprintf(temporary, "%s.tmp", path);
    sprintf(backup, "%s.bak", path);
    (void)remove(path);
    (void)remove(temporary);
    (void)remove(backup);
    demo_settings_override = path;

    demo_settings_test_defaults(&app);
    if (!demo_write_settings_fixture(path,
            "DIGS_SETTINGS=99\nP1_KEY_LEFT=99999\nMASTER_VOLUME=0\n") ||
        demo_load_input_settings(&app) != 0 ||
        app.settings_writable != 0 ||
        app.bindings.keyboard_left[0] != SDL_SCANCODE_A ||
        app.options.master_volume != 8 ||
        demo_save_input_settings(&app) != 0) {
        fprintf(stderr, "settings self-test future-schema guard failed\n");
        status = 2;
        goto done;
    }
    if (!demo_settings_first_line_equals(path, "DIGS_SETTINGS=99\n")) {
        fprintf(stderr, "settings self-test overwrote a future schema\n");
        status = 3;
        goto done;
    }

    demo_settings_test_defaults(&app);
    if (!demo_write_settings_fixture(path,
            "DIGS_SETTINGS=3\nP1_KEY_LEFT=99999\nMASTER_VOLUME=12\n"
            "HAPTIC_LEVEL=99\nP1_ROPE_MODE=99\n") ||
        demo_load_input_settings(&app) != 1 ||
        app.bindings.keyboard_left[0] != SDL_SCANCODE_A ||
        app.options.master_volume != 8 || app.options.haptic_level != 2 ||
        app.player_input[0].rope_mode != DEMO_ROPE_HOLD) {
        fprintf(stderr, "settings self-test invalid-value fallback failed\n");
        status = 4;
        goto done;
    }

    demo_settings_test_defaults(&app);
    if (!demo_write_settings_fixture(path,
            "DIGS_INPUT_SETTINGS=1\nP1_MODE=1\nP1_SENSITIVITY=2\n") ||
        demo_load_input_settings(&app) != 1 ||
        app.player_input[0].preference != DEMO_INPUT_KEYBOARD ||
        app.player_input[0].sensitivity != 2 ||
        demo_save_input_settings(&app) != 1) {
        fprintf(stderr, "settings self-test schema-one migration failed\n");
        status = 5;
        goto done;
    }
    if (!demo_settings_first_line_equals(path, "DIGS_SETTINGS=5\n")) {
        fprintf(stderr, "settings self-test did not persist schema five\n");
        status = 6;
        goto done;
    }

    /*
     * Schema four stored the time limit as a number of minutes, and only 2
     * or 3 were reachable.  Five stores an index into a longer list, so an
     * old file has to be read as minutes and mapped -- otherwise somebody
     * who had picked 3:00 comes back to 2:00 with no explanation.
     */
    demo_settings_test_defaults(&app);
    if (!demo_write_settings_fixture(path,
            "DIGS_SETTINGS=4\nMATCH_MINUTES=3\n") ||
        demo_load_input_settings(&app) != 1 ||
        demo_time_limits[app.time_limit_index] != 3) {
        fprintf(stderr, "settings self-test schema-four time migration "
                        "failed\n");
        status = 7;
        goto done;
    }
    demo_settings_test_defaults(&app);
    if (!demo_write_settings_fixture(path,
            "DIGS_SETTINGS=4\nMATCH_MINUTES=2\n") ||
        demo_load_input_settings(&app) != 1 ||
        demo_time_limits[app.time_limit_index] != 2) {
        fprintf(stderr, "settings self-test schema-four time migration "
                        "failed\n");
        status = 8;
        goto done;
    }
    /* And an out-of-range index from a hand-edited file must not stick. */
    demo_settings_test_defaults(&app);
    if (!demo_write_settings_fixture(path,
            "DIGS_SETTINGS=5\nTIME_LIMIT_INDEX=99\nLAVA_INDEX=99\n") ||
        demo_load_input_settings(&app) != 1 ||
        app.time_limit_index < 0 || app.time_limit_index > 5 ||
        app.lava_index < 0 || app.lava_index > 4) {
        fprintf(stderr, "settings self-test time clamp failed\n");
        status = 9;
        goto done;
    }

    demo_settings_test_defaults(&app);
    if (!demo_write_settings_fixture(path,
            "DIGS_SETTINGS=2\nFRAME_CAP_INDEX=0\nGI_QUALITY=2\n"
            "FLASH_MODE=1\nGORE_LEVEL=1\nFX_PROFILE=2\n") ||
        demo_load_input_settings(&app) != 1 ||
        app.options.frame_cap_index != 0 ||
        app.options.laptop_mode != 0 || app.options.dummy_mode != 0 ||
        app.options.haptic_level != 2 ||
        demo_save_input_settings(&app) != 1 ||
        !demo_settings_first_line_equals(path, "DIGS_SETTINGS=5\n")) {
        fprintf(stderr, "settings self-test schema-two migration failed\n");
        status = 7;
        goto done;
    }

    demo_settings_test_defaults(&app);
    if (!demo_write_settings_fixture(path,
            "DIGS_SETTINGS=3\nFRAME_CAP_INDEX=0\nGI_QUALITY=0\n"
            "DEBUG=1\nFULLSCREEN=1\nFLASH_MODE=0\nGORE_LEVEL=1\n"
            "CAMERA_SHAKE=0\nDAMAGE_NUMBERS=0\n"
            "DAMAGE_NUMBER_SIZE=1\nDAMAGE_NUMBER_COLOR=1\n"
            "FX_PROFILE=2\nMASTER_VOLUME=3\nLAPTOP_MODE=1\n"
            "DUMMY_MODE=1\nHAPTIC_LEVEL=3\nP1_ROPE_MODE=1\n") ||
        demo_load_input_settings(&app) != 1 ||
        app.options.frame_cap_index != 0 || app.options.gi_quality != 0 ||
        app.options.debug != 1 || app.options.fullscreen != 1 ||
        app.options.flash_mode != 0 || app.options.gore_level != 1 ||
        app.options.camera_shake != 0 || app.options.damage_numbers != 0 ||
        app.options.damage_number_size != 1 ||
        app.options.damage_number_color != 1 ||
        app.options.fx_profile != 2 || app.options.master_volume != 3 ||
        app.options.laptop_mode != 1 || app.options.dummy_mode != 1 ||
        app.options.haptic_level != 3 ||
        app.player_input[0].rope_mode != DEMO_ROPE_TOGGLE ||
        demo_save_input_settings(&app) != 1) {
        fprintf(stderr, "settings self-test schema-three round trip failed\n");
        status = 8;
        goto done;
    }

done:
    demo_settings_override = 0;
    (void)remove(path);
    (void)remove(temporary);
    (void)remove(backup);
    if (status == 0) {
        printf("DIGS settings migration self-test passed\n");
    }
    return status;
}

static int demo_camera_self_test(void)
{
    demo_app app;
    const vox_cell *cell;
    vox_u32 mouse_world_x;
    vox_u32 mouse_world_y;
    vox_u32 pixel;
    vox_u32 lava_y;
    vox_u32 false_lava_pixels;
    int player_screen_x;
    int player_screen_y;
    int lava_screen_x;
    int lava_screen_y;
    int zoom;
    int edge;
    demo_prepare_targets();
    lava_y = VOX_WORLD_HEIGHT > 8U ? VOX_WORLD_HEIGHT - 8U :
                                    VOX_WORLD_HEIGHT - 1U;
    memset(&demo_match, 0, sizeof(demo_match));
    /* A valid basin is lethal and visible before the timed rise begins. */
    demo_match.lava_level_q16 = 0U;
    demo_match.lava_surface_y = (vox_u16)lava_y;
    vox_world_init(&demo_match.world);
    demo_render_overlay_begin();
    demo_voxelize_lava_horizon();
    cell = vox_world_cell(&demo_match.world, 0U, lava_y,
                          VOX_WORLD_DEPTH - 1U);
    if (cell == 0 || cell->material != VOX_MAT_LAVA) {
        fprintf(stderr, "camera self-test: lava horizon is not authoritative\n");
        return 1;
    }
    demo_render_overlay_restore();
    memset(demo_scene_pixels, 31, sizeof(demo_scene_pixels));
    for (zoom = DEMO_CAMERA_ZOOM_MIN;
         zoom <= DEMO_CAMERA_ZOOM_MAX; ++zoom) {
        for (edge = 0; edge < 2; ++edge) {
            double half_width;
            double half_height;
            double maximum_y;
            double presented_x;
            double presented_y;
            memset(&app, 0, sizeof(app));
            memset(demo_pixels, 31, sizeof(demo_pixels));
            app.camera_zoom = zoom;
            app.camera_scale = (double)zoom;
            app.camera_world_x = edge == 0 ? -100.0 :
                                 (double)VOX_WORLD_WIDTH + 100.0;
            app.camera_world_y = edge == 0 ? -100.0 :
                                 (double)VOX_WORLD_HEIGHT + 100.0;
            app.camera_shake_x = edge == 0 ? -20.0 : 20.0;
            app.camera_shake_y = edge == 0 ? -20.0 : 20.0;
            app.frame_seconds = 1.0 / 60.0;
            app.mouse_x = (int)DEMO_WIDTH / 2;
            app.mouse_y = (int)DEMO_HEIGHT / 2;
            demo_apply_player_camera(&app, demo_scene_pixels,
                                     (int)VOX_WORLD_WIDTH,
                                     (int)VOX_WORLD_HEIGHT);
            half_width = (double)VOX_WORLD_WIDTH /
                         (app.camera_scale * 2.0);
            half_height = (double)VOX_WORLD_HEIGHT /
                          (app.camera_scale * 2.0);
            maximum_y = (double)VOX_WORLD_HEIGHT - half_height;
            presented_x = app.camera_world_x + app.camera_shake_x;
            presented_y = app.camera_world_y + app.camera_shake_y;
            if (presented_x < half_width - 0.001 ||
                presented_x > (double)VOX_WORLD_WIDTH - half_width + 0.001 ||
                presented_y < half_height - 0.001 ||
                presented_y > maximum_y + 0.001) {
                fprintf(stderr, "camera self-test: zoom %d escaped world\n",
                        zoom);
                return 2;
            }
            for (pixel = 0U; pixel < DEMO_WIDTH * DEMO_HEIGHT; ++pixel) {
                const vox_u8 *sample = &demo_pixels[
                    pixel * VOX_SOFTWARE_RGB_BYTES];
                if (sample[0] == 0U && sample[1] == 0U && sample[2] == 0U) {
                    fprintf(stderr,
                            "camera self-test: zoom %d exposed black\n",
                            zoom);
                    return 3;
                }
            }
            demo_mouse_world(&app, &mouse_world_x, &mouse_world_y);
            if (mouse_world_x != (vox_u32)(long)presented_x ||
                mouse_world_y != (vox_u32)(long)presented_y) {
                fprintf(stderr,
                        "camera self-test: center mouse transform drift\n");
                return 4;
            }
        }
    }
    memset(&app, 0, sizeof(app));
    app.local_players = 1;
    app.camera_zoom = DEMO_CAMERA_ZOOM_MAX;
    app.camera_scale = (double)DEMO_CAMERA_ZOOM_MAX;
    app.camera_world_x = (double)VOX_WORLD_WIDTH / 2.0;
    app.camera_world_y = (double)VOX_WORLD_HEIGHT / 2.0;
    app.frame_seconds = 1.0 / 60.0;
    app.render_alpha = 1.0;
    demo_match.alive[0] = 1U;
    demo_match.players[0].position_x.value_q16 =
        (vox_i32)(VOX_WORLD_WIDTH / 2U) << 16;
    demo_match.players[0].position_y.value_q16 =
        (vox_i32)(VOX_WORLD_HEIGHT / 2U) << 16;
    app.previous_player_x[0] =
        demo_match.players[0].position_x.value_q16;
    app.previous_player_y[0] =
        demo_match.players[0].position_y.value_q16;
    for (pixel = 0U; pixel < 360U; ++pixel) {
        memset(demo_scene_pixels, 31, sizeof(demo_scene_pixels));
        demo_apply_player_camera(&app, demo_scene_pixels,
                                 (int)VOX_WORLD_WIDTH,
                                 (int)VOX_WORLD_HEIGHT);
    }
    demo_world_to_screen(&app,
        demo_match.players[0].position_x.value_q16,
        demo_match.players[0].position_y.value_q16,
        &player_screen_x, &player_screen_y);
    demo_world_to_screen(&app,
        (vox_i32)(VOX_WORLD_WIDTH / 2U) << 16,
        (vox_i32)lava_y << 16, &lava_screen_x, &lava_screen_y);
    if (player_screen_x < 0 || player_screen_x >= (int)DEMO_WIDTH ||
        player_screen_y < (int)DEMO_CAMERA_SAFE_TOP_PIXELS ||
        player_screen_y >= (int)DEMO_HEIGHT ||
        app.camera_scale < 3.5 ||
        lava_screen_y < (int)DEMO_HEIGHT) {
        fprintf(stderr,
                "camera self-test: close zoom lost (%d,%d) (%d,%d) %.2f\n",
                player_screen_x, player_screen_y,
                lava_screen_x, lava_screen_y, app.camera_scale);
        return 5;
    }
    false_lava_pixels = 0U;
    for (pixel = 0U; pixel < DEMO_WIDTH * DEMO_HEIGHT; ++pixel) {
        const vox_u8 *sample = &demo_pixels[
            pixel * VOX_SOFTWARE_RGB_BYTES];
        if (sample[0] == 255U && sample[1] == 98U && sample[2] == 8U) {
            ++false_lava_pixels;
        }
    }
    if (false_lava_pixels != 0U) {
        fprintf(stderr,
                "camera self-test: false lava outside world view (%lu)\n",
                (unsigned long)false_lava_pixels);
        return 6;
    }
    app.camera_zoom = DEMO_CAMERA_ZOOM_MIN;
    app.camera_scale = DEMO_CAMERA_MIN_SCALE;
    app.camera_world_x = (double)VOX_WORLD_WIDTH / 2.0;
    app.camera_world_y = (double)VOX_WORLD_HEIGHT / 2.0;
    app.camera_shake_x = 0.0;
    app.camera_shake_y = 0.0;
    demo_constrain_camera(&app);
    demo_world_to_screen(&app,
        demo_match.players[0].position_x.value_q16,
        demo_match.players[0].position_y.value_q16,
        &player_screen_x, &player_screen_y);
    demo_world_to_screen(&app,
        (vox_i32)(VOX_WORLD_WIDTH / 2U) << 16,
        (vox_i32)lava_y << 16, &lava_screen_x, &lava_screen_y);
    if (player_screen_x < 0 || player_screen_x >= (int)DEMO_WIDTH ||
        player_screen_y < 0 || player_screen_y >= (int)DEMO_HEIGHT ||
        lava_screen_x < 0 || lava_screen_x >= (int)DEMO_WIDTH ||
        lava_screen_y < 0 || lava_screen_y >= (int)DEMO_HEIGHT) {
        fprintf(stderr,
                "camera self-test: overview lost (%d,%d) (%d,%d)\n",
                player_screen_x, player_screen_y,
                lava_screen_x, lava_screen_y);
        return 7;
    }
    {
        vox_u8 exterior[VOX_SOFTWARE_RGB_BYTES];
        demo_camera_exterior_pixel(exterior, (int)VOX_WORLD_HEIGHT + 8,
                                   (int)VOX_WORLD_HEIGHT);
        if (exterior[0] == 0U && exterior[1] == 0U && exterior[2] == 0U) {
            fprintf(stderr, "camera self-test: exterior fallback is black\n");
            return 8;
        }
    }
    {
        int first_x;
        int first_y;
        int second_x;
        int second_y;
        memset(&app, 0, sizeof(app));
        app.local_players = 2;
        app.camera_zoom = DEMO_CAMERA_ZOOM_MAX;
        app.camera_scale = (double)DEMO_CAMERA_ZOOM_MAX;
        app.camera_world_x = (double)VOX_WORLD_WIDTH / 2.0;
        app.camera_world_y = (double)VOX_WORLD_HEIGHT / 2.0;
        app.frame_seconds = 1.0 / 60.0;
        app.render_alpha = 1.0;
        demo_match.rules.player_count = 2U;
        demo_match.alive[0] = 1U;
        demo_match.alive[1] = 1U;
        demo_match.players[0].position_x.value_q16 = 80L << 16;
        demo_match.players[1].position_x.value_q16 =
            ((vox_i32)VOX_WORLD_WIDTH - 80L) << 16;
        demo_match.players[0].position_y.value_q16 =
            (vox_i32)(VOX_WORLD_HEIGHT / 2U) << 16;
        demo_match.players[1].position_y.value_q16 =
            (vox_i32)(VOX_WORLD_HEIGHT / 2U) << 16;
        app.previous_player_x[0] =
            demo_match.players[0].position_x.value_q16;
        app.previous_player_y[0] =
            demo_match.players[0].position_y.value_q16;
        app.previous_player_x[1] =
            demo_match.players[1].position_x.value_q16;
        app.previous_player_y[1] =
            demo_match.players[1].position_y.value_q16;
        for (pixel = 0U; pixel < 360U; ++pixel) {
            demo_update_player_camera(&app);
        }
        demo_world_to_screen(&app,
            demo_match.players[0].position_x.value_q16,
            demo_match.players[0].position_y.value_q16,
            &first_x, &first_y);
        demo_world_to_screen(&app,
            demo_match.players[1].position_x.value_q16,
            demo_match.players[1].position_y.value_q16,
            &second_x, &second_y);
        if (first_x < 8 || first_x >= (int)DEMO_WIDTH - 8 ||
            second_x < 8 || second_x >= (int)DEMO_WIDTH - 8 ||
            first_y < 58 || first_y >= (int)DEMO_HEIGHT - 18 ||
            second_y < 58 || second_y >= (int)DEMO_HEIGHT - 18) {
            fprintf(stderr,
                    "camera self-test: shared union escaped safe frame "
                    "(%d,%d) (%d,%d)\n",
                    first_x, first_y, second_x, second_y);
            return 9;
        }
        memset(&app, 0, sizeof(app));
        app.local_players = 1;
        app.camera_zoom = DEMO_CAMERA_ZOOM_MAX;
        app.camera_scale = (double)DEMO_CAMERA_ZOOM_MAX;
        app.camera_world_x = 80.0;
        app.camera_world_y = (double)VOX_WORLD_HEIGHT / 2.0;
        app.frame_seconds = 1.0 / 60.0;
        app.render_alpha = 1.0;
        app.previous_player_x[0] =
            demo_match.players[0].position_x.value_q16;
        app.previous_player_y[0] =
            demo_match.players[0].position_y.value_q16;
        for (pixel = 0U; pixel < 360U; ++pixel) {
            demo_update_player_camera(&app);
        }
        demo_world_to_screen(&app,
            demo_match.players[0].position_x.value_q16,
            demo_match.players[0].position_y.value_q16,
            &first_x, &first_y);
        demo_world_to_screen(&app,
            demo_match.players[1].position_x.value_q16,
            demo_match.players[1].position_y.value_q16,
            &second_x, &second_y);
        if (first_x < 140 || first_x > 180 || app.camera_scale < 3.5 ||
            (second_x >= 0 && second_x < (int)DEMO_WIDTH)) {
            fprintf(stderr,
                    "camera self-test: one-human bot changed framing "
                    "human=%d,%d bot=%d,%d scale=%.2f\n",
                    first_x, first_y, second_x, second_y,
                    app.camera_scale);
            return 10;
        }
    }
    {
        vox_software_view view;
        vox_world *world_snapshot;
        vox_u32 world_hash;
        vox_u32 match_hash;
        vox_result render_status;
        int transitions = 0;
        int x;
        app.camera_zoom = DEMO_CAMERA_ZOOM_MAX;
        app.camera_scale = (double)DEMO_CAMERA_ZOOM_MAX;
        app.camera_world_x = (double)VOX_WORLD_WIDTH / 2.0;
        app.camera_world_y = (double)VOX_WORLD_HEIGHT / 2.0;
        app.camera_shake_x = 0.0;
        app.camera_shake_y = 0.0;
        for (x = 0; x < (int)VOX_WORLD_WIDTH; ++x) {
            (void)vox_world_set(&demo_match.world, (vox_u32)x,
                VOX_WORLD_HEIGHT / 2U, VOX_WORLD_DEPTH - 1U,
                (x & 1) != 0 ? VOX_MAT_STONE : VOX_MAT_COAL,
                20L << 16);
        }
        world_snapshot = (vox_world *)malloc(sizeof(*world_snapshot));
        if (world_snapshot == 0) {
            fprintf(stderr, "camera self-test: snapshot allocation failed\n");
            return 11;
        }
        world_hash = vox_world_hash(&demo_match.world);
        match_hash = vox_digs_hash(&demo_match);
        memcpy(world_snapshot, &demo_match.world, sizeof(*world_snapshot));
        demo_camera_view(&app, &view);
        demo_render_config.gi_quality = VOX_GI_COMPATIBILITY;
        demo_render_overlay_begin();
        demo_build_render_world(&app);
        render_status = vox_software_render_view_ex(&demo_match.world,
            &demo_target, &demo_render_config, &view);
        demo_render_overlay_restore();
        if (render_status != VOX_OK ||
            memcmp(world_snapshot, &demo_match.world,
                   sizeof(*world_snapshot)) != 0 ||
            vox_world_hash(&demo_match.world) != world_hash ||
            vox_digs_hash(&demo_match) != match_hash) {
            fprintf(stderr,
                "camera self-test: view render changed canonical match "
                "status=%d world=%08lx/%08lx match=%08lx/%08lx\n",
                (int)render_status,
                (unsigned long)vox_world_hash(&demo_match.world),
                (unsigned long)world_hash,
                (unsigned long)vox_digs_hash(&demo_match),
                (unsigned long)match_hash);
            free(world_snapshot);
            return 12;
        }
        free(world_snapshot);
        for (x = 1; x < (int)DEMO_WIDTH; ++x) {
            vox_u8 before = demo_pixels[((int)DEMO_HEIGHT / 2 *
                (int)DEMO_WIDTH + x - 1) * VOX_SOFTWARE_RGB_BYTES];
            vox_u8 after = demo_pixels[((int)DEMO_HEIGHT / 2 *
                (int)DEMO_WIDTH + x) * VOX_SOFTWARE_RGB_BYTES];
            if (before != after) ++transitions;
        }
        if (transitions < 50) {
            fprintf(stderr, "camera self-test: native detail collapsed\n");
            return 13;
        }
    }
    printf("DIGS camera self-test passed zoom=%d overview-player=%d,%d "
           "overview-lava=%d,%d false-lava=%lu\n",
           DEMO_CAMERA_ZOOM_MAX, player_screen_x, player_screen_y,
           lava_screen_x, lava_screen_y,
           (unsigned long)false_lava_pixels);
    return 0;
}

static void demo_wait_for_frame(Uint64 frequency, int frame_cap,
                                int *last_frame_cap, Uint64 *deadline)
{
    Uint64 period;
    Uint64 now;
    if (frame_cap <= 0) {
        *last_frame_cap = frame_cap;
        *deadline = 0U;
        return;
    }
    period = frequency / (Uint64)frame_cap;
    if (period == 0U) {
        period = 1U;
    }
    now = SDL_GetPerformanceCounter();
    if (*last_frame_cap != frame_cap || *deadline == 0U) {
        *deadline = now + period;
        *last_frame_cap = frame_cap;
    } else {
        *deadline += period;
        if (*deadline <= now) {
            if (now - *deadline > period * 3U) {
                *deadline = now;
            }
            return;
        }
    }
    while (now < *deadline) {
        Uint64 remaining = *deadline - now;
        Uint64 delay_ms = (remaining * 1000U + frequency - 1U) /
                          frequency;
        if (delay_ms == 0U) {
            delay_ms = 1U;
        }
        SDL_Delay((Uint32)delay_ms);
        now = SDL_GetPerformanceCounter();
    }
}

/*
 * Prove the chronicle survives a round trip, repairs a mangled file, and
 * records that it was mangled.  This is the one part of the save layer that
 * a player can break from outside the game, so it is the part that has to be
 * demonstrably safe rather than merely careful.
 */
static int digs_chronicle_self_test(const char *path)
{
    static digs_chronicle written;
    static digs_chronicle read;
    char backup[1032];
    FILE *file;
    vox_u16 pair;

    (void)remove(path);
    if (strlen(path) + 5U >= sizeof(backup)) return 1;
    sprintf(backup, "%s.bak", path);
    (void)remove(backup);

    digs_chronicle_reset(&written);
    /* Nothing there yet: a first launch, and not a tampered one. */
    if (digs_chronicle_load(&read, path) != 0) return 2;
    if (read.tampered) return 3;

    pair = vox_digs_regard_index(VOX_DIGS_IDENTITY_PLAYER,
                                 VOX_DIGS_IDENTITY_RIVET);
    if (pair >= VOX_DIGS_MAX_PAIRS) return 4;
    written.memory.regard[pair].tone = (vox_u16)VOX_DIGS_TONE_FEUD;
    written.memory.regard[pair].valence = -742;
    written.memory.regard[pair].matches_met = 9U;
    written.memory.regard[pair].betrayals = 2U;
    written.memory.identities[VOX_DIGS_IDENTITY_RIVET].traits.aggression =
        199U;
    written.memory.identities[VOX_DIGS_IDENTITY_RIVET].matches_played = 12U;
    written.memory.launch_counter = 37U;
    written.matches_recorded = 12U;
    digs_chronicle_log(&written, VOX_DIGS_IDENTITY_RIVET,
                       VOX_DIGS_IDENTITY_PLAYER, 1234U);
    digs_chronicle_log(&written, VOX_DIGS_IDENTITY_PLAYER,
                       VOX_DIGS_IDENTITY_RIVET, 77U);
    written.inbox[0].from = (vox_u16)VOX_DIGS_IDENTITY_RIVET;
    written.inbox[0].unread = 1U;
    written.inbox[0].launch = 36U;
    written.inbox[0].lines[0] = 501U;
    written.inbox[0].lines[1] = 502U;
    written.inbox[0].line_count = 2U;
    written.inbox_count = 1U;

    if (!digs_chronicle_save(&written, path)) return 5;
    if (digs_chronicle_load(&read, path) != 1) return 6;
    if (read.tampered) return 7;
    if (read.memory.regard[pair].tone != VOX_DIGS_TONE_FEUD) return 8;
    if (read.memory.regard[pair].valence != -742) return 9;
    if (read.memory.regard[pair].betrayals != 2U) return 10;
    if (read.memory.identities[VOX_DIGS_IDENTITY_RIVET].traits.aggression !=
        199U) {
        return 11;
    }
    if (read.memory.launch_counter != 37U) return 12;
    if (read.log_count != 2U) return 13;
    if (digs_chronicle_log_at(&read, 0U)->line != 1234U) return 14;
    if (digs_chronicle_log_at(&read, 1U)->line != 77U) return 15;
    if (read.inbox_count != 1U || read.inbox[0].line_count != 2U) return 16;
    if (read.inbox[0].lines[1] != 502U) return 17;
    if (digs_chronicle_unread(&read) != 1U) return 18;

    /*
     * Keep a good copy, then edit the live file by hand the way a player
     * would: change a value in place and leave the checksum alone.  Appending
     * after SUM proves nothing -- the reader stops there, so appended junk is
     * inert rather than tampering.
     */
    if (!digs_chronicle_save(&read, backup)) return 19;
    {
        static char blob[65536];
        size_t got;
        char *hit;
        file = fopen(path, "rb");
        if (file == 0) return 20;
        got = fread(blob, 1U, sizeof(blob) - 1U, file);
        (void)fclose(file);
        blob[got] = '\0';
        hit = strstr(blob, "-742");
        if (hit == 0) return 27;
        hit[1] = '1';           /* -742 becomes -142: same length, new value */
        hit[2] = '4';
        hit[3] = '2';
        file = fopen(path, "wb");
        if (file == 0) return 28;
        (void)fwrite(blob, 1U, got, file);
        (void)fclose(file);
    }
    if (digs_chronicle_load(&read, path) != 1) return 21;
    if (!read.tampered) return 22;          /* it must notice */
    if (read.memory.regard[pair].valence != -742) return 23;  /* and repair */

    /* With no good copy to fall back on, it starts over and says so. */
    (void)remove(backup);
    if (digs_chronicle_load(&read, path) != 0) return 24;
    if (!read.tampered) return 25;
    if (read.memory.regard[pair].valence != 0) return 26;

    /*
     * Messages between visits.  Nobody writes to a stranger, so a chronicle
     * with no history in it must stay empty however long the gap; once the
     * miners have actually played the player, somebody should eventually
     * write, and what they write has to be real authored lines.
     */
    {
        static digs_chronicle box;
        vox_u16 attempt;
        vox_u16 wrote = 0U;
        digs_chronicle_reset(&box);
        for (attempt = 0U; attempt < 40U; ++attempt) {
            digs_chronicle_open(&box, 1000000UL + attempt * 90000UL);
        }
        if (box.inbox_count != 0U) return 29;   /* strangers stay silent */
        if (box.memory.launch_counter != 40U) return 30;

        digs_chronicle_reset(&box);
        for (pair = 0U; pair < VOX_DIGS_MAX_PAIRS; ++pair) {
            box.memory.regard[pair].matches_met = 4U;
        }
        pair = vox_digs_regard_index(VOX_DIGS_IDENTITY_PLAYER,
                                     VOX_DIGS_IDENTITY_FLAMEY);
        box.memory.regard[pair].tone = (vox_u16)VOX_DIGS_TONE_FEUD;
        box.memory.regard[pair].betrayals = 1U;
        for (attempt = 0U; attempt < 12U && wrote == 0U; ++attempt) {
            digs_chronicle_open(&box, 2000000UL + attempt * 90000UL);
            wrote = box.inbox_count;
        }
        if (wrote == 0U) return 31;             /* somebody must write */
        if (box.inbox[0].line_count == 0U) return 32;
        if (box.inbox[0].from >= VOX_DIGS_IDENTITY_PLAYER) return 33;
        for (attempt = 0U; attempt < box.inbox[0].line_count; ++attempt) {
            if (digs_lines_text(box.inbox[0].lines[attempt])[0] == '\0') {
                return 34;
            }
        }
        if (digs_chronicle_unread(&box) == 0U) return 35;
        if (!digs_chronicle_mark_read(&box, 0U)) return 36;
        if (digs_chronicle_mark_read(&box, 0U)) return 37;  /* only once */

        /* The ring must not overflow however long the player stays away. */
        for (attempt = 0U; attempt < 200U; ++attempt) {
            digs_chronicle_open(&box, 3000000UL + attempt * 90000UL);
        }
        if (box.inbox_count > DIGS_INBOX_CAPACITY) return 38;
        if (box.inbox_count == 0U) return 39;
    }

    (void)remove(path);
    printf("DIGS chronicle self-test passed inbox=%u log=%u\n",
           (unsigned int)DIGS_INBOX_CAPACITY,
           (unsigned int)DIGS_LOG_CAPACITY);
    return 0;
}

/*
 * Render one menu screen to a PPM without opening a window.
 *
 * Layout work on a 320x200 field is guesswork otherwise -- rows overlap, text
 * runs past a panel edge, and none of it shows up in a test.  This is the
 * cheapest way to actually look at what shipped.
 */
static int demo_screenshot(const char *screen_name, const char *path)
{
    static demo_app app;
    int screen = -1;
    memset(&app, 0, sizeof(app));
    app.running = 1;
    app.bots = 3;
    app.local_players = 1;
    app.map_style = VOX_DIGS_MAP_COAL_RIDGE;
    app.arsenal = DEMO_ARSENAL_FULL;
    app.seed = 0x564F5831U;
    app.time_limit_index = 2;
    app.lava_index = 0;
    app.inbox_open = -1;
    app.options.gi_quality = 1;
    strcpy(app.bot_names[0], "RIVET");
    strcpy(app.bot_names[1], "CINDER");
    strcpy(app.bot_names[2], "FLAMEY");
    strcpy(app.player_names[0], "MINER");
    demo_refresh_roster(&app);
    if (strcmp(screen_name, "title") == 0) screen = DEMO_TITLE;
    else if (strcmp(screen_name, "setup") == 0) screen = DEMO_SETUP;
    else if (strcmp(screen_name, "options") == 0) screen = DEMO_OPTIONS;
    else if (strcmp(screen_name, "options2") == 0) {
        screen = DEMO_OPTIONS;
        app.selection = 14;
    } else if (strcmp(screen_name, "inbox") == 0) screen = DEMO_INBOX;
    else if (strcmp(screen_name, "inbox-read") == 0) {
        screen = DEMO_INBOX;
        app.inbox_open = 0;
    } else if (strcmp(screen_name, "log") == 0) screen = DEMO_LOG;
    else if (strcmp(screen_name, "controls") == 0) screen = DEMO_CONTROLS;
    else if (strcmp(screen_name, "customize") == 0) screen = DEMO_CUSTOMIZE;
    if (screen < 0) {
        fprintf(stderr, "unknown screen: %s\n", screen_name);
        return 2;
    }
    /* Some plausible history, so the readers have something to show. */
    if (screen == DEMO_INBOX || screen == DEMO_LOG ||
        screen == DEMO_TITLE) {
        vox_u16 pair;
        digs_chronicle_reset(&demo_chronicle);
        for (pair = 0U; pair < VOX_DIGS_MAX_PAIRS; ++pair) {
            demo_chronicle.memory.regard[pair].matches_met = 5U;
        }
        pair = vox_digs_regard_index(VOX_DIGS_IDENTITY_PLAYER,
                                     VOX_DIGS_IDENTITY_FLAMEY);
        demo_chronicle.memory.regard[pair].tone = (vox_u16)VOX_DIGS_TONE_FEUD;
        demo_chronicle.memory.regard[pair].betrayals = 2U;
        pair = vox_digs_regard_index(VOX_DIGS_IDENTITY_PLAYER,
                                     VOX_DIGS_IDENTITY_RIVET);
        demo_chronicle.memory.regard[pair].tone =
            (vox_u16)VOX_DIGS_TONE_BONDED;
        digs_chronicle_open(&demo_chronicle, 1700000000UL);
        digs_chronicle_open(&demo_chronicle, 1700200000UL);
        for (pair = 0U; pair < 40U; ++pair) {
            digs_line_pool pool = digs_lines_pool(
                (vox_u16)(pair % DIGS_VOICE_COUNT),
                (vox_u16)VOX_DIGS_TONE_NEUTRAL,
                (vox_u16)(VOX_DIGS_STIMULUS_KILLED_THEM + (pair % 3U)));
            digs_chronicle_log(&demo_chronicle,
                               (vox_u16)(pair % VOX_DIGS_IDENTITY_COUNT),
                               (vox_u16)((pair + 1U) %
                                         VOX_DIGS_IDENTITY_COUNT),
                               (vox_u16)(pool.first + (pair % pool.count)));
        }
        demo_chronicle_ready = 1;
    }
    app.screen = (demo_screen)screen;
    demo_prepare_targets();
    demo_build_title_world();
    demo_render(&app);
    if (!demo_write_ppm(path)) {
        fprintf(stderr, "could not write %s\n", path);
        return 1;
    }
    printf("DIGS screenshot %s -> %s\n", screen_name, path);
    return 0;
}

/*
 * Every menu screen must draw something.  A screen that throws, or that
 * renders a flat panel because a helper returned early, looks exactly like a
 * screen that is fine until somebody opens it.
 */
static int demo_menu_self_test(void)
{
    static const char *screens[9] = {
        "title", "setup", "options", "options2", "inbox", "inbox-read",
        "log", "controls", "customize"
    };
    /*
     * Which screens are made of selectable rows.  The message reader and the
     * log are prose and a scroll view -- they have nothing to click, and
     * demanding rows of them would only teach the test to lie.
     */
    static const int rows_expected[9] = {1, 1, 1, 1, 1, 0, 0, 1, 1};
    const char *path = "/tmp/digs-menu-self-test.ppm";
    int i;
    for (i = 0; i < 9; ++i) {
        vox_u32 x;
        vox_u32 distinct = 0U;
        vox_u8 seen[8];
        int result = demo_screenshot(screens[i], path);
        if (result != 0) {
            fprintf(stderr, "menu self-test: %s did not render\n",
                    screens[i]);
            return 1;
        }
        for (x = 0U; x < 8U; ++x) {
            seen[x] = 0U;
        }
        /* A screen with fewer than three distinct colours drew nothing. */
        for (x = 0U; x < sizeof(demo_pixels); x += 3U) {
            vox_u8 bucket = (vox_u8)(demo_pixels[x] >> 5);
            if (bucket < 8U && !seen[bucket]) {
                seen[bucket] = 1U;
                distinct++;
            }
        }
        if (distinct < 3U) {
            fprintf(stderr, "menu self-test: %s looks blank (%lu tones)\n",
                    screens[i], (unsigned long)distinct);
            return 2;
        }
        /*
         * Every row a screen draws must be reachable with a pointer, and the
         * row under the pointer must be the row the keyboard would select.
         * A registry that drifts from the drawing is worse than no mouse at
         * all -- it clicks the wrong thing confidently.
         */
        if (rows_expected[i] && demo_menu_row_count == 0) {
            fprintf(stderr, "menu self-test: %s registered no rows\n",
                    screens[i]);
            return 3;
        }
        for (x = 0U; x < (vox_u32)demo_menu_row_count; ++x) {
            const demo_menu_row *row = &demo_menu_rows[x];
            const demo_menu_row *hit =
                demo_row_at(row->x + row->width / 2,
                            row->y + row->height / 2);
            if (hit == 0 || hit->index != row->index) {
                fprintf(stderr,
                        "menu self-test: %s row %lu is not hit-testable\n",
                        screens[i], (unsigned long)x);
                return 4;
            }
            if (row->x < 0 || row->y < 0 ||
                row->x + row->width > (int)DEMO_WIDTH ||
                row->y + row->height > (int)DEMO_HEIGHT) {
                fprintf(stderr, "menu self-test: %s row %lu is off screen\n",
                        screens[i], (unsigned long)x);
                return 5;
            }
        }
    }
    (void)remove(path);
    printf("DIGS menu self-test passed screens=9\n");
    return 0;
}

int main(int argc, char **argv)
{
    demo_app app;
    SDL_Event event;
    Uint64 frequency;
    Uint64 previous_counter;
    demo_fixed_step_clock fixed_step;
    double presentation_seconds = 0.0;
    Uint64 present_deadline = 0U;
    int last_frame_cap = -1;
    int controller_index;
    if (argc >= 2 && strcmp(argv[1], "--render-miner-icon-xpm") == 0) {
        const char *path = argc >= 3 ? argv[2] : "digs-miner.xpm";
        if (digs_miner_write_icon_xpm(path) != VOX_OK) {
            fprintf(stderr, "could not write canonical miner icon: %s\n",
                    path);
            return 1;
        }
        printf("DIGS canonical miner icon written: %s\n", path);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--input-self-test") == 0) {
        return demo_input_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--cap-self-test") == 0) {
        return demo_cap_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--haptic-self-test") == 0) {
        return demo_haptic_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--audio-cadence-self-test") == 0) {
        return demo_audio_cadence_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--bark-self-test") == 0) {
        return demo_bark_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--settings-self-test") == 0) {
        const char *path = argc >= 3 ? argv[2] :
                           "/tmp/digs-settings-self-test.cfg";
        return demo_settings_self_test(path);
    }
    if (argc >= 2 && strcmp(argv[1], "--chronicle-self-test") == 0) {
        const char *path = argc >= 3 ? argv[2] :
                           "/tmp/digs-chronicle-self-test.dat";
        return digs_chronicle_self_test(path);
    }
#ifdef DIGS_CHRONICLE_DEBUG
    /*
     * Debug builds only.  A released game has no way to erase this history:
     * it is meant to be the player's and unrepeatable, and a reset button
     * would quietly turn it into a save file.
     */
    if (argc >= 2 && strcmp(argv[1], "--chronicle-reset") == 0) {
        char path[1032];
        if (!demo_chronicle_path(path, (int)sizeof(path))) return 1;
        digs_chronicle_reset(&demo_chronicle);
        return digs_chronicle_save(&demo_chronicle, path) ? 0 : 1;
    }
    if (argc >= 2 && strcmp(argv[1], "--chronicle-dump") == 0) {
        char path[1032];
        vox_u16 i;
        if (!demo_chronicle_path(path, (int)sizeof(path))) return 1;
        (void)digs_chronicle_load(&demo_chronicle, path);
        printf("chronicle %s tampered=%u matches=%u unread=%u log=%u\n",
               path, (unsigned int)demo_chronicle.tampered,
               (unsigned int)demo_chronicle.matches_recorded,
               (unsigned int)digs_chronicle_unread(&demo_chronicle),
               (unsigned int)demo_chronicle.log_count);
        for (i = 0U; i < VOX_DIGS_IDENTITY_COUNT; ++i) {
            const vox_digs_identity_record *r =
                &demo_chronicle.memory.identities[i];
            printf("  identity %u  aggr=%3u pat=%3u caut=%3u grudge=%3u "
                   "soc=%3u  played=%u w=%u k=%u d=%u\n",
                   (unsigned int)i, (unsigned int)r->traits.aggression,
                   (unsigned int)r->traits.patience,
                   (unsigned int)r->traits.caution,
                   (unsigned int)r->traits.grudge,
                   (unsigned int)r->traits.sociability,
                   (unsigned int)r->matches_played, (unsigned int)r->wins,
                   (unsigned int)r->kills, (unsigned int)r->deaths);
        }
        for (i = 0U; i < VOX_DIGS_MAX_PAIRS; ++i) {
            const vox_digs_regard *g = &demo_chronicle.memory.regard[i];
            printf("  regard %u  tone=%-8s valence=%5d met=%u truces=%u "
                   "betrayals=%u\n", (unsigned int)i,
                   vox_digs_tone_name(g->tone), (int)g->valence,
                   (unsigned int)g->matches_met, (unsigned int)g->truces,
                   (unsigned int)g->betrayals);
        }
        return 0;
    }
#endif
    if (argc >= 2 && strcmp(argv[1], "--camera-self-test") == 0) {
        return demo_camera_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--fixed-step-self-test") == 0) {
        return demo_fixed_step_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--menu-self-test") == 0) {
        demo_prepare_targets();
        return demo_menu_self_test();
    }
    if (argc >= 4 && strcmp(argv[1], "--shot") == 0) {
        demo_prepare_targets();
        return demo_screenshot(argv[2], argv[3]);
    }
    if (argc >= 4 && strcmp(argv[1], "--capture") == 0) {
        vox_u32 frames = argc >= 5 ? (vox_u32)strtoul(argv[4], 0, 10) : 24U;
        vox_u32 interval = argc >= 6 ? (vox_u32)strtoul(argv[5], 0, 10) : 6U;
        if (frames == 0U || frames > 600U) frames = 24U;
        if (interval == 0U || interval > 600U) interval = 6U;
        demo_prepare_targets();
        return demo_capture(argv[2], argv[3], frames, interval);
    }
    if (argc >= 2 && strcmp(argv[1], "--smoke-test") == 0) {
        const char *path = argc >= 3 ? argv[2] : "/tmp/digs-demo-smoke.ppm";
        demo_prepare_targets();
        return demo_smoke_test(path);
    }
    if (argc >= 2 && strcmp(argv[1], "--benchmark") == 0) {
        vox_u32 frames = 240U;
        if (argc >= 3) {
            char *end = 0;
            unsigned long requested = strtoul(argv[2], &end, 10);
            if (end == argv[2] || *end != '\0' || requested == 0UL ||
                requested > 100000UL) {
                fprintf(stderr, "benchmark frames must be 1..100000\n");
                return 1;
            }
            frames = (vox_u32)requested;
        }
        return demo_benchmark(frames);
    }
    if (argc >= 2 &&
        (strcmp(argv[1], "--load-self-test") == 0 ||
         strcmp(argv[1], "--performance-self-test") == 0)) {
        int qualify_named_bench =
            strcmp(argv[1], "--performance-self-test") == 0;
        vox_u32 ticks = 600U;
        if (argc >= 3) {
            char *end = 0;
            unsigned long requested = strtoul(argv[2], &end, 10);
            if (end == argv[2] || *end != '\0' || requested < 60UL ||
                requested > 10000UL) {
                fprintf(stderr,
                        "%s self-test ticks must be 60..10000\n",
                        qualify_named_bench ? "performance" : "load");
                return 1;
            }
            ticks = (vox_u32)requested;
        }
        return demo_performance_self_test(ticks, qualify_named_bench);
    }
    /*
     * The licence notice used to sit on the title screen.  The menu is meant
     * to be minimal now, but GPL-3.0 requires the notice be conveyed, so it
     * moves rather than vanishes: here on every start, and in START-HERE.txt
     * beside the binary.
     */
    printf("DIGS -- Copyright (C) Pinnacle Point Development\n");
    printf("Free software under GPL-3.0-or-later, with ABSOLUTELY NO "
           "WARRANTY.\n");
    printf("See LICENSE and START-HERE.txt for the terms and the source.\n");
    memset(&app, 0, sizeof(app));
    /*
     * Open the chronicle before anything else touches it.  This is where the
     * launch is counted, how long the player has been away is worked out,
     * and each miner decides whether to have left a message about it -- the
     * one place a wall clock is read, and it never reaches the simulation.
     */
    {
        char chronicle_path[1032];
        if (demo_chronicle_path(chronicle_path, (int)sizeof(chronicle_path))) {
            (void)digs_chronicle_load(&demo_chronicle, chronicle_path);
            digs_chronicle_open(&demo_chronicle, (vox_u32)time(0));
            (void)digs_chronicle_save(&demo_chronicle, chronicle_path);
            demo_chronicle_ready = 1;
        }
    }
    app.running = 1;
    app.settings_writable = 1;
    app.screen = DEMO_TITLE;
    app.bots = 1;
    app.local_players = 1;
    app.map_style = VOX_DIGS_MAP_COAL_RIDGE;
    app.arsenal = DEMO_ARSENAL_FULL;
    app.seed = 0x564F5831U;
    app.mouse_x = 160;
    app.mouse_y = 100;
    app.mouse_activity_x = 160;
    app.mouse_activity_y = 100;
    app.cursor_visible = -1;
    app.camera_zoom = DEMO_CAMERA_ZOOM_DEFAULT;
    app.camera_scale = (double)DEMO_CAMERA_ZOOM_DEFAULT;
    app.frame_seconds = 1.0 / 60.0;
    demo_options_defaults(&app);
    demo_match_settings_defaults(&app);
    demo_input_defaults(&app);
    demo_bindings_default(&app);
    for (controller_index = 0;
         controller_index < (int)DEMO_CONTROLLER_MAX;
         ++controller_index) {
        app.controllers[controller_index].instance_id = -1;
        app.controllers[controller_index].claimed_player = -1;
    }
    SDL_SetMainReady();
    (void)SDL_SetHint("SDL_JOYSTICK_HIDAPI_SWITCH", "1");
    (void)SDL_SetHint("SDL_JOYSTICK_HIDAPI_SWITCH_HOME_LED", "0");
    (void)SDL_SetHint("SDL_JOYSTICK_HIDAPI", "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 2;
    }
    (void)demo_load_input_settings(&app);
    /* Keyboard play must remain available even when an SDL platform backend
     * cannot initialize optional controller or haptic services. */
    (void)SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    (void)SDL_InitSubSystem(SDL_INIT_HAPTIC);
    demo_load_controller_mappings();
    (void)SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    (void)SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    app.window = SDL_CreateWindow("DIGS v0.0.3 Demo",
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  DEMO_WINDOW_WIDTH, DEMO_WINDOW_HEIGHT,
                                  SDL_WINDOW_RESIZABLE);
    if (app.window == 0) {
        fprintf(stderr, "window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 3;
    }
    demo_apply_fullscreen(&app);
    app.renderer = SDL_CreateRenderer(app.window, -1,
                                      SDL_RENDERER_ACCELERATED);
    if (app.renderer == 0) {
        app.renderer = SDL_CreateRenderer(app.window, -1, 0U);
    }
    if (app.renderer == 0) {
        fprintf(stderr, "renderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 4;
    }
    if (SDL_RenderSetLogicalSize(app.renderer, (int)DEMO_WIDTH,
                                 (int)DEMO_HEIGHT) != 0 ||
        SDL_RenderSetIntegerScale(app.renderer, SDL_TRUE) != 0) {
        fprintf(stderr, "logical renderer setup failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(app.renderer);
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 5;
    }
    app.texture = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_RGB24,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    (int)DEMO_WIDTH, (int)DEMO_HEIGHT);
    if (app.texture == 0) {
        fprintf(stderr, "texture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(app.renderer);
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 6;
    }
    demo_audio_open(&app);
    demo_audio_speak_text(&app, "DIGS!", VOX_AUDIO_SPEECH_DEEP,
                          VOX_AUDIO_PRIORITY_ANNOUNCER,
                          VOX_AUDIO_PAN_CENTER);
    demo_prepare_targets();
    demo_build_title_world();
    demo_qualify_caps(&app, 0);
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0U) {
        for (controller_index = 0;
             controller_index < SDL_NumJoysticks(); ++controller_index) {
            (void)demo_open_controller(&app, controller_index);
        }
    }
    frequency = SDL_GetPerformanceFrequency();
    if (frequency == 0U) {
        fprintf(stderr, "performance timer unavailable: %s\n", SDL_GetError());
        demo_close_controllers(&app);
        demo_audio_close(&app);
        SDL_DestroyTexture(app.texture);
        SDL_DestroyRenderer(app.renderer);
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 7;
    }
    previous_counter = SDL_GetPerformanceCounter();
    demo_fixed_step_clock_init(&fixed_step);
    app.fps_stamp = SDL_GetTicks();
    while (app.running) {
        Uint64 current_counter = SDL_GetPerformanceCounter();
        Uint64 elapsed_units = current_counter - previous_counter;
        double elapsed = (double)elapsed_units / (double)frequency;
        vox_u32 catchup;
        vox_u32 tick_index;
        int frame_cap;
        previous_counter = current_counter;
        presentation_seconds += elapsed;
        demo_fixed_step_add_elapsed(&fixed_step, elapsed_units, frequency);
        while (SDL_PollEvent(&event)) {
            demo_handle_event(&app, &event);
        }
        (void)demo_sync_hardware_mouse(&app);
        demo_update_cursor_visibility(&app);
        catchup = demo_fixed_step_service(&fixed_step, DEMO_MAX_CATCHUP);
        for (tick_index = 0U; tick_index < catchup; ++tick_index) {
            demo_tick(&app);
        }
        if (!demo_fixed_step_can_present(&fixed_step)) {
            /* Simulation time is authoritative: keep every pending tick and
             * drop this presentation instead. The next pump still services
             * events and at most DEMO_MAX_CATCHUP ticks, avoiding one long
             * unresponsive catch-up loop. */
            last_frame_cap = -1;
            present_deadline = 0U;
            continue;
        }
        app.frame_seconds = presentation_seconds;
        if (app.frame_seconds > DEMO_PRESENTATION_DELTA_MAX) {
            app.frame_seconds = DEMO_PRESENTATION_DELTA_MAX;
        }
        presentation_seconds = 0.0;
        app.render_alpha = demo_fixed_step_alpha(&fixed_step, frequency);
        if (app.render_alpha < 0.0) app.render_alpha = 0.0;
        if (app.render_alpha > 1.0) app.render_alpha = 1.0;
        demo_render(&app);
        if (SDL_UpdateTexture(app.texture, 0, demo_pixels,
                              (int)demo_ui.stride) != 0 ||
            SDL_RenderClear(app.renderer) != 0 ||
            SDL_RenderCopy(app.renderer, app.texture, 0, 0) != 0) {
            fprintf(stderr, "frame presentation failed: %s\n", SDL_GetError());
            app.running = 0;
            continue;
        }
        SDL_RenderPresent(app.renderer);
        ++app.rendered_frames;
        if (SDL_GetTicks() - app.fps_stamp >= 1000U) {
            vox_u32 now = SDL_GetTicks();
            app.measured_fps = (double)app.rendered_frames * 1000.0 /
                               (double)(now - app.fps_stamp);
            app.rendered_frames = 0U;
            app.fps_stamp = now;
        }
        frame_cap = demo_frame_caps[app.options.frame_cap_index];
        demo_wait_for_frame(frequency, frame_cap, &last_frame_cap,
                            &present_deadline);
    }
    (void)demo_save_input_settings(&app);
    (void)SDL_ShowCursor(SDL_ENABLE);
    demo_close_controllers(&app);
    demo_audio_close(&app);
    SDL_DestroyTexture(app.texture);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}

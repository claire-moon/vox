/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

#include "digs_miner_art.h"
#include "vox/vox_audio.h"
#include "vox/vox_game.h"
#include "vox/vox_render.h"
#include "vox/vox_script.h"
#include "vox_sdl_ui.h"

#define DEMO_WIDTH 320U
#define DEMO_HEIGHT 200U
#define DEMO_WINDOW_WIDTH 1280
#define DEMO_WINDOW_HEIGHT 800
#define DEMO_SIM_HZ 60U
#define DEMO_MAX_CATCHUP 8U
#define DEMO_CAMERA_ZOOM_MIN 1
#define DEMO_CAMERA_ZOOM_MAX 4
#define DEMO_CAMERA_ZOOM_DEFAULT 2
#define DEMO_AUDIO_RATE 44100
#define DEMO_AUDIO_FRAMES 512U
#define DEMO_LOCAL_MAX 2U
#define DEMO_CONTROLLER_MAX 2U
#define DEMO_DAMAGE_POPUP_MAX 16U
#define DEMO_INPUT_SWITCH_HYSTERESIS_MS 750U
#define DEMO_CONTROLLER_ACTIVITY_MARGIN 0.08
#define DEMO_CONTROLLER_CALIBRATION_MS 750U
#define DEMO_INPUT_SETTINGS_VERSION 1

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
    DEMO_FEEDBACK = 6,
    DEMO_HOW_TO = 7,
    DEMO_INDEX = 8,
    DEMO_CONTROLS = 9,
    DEMO_SCRIPT_ERROR = 10,
    DEMO_INPUT_OPTIONS = 11
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
} demo_controller;

typedef struct demo_player_input {
    int preference;
    int active_source;
    int sensitivity;
    int deadzone;
    int aim_slowdown;
    vox_u32 switch_stamp;
    int suppress_ticks;
    double aim_direction_x;
    double aim_direction_y;
    double aim_distance;
    double aim_magnitude;
} demo_player_input;

typedef struct demo_damage_popup {
    vox_i32 world_x_q16;
    vox_i32 world_y_q16;
    vox_u16 amount;
    vox_u16 ttl;
    vox_u16 target;
    vox_u16 active;
} demo_damage_popup;

typedef struct demo_bindings {
    SDL_Scancode keyboard_left[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_right[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_jump[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_steam[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_rope[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_fire[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_previous[DEMO_LOCAL_MAX];
    SDL_Scancode keyboard_next[DEMO_LOCAL_MAX];
    SDL_GameControllerButton pad_jump;
    SDL_GameControllerButton pad_steam;
    SDL_GameControllerButton pad_rope;
    SDL_GameControllerButton pad_fire;
    SDL_GameControllerButton pad_previous;
    SDL_GameControllerButton pad_next;
} demo_bindings;

typedef struct demo_app {
    int running;
    demo_screen screen;
    int selection;
    int bots;
    int map_style;
    int arsenal;
    vox_u32 seed;
    int local_players;
    int game_mode;
    int friendly_fire;
    int selected_tool[DEMO_LOCAL_MAX];
    vox_u32 aim_world_x[DEMO_LOCAL_MAX];
    vox_u32 aim_world_y[DEMO_LOCAL_MAX];
    int mouse_x;
    int mouse_y;
    int mouse_inside;
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
    vox_u16 index_selection;
    vox_u16 index_scroll;
    double index_visual_row;
    int binding_capture;
    int binding_player;
    int keyboard_previous_down[DEMO_LOCAL_MAX];
    int keyboard_next_down[DEMO_LOCAL_MAX];
    int controller_disconnected;
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
    demo_controller controllers[DEMO_CONTROLLER_MAX];
    demo_player_input player_input[DEMO_LOCAL_MAX];
    demo_bindings bindings;
    demo_damage_popup damage_popups[DEMO_DAMAGE_POPUP_MAX];
    vox_u16 bot_health_ttl[VOX_DIGS_MAX_SLOTS];
    vox_script_runtime scripts;
    int scripts_ready;
    char script_manifest[512];
} demo_app;

static vox_u8 demo_pixels[DEMO_WIDTH * DEMO_HEIGHT * VOX_SOFTWARE_RGB_BYTES];
static vox_u8 demo_camera_pixels[DEMO_WIDTH * DEMO_HEIGHT * VOX_SOFTWARE_RGB_BYTES];
static vox_digs_match demo_match;
static vox_world demo_title_world;
static vox_world demo_render_world;
static vox_ui_surface demo_ui;
static vox_software_target demo_target;
static vox_software_config demo_render_config;

static const int demo_frame_caps[6] = {30, 60, 90, 120, 144, 0};
static const char *demo_frame_names[6] = {"30", "60", "90", "120", "144", "UNLIMITED"};
static const char *demo_map_names[3] = {"COAL RIDGE", "DEEPWORKS", "FURNACE YARD"};
static const char *demo_gi_names[3] = {"COMPATIBILITY", "BALANCED", "SHOWCASE"};
static const char *demo_mode_names[2] = {"FREE FOR ALL", "MINERS VS MACHINES"};
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
static const char *demo_arsenal_names[DEMO_ARSENAL_COUNT] = {
    "FULL WORKS", "MINER KIT", "POWDER KEG"
};
static const vox_u16 demo_arsenal_masks[DEMO_ARSENAL_COUNT] = {
    0x03FFU, 0x00F1U, 0x030EU
};

static demo_controller *demo_controller_for_player(demo_app *app,
                                                   int player);
static void demo_refresh_controller_claims(demo_app *app);
static int demo_save_input_settings(demo_app *app);

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
    event.event_id = ++app->audio_event_id;
    (void)vox_audio_emit(&app->audio, &event);
}

static void demo_audio_play(demo_app *app, int sound)
{
    vox_u16 preset = VOX_AUDIO_PRESET_UI_MOVE;
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

static void demo_audio_pump(demo_app *app)
{
    vox_i16 samples[DEMO_AUDIO_FRAMES * VOX_AUDIO_OUTPUT_CHANNELS];
    Uint32 chunk_bytes = (Uint32)sizeof(samples);
    Uint32 queued;
    int chunk;
    if (app == 0 || app->audio_device == 0U) {
        return;
    }
    queued = SDL_GetQueuedAudioSize(app->audio_device);
    for (chunk = 0; chunk < 3 && queued < chunk_bytes * 2U; ++chunk) {
        if (vox_audio_active_voice_count(&app->audio) == 0U && queued != 0U) {
            break;
        }
        if (vox_audio_render(&app->audio, samples,
                             DEMO_AUDIO_FRAMES) != VOX_OK ||
            SDL_QueueAudio(app->audio_device, samples, chunk_bytes) != 0) {
            SDL_ClearQueuedAudio(app->audio_device);
            break;
        }
        queued += chunk_bytes;
    }
}

static void demo_audio_open(demo_app *app)
{
    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;
    if (app == 0 || SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        return;
    }
    memset(&desired, 0, sizeof(desired));
    memset(&obtained, 0, sizeof(obtained));
    desired.freq = DEMO_AUDIO_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = VOX_AUDIO_OUTPUT_CHANNELS;
    desired.samples = 512U;
    desired.callback = 0;
    app->audio_device = SDL_OpenAudioDevice(0, 0, &desired, &obtained,
                                             SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (app->audio_device == 0U) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    if (obtained.format != AUDIO_S16SYS || obtained.channels != 2U ||
        vox_audio_init(&app->audio, (vox_u32)obtained.freq,
                       0xD1655EEDU) != VOX_OK) {
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
        SDL_ClearQueuedAudio(app->audio_device);
        vox_audio_stop_all(&app->audio);
        SDL_CloseAudioDevice(app->audio_device);
        app->audio_device = 0U;
    }
}

static int demo_scripts_open(demo_app *app)
{
    static const char suffix[] = "share/digs/scripts/manifest.txt";
    static const char source_path[] = "games/digs/scripts/manifest.txt";
    vox_script_report report;
    char *base;
    vox_result status;
    if (vox_script_runtime_init(&app->scripts, 0) != VOX_OK) {
        return 0;
    }
    app->script_manifest[0] = '\0';
    base = SDL_GetBasePath();
    if (base != 0 && strlen(base) + sizeof(suffix) <
        sizeof(app->script_manifest)) {
        strcpy(app->script_manifest, base);
        strcat(app->script_manifest, suffix);
    }
    if (base != 0) SDL_free(base);
    status = app->script_manifest[0] == '\0' ? VOX_SCRIPT_ERR_IO :
             vox_script_reload_manifest(&app->scripts,
                                         app->script_manifest, &report);
    if (status != VOX_OK) {
        strcpy(app->script_manifest, source_path);
        status = vox_script_reload_manifest(&app->scripts,
                                             app->script_manifest, &report);
    }
    if (status != VOX_OK) {
        fprintf(stderr, "DIGS script load failed: %s\n",
                vox_script_last_error(&app->scripts));
        return 0;
    }
    app->scripts_ready = 1;
    return 1;
}

static int demo_scripts_reload(demo_app *app)
{
    vox_script_report report;
    vox_result status;
    if (!app->scripts_ready) return demo_scripts_open(app);
    status = vox_script_reload_manifest(&app->scripts,
                                         app->script_manifest, &report);
    if (status != VOX_OK) {
        fprintf(stderr, "DIGS F5 reload rejected: %s\n",
                vox_script_last_error(&app->scripts));
        return 0;
    }
    fprintf(stdout, "DIGS scripts reloaded source=%08lx catalog=%08lx\n",
            (unsigned long)report.source_hash,
            (unsigned long)report.catalog_hash);
    return 1;
}

static void demo_scripts_close(demo_app *app)
{
    vox_script_runtime_shutdown(&app->scripts);
    app->scripts_ready = 0;
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
    return -1;
}

static void demo_bindings_default(demo_app *app)
{
    app->bindings.keyboard_left[0] = SDL_SCANCODE_A;
    app->bindings.keyboard_right[0] = SDL_SCANCODE_D;
    app->bindings.keyboard_jump[0] = SDL_SCANCODE_SPACE;
    app->bindings.keyboard_steam[0] = SDL_SCANCODE_LSHIFT;
    app->bindings.keyboard_rope[0] = SDL_SCANCODE_Q;
    app->bindings.keyboard_fire[0] = SDL_SCANCODE_E;
    app->bindings.keyboard_previous[0] = SDL_SCANCODE_Z;
    app->bindings.keyboard_next[0] = SDL_SCANCODE_X;
    app->bindings.keyboard_left[1] = SDL_SCANCODE_LEFT;
    app->bindings.keyboard_right[1] = SDL_SCANCODE_RIGHT;
    app->bindings.keyboard_jump[1] = SDL_SCANCODE_UP;
    app->bindings.keyboard_steam[1] = SDL_SCANCODE_RSHIFT;
    app->bindings.keyboard_rope[1] = SDL_SCANCODE_SLASH;
    app->bindings.keyboard_fire[1] = SDL_SCANCODE_RCTRL;
    app->bindings.keyboard_previous[1] = SDL_SCANCODE_COMMA;
    app->bindings.keyboard_next[1] = SDL_SCANCODE_PERIOD;
    app->bindings.pad_jump = SDL_CONTROLLER_BUTTON_A;
    app->bindings.pad_steam = SDL_CONTROLLER_BUTTON_X;
    app->bindings.pad_rope = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    app->bindings.pad_fire = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    app->bindings.pad_previous = SDL_CONTROLLER_BUTTON_Y;
    app->bindings.pad_next = SDL_CONTROLLER_BUTTON_B;
}

static int demo_controller_connected(const demo_controller *controller)
{
    return controller != 0 && controller->joystick != 0;
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
        app->player_input[player].switch_stamp = 0U;
        app->player_input[player].suppress_ticks = 0;
        app->player_input[player].aim_direction_x = player == 0 ? 1.0 : -1.0;
        app->player_input[player].aim_direction_y = 0.0;
        app->player_input[player].aim_distance = 24.0;
        app->player_input[player].aim_magnitude = 1.0;
    }
}

static int demo_input_settings_path(char *path, int capacity,
                                    const char *suffix)
{
    char *base;
    size_t required;
    if (path == 0 || capacity <= 0 || suffix == 0) return 0;
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

static void demo_validate_input_settings(demo_app *app)
{
    int player;
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
        input->active_source = input->preference == DEMO_INPUT_CONTROLLER ?
                               DEMO_SOURCE_CONTROLLER : DEMO_SOURCE_KEYBOARD;
    }
}

static int demo_load_input_settings(demo_app *app)
{
    char path[1024];
    char line[128];
    FILE *file;
    int version = 0;
    if (!demo_input_settings_path(path, (int)sizeof(path), "settings.cfg")) {
        return 0;
    }
    file = fopen(path, "r");
    if (file == 0) return 0;
    while (fgets(line, (int)sizeof(line), file) != 0) {
        int value;
        if (sscanf(line, "DIGS_INPUT_SETTINGS=%d", &value) == 1) {
            version = value;
        } else if (sscanf(line, "P1_MODE=%d", &value) == 1) {
            app->player_input[0].preference = value;
        } else if (sscanf(line, "P1_SENSITIVITY=%d", &value) == 1) {
            app->player_input[0].sensitivity = value;
        } else if (sscanf(line, "P1_DEADZONE=%d", &value) == 1) {
            app->player_input[0].deadzone = value;
        } else if (sscanf(line, "P1_SLOWDOWN=%d", &value) == 1) {
            app->player_input[0].aim_slowdown = value;
        } else if (sscanf(line, "P2_MODE=%d", &value) == 1) {
            app->player_input[1].preference = value;
        } else if (sscanf(line, "P2_SENSITIVITY=%d", &value) == 1) {
            app->player_input[1].sensitivity = value;
        } else if (sscanf(line, "P2_DEADZONE=%d", &value) == 1) {
            app->player_input[1].deadzone = value;
        } else if (sscanf(line, "P2_SLOWDOWN=%d", &value) == 1) {
            app->player_input[1].aim_slowdown = value;
        }
    }
    (void)fclose(file);
    if (version != DEMO_INPUT_SETTINGS_VERSION) {
        demo_input_defaults(app);
        return 0;
    }
    demo_validate_input_settings(app);
    return 1;
}

static int demo_save_input_settings(demo_app *app)
{
    char path[1024];
    char temporary[1032];
    FILE *file;
    int player;
    if (!demo_input_settings_path(path, (int)sizeof(path), "settings.cfg")) {
        return 0;
    }
    if (strlen(path) + 5U >= sizeof(temporary)) return 0;
    sprintf(temporary, "%s.tmp", path);
    file = fopen(temporary, "w");
    if (file == 0) return 0;
    if (fprintf(file, "DIGS_INPUT_SETTINGS=%d\n",
                DEMO_INPUT_SETTINGS_VERSION) < 0) {
        (void)fclose(file);
        (void)remove(temporary);
        return 0;
    }
    for (player = 0; player < (int)DEMO_LOCAL_MAX; ++player) {
        const demo_player_input *input = &app->player_input[player];
        if (fprintf(file,
                    "P%d_MODE=%d\nP%d_SENSITIVITY=%d\n"
                    "P%d_DEADZONE=%d\nP%d_SLOWDOWN=%d\n",
                    player + 1, input->preference,
                    player + 1, input->sensitivity,
                    player + 1, input->deadzone,
                    player + 1, input->aim_slowdown) < 0) {
            (void)fclose(file);
            (void)remove(temporary);
            return 0;
        }
    }
    if (fclose(file) != 0) {
        (void)remove(temporary);
        return 0;
    }
    if (rename(temporary, path) != 0) {
        /* ISO C rename replaces atomically on POSIX.  Older Windows CRTs do
         * not replace an existing file, so retain a conservative fallback. */
        (void)remove(path);
        if (rename(temporary, path) != 0) {
            (void)remove(temporary);
            return 0;
        }
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
            if (app->screen == DEMO_PLAY && claimed_player >= 0) {
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
    vox_software_config_default(&demo_render_config);
}

static void demo_build_title_world(void)
{
    vox_u32 x;
    vox_u32 tick;
    (void)vox_digs_generate_map(&demo_title_world, VOX_DIGS_MAP_FURNACE_YARD,
                                0xD1655EEDU);
    for (x = 76U; x < 108U; ++x) {
        (void)vox_world_set(&demo_title_world, x, 55U,
                            VOX_WORLD_DEPTH - 1U, VOX_MAT_LAVA, 700L << 16);
    }
    (void)vox_world_blast(&demo_title_world, 91U, 54U, 0U, 4U, 700L << 16);
    for (tick = 0U; tick < 4U; ++tick) {
        (void)vox_world_step(&demo_title_world, 0);
    }
}

static void demo_dark_panel(int x, int y, int width, int height)
{
    vox_ui_rect(&demo_ui, x, y, width, height, DEMO_VGA_BLACK);
    vox_ui_frame(&demo_ui, x, y, width, height, DEMO_VGA_BROWN);
}

static void demo_menu_item(int y, const char *label, int selected)
{
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

static void demo_draw_title(demo_app *app)
{
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(62, 10, 196, 180);
    vox_ui_text_center_shadow(&demo_ui, 160, 21, 4, "DIGS",
                              DEMO_VGA_YELLOW);
    vox_ui_rect(&demo_ui, 82, 65, 156, 1, DEMO_VGA_DARK_GRAY);
    vox_ui_rect(&demo_ui, 82, 66, 156, 1, DEMO_VGA_BROWN);
    demo_menu_item(73, "START MATCH", app->selection == 0);
    demo_menu_item(87, "FOUNDRY LAB", app->selection == 1);
    demo_menu_item(101, "HOW TO PLAY", app->selection == 2);
    demo_menu_item(115, "MINER'S INDEX", app->selection == 3);
    demo_menu_item(129, "CONTROLS", app->selection == 4);
    demo_menu_item(143, "OPTIONS", app->selection == 5);
    demo_menu_item(157, "QA FEEDBACK", app->selection == 6);
    demo_menu_item(171, "QUIT", app->selection == 7);
    vox_ui_text_center(&demo_ui, 160, 181, 1,
                       "GPL-3.0-OR-LATER  V0.0.2",
                       DEMO_VGA_DARK_GRAY);
}

static void demo_value_line(int y, const char *label, const char *value,
                            int selected)
{
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
    vox_ui_text_center_shadow(&demo_ui, 160, 16, 2, "MATCH SETUP",
                              DEMO_VGA_YELLOW);
    sprintf(value, "%d", app->local_players);
    demo_value_line(43, "LOCAL PLAYERS", value, app->selection == 0);
    sprintf(value, "%d", app->bots);
    demo_value_line(57, "BOTS", value, app->selection == 1);
    demo_value_line(71, "MODE", demo_mode_names[app->game_mode],
                    app->selection == 2);
    demo_value_line(85, "FRIENDLY FIRE", demo_toggle_names[app->friendly_fire],
                    app->selection == 3);
    demo_value_line(99, "MAP", demo_map_names[app->map_style],
                    app->selection == 4);
    sprintf(value, "%08lX", (unsigned long)app->seed);
    demo_value_line(113, "SEED", value, app->selection == 5);
    demo_value_line(127, "ARSENAL", demo_arsenal_names[app->arsenal],
                    app->selection == 6);
    demo_menu_item(148, "START MATCH", app->selection == 7);
    demo_menu_item(164, "BACK", app->selection == 8);
    vox_ui_text_center(&demo_ui, 160, 180, 1,
                       "ARROWS CHANGE  ENTER SELECTS", DEMO_VGA_DARK_GRAY);
}

static void demo_draw_options(demo_app *app)
{
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(28, 8, 264, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 14, 2, "OPTIONS",
                              DEMO_VGA_YELLOW);
    demo_value_line(34, "FRAME CAP", demo_frame_names[app->options.frame_cap_index],
                    app->selection == 0);
    demo_value_line(47, "LIGHTFIELD", demo_gi_names[app->options.gi_quality],
                    app->selection == 1);
    demo_value_line(60, "FX PROFILE", demo_fx_names[app->options.fx_profile],
                    app->selection == 2);
    demo_value_line(73, "FLASHES", demo_flash_names[app->options.flash_mode],
                    app->selection == 3);
    demo_value_line(86, "GORE", demo_gore_names[app->options.gore_level],
                    app->selection == 4);
    demo_value_line(99, "CAMERA SHAKE",
                    demo_toggle_names[app->options.camera_shake],
                    app->selection == 5);
    demo_value_line(112, "DAMAGE NUMBERS",
                    demo_toggle_names[app->options.damage_numbers],
                    app->selection == 6);
    demo_value_line(125, "NUMBER SIZE",
                    app->options.damage_number_size ? "LARGE" : "SMALL",
                    app->selection == 7);
    demo_value_line(138, "NUMBER COLOR",
                    demo_number_color_names[app->options.damage_number_color],
                    app->selection == 8);
    demo_value_line(151, "FULLSCREEN",
                    demo_toggle_names[app->options.fullscreen],
                    app->selection == 9);
    demo_menu_item(165, "INPUT & CONTROLLER", app->selection == 10);
    demo_menu_item(180, "BACK", app->selection == 11);
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
    int slot;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(28, 8, 264, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 14, 2,
                              "INPUT & CONTROLLER", DEMO_VGA_YELLOW);
    demo_input_mode_value(app, 0, p1_mode);
    demo_input_mode_value(app, 1, p2_mode);
    demo_value_line(34, "P1 MODE", p1_mode, app->selection == 0);
    demo_value_line(46, "P1 SENSITIVITY",
                    demo_sensitivity_names[app->player_input[0].sensitivity],
                    app->selection == 1);
    demo_value_line(58, "P1 DEADZONE",
                    demo_deadzone_names[app->player_input[0].deadzone],
                    app->selection == 2);
    demo_value_line(70, "P1 AIM SLOW",
                    demo_slowdown_names[app->player_input[0].aim_slowdown],
                    app->selection == 3);
    demo_value_line(86, "P2 MODE", p2_mode, app->selection == 4);
    demo_value_line(98, "P2 SENSITIVITY",
                    demo_sensitivity_names[app->player_input[1].sensitivity],
                    app->selection == 5);
    demo_value_line(110, "P2 DEADZONE",
                    demo_deadzone_names[app->player_input[1].deadzone],
                    app->selection == 6);
    demo_value_line(122, "P2 AIM SLOW",
                    demo_slowdown_names[app->player_input[1].aim_slowdown],
                    app->selection == 7);
    for (slot = 0; slot < (int)DEMO_CONTROLLER_MAX; ++slot) {
        if (app->controllers[slot].calibrating) calibrating = 1;
    }
    demo_value_line(138, "CALIBRATE PADS",
                    calibrating ? "KEEP STICKS STILL" : "START",
                    app->selection == 8);
    demo_menu_item(153, "RESTORE INPUT DEFAULTS", app->selection == 9);
    demo_menu_item(169, "BACK", app->selection == 10);
    vox_ui_text_center(&demo_ui, 160, 184, 1,
                       "ARROWS CHANGE  ENTER SELECTS",
                       DEMO_VGA_DARK_GRAY);
}

static void demo_draw_feedback(demo_app *app)
{
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(24, 18, 272, 166);
    vox_ui_text_center_shadow(&demo_ui, 160, 28, 2, "QA FEEDBACK",
                              DEMO_VGA_YELLOW);
    vox_ui_text(&demo_ui, 39, 62, 1,
                "1 OPEN QA/VOX_QA_FEEDBACK.XLSX", DEMO_VGA_LIGHT_GRAY);
    vox_ui_text(&demo_ui, 39, 78, 1,
                "2 RECORD STEPS EXPECTED ACTUAL", DEMO_VGA_LIGHT_GRAY);
    vox_ui_text(&demo_ui, 39, 94, 1,
                "3 RUN TOOLS/VOX-TEST-COCKPIT.SH", DEMO_VGA_LIGHT_GRAY);
    vox_ui_text(&demo_ui, 39, 110, 1,
                "4 ATTACH PACKET TO GITHUB ISSUE", DEMO_VGA_LIGHT_GRAY);
    vox_ui_text_center(&demo_ui, 160, 134, 1,
                       "CLAIRE-MOON/VOX  DEMO FEEDBACK",
                       DEMO_VGA_LIGHT_CYAN);
    vox_ui_text_center(&demo_ui, 160, 158, 1,
                       "ENTER OR ESC RETURNS", DEMO_VGA_YELLOW);
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

static void demo_draw_how_to(demo_app *app)
{
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(10, 8, 300, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 15, 2, "HOW TO PLAY",
                              DEMO_VGA_YELLOW);
    vox_ui_text(&demo_ui, 20, 40, 1, "MOVE", DEMO_VGA_LIGHT_CYAN);
    vox_ui_text(&demo_ui, 72, 40, 1, "A D OR LEFT STICK", DEMO_VGA_WHITE);
    vox_ui_text(&demo_ui, 20, 52, 1, "JUMP", DEMO_VGA_LIGHT_CYAN);
    vox_ui_text(&demo_ui, 72, 52, 1, "SPACE OR PAD A", DEMO_VGA_WHITE);
    vox_ui_text(&demo_ui, 20, 64, 1, "ROPE", DEMO_VGA_LIGHT_CYAN);
    vox_ui_text(&demo_ui, 72, 64, 1, "HOLD Q OR LB  RELEASE LAUNCHES",
                DEMO_VGA_WHITE);
    vox_ui_text(&demo_ui, 20, 76, 1, "AIM", DEMO_VGA_LIGHT_CYAN);
    vox_ui_text(&demo_ui, 72, 76, 1, "MOUSE OR RIGHT STICK", DEMO_VGA_WHITE);
    vox_ui_text(&demo_ui, 20, 88, 1, "FIRE", DEMO_VGA_LIGHT_CYAN);
    vox_ui_text(&demo_ui, 72, 88, 1, "LMB E OR RB", DEMO_VGA_WHITE);
    vox_ui_text(&demo_ui, 20, 100, 1, "STEAM", DEMO_VGA_LIGHT_CYAN);
    vox_ui_text(&demo_ui, 72, 100, 1, "SHIFT OR PAD X", DEMO_VGA_WHITE);
    vox_ui_text_wrap(&demo_ui, 20, 119, 280, 5, 1,
        "DESTROY TERRAIN OUTSMART RIVALS AND STAY ABOVE THE RISING LAVA. "
        "SPAWNS HAVE A FIVE SECOND SHIELD THAT ENDS WHEN YOU ATTACK. "
        "ROPE TO SOLID BEAMS OR ROCK AND USE TOOLS TO BUILD YOUR OWN ROUTE.",
        170U, 170U, 170U);
    vox_ui_text_center(&demo_ui, 160, 178, 1, "ESC OR PAD B RETURNS",
                       DEMO_VGA_YELLOW);
}

static void demo_draw_index(demo_app *app)
{
    const vox_script_catalog *catalog;
    const vox_script_entry *entry;
    vox_u16 count;
    vox_u16 row;
    double target_row;
    double factor;
    int highlight_y;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(6, 6, 308, 188);
    vox_ui_text_center_shadow(&demo_ui, 160, 12, 2, "MINER'S INDEX",
                              DEMO_VGA_YELLOW);
    catalog = app->scripts_ready ? vox_script_catalog_get(&app->scripts) : 0;
    count = catalog == 0 ? 0U : catalog->entry_count;
    if (count == 0U) {
        vox_ui_text_center(&demo_ui, 160, 92, 1,
                           "CATALOG UNAVAILABLE", DEMO_VGA_LIGHT_RED);
        return;
    }
    if (app->index_selection >= count) {
        app->index_selection = (vox_u16)(count - 1U);
    }
    target_row = (double)(app->index_selection - app->index_scroll);
    factor = app->frame_seconds * 18.0;
    if (factor < 0.05) factor = 0.05;
    if (factor > 1.0) factor = 1.0;
    app->index_visual_row += (target_row - app->index_visual_row) * factor;
    highlight_y = 39 + (int)(app->index_visual_row * 20.0);
    vox_ui_rect(&demo_ui, 11, highlight_y - 2, 111, 12, DEMO_VGA_BLUE);
    vox_ui_frame(&demo_ui, 11, highlight_y - 2, 111, 12,
                 DEMO_VGA_LIGHT_CYAN);
    if (app->index_scroll > 0U) {
        vox_ui_text_center(&demo_ui, 66, 28, 1, "^ MORE ^",
                           DEMO_VGA_LIGHT_CYAN);
    }
    for (row = 0U; row < VOX_SCRIPT_INDEX_VISIBLE_ROWS; ++row) {
        vox_u16 ordinal = (vox_u16)(app->index_scroll + row);
        if (ordinal < count) {
            char label[20];
            const vox_script_entry *listed =
                vox_script_catalog_entry(catalog, ordinal);
            demo_short_label(label, (int)sizeof(label),
                             listed == 0 ? "UNKNOWN" : listed->title, 17);
            vox_ui_text(&demo_ui, 15, 39 + (int)row * 20, 1, label,
                        ordinal == app->index_selection ? 255U : 170U,
                        ordinal == app->index_selection ? 255U : 170U,
                        ordinal == app->index_selection ? 85U : 170U);
        }
    }
    if ((vox_u16)(app->index_scroll + VOX_SCRIPT_INDEX_VISIBLE_ROWS) <
        count) {
        vox_ui_text_center(&demo_ui, 66, 163, 1, "V MORE V",
                           DEMO_VGA_LIGHT_CYAN);
    }
    vox_ui_frame(&demo_ui, 128, 31, 180, 140, DEMO_VGA_BROWN);
    entry = vox_script_catalog_entry(catalog, app->index_selection);
    if (entry != 0) {
        char heading[30];
        demo_short_label(heading, (int)sizeof(heading), entry->title, 27);
        vox_ui_text(&demo_ui, 134, 38, 1, heading, DEMO_VGA_YELLOW);
        vox_ui_text(&demo_ui, 134, 49, 1, entry->category,
                    DEMO_VGA_LIGHT_CYAN);
        (void)vox_ui_text_wrap(&demo_ui, 134, 63, 168, 4, 1,
                               entry->summary, 255U, 255U, 255U);
        (void)vox_ui_text_wrap(&demo_ui, 134, 101, 168, 7, 1,
                               entry->detail, 170U, 170U, 170U);
    }
    vox_ui_text_center(&demo_ui, 160, 180, 1,
                       "UP DOWN BROWSE  ESC OR B BACK", DEMO_VGA_DARK_GRAY);
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
    return 0;
}

static void demo_assign_keyboard_binding(demo_app *app, int player,
                                         int action, SDL_Scancode code)
{
    SDL_Scancode *destination;
    int other;
    destination = demo_keyboard_binding(app, player, action);
    if (destination == 0 || code == SDL_SCANCODE_UNKNOWN) return;
    for (other = 0; other < 8; ++other) {
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
    for (other = 2; other < 8; ++other) {
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
    static const char *actions[8] = {
        "MOVE LEFT", "MOVE RIGHT", "JUMP", "STEAM", "ROPE", "FIRE",
        "PREV WEAPON", "NEXT WEAPON"
    };
    char line[80];
    int action;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(18, 8, 284, 184);
    vox_ui_text_center_shadow(&demo_ui, 160, 15, 2, "CONTROLS",
                              DEMO_VGA_YELLOW);
    demo_value_line(36, "DEVICE",
        app->binding_player == 0 ? "P1 KEYBOARD" :
        (app->binding_player == 1 ? "P2 KEYBOARD" : "CONTROLLER"),
        app->selection == 0);
    for (action = 0; action < 8; ++action) {
        const char *binding_name;
        if (app->binding_player < 2) {
            SDL_Scancode *binding = demo_keyboard_binding(
                app, app->binding_player, action);
            binding_name = binding == 0 ? "UNBOUND" :
                           demo_scancode_label(*binding);
        } else if (action < 2) {
            binding_name = "STICK OR DPAD";
        } else {
            SDL_GameControllerButton *binding = demo_pad_binding(app, action);
            const char *pad_name = binding == 0 ? 0 :
                SDL_GameControllerGetStringForButton(*binding);
            binding_name = pad_name == 0 ? "UNBOUND" : pad_name;
        }
        sprintf(line, "%s", binding_name);
        demo_value_line(48 + action * 12, actions[action], line,
                        app->selection == action + 1);
    }
    demo_menu_item(150, "RESTORE DEFAULTS", app->selection == 9);
    demo_menu_item(164, "BACK", app->selection == 10);
    vox_ui_text_center(&demo_ui, 160, 180, 1,
        app->binding_capture ? "PRESS A NEW KEY  ESC CANCELS" :
        "ENTER REBINDS  LEFT RIGHT DEVICE", DEMO_VGA_LIGHT_CYAN);
}

static void demo_draw_script_error(demo_app *app)
{
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(18, 24, 284, 152);
    vox_ui_text_center_shadow(&demo_ui, 160, 35, 2, "SCRIPT ERROR",
                              DEMO_VGA_LIGHT_RED);
    (void)vox_ui_text_wrap(&demo_ui, 32, 68, 256, 8, 1,
                           vox_script_last_error(&app->scripts),
                           255U, 255U, 255U);
    vox_ui_text_center(&demo_ui, 160, 154, 1,
                       "F5 RETRIES  ESC QUITS", DEMO_VGA_YELLOW);
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

static void demo_render_voxel(int x, int y, vox_u16 material)
{
    if (x < 0 || y < 0 || x >= (int)VOX_WORLD_WIDTH ||
        y >= (int)VOX_WORLD_HEIGHT || material == VOX_MAT_AIR) {
        return;
    }
    (void)vox_world_set(&demo_render_world, (vox_u32)x, (vox_u32)y,
                        VOX_WORLD_DEPTH - 1U, material,
                        demo_material_temperature(material));
}

static void demo_voxelize_miner(vox_u16 player)
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
    pose.coat_material = coats[player];
    pose.facing_right = demo_match.facing_right[player];
    for (part = 0U; part < VOX_DIGS_ANATOMY_PART_COUNT; ++part) {
        if ((demo_match.anatomy[player][part].flags &
             VOX_DIGS_PART_SEVERED) != 0U) {
            pose.severed_mask |= (vox_u32)1U << part;
        }
    }
    (void)digs_miner_voxelize(&demo_render_world, x, y, &pose);
}

static void demo_voxelize_rope(vox_u16 player)
{
    const vox_digs_rope *rope = &demo_match.ropes[player];
    int x0;
    int y0;
    int x1;
    int y1;
    int dx;
    int dy;
    int sx;
    int sy;
    int error;
    if (!rope->active) return;
    x0 = (int)(demo_match.players[player].position_x.value_q16 / 65536L);
    y0 = (int)(demo_match.players[player].position_y.value_q16 / 65536L);
    x1 = (int)(rope->anchor_x_q16 / 65536L);
    y1 = (int)(rope->anchor_y_q16 / 65536L);
    dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    dy = y1 >= y0 ? y1 - y0 : y0 - y1;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    error = dx - dy;
    for (;;) {
        demo_render_voxel(x0, y0, VOX_MAT_METAL);
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

static void demo_build_render_world(demo_app *app)
{
    vox_u16 player;
    vox_u16 index;
    memcpy(&demo_render_world, &demo_match.world, sizeof(demo_render_world));
    for (player = 0U; player < VOX_DIGS_MAX_SLOTS; ++player) {
        if (demo_match.alive[player]) {
            demo_voxelize_rope(player);
            demo_voxelize_miner(player);
        }
    }
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

static void demo_apply_player_camera(demo_app *app)
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
    int player;
    int center_x;
    int center_y;
    int x;
    int y;
    int user_zoom = demo_normalize_camera_zoom(app);
    minimum_x = (double)VOX_WORLD_WIDTH;
    maximum_x = 0.0;
    minimum_y = (double)VOX_WORLD_HEIGHT;
    maximum_y = 0.0;
    active = 0;
    target_sum_x = 0.0;
    for (player = 0; player < app->local_players; ++player) {
        if (demo_match.alive[player]) {
            double previous_x = (double)app->previous_player_x[player] /
                                65536.0;
            double previous_y = (double)app->previous_player_y[player] /
                                65536.0;
            double current_x = (double)demo_match.players[player].
                               position_x.value_q16 / 65536.0;
            double current_y = (double)demo_match.players[player].
                               position_y.value_q16 / 65536.0;
            double interpolated_x = previous_x +
                                    (current_x - previous_x) *
                                    app->render_alpha;
            double interpolated_y = previous_y +
                                    (current_y - previous_y) *
                                    app->render_alpha;
            double look_x = (double)demo_match.players[player].
                            velocity_x.value_q16 / 65536.0 * 2.5;
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
    fit_y = (double)VOX_WORLD_HEIGHT /
            ((maximum_y - minimum_y) + 40.0);
    target_scale = fit_x < fit_y ? fit_x : fit_y;
    if (target_scale > (double)user_zoom) target_scale = (double)user_zoom;
    if (target_scale < 0.75) target_scale = 0.75;
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
    if (app->camera_scale < 0.70) app->camera_scale = 0.70;
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
    app->camera_trauma -= dt * 1.8;
    if (app->camera_trauma < 0.0) app->camera_trauma = 0.0;
    center_x = (int)((app->camera_world_x + app->camera_shake_x) *
                     (double)DEMO_WIDTH / (double)VOX_WORLD_WIDTH);
    center_y = (int)((app->camera_world_y + app->camera_shake_y) *
                     (double)DEMO_HEIGHT / (double)VOX_WORLD_HEIGHT);
    memcpy(demo_camera_pixels, demo_pixels, sizeof(demo_pixels));
    for (y = 0; y < (int)DEMO_HEIGHT; ++y) {
        for (x = 0; x < (int)DEMO_WIDTH; ++x) {
            int source_x = center_x + (int)((double)(x -
                           (int)DEMO_WIDTH / 2) / app->camera_scale);
            int source_y = center_y + (int)((double)(y -
                           (int)DEMO_HEIGHT / 2) / app->camera_scale);
            vox_u8 *destination = &demo_pixels[(y * (int)DEMO_WIDTH + x) *
                                               VOX_SOFTWARE_RGB_BYTES];
            if (source_x >= 0 && source_y >= 0 &&
                source_x < (int)DEMO_WIDTH && source_y < (int)DEMO_HEIGHT) {
                const vox_u8 *source = &demo_camera_pixels[
                    (source_y * (int)DEMO_WIDTH + source_x) *
                    VOX_SOFTWARE_RGB_BYTES];
                destination[0] = source[0];
                destination[1] = source[1];
                destination[2] = source[2];
            } else {
                destination[0] = 0U;
                destination[1] = 0U;
                destination[2] = 0U;
            }
        }
    }
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
    const vox_digs_weapon_properties *weapon =
        vox_digs_weapon_get((vox_u16)app->selected_tool[0]);
    vox_u32 remaining = demo_match.tick < demo_match.rules.match_ticks ?
                        demo_match.rules.match_ticks - demo_match.tick : 0U;
    vox_u32 seconds = remaining /
                      VOX_DIGS_TICKS_PER_SECOND;
    int health_width = (int)((vox_u32)demo_match.health[0] * 42U /
                             VOX_DIGS_MAX_HEALTH);
    int steam_width = (int)((vox_u32)demo_match.steam_q16[0] * 40U / 65535U);
    vox_ui_rect(&demo_ui, 3, 3, 166, 29, DEMO_VGA_BLACK);
    vox_ui_frame(&demo_ui, 3, 3, 166, 29, DEMO_VGA_BROWN);
    sprintf(text, "P1 K%u D%u HP%u  %lu:%02lu",
            (unsigned int)demo_match.scores[0],
            (unsigned int)demo_match.deaths[0],
            (unsigned int)demo_match.health[0],
            (unsigned long)(seconds / 60U),
            (unsigned long)(seconds % 60U));
    vox_ui_text(&demo_ui, 7, 7, 1, text, DEMO_VGA_WHITE);
    vox_ui_text(&demo_ui, 7, 18, 1, "HP", DEMO_VGA_LIGHT_RED);
    vox_ui_rect(&demo_ui, 22, 18, 44, 5, DEMO_VGA_BROWN);
    vox_ui_rect(&demo_ui, 23, 19, health_width, 3, DEMO_VGA_LIGHT_RED);
    vox_ui_text(&demo_ui, 73, 18, 1, "STEAM", DEMO_VGA_LIGHT_CYAN);
    vox_ui_rect(&demo_ui, 112, 18, 42, 5, DEMO_VGA_BLUE);
    vox_ui_rect(&demo_ui, 113, 19, steam_width, 3, DEMO_VGA_CYAN);
    vox_ui_rect(&demo_ui, 174, 3, 143, 29, DEMO_VGA_BLACK);
    vox_ui_frame(&demo_ui, 174, 3, 143, 29, DEMO_VGA_BROWN);
    vox_ui_text(&demo_ui, 179, 7, 1,
                weapon == 0 ? "UNKNOWN" : weapon->name,
                DEMO_VGA_YELLOW);
    sprintf(text, "CD %u  LMB FIRE",
            (unsigned int)demo_match.weapon_cooldown[0]);
    vox_ui_text(&demo_ui, 179, 15, 1, text, DEMO_VGA_LIGHT_GRAY);
    vox_ui_text(&demo_ui, 179, 23, 1, "1-0 WEAPON  WHEEL ZOOM",
                DEMO_VGA_DARK_GRAY);
    if (app->local_players > 1) {
        const vox_digs_weapon_properties *p2_weapon =
            vox_digs_weapon_get((vox_u16)app->selected_tool[1]);
        int p2_health = (int)((vox_u32)demo_match.health[1] * 56U /
                              VOX_DIGS_MAX_HEALTH);
        vox_ui_rect(&demo_ui, 3, 35, 166, 21, DEMO_VGA_BLACK);
        vox_ui_frame(&demo_ui, 3, 35, 166, 21, DEMO_VGA_CYAN);
        sprintf(text, "P2 K%u D%u HP%u",
                (unsigned int)demo_match.scores[1],
                (unsigned int)demo_match.deaths[1],
                (unsigned int)demo_match.health[1]);
        vox_ui_text(&demo_ui, 7, 39, 1, text, DEMO_VGA_LIGHT_CYAN);
        vox_ui_rect(&demo_ui, 105, 40, 58, 5, DEMO_VGA_BLUE);
        vox_ui_rect(&demo_ui, 106, 41, p2_health, 3, DEMO_VGA_LIGHT_RED);
        vox_ui_rect(&demo_ui, 174, 35, 143, 21, DEMO_VGA_BLACK);
        vox_ui_frame(&demo_ui, 174, 35, 143, 21, DEMO_VGA_CYAN);
        vox_ui_text(&demo_ui, 179, 39, 1,
                    p2_weapon == 0 ? "UNKNOWN" : p2_weapon->name,
                    DEMO_VGA_LIGHT_CYAN);
        vox_ui_text(&demo_ui, 179, 48, 1, "PAD 2 OR P2 KEYS",
                    DEMO_VGA_DARK_GRAY);
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

static void demo_draw_world_feedback(demo_app *app)
{
    static const char *ai_names[4] = {
        "ROAM", "SEARCH", "ATTACK", "RETREAT"
    };
    int player;
    int slot;
    for (player = 0; player < (int)demo_match.rules.player_count; ++player) {
        int x;
        int y;
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
        if (app->options.debug && vox_digs_player_is_bot(&demo_match,
                                                         (vox_u16)player)) {
            vox_u16 mode = demo_match.bots[player].mode;
            const char *name = mode < 4U ? ai_names[mode] : "AI";
            vox_ui_text_center(&demo_ui, x, y - 24, 1, name,
                               DEMO_VGA_LIGHT_CYAN);
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
        vox_ui_frame(&demo_ui, x - 3, y - 3, 7, 7,
                     player == 0 ? 255U : 85U,
                     player == 0 ? 255U : 255U,
                     player == 0 ? 85U : 255U);
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
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    demo_build_render_world(app);
    (void)vox_software_render_ex(&demo_render_world, &demo_target,
                                 &demo_render_config);
    demo_apply_player_camera(app);
    /* Keep the mouse player's authoritative aim synchronized to the camera
     * transform that is actually being presented this frame. */
    if (app->screen == DEMO_PLAY && app->mouse_inside &&
        app->player_input[0].active_source == DEMO_SOURCE_KEYBOARD) {
        demo_mouse_world(app, &app->aim_world_x[0], &app->aim_world_y[0]);
    }
    if (app->screen == DEMO_PLAY && app->mouse_inside &&
        app->player_input[0].active_source == DEMO_SOURCE_KEYBOARD) {
        vox_ui_frame(&demo_ui, app->mouse_x - 3, app->mouse_y - 3,
                     7, 7, DEMO_VGA_YELLOW);
        vox_ui_rect(&demo_ui, app->mouse_x, app->mouse_y, 1, 1,
                    DEMO_VGA_WHITE);
    }
    demo_draw_world_feedback(app);
    demo_draw_hud(app);
    demo_draw_debug(app);
    if (app->screen == DEMO_PAUSE) {
        demo_dark_panel(88, 65, 144, 70);
        vox_ui_text_center_shadow(&demo_ui, 160, 76, 2, "PAUSED",
                                  DEMO_VGA_YELLOW);
        if (app->controller_disconnected) {
            vox_ui_text_center(&demo_ui, 160, 94, 1,
                               "CONTROLLER LOST  RECONNECT OR RECLAIM",
                               DEMO_VGA_LIGHT_RED);
        }
        demo_menu_item(105, "RESUME", app->selection == 0);
        demo_menu_item(120, "EXIT TO TITLE", app->selection == 1);
    }
    demo_apply_flash(app);
}

static void demo_draw_results(demo_app *app)
{
    char line[64];
    demo_draw_play(app);
    demo_dark_panel(67, 48, 186, 104);
    vox_ui_text_center_shadow(&demo_ui, 160, 59, 2, "MATCH RESULTS",
                              DEMO_VGA_YELLOW);
    sprintf(line, "PLAYER SCORE %u", (unsigned int)demo_match.scores[0]);
    vox_ui_text_center(&demo_ui, 160, 91, 1, line, DEMO_VGA_WHITE);
    sprintf(line, "STATE HASH %08lX", (unsigned long)demo_match.state_hash);
    vox_ui_text_center(&demo_ui, 160, 105, 1, line,
                       DEMO_VGA_LIGHT_CYAN);
    vox_ui_text_center(&demo_ui, 160, 130, 1,
                       "ENTER RETURNS TO TITLE", DEMO_VGA_YELLOW);
}

static void demo_render(demo_app *app)
{
    if (app->screen == DEMO_TITLE) {
        demo_draw_title(app);
    } else if (app->screen == DEMO_SETUP) {
        demo_draw_setup(app);
    } else if (app->screen == DEMO_OPTIONS) {
        demo_draw_options(app);
    } else if (app->screen == DEMO_FEEDBACK) {
        demo_draw_feedback(app);
    } else if (app->screen == DEMO_HOW_TO) {
        demo_draw_how_to(app);
    } else if (app->screen == DEMO_INDEX) {
        demo_draw_index(app);
    } else if (app->screen == DEMO_CONTROLS) {
        demo_draw_controls(app);
    } else if (app->screen == DEMO_INPUT_OPTIONS) {
        demo_draw_input_options(app);
    } else if (app->screen == DEMO_SCRIPT_ERROR) {
        demo_draw_script_error(app);
    } else if (app->screen == DEMO_RESULTS) {
        demo_draw_results(app);
    } else {
        demo_draw_play(app);
    }
}

static void demo_prepare_foundry_world(void)
{
    vox_u32 x;
    vox_u32 y;
    vox_u32 z;
    for (z = 0U; z < VOX_WORLD_DEPTH; ++z) {
        for (x = 22U; x <= 29U; ++x) {
            (void)vox_world_set(&demo_match.world, x, 24U, z,
                                VOX_MAT_SAND, 20L << 16);
            (void)vox_world_set(&demo_match.world, x, 25U, z,
                                VOX_MAT_SAND, 20L << 16);
        }
        for (y = 31U; y <= 40U; ++y) {
            for (x = 36U; x <= 46U; ++x) {
                int boundary = y == 31U || y == 40U ||
                               x == 36U || x == 46U;
                (void)vox_world_set(&demo_match.world, x, y, z,
                                    boundary ? VOX_MAT_METAL : VOX_MAT_AIR,
                                    20L << 16);
            }
        }
    }
    for (y = 34U; y <= 38U; ++y) {
        for (x = 39U; x <= 43U; ++x) {
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
    vox_digs_rules_classic(&rules);
    rules.player_count = (vox_u16)(foundry ? 1 :
                         app->local_players + app->bots);
    rules.bot_mask = 0U;
    for (player = (vox_u16)(foundry ? 1 : app->local_players);
         player < rules.player_count; ++player) {
        rules.bot_mask = (vox_u16)(rules.bot_mask |
                                   (vox_u16)(1U << player));
    }
    rules.team_mode = (vox_u16)(foundry ? VOX_DIGS_MODE_FFA :
                                app->game_mode);
    rules.friendly_fire = (vox_u16)app->friendly_fire;
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
        rules.match_ticks = 2U * 60U * VOX_DIGS_TICKS_PER_SECOND;
        rules.lava_start_tick = rules.match_ticks -
                                30U * VOX_DIGS_TICKS_PER_SECOND;
    }
    if (vox_digs_match_init(&demo_match, &rules) != VOX_OK) {
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

static void demo_fire_player(demo_app *app, int player)
{
    if (player < 0 || player >= app->local_players ||
        !demo_match.alive[player]) {
        return;
    }
    demo_clamp_aim(app, player);
    if (vox_digs_fire_weapon(&demo_match, (vox_u16)player,
                             (vox_u16)app->selected_tool[player],
                             app->aim_world_x[player],
                             app->aim_world_y[player]) == VOX_OK) {
        vox_i16 pan = (vox_i16)((long)app->aim_world_x[player] * 65534L /
                                (long)(VOX_WORLD_WIDTH - 1U) - 32767L);
        demo_audio_emit(app, VOX_AUDIO_PRESET_FIRE,
                        (vox_u16)app->selected_tool[player], pan);
    }
}

static double demo_weapon_aim_range(demo_app *app, int player,
                                    int rope_held)
{
    const vox_digs_weapon_properties *properties;
    if (rope_held) return 30.0;
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
        if (demo_match.rules.team_mode == VOX_DIGS_MODE_MINERS_VS_MACHINES &&
            (demo_match.rules.bot_mask & (vox_u16)(1U << target)) == 0U) {
            continue;
        }
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
    int player;
    for (player = 0; player < app->local_players; ++player) {
        vox_digs_input input;
        demo_controller *controller = demo_controller_for_player(app, player);
        Sint16 move_x = 0;
        Sint16 move_y = 0;
        int fire = 0;
        int previous_down = 0;
        int next_down = 0;
        int use_controller;
        SDL_Scancode *left = demo_keyboard_binding(app, player, 0);
        SDL_Scancode *right = demo_keyboard_binding(app, player, 1);
        SDL_Scancode *jump = demo_keyboard_binding(app, player, 2);
        SDL_Scancode *steam = demo_keyboard_binding(app, player, 3);
        SDL_Scancode *rope = demo_keyboard_binding(app, player, 4);
        SDL_Scancode *fire_key = demo_keyboard_binding(app, player, 5);
        SDL_Scancode *previous_key = demo_keyboard_binding(app, player, 6);
        SDL_Scancode *next_key = demo_keyboard_binding(app, player, 7);
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
            if (player == 0 && app->mouse_inside) {
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
            if (rope != 0 && keys[*rope]) {
                input.actions = (vox_u16)(input.actions |
                                          VOX_DIGS_ACTION_ROPE);
            }
            if (fire_key != 0 && keys[*fire_key]) fire = 1;
            if (player == 0) {
                if (keys[SDL_SCANCODE_W]) move_y = -32767;
                if (keys[SDL_SCANCODE_S]) move_y = 32767;
            } else {
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
            int rope_held = demo_controller_button(controller,
                                                   app->bindings.pad_rope);
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
            if (rope_held) {
                input.actions = (vox_u16)(input.actions |
                                          VOX_DIGS_ACTION_ROPE);
            }
            if (demo_controller_button(controller, app->bindings.pad_fire) ||
                demo_controller_axis(controller,
                    SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) {
                fire = 1;
            }
            demo_update_controller_aim(app, player, controller,
                                       aim_x, aim_y, rope_held);
        }
        if (app->player_input[player].suppress_ticks > 0) {
            input.actions = 0U;
            move_x = 0;
            move_y = 0;
            fire = 0;
            previous_down = 0;
            next_down = 0;
            --app->player_input[player].suppress_ticks;
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
        if (fire) demo_fire_player(app, player);
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

static void demo_rumble_player(demo_app *app, vox_u16 player,
                               vox_u16 magnitude)
{
    demo_controller *controller;
    vox_u16 strength;
    if (player >= (vox_u16)app->local_players) return;
    controller = demo_controller_for_player(app, (int)player);
    if (controller == 0) return;
    strength = (vox_u16)(magnitude >= 100U ? 65535U :
                         10000U + (vox_u32)magnitude * 500U);
    if (controller->raw_fallback) {
        (void)SDL_JoystickRumble(controller->joystick,
                                strength, strength,
                                magnitude >= 100U ? 240U : 90U);
    } else {
        (void)SDL_GameControllerRumble(controller->handle,
                                      strength, strength,
                                      magnitude >= 100U ? 240U : 90U);
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
            if (event->target < (vox_u16)app->local_players) {
                app->camera_trauma += (double)event->magnitude / 180.0;
                app->flash_kind = 1;
                app->flash_strength = app->options.flash_mode == 2 ? 0.48 :
                                      (app->options.flash_mode == 1 ? 0.20 : 0.0);
                demo_rumble_player(app, event->target, event->magnitude);
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
        } else if (event->type == VOX_DIGS_EVENT_KILL) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_KILL,
                            event->variant, pan);
            if (demo_event_is_local(app, event)) {
                app->camera_trauma += 0.9;
                app->flash_kind = 1;
                app->flash_strength = app->options.flash_mode == 2 ? 0.75 :
                                      (app->options.flash_mode == 1 ? 0.30 : 0.0);
            }
            demo_rumble_player(app, event->target, 100U);
        } else if (event->type == VOX_DIGS_EVENT_SPAWN) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_SPAWN,
                            event->variant, pan);
        } else if (event->type == VOX_DIGS_EVENT_ROPE_ATTACH) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_ROPE_ATTACH,
                            event->variant, pan);
        } else if (event->type == VOX_DIGS_EVENT_ROPE_DETACH ||
                   event->type == VOX_DIGS_EVENT_ROPE_BREAK) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_ROPE_BREAK,
                            event->variant, pan);
        } else if (event->type == VOX_DIGS_EVENT_AI_BARK) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_BARK_ALERT,
                            event->variant, pan);
        } else if (event->type == VOX_DIGS_EVENT_WEAPON_FIRE &&
                   event->source >= (vox_u16)app->local_players) {
            demo_audio_emit(app, VOX_AUDIO_PRESET_FIRE,
                            event->variant, pan);
        }
        if (app->camera_trauma > 1.0) app->camera_trauma = 1.0;
    }
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
    }
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

static void demo_fire_at_mouse(demo_app *app)
{
    vox_u32 world_x;
    vox_u32 world_y;
    demo_mouse_world(app, &world_x, &world_y);
    if (world_x >= VOX_WORLD_WIDTH) {
        world_x = VOX_WORLD_WIDTH - 1U;
    }
    if (world_y >= VOX_WORLD_HEIGHT) {
        world_y = VOX_WORLD_HEIGHT - 1U;
    }
    app->aim_world_x[0] = world_x;
    app->aim_world_y[0] = world_y;
    demo_fire_player(app, 0);
}

static void demo_handle_title_key(demo_app *app, SDL_Keycode key)
{
    if (key == SDLK_UP) {
        app->selection = (app->selection + 7) % 8;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 8;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, DEMO_SOUND_SELECT);
        if (app->selection == 0) {
            app->screen = DEMO_SETUP;
            app->selection = 0;
        } else if (app->selection == 1) {
            (void)demo_start_match(app, 1);
        } else if (app->selection == 2) {
            app->screen = DEMO_HOW_TO;
            app->selection = 0;
        } else if (app->selection == 3) {
            app->screen = DEMO_INDEX;
            app->selection = 0;
            app->index_selection = 0U;
            app->index_scroll = 0U;
            app->index_visual_row = 0.0;
        } else if (app->selection == 4) {
            app->screen = DEMO_CONTROLS;
            app->selection = 0;
        } else if (app->selection == 5) {
            app->screen = DEMO_OPTIONS;
            app->selection = 0;
        } else if (app->selection == 6) {
            app->screen = DEMO_FEEDBACK;
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
        app->selection = (app->selection + 8) % 9;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 9;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (direction != 0) {
        demo_audio_play(app, DEMO_SOUND_MOVE);
        if (app->selection == 0) {
            app->local_players = app->local_players == 1 ? 2 : 1;
            if (app->local_players + app->bots > 4) {
                app->bots = 4 - app->local_players;
            }
            demo_refresh_controller_claims(app);
        } else if (app->selection == 1) {
            app->bots = (app->bots + direction + 3) % 3;
        } else if (app->selection == 2) {
            app->game_mode = 1 - app->game_mode;
        } else if (app->selection == 3) {
            app->friendly_fire = !app->friendly_fire;
        } else if (app->selection == 4) {
            app->map_style = (app->map_style + direction + 3) % 3;
        } else if (app->selection == 5) {
            app->seed += direction > 0 ? 1U : (vox_u32)-1;
        } else if (app->selection == 6) {
            app->arsenal = (app->arsenal + direction +
                            DEMO_ARSENAL_COUNT) % DEMO_ARSENAL_COUNT;
        }
    } else if (key == SDLK_r && app->selection == 5) {
        app->seed = app->seed * 1664525U + 1013904223U;
        demo_audio_play(app, DEMO_SOUND_SELECT);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, DEMO_SOUND_SELECT);
        if (app->selection == 7) {
            (void)demo_start_match(app, 0);
        } else if (app->selection == 8) {
            app->screen = DEMO_TITLE;
            app->selection = 0;
        }
    }
}

static void demo_handle_options_key(demo_app *app, SDL_Keycode key)
{
    int direction = key == SDLK_LEFT ? -1 : (key == SDLK_RIGHT ? 1 : 0);
    int change = direction == 0 ? 1 : direction;
    if (key == SDLK_ESCAPE) {
        app->screen = DEMO_TITLE;
        app->selection = 0;
    } else if (key == SDLK_UP) {
        app->selection = (app->selection + 11) % 12;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 12;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (direction != 0 || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, direction == 0 ? DEMO_SOUND_SELECT :
                        DEMO_SOUND_MOVE);
        if (app->selection == 0) {
            app->options.frame_cap_index =
                (app->options.frame_cap_index + change + 6) % 6;
        } else if (app->selection == 1) {
            app->options.gi_quality =
                (app->options.gi_quality + change + 3) % 3;
        } else if (app->selection == 2) {
            app->options.fx_profile =
                (app->options.fx_profile + change + 3) % 3;
        } else if (app->selection == 3) {
            app->options.flash_mode =
                (app->options.flash_mode + change + 3) % 3;
        } else if (app->selection == 4) {
            app->options.gore_level =
                (app->options.gore_level + change + 3) % 3;
        } else if (app->selection == 5) {
            app->options.camera_shake = !app->options.camera_shake;
        } else if (app->selection == 6) {
            app->options.damage_numbers = !app->options.damage_numbers;
        } else if (app->selection == 7) {
            app->options.damage_number_size =
                !app->options.damage_number_size;
        } else if (app->selection == 8) {
            app->options.damage_number_color =
                !app->options.damage_number_color;
        } else if (app->selection == 9) {
            app->options.fullscreen = !app->options.fullscreen;
            demo_apply_fullscreen(app);
        } else if (app->selection == 10 && direction == 0) {
            app->screen = DEMO_INPUT_OPTIONS;
            app->selection = 0;
        } else if (app->selection == 11 && direction == 0) {
            app->screen = DEMO_TITLE;
            app->selection = 0;
        }
    }
}

static void demo_handle_input_options_key(demo_app *app, SDL_Keycode key)
{
    int direction = key == SDLK_LEFT ? -1 : (key == SDLK_RIGHT ? 1 : 0);
    int change = direction == 0 ? 1 : direction;
    int player = app->selection >= 4 && app->selection <= 7 ? 1 : 0;
    int field = app->selection - player * 4;
    if (key == SDLK_ESCAPE) {
        app->screen = DEMO_OPTIONS;
        app->selection = 10;
    } else if (key == SDLK_UP) {
        app->selection = (app->selection + 10) % 11;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 11;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (direction != 0 || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, direction == 0 ? DEMO_SOUND_SELECT :
                        DEMO_SOUND_MOVE);
        if (app->selection <= 7) {
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
            }
            (void)demo_save_input_settings(app);
        } else if (app->selection == 8 && direction == 0) {
            demo_begin_controller_calibration(app);
        } else if (app->selection == 9 && direction == 0) {
            demo_input_defaults(app);
            demo_refresh_controller_claims(app);
            (void)demo_save_input_settings(app);
        } else if (app->selection == 10 && direction == 0) {
            app->screen = DEMO_OPTIONS;
            app->selection = 10;
        }
    }
}

static void demo_handle_index_key(demo_app *app, SDL_Keycode key)
{
    const vox_script_catalog *catalog = app->scripts_ready ?
        vox_script_catalog_get(&app->scripts) : 0;
    vox_u16 count = catalog == 0 ? 0U : catalog->entry_count;
    if (key == SDLK_ESCAPE) {
        app->screen = DEMO_TITLE;
        app->selection = 0;
        demo_audio_play(app, DEMO_SOUND_PAUSE);
    } else if (count > 0U && key == SDLK_UP) {
        if (app->index_selection > 0U) --app->index_selection;
        if (app->index_selection < app->index_scroll) {
            app->index_scroll = app->index_selection;
        }
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (count > 0U && key == SDLK_DOWN) {
        if (app->index_selection + 1U < count) ++app->index_selection;
        if (app->index_selection >= app->index_scroll +
                                    VOX_SCRIPT_INDEX_VISIBLE_ROWS) {
            app->index_scroll = (vox_u16)(app->index_selection -
                                VOX_SCRIPT_INDEX_VISIBLE_ROWS + 1U);
        }
        demo_audio_play(app, DEMO_SOUND_MOVE);
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
            demo_audio_play(app, DEMO_SOUND_SELECT);
        }
        return;
    }
    if (key == SDLK_ESCAPE) {
        app->screen = DEMO_TITLE;
        app->selection = 0;
    } else if (key == SDLK_UP) {
        app->selection = (app->selection + 10) % 11;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 11;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (app->selection == 0 &&
               (key == SDLK_LEFT || key == SDLK_RIGHT)) {
        int direction = key == SDLK_LEFT ? -1 : 1;
        app->binding_player = (app->binding_player + direction + 3) % 3;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (app->selection >= 1 && app->selection <= 8) {
            int action = app->selection - 1;
            if (app->binding_player < 2 || action >= 2) {
                app->binding_capture = action + 1;
                demo_audio_play(app, DEMO_SOUND_SELECT);
            }
        } else if (app->selection == 9) {
            demo_bindings_default(app);
            demo_audio_play(app, DEMO_SOUND_SELECT);
        } else if (app->selection == 10) {
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
    } else if (key == SDLK_F5) {
        if (demo_scripts_reload(app) && app->screen == DEMO_SCRIPT_ERROR) {
            app->screen = DEMO_TITLE;
            app->selection = 0;
        }
    } else if (key == SDLK_F11) {
        app->options.fullscreen = !app->options.fullscreen;
        demo_apply_fullscreen(app);
    } else if (app->screen == DEMO_SCRIPT_ERROR) {
        if (key == SDLK_ESCAPE) app->running = 0;
    } else if (app->screen == DEMO_TITLE) {
        demo_handle_title_key(app, key);
    } else if (app->screen == DEMO_SETUP) {
        demo_handle_setup_key(app, key);
    } else if (app->screen == DEMO_OPTIONS) {
        demo_handle_options_key(app, key);
    } else if (app->screen == DEMO_INPUT_OPTIONS) {
        demo_handle_input_options_key(app, key);
    } else if (app->screen == DEMO_INDEX) {
        demo_handle_index_key(app, key);
    } else if (app->screen == DEMO_CONTROLS) {
        demo_handle_controls_key(app, key, scancode);
    } else if ((app->screen == DEMO_HOW_TO ||
                app->screen == DEMO_FEEDBACK) &&
               (key == SDLK_RETURN || key == SDLK_KP_ENTER ||
                key == SDLK_ESCAPE)) {
        app->screen = DEMO_TITLE;
        app->selection = 0;
        demo_audio_play(app, DEMO_SOUND_SELECT);
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
        for (action = 0; action < 8; ++action) {
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
        demo_audio_play(app, DEMO_SOUND_SELECT);
        return;
    }
    if (app->screen == DEMO_PLAY && controller != 0) {
        if (button == SDL_CONTROLLER_BUTTON_START) {
            demo_handle_key(app, SDLK_ESCAPE, SDL_SCANCODE_UNKNOWN);
            return;
        }
        if (player >= 0) {
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
        if (button == SDL_CONTROLLER_BUTTON_DPAD_UP) key = SDLK_UP;
        else if (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) key = SDLK_DOWN;
        else if (button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) key = SDLK_LEFT;
        else if (button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) key = SDLK_RIGHT;
        else if (button == SDL_CONTROLLER_BUTTON_A ||
                 button == SDL_CONTROLLER_BUTTON_START) key = SDLK_RETURN;
        else if (button == SDL_CONTROLLER_BUTTON_B) key = SDLK_ESCAPE;
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
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN &&
               event->button.button == SDL_BUTTON_LEFT &&
               app->screen == DEMO_PLAY) {
        if (demo_sync_hardware_mouse(app)) {
            int changed = demo_activate_source(app, 0,
                                               DEMO_SOURCE_KEYBOARD, 1);
            if (!changed && app->player_input[0].active_source ==
                            DEMO_SOURCE_KEYBOARD) {
                demo_fire_at_mouse(app);
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
            } else {
                app->camera_zoom += direction;
                if (app->camera_zoom < DEMO_CAMERA_ZOOM_MIN) {
                    app->camera_zoom = DEMO_CAMERA_ZOOM_MIN;
                } else if (app->camera_zoom > DEMO_CAMERA_ZOOM_MAX) {
                    app->camera_zoom = DEMO_CAMERA_ZOOM_MAX;
                }
            }
            demo_audio_play(app, DEMO_SOUND_MOVE);
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
    app.camera_zoom = 0;
    app.camera_scale = 2.0;
    app.frame_seconds = 1.0 / 60.0;
    app.options.debug = 1;
    app.measured_fps = 60.0;
    demo_render(&app);
    if (app.camera_zoom != DEMO_CAMERA_ZOOM_DEFAULT) {
        return 4;
    }
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
        demo_weapon_aim_range(&app, 0, 1) != 30.0) {
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
    printf("DIGS input self-test passed\n");
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

int main(int argc, char **argv)
{
    demo_app app;
    SDL_Event event;
    Uint64 frequency;
    Uint64 previous_counter;
    double accumulator = 0.0;
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
    memset(&app, 0, sizeof(app));
    app.running = 1;
    app.screen = DEMO_TITLE;
    app.bots = 1;
    app.local_players = 1;
    app.game_mode = VOX_DIGS_MODE_FFA;
    app.friendly_fire = 0;
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
    app.options.frame_cap_index = 1;
    app.options.gi_quality = VOX_GI_BALANCED;
    app.options.flash_mode = 2;
    app.options.gore_level = 2;
    app.options.camera_shake = 1;
    app.options.damage_numbers = 1;
    app.options.damage_number_size = 0;
    app.options.fx_profile = 1;
    demo_input_defaults(&app);
    demo_bindings_default(&app);
    for (controller_index = 0;
         controller_index < (int)DEMO_CONTROLLER_MAX;
         ++controller_index) {
        app.controllers[controller_index].instance_id = -1;
        app.controllers[controller_index].claimed_player = -1;
    }
    SDL_SetMainReady();
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
    app.window = SDL_CreateWindow("DIGS v0.0.2 Demo",
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  DEMO_WINDOW_WIDTH, DEMO_WINDOW_HEIGHT,
                                  SDL_WINDOW_RESIZABLE);
    if (app.window == 0) {
        fprintf(stderr, "window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 3;
    }
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
    demo_prepare_targets();
    demo_build_title_world();
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0U) {
        for (controller_index = 0;
             controller_index < SDL_NumJoysticks(); ++controller_index) {
            (void)demo_open_controller(&app, controller_index);
        }
    }
    if (!demo_scripts_open(&app)) {
        app.screen = DEMO_SCRIPT_ERROR;
    }
    frequency = SDL_GetPerformanceFrequency();
    if (frequency == 0U) {
        fprintf(stderr, "performance timer unavailable: %s\n", SDL_GetError());
        demo_scripts_close(&app);
        demo_close_controllers(&app);
        demo_audio_close(&app);
        SDL_DestroyTexture(app.texture);
        SDL_DestroyRenderer(app.renderer);
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 7;
    }
    previous_counter = SDL_GetPerformanceCounter();
    app.fps_stamp = SDL_GetTicks();
    while (app.running) {
        Uint64 current_counter = SDL_GetPerformanceCounter();
        double elapsed = (double)(current_counter - previous_counter) /
                         (double)frequency;
        vox_u32 catchup = 0U;
        int frame_cap;
        previous_counter = current_counter;
        if (elapsed > 0.25) {
            elapsed = 0.25;
        }
        app.frame_seconds = elapsed;
        accumulator += elapsed;
        while (SDL_PollEvent(&event)) {
            demo_handle_event(&app, &event);
        }
        (void)demo_sync_hardware_mouse(&app);
        demo_update_cursor_visibility(&app);
        while (accumulator >= 1.0 / (double)DEMO_SIM_HZ &&
               catchup < DEMO_MAX_CATCHUP) {
            demo_tick(&app);
            accumulator -= 1.0 / (double)DEMO_SIM_HZ;
            ++catchup;
        }
        if (catchup == DEMO_MAX_CATCHUP) {
            accumulator = 0.0;
        }
        app.render_alpha = accumulator * (double)DEMO_SIM_HZ;
        if (app.render_alpha < 0.0) app.render_alpha = 0.0;
        if (app.render_alpha > 1.0) app.render_alpha = 1.0;
        demo_render(&app);
        demo_audio_pump(&app);
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
    demo_scripts_close(&app);
    demo_close_controllers(&app);
    demo_audio_close(&app);
    SDL_DestroyTexture(app.texture);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}

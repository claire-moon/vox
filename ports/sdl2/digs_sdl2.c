/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

#include "vox/vox_game.h"
#include "vox/vox_render.h"
#include "vox_sdl_ui.h"

#define DEMO_WIDTH 320U
#define DEMO_HEIGHT 200U
#define DEMO_WINDOW_WIDTH 1280
#define DEMO_WINDOW_HEIGHT 800
#define DEMO_SIM_HZ 60U
#define DEMO_MAX_CATCHUP 8U
#define DEMO_AUDIO_RATE 22050
#define DEMO_AUDIO_MAX_SAMPLES 3087

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
    DEMO_FEEDBACK = 6
} demo_screen;

typedef struct demo_options {
    int frame_cap_index;
    int gi_quality;
    int debug;
    int fullscreen;
} demo_options;

typedef struct demo_app {
    int running;
    demo_screen screen;
    int selection;
    int bots;
    int map_style;
    int arsenal;
    vox_u32 seed;
    int selected_tool;
    int mouse_x;
    int mouse_y;
    int foundry;
    double measured_fps;
    vox_u32 rendered_frames;
    vox_u32 fps_stamp;
    demo_options options;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_AudioDeviceID audio_device;
    vox_u32 audio_noise;
} demo_app;

static vox_u8 demo_pixels[DEMO_WIDTH * DEMO_HEIGHT * VOX_SOFTWARE_RGB_BYTES];
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
static const char *demo_arsenal_names[DEMO_ARSENAL_COUNT] = {
    "FULL WORKS", "MINER KIT", "POWDER KEG"
};
static const vox_u16 demo_arsenal_masks[DEMO_ARSENAL_COUNT] = {
    0x03FFU, 0x00F1U, 0x030EU
};

static void demo_audio_play(demo_app *app, int sound)
{
    Sint16 samples[DEMO_AUDIO_MAX_SAMPLES];
    int count;
    int frequency;
    int volume;
    int noise_mix;
    int phase;
    int index;
    vox_u32 noise;
    if (app == 0 || app->audio_device == 0U) {
        return;
    }
    count = DEMO_AUDIO_RATE * 45 / 1000;
    frequency = 420;
    volume = 3500;
    noise_mix = 0;
    if (sound == DEMO_SOUND_SELECT) {
        count = DEMO_AUDIO_RATE * 70 / 1000;
        frequency = 720;
        volume = 4700;
    } else if (sound == DEMO_SOUND_FIRE) {
        count = DEMO_AUDIO_RATE * 110 / 1000;
        frequency = 95;
        volume = 9000;
        noise_mix = 3;
    } else if (sound == DEMO_SOUND_START) {
        count = DEMO_AUDIO_RATE * 140 / 1000;
        frequency = 235;
        volume = 6500;
    } else if (sound == DEMO_SOUND_PAUSE) {
        count = DEMO_AUDIO_RATE * 85 / 1000;
        frequency = 165;
        volume = 5000;
    }
    if (count > DEMO_AUDIO_MAX_SAMPLES) {
        count = DEMO_AUDIO_MAX_SAMPLES;
    }
    phase = 0;
    noise = app->audio_noise;
    for (index = 0; index < count; ++index) {
        int tone;
        int random_value;
        int envelope;
        long mixed;
        int pitch = frequency;
        if (sound == DEMO_SOUND_START) {
            pitch += index * 260 / count;
        } else if (sound == DEMO_SOUND_FIRE) {
            pitch += index * 35 / count;
        }
        phase += pitch;
        while (phase >= DEMO_AUDIO_RATE) {
            phase -= DEMO_AUDIO_RATE;
        }
        tone = phase < DEMO_AUDIO_RATE / 2 ? volume : -volume;
        noise = noise * 1664525U + 1013904223U;
        random_value = (int)((noise >> 17) & 0x7FFFU) - 16384;
        if (noise_mix != 0) {
            tone = (tone + random_value * noise_mix) / (noise_mix + 1);
        }
        envelope = count - index;
        mixed = (long)tone * (long)envelope / (long)count;
        samples[index] = (Sint16)mixed;
    }
    app->audio_noise = noise;
    if (SDL_GetQueuedAudioSize(app->audio_device) >
        (Uint32)(DEMO_AUDIO_RATE * (int)sizeof(Sint16) / 3)) {
        SDL_ClearQueuedAudio(app->audio_device);
    }
    (void)SDL_QueueAudio(app->audio_device, samples,
                         (Uint32)(count * (int)sizeof(Sint16)));
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
    desired.channels = 1U;
    desired.samples = 512U;
    desired.callback = 0;
    app->audio_device = SDL_OpenAudioDevice(0, 0, &desired, &obtained, 0);
    if (app->audio_device == 0U) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    app->audio_noise = 0xD1655EEDU;
    SDL_PauseAudioDevice(app->audio_device, 0);
}

static void demo_audio_close(demo_app *app)
{
    if (app != 0 && app->audio_device != 0U) {
        SDL_ClearQueuedAudio(app->audio_device);
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

static void demo_cycle_weapon(demo_app *app, int direction)
{
    int step;
    for (step = 1; step <= (int)VOX_DIGS_TOOL_COUNT; ++step) {
        int weapon = app->selected_tool + direction * step;
        while (weapon < 0) {
            weapon += (int)VOX_DIGS_TOOL_COUNT;
        }
        weapon %= (int)VOX_DIGS_TOOL_COUNT;
        if (demo_weapon_is_allowed(demo_match.rules.weapon_mask, weapon)) {
            app->selected_tool = weapon;
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
    demo_dark_panel(62, 18, 196, 164);
    vox_ui_text_center_shadow(&demo_ui, 160, 29, 4, "VOX",
                              DEMO_VGA_YELLOW);
    vox_ui_text_center_shadow(&demo_ui, 160, 62, 2, "DIGS",
                              DEMO_VGA_LIGHT_CYAN);
    vox_ui_rect(&demo_ui, 82, 84, 156, 1, DEMO_VGA_DARK_GRAY);
    vox_ui_rect(&demo_ui, 82, 85, 156, 1, DEMO_VGA_BROWN);
    demo_menu_item(94, "START MATCH", app->selection == 0);
    demo_menu_item(110, "VOX FOUNDRY LAB", app->selection == 1);
    demo_menu_item(126, "OPTIONS", app->selection == 2);
    demo_menu_item(142, "QA FEEDBACK", app->selection == 3);
    demo_menu_item(158, "QUIT", app->selection == 4);
    vox_ui_text_center(&demo_ui, 160, 171, 1,
                       "GPL-3.0-OR-LATER  V0.0.1 DEMO",
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
    demo_dark_panel(32, 18, 256, 166);
    vox_ui_text_center_shadow(&demo_ui, 160, 28, 2, "MATCH SETUP",
                              DEMO_VGA_YELLOW);
    sprintf(value, "%d", app->bots);
    demo_value_line(61, "BOTS", value, app->selection == 0);
    demo_value_line(79, "MAP", demo_map_names[app->map_style],
                    app->selection == 1);
    sprintf(value, "%08lX", (unsigned long)app->seed);
    demo_value_line(97, "SEED", value, app->selection == 2);
    demo_value_line(115, "ARSENAL", demo_arsenal_names[app->arsenal],
                    app->selection == 3);
    demo_menu_item(140, "START DEATHMATCH", app->selection == 4);
    demo_menu_item(157, "BACK", app->selection == 5);
    vox_ui_text_center(&demo_ui, 160, 174, 1,
                       "ARROWS CHANGE  ENTER SELECTS", DEMO_VGA_DARK_GRAY);
}

static void demo_draw_options(demo_app *app)
{
    const char *toggle;
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    (void)vox_software_render_ex(&demo_title_world, &demo_target,
                                 &demo_render_config);
    demo_dark_panel(32, 18, 256, 166);
    vox_ui_text_center_shadow(&demo_ui, 160, 28, 2, "OPTIONS",
                              DEMO_VGA_YELLOW);
    demo_value_line(62, "FRAME CAP", demo_frame_names[app->options.frame_cap_index],
                    app->selection == 0);
    demo_value_line(82, "VOX LIGHTFIELD", demo_gi_names[app->options.gi_quality],
                    app->selection == 1);
    toggle = app->options.debug ? "ON" : "OFF";
    demo_value_line(102, "DEBUG OVERLAY", toggle, app->selection == 2);
    toggle = app->options.fullscreen ? "ON" : "OFF";
    demo_value_line(122, "FULLSCREEN", toggle, app->selection == 3);
    demo_menu_item(151, "BACK", app->selection == 4);
    vox_ui_text_center(&demo_ui, 160, 174, 1,
                       "SIMULATION REMAINS FIXED AT 60 HZ",
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
    int x = demo_q16_to_screen(body->position_x.value_q16,
                               VOX_WORLD_WIDTH, VOX_WORLD_WIDTH);
    int y = demo_q16_to_screen(body->position_y.value_q16,
                               VOX_WORLD_HEIGHT, VOX_WORLD_HEIGHT);
    int lamp_x = demo_match.facing_right[player] ? x + 2 : x - 2;
    int row;
    int column;
    for (column = -1; column <= 1; ++column) {
        demo_render_voxel(x + column, y - 4, VOX_MAT_METAL);
        demo_render_voxel(x + column, y - 3, VOX_MAT_FLESH);
    }
    demo_render_voxel(lamp_x, y - 4, VOX_MAT_LAVA);
    for (row = -2; row <= 0; ++row) {
        for (column = -1; column <= 1; ++column) {
            demo_render_voxel(x + column, y + row, coats[player]);
        }
    }
    demo_render_voxel(x - 2, y - 1, coats[player]);
    demo_render_voxel(x + 2, y - 1, coats[player]);
    demo_render_voxel(x - 1, y + 1, VOX_MAT_METAL);
    demo_render_voxel(x + 1, y + 1, VOX_MAT_METAL);
    demo_render_voxel(x - 1, y + 2, VOX_MAT_COAL);
    demo_render_voxel(x + 1, y + 2, VOX_MAT_COAL);
}

static void demo_build_render_world(void)
{
    vox_u16 player;
    vox_u16 index;
    memcpy(&demo_render_world, &demo_match.world, sizeof(demo_render_world));
    for (player = 0U; player < VOX_DIGS_MAX_SLOTS; ++player) {
        if (demo_match.alive[player]) {
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
    for (index = 0U; index < VOX_DIGS_MAX_EFFECTS; ++index) {
        const vox_digs_effect *effect = &demo_match.effects[index];
        if (effect->active) {
            int x = demo_q16_to_screen(effect->position_x_q16,
                                       VOX_WORLD_WIDTH, VOX_WORLD_WIDTH);
            int y = demo_q16_to_screen(effect->position_y_q16,
                                       VOX_WORLD_HEIGHT, VOX_WORLD_HEIGHT);
            demo_render_voxel(x, y, effect->material);
        }
    }
}

static void demo_draw_hud(demo_app *app)
{
    char text[96];
    const vox_digs_weapon_properties *weapon =
        vox_digs_weapon_get((vox_u16)app->selected_tool);
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
    vox_ui_text(&demo_ui, 179, 23, 1, "1-0 OR WHEEL SELECT",
                DEMO_VGA_DARK_GRAY);
}

static void demo_draw_debug(demo_app *app)
{
    char text[128];
    const vox_cell *cell;
    vox_u32 world_x = (vox_u32)app->mouse_x * VOX_WORLD_WIDTH / DEMO_WIDTH;
    vox_u32 world_y = (vox_u32)app->mouse_y * VOX_WORLD_HEIGHT / DEMO_HEIGHT;
    if (!app->options.debug) {
        return;
    }
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

static void demo_draw_play(demo_app *app)
{
    demo_render_config.gi_quality = (vox_u16)app->options.gi_quality;
    demo_build_render_world();
    (void)vox_software_render_ex(&demo_render_world, &demo_target,
                                 &demo_render_config);
    vox_ui_frame(&demo_ui, app->mouse_x - 3, app->mouse_y - 3,
                 7, 7, DEMO_VGA_YELLOW);
    vox_ui_rect(&demo_ui, app->mouse_x, app->mouse_y, 1, 1,
                DEMO_VGA_WHITE);
    demo_draw_hud(app);
    demo_draw_debug(app);
    if (app->screen == DEMO_PAUSE) {
        demo_dark_panel(88, 65, 144, 70);
        vox_ui_text_center_shadow(&demo_ui, 160, 76, 2, "PAUSED",
                                  DEMO_VGA_YELLOW);
        demo_menu_item(105, "RESUME", app->selection == 0);
        demo_menu_item(120, "EXIT TO TITLE", app->selection == 1);
    }
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
    vox_digs_rules_classic(&rules);
    rules.bot_count = (vox_u16)(foundry ? 0 : app->bots);
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
    app->selected_tool = demo_first_weapon(rules.weapon_mask);
    app->screen = DEMO_PLAY;
    app->selection = 0;
    demo_audio_play(app, DEMO_SOUND_START);
    return 1;
}

static void demo_submit_human_input(void)
{
    const vox_u8 *keys = SDL_GetKeyboardState(0);
    vox_digs_input input;
    input.abi_version = VOX_ABI_VERSION;
    input.struct_size = (vox_u32)sizeof(input);
    input.player = 0U;
    input.actions = 0U;
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
        input.actions = (vox_u16)(input.actions | VOX_DIGS_ACTION_LEFT);
    }
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
        input.actions = (vox_u16)(input.actions | VOX_DIGS_ACTION_RIGHT);
    }
    if (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_W] ||
        keys[SDL_SCANCODE_UP]) {
        input.actions = (vox_u16)(input.actions | VOX_DIGS_ACTION_JUMP);
    }
    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
        input.actions = (vox_u16)(input.actions | VOX_DIGS_ACTION_STEAM);
    }
    (void)vox_digs_submit_input(&demo_match, &input);
}

static void demo_tick(demo_app *app)
{
    if (app->screen != DEMO_PLAY) {
        return;
    }
    demo_submit_human_input();
    if (vox_digs_match_step(&demo_match) != VOX_OK) {
        app->screen = DEMO_RESULTS;
        return;
    }
    if (demo_match.phase == VOX_DIGS_RESULTS) {
        app->screen = DEMO_RESULTS;
    }
}

static void demo_apply_fullscreen(demo_app *app)
{
    Uint32 flags = app->options.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0U;
    (void)SDL_SetWindowFullscreen(app->window, flags);
}

static int demo_window_to_logical(demo_app *app, int window_x, int window_y,
                                  int *logical_x, int *logical_y)
{
    int window_width;
    int window_height;
    int view_width;
    int view_height;
    int view_x;
    int view_y;
    int inside;
    double scale_x;
    double scale_y;
    double scale;
    SDL_GetWindowSize(app->window, &window_width, &window_height);
    if (window_width <= 0 || window_height <= 0) {
        *logical_x = 0;
        *logical_y = 0;
        return 0;
    }
    scale_x = (double)window_width / (double)DEMO_WIDTH;
    scale_y = (double)window_height / (double)DEMO_HEIGHT;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale >= 1.0) {
        scale = (double)(int)scale;
    }
    view_width = (int)((double)DEMO_WIDTH * scale);
    view_height = (int)((double)DEMO_HEIGHT * scale);
    if (view_width <= 0 || view_height <= 0) {
        *logical_x = 0;
        *logical_y = 0;
        return 0;
    }
    view_x = (window_width - view_width) / 2;
    view_y = (window_height - view_height) / 2;
    inside = window_x >= view_x && window_y >= view_y &&
             window_x < view_x + view_width &&
             window_y < view_y + view_height;
    *logical_x = (int)((double)(window_x - view_x) / scale);
    *logical_y = (int)((double)(window_y - view_y) / scale);
    if (*logical_x < 0) {
        *logical_x = 0;
    } else if (*logical_x >= (int)DEMO_WIDTH) {
        *logical_x = (int)DEMO_WIDTH - 1;
    }
    if (*logical_y < 0) {
        *logical_y = 0;
    } else if (*logical_y >= (int)DEMO_HEIGHT) {
        *logical_y = (int)DEMO_HEIGHT - 1;
    }
    return inside;
}

static void demo_fire_at_mouse(demo_app *app)
{
    vox_u32 world_x = (vox_u32)app->mouse_x * VOX_WORLD_WIDTH / DEMO_WIDTH;
    vox_u32 world_y = (vox_u32)app->mouse_y * VOX_WORLD_HEIGHT / DEMO_HEIGHT;
    if (world_x >= VOX_WORLD_WIDTH) {
        world_x = VOX_WORLD_WIDTH - 1U;
    }
    if (world_y >= VOX_WORLD_HEIGHT) {
        world_y = VOX_WORLD_HEIGHT - 1U;
    }
    if (vox_digs_fire_weapon(&demo_match, 0U,
                             (vox_u16)app->selected_tool,
                             world_x, world_y) == VOX_OK) {
        demo_audio_play(app, DEMO_SOUND_FIRE);
    }
}

static void demo_handle_title_key(demo_app *app, SDL_Keycode key)
{
    if (key == SDLK_UP) {
        app->selection = (app->selection + 4) % 5;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 5;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, DEMO_SOUND_SELECT);
        if (app->selection == 0) {
            app->screen = DEMO_SETUP;
            app->selection = 0;
        } else if (app->selection == 1) {
            (void)demo_start_match(app, 1);
        } else if (app->selection == 2) {
            app->screen = DEMO_OPTIONS;
            app->selection = 0;
        } else if (app->selection == 3) {
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
        app->selection = (app->selection + 5) % 6;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 6;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (direction != 0) {
        demo_audio_play(app, DEMO_SOUND_MOVE);
        if (app->selection == 0) {
            app->bots = (app->bots + direction + 4) % 4;
        } else if (app->selection == 1) {
            app->map_style = (app->map_style + direction + 3) % 3;
        } else if (app->selection == 2) {
            app->seed += direction > 0 ? 1U : (vox_u32)-1;
        } else if (app->selection == 3) {
            app->arsenal = (app->arsenal + direction +
                            DEMO_ARSENAL_COUNT) % DEMO_ARSENAL_COUNT;
        }
    } else if (key == SDLK_r && app->selection == 2) {
        app->seed = app->seed * 1664525U + 1013904223U;
        demo_audio_play(app, DEMO_SOUND_SELECT);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, DEMO_SOUND_SELECT);
        if (app->selection == 4) {
            (void)demo_start_match(app, 0);
        } else if (app->selection == 5) {
            app->screen = DEMO_TITLE;
            app->selection = 0;
        }
    }
}

static void demo_handle_options_key(demo_app *app, SDL_Keycode key)
{
    int direction = key == SDLK_LEFT ? -1 : (key == SDLK_RIGHT ? 1 : 0);
    if (key == SDLK_ESCAPE) {
        app->screen = DEMO_TITLE;
        app->selection = 0;
    } else if (key == SDLK_UP) {
        app->selection = (app->selection + 4) % 5;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (key == SDLK_DOWN) {
        app->selection = (app->selection + 1) % 5;
        demo_audio_play(app, DEMO_SOUND_MOVE);
    } else if (direction != 0 || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        demo_audio_play(app, direction == 0 ? DEMO_SOUND_SELECT :
                        DEMO_SOUND_MOVE);
        if (app->selection == 0 && direction != 0) {
            app->options.frame_cap_index =
                (app->options.frame_cap_index + direction + 6) % 6;
        } else if (app->selection == 1 && direction != 0) {
            app->options.gi_quality =
                (app->options.gi_quality + direction + 3) % 3;
        } else if (app->selection == 2) {
            app->options.debug = !app->options.debug;
        } else if (app->selection == 3) {
            app->options.fullscreen = !app->options.fullscreen;
            demo_apply_fullscreen(app);
        } else if (app->selection == 4 && direction == 0) {
            app->screen = DEMO_TITLE;
            app->selection = 0;
        }
    }
}

static void demo_handle_event(demo_app *app, const SDL_Event *event)
{
    if (event->type == SDL_QUIT) {
        app->running = 0;
    } else if (event->type == SDL_MOUSEMOTION) {
        (void)demo_window_to_logical(app, event->motion.x, event->motion.y,
                                     &app->mouse_x, &app->mouse_y);
    } else if (event->type == SDL_MOUSEBUTTONDOWN &&
               event->button.button == SDL_BUTTON_LEFT &&
               app->screen == DEMO_PLAY) {
        int logical_x;
        int logical_y;
        if (demo_window_to_logical(app, event->button.x, event->button.y,
                                   &logical_x, &logical_y)) {
            app->mouse_x = logical_x;
            app->mouse_y = logical_y;
            demo_fire_at_mouse(app);
        }
    } else if (event->type == SDL_MOUSEWHEEL && app->screen == DEMO_PLAY) {
        if (event->wheel.y != 0) {
            int direction = event->wheel.y > 0 ? 1 : -1;
            if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                direction = -direction;
            }
            demo_cycle_weapon(app, direction);
            demo_audio_play(app, DEMO_SOUND_MOVE);
        }
    } else if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        SDL_Keycode key = event->key.keysym.sym;
        int weapon_key = demo_key_weapon(key);
        if (key == SDLK_F1) {
            app->options.debug = !app->options.debug;
        } else if (key == SDLK_F11) {
            app->options.fullscreen = !app->options.fullscreen;
            demo_apply_fullscreen(app);
        } else if (app->screen == DEMO_TITLE) {
            demo_handle_title_key(app, key);
        } else if (app->screen == DEMO_SETUP) {
            demo_handle_setup_key(app, key);
        } else if (app->screen == DEMO_OPTIONS) {
            demo_handle_options_key(app, key);
        } else if (app->screen == DEMO_FEEDBACK &&
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
                app->selected_tool = weapon_key;
                demo_audio_play(app, DEMO_SOUND_MOVE);
            }
        } else if (app->screen == DEMO_PLAY && key == SDLK_r) {
            (void)demo_start_match(app, app->foundry);
        }
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
    app.map_style = VOX_DIGS_MAP_FURNACE_YARD;
    app.arsenal = DEMO_ARSENAL_FULL;
    app.seed = 0xD1655EEDU;
    app.options.gi_quality = VOX_GI_BALANCED;
    demo_prepare_targets();
    vox_digs_rules_classic(&rules);
    rules.match_ticks = 1200U;
    rules.score_limit = 100U;
    rules.lava_start_tick = 600U;
    rules.bot_count = 1U;
    rules.map_style = VOX_DIGS_MAP_FURNACE_YARD;
    rules.weapon_mask = demo_arsenal_masks[DEMO_ARSENAL_FULL];
    rules.seed = app.seed;
    if (vox_digs_match_init(&demo_match, &rules) != VOX_OK) {
        return 2;
    }
    min_lava_y = demo_match.lava_surface_y;
    for (tick = 0U; tick < 750U; ++tick) {
        vox_digs_input input;
        input.abi_version = VOX_ABI_VERSION;
        input.struct_size = (vox_u32)sizeof(input);
        input.player = 0U;
        input.actions = tick < 120U ? VOX_DIGS_ACTION_RIGHT :
                        (tick < 200U ? (VOX_DIGS_ACTION_RIGHT |
                                        VOX_DIGS_ACTION_STEAM) : 0U);
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
                app.selected_tool = weapon;
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
    app.options.debug = 1;
    app.measured_fps = 60.0;
    demo_render(&app);
    if (!demo_write_ppm(path)) {
        return 4;
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
    app.map_style = VOX_DIGS_MAP_COAL_RIDGE;
    app.arsenal = DEMO_ARSENAL_FULL;
    app.seed = 0x564F5831U;
    app.mouse_x = 160;
    app.mouse_y = 100;
    app.options.frame_cap_index = 1;
    app.options.gi_quality = VOX_GI_BALANCED;
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 2;
    }
    (void)SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    app.window = SDL_CreateWindow("VOX + DIGS v0.0.1 Demo",
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
    (void)SDL_RenderSetLogicalSize(app.renderer, (int)DEMO_WIDTH,
                                   (int)DEMO_HEIGHT);
    (void)SDL_RenderSetIntegerScale(app.renderer, SDL_TRUE);
    app.texture = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_RGB24,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    (int)DEMO_WIDTH, (int)DEMO_HEIGHT);
    if (app.texture == 0) {
        fprintf(stderr, "texture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(app.renderer);
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 5;
    }
    demo_audio_open(&app);
    demo_prepare_targets();
    demo_build_title_world();
    frequency = SDL_GetPerformanceFrequency();
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
        accumulator += elapsed;
        while (SDL_PollEvent(&event)) {
            demo_handle_event(&app, &event);
        }
        while (accumulator >= 1.0 / (double)DEMO_SIM_HZ &&
               catchup < DEMO_MAX_CATCHUP) {
            demo_tick(&app);
            accumulator -= 1.0 / (double)DEMO_SIM_HZ;
            ++catchup;
        }
        if (catchup == DEMO_MAX_CATCHUP) {
            accumulator = 0.0;
        }
        demo_render(&app);
        (void)SDL_UpdateTexture(app.texture, 0, demo_pixels,
                                (int)demo_ui.stride);
        (void)SDL_RenderClear(app.renderer);
        (void)SDL_RenderCopy(app.renderer, app.texture, 0, 0);
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
    demo_audio_close(&app);
    SDL_DestroyTexture(app.texture);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}

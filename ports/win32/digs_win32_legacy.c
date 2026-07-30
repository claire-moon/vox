/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * DIGS95.EXE is deliberately a small, ANSI Win32 host.  It avoids SDL2,
 * DirectX, XInput, Unicode-only APIs, and the modern Windows API-set DLLs so
 * the executable has a realistic historical compatibility path.  The
 * simulation, software renderer, bitmap UI, and synthesizer remain the same
 * portable VOX components used by the current desktop host.
 */
#ifndef WINVER
#define WINVER 0x0400
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <mmsystem.h>
#include <string.h>

#include "vox/vox_audio.h"
#include "vox/vox_game.h"
#include "vox/vox_render.h"
#include "vox_sdl_ui.h"

#define LEGACY_WIDTH 320
#define LEGACY_HEIGHT 200
#define LEGACY_TICKS_PER_SECOND 60U
#define LEGACY_AUDIO_RATE 22050U
#define LEGACY_AUDIO_FRAMES 1024U
#define LEGACY_AUDIO_BUFFER_COUNT 3

#define LEGACY_SCREEN_TITLE 0
#define LEGACY_SCREEN_PLAY 1
#define LEGACY_SCREEN_PAUSE 2
#define LEGACY_SCREEN_RESULTS 3

typedef struct legacy_audio_buffer {
    WAVEHDR header;
    vox_i16 samples[LEGACY_AUDIO_FRAMES * VOX_AUDIO_OUTPUT_CHANNELS];
    int prepared;
} legacy_audio_buffer;

typedef struct legacy_app {
    HWND window;
    HINSTANCE instance;
    vox_digs_match match;
    vox_software_target target;
    vox_software_config render_config;
    vox_ui_surface ui;
    vox_audio_engine audio;
    legacy_audio_buffer audio_buffers[LEGACY_AUDIO_BUFFER_COUNT];
    HWAVEOUT wave_out;
    vox_u8 rgb[LEGACY_WIDTH * LEGACY_HEIGHT * VOX_SOFTWARE_RGB_BYTES];
    vox_u8 bgrx[LEGACY_WIDTH * LEGACY_HEIGHT * 4U];
    int keys[256];
    int mouse_x;
    int mouse_y;
    int mouse_inside;
    int left_down;
    int right_down;
    int fire_armed;
    int cursor_hidden;
    int screen;
    int selected_weapon;
    int audio_open;
    int flash_ticks;
    int flash_kind;
    int notice_ticks;
    unsigned long audio_event_id;
    unsigned long match_serial;
    char notice[64];
} legacy_app;

static legacy_app legacy;

static LRESULT CALLBACK legacy_window_proc(HWND window, UINT message,
                                           WPARAM w_param, LPARAM l_param);

static int legacy_abs(int value)
{
    return value < 0 ? -value : value;
}

static void legacy_copy_text(char *destination, const char *source,
                             unsigned int capacity)
{
    unsigned int index;
    if (destination == 0 || capacity == 0U) {
        return;
    }
    if (source == 0) {
        destination[0] = '\0';
        return;
    }
    index = 0U;
    while (source[index] != '\0' && index + 1U < capacity) {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static void legacy_set_notice(const char *text, int ticks)
{
    legacy_copy_text(legacy.notice, text, (unsigned int)sizeof(legacy.notice));
    legacy.notice_ticks = ticks;
}

/* Show exactly one aim indicator in play: the software crosshair tracks the
 * hardware pointer's client coordinates, while the visible hardware cursor is
 * hidden only inside the active game viewport. */
static void legacy_set_cursor_hidden(int hidden)
{
    if (hidden && !legacy.cursor_hidden) {
        (void)ShowCursor(FALSE);
        legacy.cursor_hidden = 1;
    } else if (!hidden && legacy.cursor_hidden) {
        (void)ShowCursor(TRUE);
        legacy.cursor_hidden = 0;
    }
}

static int legacy_world_x_to_screen(vox_i32 position_q16)
{
    long world_x;
    world_x = (long)(position_q16 / 65536L);
    return (int)(world_x * LEGACY_WIDTH / (long)VOX_WORLD_WIDTH);
}

static int legacy_world_y_to_screen(vox_i32 position_q16)
{
    long world_y;
    world_y = (long)(position_q16 / 65536L);
    return (int)(world_y * LEGACY_HEIGHT / (long)VOX_WORLD_HEIGHT);
}

static void legacy_client_viewport(HWND window, int *offset_x, int *offset_y,
                                   int *width, int *height)
{
    RECT client;
    int client_width;
    int client_height;
    int viewport_width;
    int viewport_height;
    int viewport_x;
    int viewport_y;

    GetClientRect(window, &client);
    client_width = client.right - client.left;
    client_height = client.bottom - client.top;
    if (client_width < 1) {
        client_width = 1;
    }
    if (client_height < 1) {
        client_height = 1;
    }
    if (client_width * LEGACY_HEIGHT > client_height * LEGACY_WIDTH) {
        viewport_height = client_height;
        viewport_width = client_height * LEGACY_WIDTH / LEGACY_HEIGHT;
        viewport_x = (client_width - viewport_width) / 2;
        viewport_y = 0;
    } else {
        viewport_width = client_width;
        viewport_height = client_width * LEGACY_HEIGHT / LEGACY_WIDTH;
        viewport_x = 0;
        viewport_y = (client_height - viewport_height) / 2;
    }
    if (viewport_width < 1) {
        viewport_width = 1;
    }
    if (viewport_height < 1) {
        viewport_height = 1;
    }
    *offset_x = viewport_x;
    *offset_y = viewport_y;
    *width = viewport_width;
    *height = viewport_height;
}

static void legacy_update_mouse(HWND window, int client_x, int client_y)
{
    int offset_x;
    int offset_y;
    int viewport_width;
    int viewport_height;
    int logical_x;
    int logical_y;

    legacy_client_viewport(window, &offset_x, &offset_y,
                           &viewport_width, &viewport_height);
    legacy.mouse_inside = client_x >= offset_x &&
                          client_x < offset_x + viewport_width &&
                          client_y >= offset_y &&
                          client_y < offset_y + viewport_height;
    logical_x = (client_x - offset_x) * LEGACY_WIDTH / viewport_width;
    logical_y = (client_y - offset_y) * LEGACY_HEIGHT / viewport_height;
    if (logical_x < 0) {
        logical_x = 0;
    }
    if (logical_y < 0) {
        logical_y = 0;
    }
    if (logical_x >= LEGACY_WIDTH) {
        logical_x = LEGACY_WIDTH - 1;
    }
    if (logical_y >= LEGACY_HEIGHT) {
        logical_y = LEGACY_HEIGHT - 1;
    }
    legacy.mouse_x = logical_x;
    legacy.mouse_y = logical_y;
    legacy_set_cursor_hidden(legacy.screen == LEGACY_SCREEN_PLAY &&
                             legacy.mouse_inside);
}

static void legacy_audio_emit(vox_u16 preset, vox_u16 variant,
                              vox_i16 pan_q15)
{
    vox_audio_event event;
    if (!legacy.audio_open) {
        return;
    }
    vox_audio_event_init(&event, preset);
    event.variant = variant;
    event.pan_q15 = pan_q15;
    legacy.audio_event_id++;
    event.event_id = (vox_u32)legacy.audio_event_id;
    (void)vox_audio_emit(&legacy.audio, &event);
}

static vox_u16 legacy_weapon_preset(vox_u16 weapon)
{
    switch (weapon) {
    case VOX_DIGS_TOOL_PULASKI:
        return VOX_AUDIO_PRESET_PULASKI;
    case VOX_DIGS_TOOL_POPPER:
        return VOX_AUDIO_PRESET_POPPER;
    case VOX_DIGS_TOOL_SMOKER:
        return VOX_AUDIO_PRESET_SMOKER;
    case VOX_DIGS_TOOL_HOT_RAIL:
        return VOX_AUDIO_PRESET_HOT_RAIL;
    case VOX_DIGS_TOOL_HYDROSHOT:
        return VOX_AUDIO_PRESET_HYDROSHOT;
    case VOX_DIGS_TOOL_GIANT_HAMMER:
        return VOX_AUDIO_PRESET_GIANT_HAMMER;
    case VOX_DIGS_TOOL_BOLT_ACTION:
        return VOX_AUDIO_PRESET_BOLT_ACTION;
    case VOX_DIGS_TOOL_SCATTERBRAIN:
        return VOX_AUDIO_PRESET_SCATTERBRAIN;
    case VOX_DIGS_TOOL_FIRECRACKER:
        return VOX_AUDIO_PRESET_FIRECRACKER;
    case VOX_DIGS_TOOL_BORE_DRILL:
        return VOX_AUDIO_PRESET_BORE_DRILL;
    case VOX_DIGS_TOOL_RAIL_GUN:
        return VOX_AUDIO_PRESET_KILL;
    default:
        return VOX_AUDIO_PRESET_FIRE;
    }
}

static void legacy_audio_event(const vox_digs_event *event)
{
    long world_x;
    long numerator;
    vox_i16 pan;
    vox_u16 preset;

    if (event == 0) {
        return;
    }
    world_x = (long)(event->position_x_q16 / 65536L);
    numerator = world_x * 65534L;
    pan = (vox_i16)(numerator / (long)(VOX_WORLD_WIDTH - 1U) - 32767L);
    if (event->type == VOX_DIGS_EVENT_DAMAGE) {
        legacy_audio_emit(VOX_AUDIO_PRESET_HIT, event->variant, pan);
        if (event->source == 0U || event->target == 0U) {
            legacy.flash_ticks = 7;
            legacy.flash_kind = 1;
            legacy_set_notice("HIT!", 35);
        }
    } else if (event->type == VOX_DIGS_EVENT_EXPLOSION) {
        legacy_audio_emit(VOX_AUDIO_PRESET_EXPLOSION, event->variant, pan);
        if (event->source == 0U || event->target == 0U) {
            legacy.flash_ticks = 5;
            legacy.flash_kind = 2;
        }
    } else if (event->type == VOX_DIGS_EVENT_KILL) {
        legacy_audio_emit(VOX_AUDIO_PRESET_KILL, event->variant, pan);
        if (event->source == 0U || event->target == 0U) {
            legacy.flash_ticks = 12;
            legacy.flash_kind = 1;
            legacy_set_notice(event->source == 0U ? "TARGET DOWN!" :
                              "YOU WERE DIGESTED!", 100);
        }
    } else if (event->type == VOX_DIGS_EVENT_SPAWN) {
        legacy_audio_emit(VOX_AUDIO_PRESET_SPAWN, event->variant, pan);
    } else if (event->type == VOX_DIGS_EVENT_ROPE_ATTACH) {
        legacy_audio_emit(VOX_AUDIO_PRESET_ROPE_ATTACH, event->variant, pan);
    } else if (event->type == VOX_DIGS_EVENT_ROPE_BREAK ||
               event->type == VOX_DIGS_EVENT_ROPE_DETACH) {
        legacy_audio_emit(VOX_AUDIO_PRESET_ROPE_BREAK, event->variant, pan);
    } else if (event->type == VOX_DIGS_EVENT_ROPE_CAST ||
               event->type == VOX_DIGS_EVENT_ROPE_HIT) {
        legacy_audio_emit(VOX_AUDIO_PRESET_ROPE_THROW, event->variant, pan);
    } else if (event->type == VOX_DIGS_EVENT_WEAPON_FIRE) {
        preset = legacy_weapon_preset(event->weapon);
        legacy_audio_emit(preset,
                          (vox_u16)(event->variant + event->weapon * 29U),
                          pan);
    }
}

static void legacy_process_events(void)
{
    vox_u16 ordinal;
    vox_u16 count;
    const vox_digs_event *event;

    count = legacy.match.event_count;
    for (ordinal = 0U; ordinal < count; ++ordinal) {
        event = vox_digs_event_get(&legacy.match, ordinal);
        legacy_audio_event(event);
    }
    if (count > 0U) {
        (void)vox_digs_consume_events(&legacy.match, count);
    }
}

static void legacy_audio_fill(unsigned int index)
{
    if (index >= LEGACY_AUDIO_BUFFER_COUNT || !legacy.audio_open) {
        return;
    }
    (void)vox_audio_render(&legacy.audio, legacy.audio_buffers[index].samples,
                           LEGACY_AUDIO_FRAMES);
}

static void legacy_audio_pump(void)
{
    unsigned int index;
    MMRESULT result;

    if (!legacy.audio_open || legacy.wave_out == 0) {
        return;
    }
    for (index = 0U; index < LEGACY_AUDIO_BUFFER_COUNT; ++index) {
        legacy_audio_buffer *buffer;
        buffer = &legacy.audio_buffers[index];
        if ((buffer->header.dwFlags & WHDR_DONE) == 0U) {
            continue;
        }
        legacy_audio_fill(index);
        result = waveOutWrite(legacy.wave_out, &buffer->header,
                              (UINT)sizeof(buffer->header));
        if (result != MMSYSERR_NOERROR) {
            legacy.audio_open = 0;
            return;
        }
    }
}

static void legacy_audio_close(void)
{
    unsigned int index;
    unsigned int attempt;
    MMRESULT result;

    if (legacy.wave_out != 0) {
        (void)waveOutReset(legacy.wave_out);
        for (index = 0U; index < LEGACY_AUDIO_BUFFER_COUNT; ++index) {
            if (legacy.audio_buffers[index].prepared) {
                for (attempt = 0U; attempt < 16U; ++attempt) {
                    result = waveOutUnprepareHeader(legacy.wave_out,
                        &legacy.audio_buffers[index].header,
                        (UINT)sizeof(legacy.audio_buffers[index].header));
                    if (result == MMSYSERR_NOERROR) {
                        legacy.audio_buffers[index].prepared = 0;
                        break;
                    }
                    if (result != WAVERR_STILLPLAYING) {
                        break;
                    }
                    Sleep(1U);
                }
            }
        }
        for (attempt = 0U; attempt < 16U; ++attempt) {
            result = waveOutClose(legacy.wave_out);
            if (result == MMSYSERR_NOERROR || result != WAVERR_STILLPLAYING) {
                break;
            }
            Sleep(1U);
        }
    }
    legacy.wave_out = 0;
    legacy.audio_open = 0;
}

static void legacy_audio_open(void)
{
    WAVEFORMATEX format;
    MMRESULT result;
    unsigned int index;

    memset(&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = VOX_AUDIO_OUTPUT_CHANNELS;
    format.nSamplesPerSec = LEGACY_AUDIO_RATE;
    format.wBitsPerSample = 16U;
    format.nBlockAlign = (WORD)(format.nChannels *
                                (format.wBitsPerSample / 8U));
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    legacy.wave_out = 0;
    result = waveOutOpen(&legacy.wave_out, WAVE_MAPPER, &format, 0, 0,
                         CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR || legacy.wave_out == 0) {
        legacy.wave_out = 0;
        return;
    }
    if (vox_audio_init(&legacy.audio, LEGACY_AUDIO_RATE, 0xD19595U) != VOX_OK) {
        (void)waveOutClose(legacy.wave_out);
        legacy.wave_out = 0;
        return;
    }
    legacy.audio_open = 1;
    (void)vox_audio_set_ambience(&legacy.audio, VOX_AUDIO_AMBIENCE_WIND,
                                  2200U, VOX_AUDIO_PAN_CENTER);
    for (index = 0U; index < LEGACY_AUDIO_BUFFER_COUNT; ++index) {
        legacy_audio_buffer *buffer;
        buffer = &legacy.audio_buffers[index];
        memset(buffer, 0, sizeof(*buffer));
        buffer->header.lpData = (LPSTR)buffer->samples;
        buffer->header.dwBufferLength =
            (DWORD)sizeof(buffer->samples);
        result = waveOutPrepareHeader(legacy.wave_out, &buffer->header,
                                      (UINT)sizeof(buffer->header));
        if (result != MMSYSERR_NOERROR) {
            legacy_audio_close();
            return;
        }
        buffer->prepared = 1;
        legacy_audio_fill(index);
        result = waveOutWrite(legacy.wave_out, &buffer->header,
                              (UINT)sizeof(buffer->header));
        if (result != MMSYSERR_NOERROR) {
            legacy_audio_close();
            return;
        }
    }
    legacy_audio_emit(VOX_AUDIO_PRESET_UI_ACCEPT, 0U, VOX_AUDIO_PAN_CENTER);
}

static void legacy_setup_render_targets(void)
{
    legacy.target.abi_version = VOX_ABI_VERSION;
    legacy.target.struct_size = (vox_u32)sizeof(legacy.target);
    legacy.target.width = LEGACY_WIDTH;
    legacy.target.height = LEGACY_HEIGHT;
    legacy.target.stride = LEGACY_WIDTH * VOX_SOFTWARE_RGB_BYTES;
    legacy.target.pixels = legacy.rgb;
    vox_software_config_default(&legacy.render_config);
    legacy.render_config.gi_quality = VOX_GI_COMPATIBILITY;
    legacy.ui.pixels = legacy.rgb;
    legacy.ui.width = LEGACY_WIDTH;
    legacy.ui.height = LEGACY_HEIGHT;
    legacy.ui.stride = legacy.target.stride;
}

static int legacy_begin_match(void)
{
    vox_digs_rules rules;
    vox_result result;

    vox_digs_rules_classic(&rules);
    rules.seed = 0xD1959501U + (vox_u32)(legacy.match_serial * 977UL);
    rules.match_ticks = 120U * VOX_DIGS_TICKS_PER_SECOND;
    rules.lava_start_tick = 75U * VOX_DIGS_TICKS_PER_SECOND;
    rules.score_limit = 5U;
    rules.player_count = 2U;
    rules.bot_mask = 0x0002U;
    rules.fx_budget = VOX_DIGS_FX_RETRO;
    result = vox_digs_match_init(&legacy.match, &rules);
    if (result != VOX_OK) {
        legacy_set_notice("MATCH INIT FAILED", 180);
        return 0;
    }
    legacy.match_serial++;
    legacy.selected_weapon = VOX_DIGS_TOOL_POPPER;
    legacy.flash_ticks = 0;
    legacy.notice_ticks = 0;
    legacy.fire_armed = legacy.left_down ? 0 : 1;
    legacy_set_notice("GET TO WORK!", 75);
    legacy_process_events();
    legacy.screen = LEGACY_SCREEN_PLAY;
    legacy_set_cursor_hidden(legacy.mouse_inside);
    return 1;
}

static void legacy_select_weapon(int key)
{
    int weapon;

    weapon = -1;
    if (key >= '1' && key <= '9') {
        weapon = key - '1';
    } else if (key == '0') {
        weapon = 9;
    } else if (key == 'R') {
        weapon = VOX_DIGS_TOOL_RAIL_GUN;
    }
    if (weapon >= 0 && weapon < (int)VOX_DIGS_TOOL_COUNT) {
        legacy.selected_weapon = weapon;
        legacy_audio_emit(VOX_AUDIO_PRESET_UI_MOVE, (vox_u16)weapon,
                          VOX_AUDIO_PAN_CENTER);
    }
}

static void legacy_submit_player_input(void)
{
    vox_digs_input input;
    vox_u16 actions;

    if (!legacy.match.alive[0]) {
        if (legacy.match.respawn_ready[0] && legacy.left_down) {
            (void)vox_digs_request_respawn(&legacy.match, 0U);
        }
        return;
    }
    actions = 0U;
    if (legacy.keys['A'] || legacy.keys[VK_LEFT]) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_LEFT);
    }
    if (legacy.keys['D'] || legacy.keys[VK_RIGHT]) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_RIGHT);
    }
    if (legacy.keys['W'] || legacy.keys[VK_UP] || legacy.keys[VK_SPACE]) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_JUMP);
    }
    if (legacy.keys['S'] || legacy.keys[VK_DOWN]) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_STEAM);
    }
    if (legacy.right_down && legacy.mouse_inside) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_ROPE);
    }
    if (legacy.left_down && legacy.fire_armed && legacy.mouse_inside) {
        actions = (vox_u16)(actions | VOX_DIGS_ACTION_FIRE);
    }
    memset(&input, 0, sizeof(input));
    input.abi_version = VOX_ABI_VERSION;
    input.struct_size = (vox_u32)sizeof(input);
    input.player = 0U;
    input.actions = actions;
    input.aim_x = (vox_u16)((unsigned int)legacy.mouse_x * VOX_WORLD_WIDTH /
                            (unsigned int)LEGACY_WIDTH);
    input.aim_y = (vox_u16)((unsigned int)legacy.mouse_y * VOX_WORLD_HEIGHT /
                            (unsigned int)LEGACY_HEIGHT);
    if (input.aim_x >= VOX_WORLD_WIDTH) {
        input.aim_x = (vox_u16)(VOX_WORLD_WIDTH - 1U);
    }
    if (input.aim_y >= VOX_WORLD_HEIGHT) {
        input.aim_y = (vox_u16)(VOX_WORLD_HEIGHT - 1U);
    }
    if (actions & VOX_DIGS_ACTION_LEFT) {
        input.move_x_q15 = -32767;
    } else if (actions & VOX_DIGS_ACTION_RIGHT) {
        input.move_x_q15 = 32767;
    }
    if (actions & VOX_DIGS_ACTION_JUMP) {
        input.move_y_q15 = -32767;
    } else if (actions & VOX_DIGS_ACTION_STEAM) {
        input.move_y_q15 = 32767;
    }
    input.selected_weapon = (vox_u16)legacy.selected_weapon;
    (void)vox_digs_submit_input(&legacy.match, &input);
}

static void legacy_tick(void)
{
    vox_result result;
    vox_u16 lava_gain;

    if (legacy.screen != LEGACY_SCREEN_PLAY) {
        return;
    }
    legacy_submit_player_input();
    result = vox_digs_match_step(&legacy.match);
    if (result != VOX_OK) {
        legacy_set_notice("SIMULATION HALTED", 180);
        legacy.screen = LEGACY_SCREEN_TITLE;
        return;
    }
    legacy_process_events();
    lava_gain = legacy.match.lava_level_q16 > 0 ? 4000U : 0U;
    if (legacy.audio_open) {
        (void)vox_audio_set_ambience(&legacy.audio, VOX_AUDIO_AMBIENCE_LAVA,
                                      lava_gain, VOX_AUDIO_PAN_CENTER);
    }
    if (legacy.flash_ticks > 0) {
        legacy.flash_ticks--;
    }
    if (legacy.notice_ticks > 0) {
        legacy.notice_ticks--;
    }
    if (legacy.match.phase == VOX_DIGS_RESULTS) {
        legacy.screen = LEGACY_SCREEN_RESULTS;
        legacy_set_notice("SHIFT COMPLETE", 180);
    }
}

static void legacy_draw_line(int x0, int y0, int x1, int y1,
                             vox_u8 red, vox_u8 green, vox_u8 blue)
{
    int delta_x;
    int delta_y;
    int step_x;
    int step_y;
    int error;
    int double_error;

    delta_x = legacy_abs(x1 - x0);
    delta_y = -legacy_abs(y1 - y0);
    step_x = x0 < x1 ? 1 : -1;
    step_y = y0 < y1 ? 1 : -1;
    error = delta_x + delta_y;
    for (;;) {
        vox_ui_rect(&legacy.ui, x0, y0, 1, 1, red, green, blue);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        double_error = 2 * error;
        if (double_error >= delta_y) {
            error += delta_y;
            x0 += step_x;
        }
        if (double_error <= delta_x) {
            error += delta_x;
            y0 += step_y;
        }
    }
}

static void legacy_draw_player(vox_u16 player)
{
    int x;
    int y;
    vox_u8 red;
    vox_u8 green;
    vox_u8 blue;
    const char *name;

    if (!vox_digs_player_is_active(&legacy.match, player) ||
        !legacy.match.alive[player]) {
        return;
    }
    x = legacy_world_x_to_screen(legacy.match.players[player].position_x.value_q16);
    y = legacy_world_y_to_screen(legacy.match.players[player].position_y.value_q16);
    red = player == 0U ? 66U : 255U;
    green = player == 0U ? 238U : 96U;
    blue = player == 0U ? 255U : 72U;
    vox_ui_rect(&legacy.ui, x - 2, y - 5, 5, 6, red, green, blue);
    vox_ui_rect(&legacy.ui, x - 1, y - 7, 3, 2, 255U, 225U, 92U);
    vox_ui_rect(&legacy.ui, x - 3, y + 1, 2, 2, 45U, 45U, 55U);
    vox_ui_rect(&legacy.ui, x + 2, y + 1, 2, 2, 45U, 45U, 55U);
    if (legacy.match.spawn_shield_ticks[player] > 0U) {
        vox_ui_frame(&legacy.ui, x - 4, y - 9, 9, 13, 245U, 245U, 245U);
    }
    name = player == 0U ? "P1" : "BOT";
    vox_ui_text_shadow(&legacy.ui, x - vox_ui_text_width(name, 1) / 2,
                       y - 16, 1, name, red, green, blue);
    if (legacy.match.ropes[player].active) {
        int anchor_x;
        int anchor_y;
        anchor_x = legacy_world_x_to_screen(legacy.match.ropes[player].anchor_x_q16);
        anchor_y = legacy_world_y_to_screen(legacy.match.ropes[player].anchor_y_q16);
        legacy_draw_line(x, y - 4, anchor_x, anchor_y, 225U, 201U, 116U);
    }
}

static void legacy_draw_crosshair(void)
{
    int x;
    int y;
    int charge;

    if (!legacy.mouse_inside || legacy.screen != LEGACY_SCREEN_PLAY) {
        return;
    }
    x = legacy.mouse_x;
    y = legacy.mouse_y;
    charge = legacy.match.weapon_charging[0] ? 6 : 4;
    vox_ui_rect(&legacy.ui, x - charge, y - charge, 3, 1, 255U, 239U, 90U);
    vox_ui_rect(&legacy.ui, x - charge, y - charge, 1, 3, 255U, 239U, 90U);
    vox_ui_rect(&legacy.ui, x + charge - 2, y - charge, 3, 1,
                255U, 239U, 90U);
    vox_ui_rect(&legacy.ui, x + charge, y - charge, 1, 3,
                255U, 239U, 90U);
    vox_ui_rect(&legacy.ui, x - charge, y + charge, 3, 1,
                255U, 239U, 90U);
    vox_ui_rect(&legacy.ui, x - charge, y + charge - 2, 1, 3,
                255U, 239U, 90U);
    vox_ui_rect(&legacy.ui, x + charge - 2, y + charge, 3, 1,
                255U, 239U, 90U);
    vox_ui_rect(&legacy.ui, x + charge, y + charge - 2, 1, 3,
                255U, 239U, 90U);
}

static void legacy_draw_hud(void)
{
    const vox_digs_weapon_properties *weapon;
    char line[80];
    unsigned long remaining;
    unsigned long seconds;
    unsigned long minutes;
    unsigned long remainder;

    vox_ui_rect(&legacy.ui, 4, 4, 150, 26, 2U, 5U, 10U);
    vox_ui_frame(&legacy.ui, 4, 4, 150, 26, 194U, 112U, 18U);
    wsprintfA(line, "P1 HP %u  STEAM %u", (unsigned int)legacy.match.health[0],
              (unsigned int)legacy.match.steam_q16[0]);
    vox_ui_text(&legacy.ui, 8, 8, 1, line, 220U, 240U, 240U);
    weapon = vox_digs_weapon_get((vox_u16)legacy.selected_weapon);
    vox_ui_rect(&legacy.ui, 160, 4, 156, 26, 2U, 5U, 10U);
    vox_ui_frame(&legacy.ui, 160, 4, 156, 26, 194U, 112U, 18U);
    vox_ui_text(&legacy.ui, 165, 8, 1,
                weapon != 0 ? weapon->name : "TOOL ERROR",
                255U, 235U, 65U);
    vox_ui_text(&legacy.ui, 165, 18, 1,
                "LMB FIRE RMB ROPE  1-0 R TOOL", 150U, 175U, 182U);
    remaining = legacy.match.rules.match_ticks > legacy.match.tick ?
        legacy.match.rules.match_ticks - legacy.match.tick : 0UL;
    seconds = remaining / VOX_DIGS_TICKS_PER_SECOND;
    minutes = seconds / 60UL;
    remainder = seconds % 60UL;
    wsprintfA(line, "TIME %lu:%02lu", minutes, remainder);
    vox_ui_rect(&legacy.ui, 4, 178, 96, 18, 2U, 5U, 10U);
    vox_ui_frame(&legacy.ui, 4, 178, 96, 18, 146U, 228U, 235U);
    vox_ui_text(&legacy.ui, 9, 183, 1, line, 160U, 255U, 255U);
    if (legacy.notice_ticks > 0 && legacy.notice[0] != '\0') {
        vox_ui_text_center_shadow(&legacy.ui, 160, 36, 1, legacy.notice,
                                  255U, 242U, 105U);
    }
}

static void legacy_apply_flash(void)
{
    unsigned int pixel;
    vox_u8 target_green;
    vox_u8 target_blue;
    vox_u8 *destination;

    if (legacy.flash_ticks <= 0) {
        return;
    }
    target_green = legacy.flash_kind == 1 ? 0U : 255U;
    target_blue = legacy.flash_kind == 1 ? 0U : 255U;
    for (pixel = 0U; pixel < LEGACY_WIDTH * LEGACY_HEIGHT; ++pixel) {
        destination = &legacy.rgb[pixel * VOX_SOFTWARE_RGB_BYTES];
        destination[0] = (vox_u8)((unsigned int)destination[0] / 2U + 127U);
        destination[1] = (vox_u8)(((unsigned int)destination[1] +
                                    (unsigned int)target_green) / 2U);
        destination[2] = (vox_u8)(((unsigned int)destination[2] +
                                    (unsigned int)target_blue) / 2U);
    }
}

static void legacy_draw_world(void)
{
    vox_result result;
    vox_u16 player;

    result = vox_software_render_ex(&legacy.match.world, &legacy.target,
                                    &legacy.render_config);
    if (result != VOX_OK) {
        vox_ui_fill(&legacy.ui, 0U, 0U, 0U);
        vox_ui_text_center(&legacy.ui, 160, 94, 1, "RENDER ERROR",
                           255U, 80U, 80U);
        return;
    }
    for (player = 0U; player < legacy.match.rules.player_count; ++player) {
        legacy_draw_player(player);
    }
    legacy_draw_crosshair();
    legacy_draw_hud();
    legacy_apply_flash();
}

static void legacy_draw_panel(int left, int top, int width, int height)
{
    vox_ui_rect(&legacy.ui, left, top, width, height, 0U, 0U, 0U);
    vox_ui_frame(&legacy.ui, left, top, width, height, 194U, 112U, 18U);
}

static void legacy_draw_title(void)
{
    vox_ui_fill(&legacy.ui, 8U, 17U, 35U);
    vox_ui_rect(&legacy.ui, 0, 144, LEGACY_WIDTH, 56, 66U, 43U, 24U);
    vox_ui_rect(&legacy.ui, 0, 170, LEGACY_WIDTH, 30, 47U, 35U, 26U);
    legacy_draw_panel(42, 28, 236, 136);
    vox_ui_text_center_shadow(&legacy.ui, 160, 42, 4, "DIGS",
                              255U, 244U, 58U);
    vox_ui_text_center(&legacy.ui, 160, 83, 1,
                       "NATIVE WIN32 LEGACY EDITION",
                       156U, 210U, 226U);
    vox_ui_text_center(&legacy.ui, 160, 107, 1,
                       "ENTER OR LEFT CLICK: START SHIFT",
                       255U, 226U, 108U);
    vox_ui_text_center(&legacy.ui, 160, 121, 1,
                       "A D MOVE  W JUMP  S STEAM  RMB ROPE",
                       202U, 202U, 202U);
    vox_ui_text_center(&legacy.ui, 160, 139, 1,
                       "ESC QUITS", 202U, 202U, 202U);
    if (legacy.notice_ticks > 0 && legacy.notice[0] != '\0') {
        vox_ui_text_center(&legacy.ui, 160, 172, 1, legacy.notice,
                           255U, 96U, 80U);
    }
}

static void legacy_draw_pause(void)
{
    legacy_draw_world();
    legacy_draw_panel(88, 74, 144, 56);
    vox_ui_text_center_shadow(&legacy.ui, 160, 84, 1, "PAUSED",
                              255U, 242U, 105U);
    vox_ui_text_center(&legacy.ui, 160, 101, 1,
                       "ESC RESUME  R RESTART", 210U, 220U, 225U);
    vox_ui_text_center(&legacy.ui, 160, 113, 1,
                       "Q TITLE", 210U, 220U, 225U);
}

static void legacy_draw_results(void)
{
    char line[80];
    const char *outcome;

    legacy_draw_world();
    legacy_draw_panel(66, 62, 188, 76);
    if (legacy.match.result_draw) {
        outcome = "DRAW";
    } else if (legacy.match.winner_player == 0U) {
        outcome = "SHIFT WON!";
    } else {
        outcome = "SHIFT LOST";
    }
    vox_ui_text_center_shadow(&legacy.ui, 160, 72, 1, outcome,
                              255U, 242U, 105U);
    wsprintfA(line, "P1 K%u D%u  BOT K%u D%u",
              (unsigned int)legacy.match.scores[0],
              (unsigned int)legacy.match.deaths[0],
              (unsigned int)legacy.match.scores[1],
              (unsigned int)legacy.match.deaths[1]);
    vox_ui_text_center(&legacy.ui, 160, 92, 1, line,
                       210U, 220U, 225U);
    vox_ui_text_center(&legacy.ui, 160, 115, 1,
                       "ENTER OR CLICK: TITLE", 255U, 225U, 108U);
}

static void legacy_convert_pixels(void)
{
    unsigned int index;
    vox_u8 *source;
    vox_u8 *destination;

    for (index = 0U; index < LEGACY_WIDTH * LEGACY_HEIGHT; ++index) {
        source = &legacy.rgb[index * VOX_SOFTWARE_RGB_BYTES];
        destination = &legacy.bgrx[index * 4U];
        destination[0] = source[2];
        destination[1] = source[1];
        destination[2] = source[0];
        destination[3] = 0U;
    }
}

static void legacy_render(void)
{
    if (legacy.screen == LEGACY_SCREEN_TITLE) {
        legacy_draw_title();
    } else if (legacy.screen == LEGACY_SCREEN_PAUSE) {
        legacy_draw_pause();
    } else if (legacy.screen == LEGACY_SCREEN_RESULTS) {
        legacy_draw_results();
    } else {
        legacy_draw_world();
    }
    legacy_convert_pixels();
}

static void legacy_present(HWND window, HDC device_context)
{
    BITMAPINFO bitmap_info;
    RECT client;
    int offset_x;
    int offset_y;
    int width;
    int height;
    HBRUSH black;

    memset(&bitmap_info, 0, sizeof(bitmap_info));
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = LEGACY_WIDTH;
    bitmap_info.bmiHeader.biHeight = -LEGACY_HEIGHT;
    bitmap_info.bmiHeader.biPlanes = 1U;
    bitmap_info.bmiHeader.biBitCount = 32U;
    bitmap_info.bmiHeader.biCompression = BI_RGB;
    legacy_client_viewport(window, &offset_x, &offset_y, &width, &height);
    GetClientRect(window, &client);
    black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    FillRect(device_context, &client, black);
    (void)SetStretchBltMode(device_context, COLORONCOLOR);
    (void)StretchDIBits(device_context, offset_x, offset_y, width, height,
                        0, 0, LEGACY_WIDTH, LEGACY_HEIGHT,
                        legacy.bgrx, &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

static void legacy_key_down(int key, int repeated)
{
    if (key >= 0 && key < 256) {
        legacy.keys[key] = 1;
    }
    if (repeated) {
        return;
    }
    if (legacy.screen == LEGACY_SCREEN_TITLE) {
        if (key == VK_RETURN || key == VK_SPACE) {
            (void)legacy_begin_match();
        } else if (key == VK_ESCAPE) {
            legacy_set_cursor_hidden(0);
            DestroyWindow(legacy.window);
        }
        return;
    }
    if (legacy.screen == LEGACY_SCREEN_PLAY) {
        if (key == VK_ESCAPE) {
            legacy.screen = LEGACY_SCREEN_PAUSE;
            legacy_set_cursor_hidden(0);
            legacy_audio_emit(VOX_AUDIO_PRESET_UI_BACK, 0U,
                              VOX_AUDIO_PAN_CENTER);
        } else {
            legacy_select_weapon(key);
        }
        return;
    }
    if (legacy.screen == LEGACY_SCREEN_PAUSE) {
        if (key == VK_ESCAPE || key == VK_RETURN) {
            legacy.screen = LEGACY_SCREEN_PLAY;
            legacy_set_cursor_hidden(legacy.mouse_inside);
            legacy_audio_emit(VOX_AUDIO_PRESET_UI_ACCEPT, 0U,
                              VOX_AUDIO_PAN_CENTER);
        } else if (key == 'R') {
            (void)legacy_begin_match();
        } else if (key == 'Q') {
            legacy.screen = LEGACY_SCREEN_TITLE;
            legacy_set_cursor_hidden(0);
        }
        return;
    }
    if (legacy.screen == LEGACY_SCREEN_RESULTS &&
        (key == VK_RETURN || key == VK_SPACE || key == VK_ESCAPE)) {
        legacy.screen = LEGACY_SCREEN_TITLE;
        legacy_set_cursor_hidden(0);
    }
}

static void legacy_key_up(int key)
{
    if (key >= 0 && key < 256) {
        legacy.keys[key] = 0;
    }
}

static LRESULT CALLBACK legacy_window_proc(HWND window, UINT message,
                                           WPARAM w_param, LPARAM l_param)
{
    PAINTSTRUCT paint;
    HDC device_context;
    int mouse_x;
    int mouse_y;
    int repeated;

    if (message == WM_PAINT) {
        device_context = BeginPaint(window, &paint);
        legacy_present(window, device_context);
        EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_ERASEBKGND) {
        return 1;
    }
    if (message == WM_MOUSEMOVE) {
        mouse_x = (int)(short)LOWORD(l_param);
        mouse_y = (int)(short)HIWORD(l_param);
        legacy_update_mouse(window, mouse_x, mouse_y);
        return 0;
    }
    if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN) {
        mouse_x = (int)(short)LOWORD(l_param);
        mouse_y = (int)(short)HIWORD(l_param);
        legacy_update_mouse(window, mouse_x, mouse_y);
        if (message == WM_LBUTTONDOWN) {
            legacy.left_down = 1;
            SetCapture(window);
            if (legacy.screen == LEGACY_SCREEN_TITLE) {
                (void)legacy_begin_match();
            } else if (legacy.screen == LEGACY_SCREEN_RESULTS) {
                legacy.screen = LEGACY_SCREEN_TITLE;
                legacy_set_cursor_hidden(0);
            }
        } else {
            legacy.right_down = 1;
            SetCapture(window);
        }
        return 0;
    }
    if (message == WM_LBUTTONUP || message == WM_RBUTTONUP) {
        if (message == WM_LBUTTONUP) {
            legacy.left_down = 0;
            legacy.fire_armed = 1;
        } else {
            legacy.right_down = 0;
        }
        if (!legacy.left_down && !legacy.right_down) {
            ReleaseCapture();
        }
        return 0;
    }
    if (message == WM_KEYDOWN) {
        repeated = (l_param & 0x40000000L) != 0L;
        legacy_key_down((int)(w_param & 0xffU), repeated);
        return 0;
    }
    if (message == WM_KEYUP) {
        legacy_key_up((int)(w_param & 0xffU));
        return 0;
    }
    if (message == WM_KILLFOCUS) {
        memset(legacy.keys, 0, sizeof(legacy.keys));
        legacy.left_down = 0;
        legacy.right_down = 0;
        legacy.fire_armed = 1;
        ReleaseCapture();
        legacy_set_cursor_hidden(0);
        if (legacy.screen == LEGACY_SCREEN_PLAY) {
            legacy.screen = LEGACY_SCREEN_PAUSE;
        }
        return 0;
    }
    if (message == WM_CLOSE) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, w_param, l_param);
}

static int legacy_self_test(void)
{
    vox_digs_rules rules;
    vox_digs_input input;
    vox_u32 tick;

    legacy_setup_render_targets();
    vox_digs_rules_classic(&rules);
    rules.match_ticks = 600U;
    rules.score_limit = 100U;
    rules.lava_start_tick = 300U;
    if (vox_digs_match_init(&legacy.match, &rules) != VOX_OK) {
        return 2;
    }
    memset(&input, 0, sizeof(input));
    input.abi_version = VOX_ABI_VERSION;
    input.struct_size = (vox_u32)sizeof(input);
    input.player = 0U;
    for (tick = 0U; tick < rules.match_ticks &&
         legacy.match.phase == VOX_DIGS_RUNNING;
         ++tick) {
        if (tick < 60U) {
            input.actions = VOX_DIGS_ACTION_RIGHT;
        } else if (tick == 60U) {
            input.actions = (vox_u16)(VOX_DIGS_ACTION_RIGHT |
                                      VOX_DIGS_ACTION_JUMP);
        } else if (tick < 120U) {
            input.actions = (vox_u16)(VOX_DIGS_ACTION_RIGHT |
                                      VOX_DIGS_ACTION_STEAM);
        } else {
            input.actions = 0U;
        }
        input.move_x_q15 = (input.actions & VOX_DIGS_ACTION_RIGHT) ?
                           32767 : 0;
        input.move_y_q15 = 0;
        if (legacy.match.alive[0] &&
            vox_digs_submit_input(&legacy.match, &input) != VOX_OK) {
            return 3;
        }
        if (vox_digs_match_step(&legacy.match) != VOX_OK) {
            return 4;
        }
    }
    if (legacy.match.state_hash != 0x421b35b3U) {
        return 5;
    }
    if (vox_software_render_ex(&legacy.match.world, &legacy.target,
                               &legacy.render_config) != VOX_OK ||
        vox_software_hash(&legacy.target) == 0U) {
        return 6;
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance,
                   LPSTR command_line, int show_command)
{
    WNDCLASSA window_class;
    RECT desired;
    MSG message;
    DWORD previous_time;
    DWORD current_time;
    DWORD elapsed;
    unsigned long tick_units;
    unsigned long present_units;
    int scale;
    int screen_width;
    int screen_height;
    int running;

    (void)previous_instance;
    (void)command_line;
    if (strstr(GetCommandLineA(), "--self-test") != 0) {
        return legacy_self_test();
    }
    memset(&legacy, 0, sizeof(legacy));
    legacy.instance = instance;
    legacy.screen = LEGACY_SCREEN_TITLE;
    legacy_setup_render_targets();
    memset(&window_class, 0, sizeof(window_class));
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = legacy_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorA(0, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    window_class.lpszClassName = "DIGS95LegacyWindow";
    if (RegisterClassA(&window_class) == 0) {
        MessageBoxA(0, "DIGS could not register its Win32 window.", "DIGS",
                    MB_OK | MB_ICONSTOP);
        return 10;
    }
    screen_width = GetSystemMetrics(SM_CXSCREEN);
    screen_height = GetSystemMetrics(SM_CYSCREEN);
    scale = 3;
    if (screen_width < 1024 || screen_height < 768) {
        scale = 2;
    }
    if (screen_width < 800 || screen_height < 560) {
        scale = 1;
    }
    desired.left = 0;
    desired.top = 0;
    desired.right = LEGACY_WIDTH * scale;
    desired.bottom = LEGACY_HEIGHT * scale;
    (void)AdjustWindowRect(&desired, WS_OVERLAPPEDWINDOW, FALSE);
    legacy.window = CreateWindowExA(0UL, window_class.lpszClassName,
        "DIGS - Legacy Win32", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, desired.right - desired.left,
        desired.bottom - desired.top, 0, 0, instance, 0);
    if (legacy.window == 0) {
        MessageBoxA(0, "DIGS could not create its Win32 window.", "DIGS",
                    MB_OK | MB_ICONSTOP);
        return 11;
    }
    ShowWindow(legacy.window, show_command);
    UpdateWindow(legacy.window);
    (void)timeBeginPeriod(1U);
    legacy_audio_open();
    legacy_render();
    InvalidateRect(legacy.window, 0, FALSE);
    previous_time = timeGetTime();
    tick_units = 0UL;
    present_units = 0UL;
    running = 1;
    while (running) {
        while (PeekMessageA(&message, 0, 0U, 0U, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = 0;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        current_time = timeGetTime();
        elapsed = current_time - previous_time;
        previous_time = current_time;
        if (elapsed > 250U) {
            elapsed = 250U;
        }
        tick_units += (unsigned long)elapsed * LEGACY_TICKS_PER_SECOND;
        present_units += (unsigned long)elapsed * LEGACY_TICKS_PER_SECOND;
        while (tick_units >= 1000UL) {
            legacy_tick();
            tick_units -= 1000UL;
        }
        if (present_units >= 1000UL) {
            legacy_audio_pump();
            legacy_render();
            InvalidateRect(legacy.window, 0, FALSE);
            present_units %= 1000UL;
        }
        Sleep(1U);
    }
    legacy_audio_close();
    legacy_set_cursor_hidden(0);
    (void)timeEndPeriod(1U);
    return (int)message.wParam;
}

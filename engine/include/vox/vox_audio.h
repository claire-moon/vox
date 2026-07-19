/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_AUDIO_H
#define VOX_AUDIO_H

#include "vox/vox_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * VOX audio is an allocation-free, dual-chip-inspired sound-effect engine.
 * Its eight voices correspond to two banks of four programmable voices.  It
 * deliberately generates sound rather than playing samples, and does not own
 * an operating-system audio device.  Emit and render calls for one engine must
 * be serialized by its owner if a backend uses more than one thread.
 */
#define VOX_AUDIO_VERSION 1U
#define VOX_AUDIO_VOICE_COUNT 8U
#define VOX_AUDIO_OUTPUT_CHANNELS 2U
#define VOX_AUDIO_GAIN_MAX 32767U
#define VOX_AUDIO_PAN_LEFT (-32767)
#define VOX_AUDIO_PAN_CENTER 0
#define VOX_AUDIO_PAN_RIGHT 32767
#define VOX_AUDIO_RATE_MIN 8000U
#define VOX_AUDIO_RATE_MAX 192000U

typedef enum vox_audio_waveform {
    VOX_AUDIO_WAVE_TONE = 0,
    VOX_AUDIO_WAVE_POLY4 = 1,
    VOX_AUDIO_WAVE_POLY9 = 2,
    VOX_AUDIO_WAVE_POLY17 = 3
} vox_audio_waveform;

typedef enum vox_audio_preset {
    VOX_AUDIO_PRESET_NONE = 0,
    VOX_AUDIO_PRESET_FIRE = 1,
    VOX_AUDIO_PRESET_HIT = 2,
    VOX_AUDIO_PRESET_EXPLOSION = 3,
    VOX_AUDIO_PRESET_KILL = 4,
    VOX_AUDIO_PRESET_SPAWN = 5,
    VOX_AUDIO_PRESET_ROPE_THROW = 6,
    VOX_AUDIO_PRESET_ROPE_ATTACH = 7,
    VOX_AUDIO_PRESET_ROPE_BREAK = 8,
    VOX_AUDIO_PRESET_UI_MOVE = 9,
    VOX_AUDIO_PRESET_UI_ACCEPT = 10,
    VOX_AUDIO_PRESET_UI_BACK = 11,
    VOX_AUDIO_PRESET_BARK_ALERT = 12,
    VOX_AUDIO_PRESET_BARK_HURT = 13,
    VOX_AUDIO_PRESET_BARK_KILL = 14,
    VOX_AUDIO_PRESET_COUNT = 15
} vox_audio_preset;

/*
 * Event IDs and variants are mixed into oscillator phase, polynomial state,
 * and patch jitter.  A replay that emits the same event stream therefore
 * produces the same PCM stream byte for byte.
 */
typedef struct vox_audio_event {
    vox_u16 preset;
    vox_u16 variant;
    vox_i16 pan_q15;
    vox_i16 pitch_percent;
    vox_u16 gain_q15;
    vox_u16 reserved;
    vox_u32 event_id;
} vox_audio_event;

/* Public so callers can place the complete engine in static or stack memory. */
typedef struct vox_audio_voice {
    vox_u32 phase;
    vox_u32 step;
    vox_u32 step_target;
    vox_u32 step_delta;
    vox_u32 step_remainder;
    vox_u32 step_error;
    vox_u32 slide_denominator;
    vox_u32 slide_samples_left;
    vox_u32 lfsr;
    vox_u32 age_samples;
    vox_u32 env_step;
    vox_u32 env_remainder;
    vox_u32 env_error;
    vox_u32 env_denominator;
    vox_u32 env_samples_left;
    vox_u32 env_decay_samples;
    vox_u32 env_hold_samples;
    vox_u32 env_release_samples;
    vox_u16 env_level_q15;
    vox_u16 env_target_q15;
    vox_u16 env_peak_q15;
    vox_u16 env_sustain_q15;
    vox_i16 pan_q15;
    vox_u8 active;
    vox_u8 waveform;
    vox_u8 polynomial_bits;
    vox_u8 priority;
    vox_u8 slide_up;
    vox_u8 env_stage;
    vox_u8 env_direction;
    vox_u8 bank;
} vox_audio_voice;

typedef struct vox_audio_engine {
    vox_u32 sample_rate;
    vox_u32 seed;
    vox_u32 event_serial;
    vox_u32 rendered_frames;
    vox_u32 voice_cursor;
    vox_audio_voice voices[VOX_AUDIO_VOICE_COUNT];
} vox_audio_engine;

/* Populate an event with safe defaults (center, unity gain, no pitch shift). */
void vox_audio_event_init(vox_audio_event *event, vox_u16 preset);

/* Initialize or reset caller-owned state.  No allocation or device I/O occurs. */
vox_result vox_audio_init(vox_audio_engine *engine, vox_u32 sample_rate,
                          vox_u32 seed);
void vox_audio_stop_all(vox_audio_engine *engine);

/* Queue a preset immediately into the deterministic eight-voice synthesizer. */
vox_result vox_audio_emit(vox_audio_engine *engine,
                          const vox_audio_event *event);

/*
 * Generate frame_count interleaved left/right signed-16 samples.  The output
 * buffer must hold frame_count * VOX_AUDIO_OUTPUT_CHANNELS elements.
 */
vox_result vox_audio_render(vox_audio_engine *engine,
                            vox_i16 *interleaved_stereo,
                            vox_u32 frame_count);

vox_u32 vox_audio_active_voice_count(const vox_audio_engine *engine);
vox_u32 vox_audio_state_hash(const vox_audio_engine *engine);

#ifdef __cplusplus
}
#endif

#endif

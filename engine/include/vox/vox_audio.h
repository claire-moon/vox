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
#define VOX_AUDIO_VERSION 3U
#define VOX_AUDIO_VOICE_COUNT 8U
#define VOX_AUDIO_OUTPUT_CHANNELS 2U
#define VOX_AUDIO_NOTE_CAPACITY 32U
#define VOX_AUDIO_SPEECH_CAPACITY 4U
#define VOX_AUDIO_SPEECH_TOKEN_CAPACITY 48U
#define VOX_AUDIO_ALLOPHONE_COUNT 40U
#define VOX_AUDIO_AMBIENCE_COUNT 3U
#define VOX_AUDIO_ENGINE_BYTES_MAX 8192U
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
    VOX_AUDIO_PRESET_ZOOM_CLICK = 15,
    VOX_AUDIO_PRESET_HIT_CONFIRM = 16,
    VOX_AUDIO_PRESET_KILL_CONFIRM = 17,
    VOX_AUDIO_PRESET_AMBIENCE_WIND = 18,
    VOX_AUDIO_PRESET_AMBIENCE_WATER = 19,
    VOX_AUDIO_PRESET_AMBIENCE_LAVA = 20,
    /*
     * DIGS v0.0.4 weapon palette.  These are source-native patch IDs rather
     * than recorded samples: callers may use event_id and variant to make
     * repeated shots differ deterministically without changing the patch's
     * recognizable identity.
     */
    VOX_AUDIO_PRESET_PULASKI = 21,
    VOX_AUDIO_PRESET_POPPER = 22,
    VOX_AUDIO_PRESET_SMOKER = 23,
    VOX_AUDIO_PRESET_HOT_RAIL = 24,
    VOX_AUDIO_PRESET_HYDROSHOT = 25,
    VOX_AUDIO_PRESET_GIANT_HAMMER = 26,
    VOX_AUDIO_PRESET_BOLT_ACTION = 27,
    VOX_AUDIO_PRESET_SCATTERBRAIN = 28,
    VOX_AUDIO_PRESET_FIRECRACKER = 29,
    VOX_AUDIO_PRESET_BORE_DRILL = 30,
    VOX_AUDIO_PRESET_COUNT = 31
} vox_audio_preset;

typedef enum vox_audio_bus {
    VOX_AUDIO_BUS_SFX = 0,
    VOX_AUDIO_BUS_UI = 1,
    VOX_AUDIO_BUS_SPEECH = 2,
    VOX_AUDIO_BUS_AMBIENCE = 3,
    VOX_AUDIO_BUS_COUNT = 4
} vox_audio_bus;

typedef enum vox_audio_priority {
    VOX_AUDIO_PRIORITY_AMBIENCE = 32,
    VOX_AUDIO_PRIORITY_UI = 96,
    VOX_AUDIO_PRIORITY_GAMEPLAY = 160,
    VOX_AUDIO_PRIORITY_BOT_BARK = 168,
    VOX_AUDIO_PRIORITY_PLAYER_BARK = 176,
    VOX_AUDIO_PRIORITY_BARK = VOX_AUDIO_PRIORITY_PLAYER_BARK,
    VOX_AUDIO_PRIORITY_ANNOUNCER = 240
} vox_audio_priority;

typedef enum vox_audio_speech_profile {
    VOX_AUDIO_SPEECH_DEEP = 0,
    VOX_AUDIO_SPEECH_HIGH = 1,
    VOX_AUDIO_SPEECH_PROFILE_COUNT = 2
} vox_audio_speech_profile;

/*
 * Forty deliberately small, language-neutral synthesis tokens.  Text and
 * pronunciation remain the caller's responsibility: VOX copies only these
 * stable IDs, so no dictionary, heap, locale, or operating-system TTS is
 * needed at run time.
 */
typedef enum vox_audio_allophone {
    VOX_AUDIO_ALLOPHONE_SILENCE = 0,
    VOX_AUDIO_ALLOPHONE_A = 1,
    VOX_AUDIO_ALLOPHONE_E = 2,
    VOX_AUDIO_ALLOPHONE_I = 3,
    VOX_AUDIO_ALLOPHONE_O = 4,
    VOX_AUDIO_ALLOPHONE_U = 5,
    VOX_AUDIO_ALLOPHONE_AE = 6,
    VOX_AUDIO_ALLOPHONE_AH = 7,
    VOX_AUDIO_ALLOPHONE_AW = 8,
    VOX_AUDIO_ALLOPHONE_AY = 9,
    VOX_AUDIO_ALLOPHONE_EH = 10,
    VOX_AUDIO_ALLOPHONE_ER = 11,
    VOX_AUDIO_ALLOPHONE_EY = 12,
    VOX_AUDIO_ALLOPHONE_IH = 13,
    VOX_AUDIO_ALLOPHONE_IY = 14,
    VOX_AUDIO_ALLOPHONE_OW = 15,
    VOX_AUDIO_ALLOPHONE_OY = 16,
    VOX_AUDIO_ALLOPHONE_UH = 17,
    VOX_AUDIO_ALLOPHONE_UW = 18,
    VOX_AUDIO_ALLOPHONE_B = 19,
    VOX_AUDIO_ALLOPHONE_CH = 20,
    VOX_AUDIO_ALLOPHONE_D = 21,
    VOX_AUDIO_ALLOPHONE_F = 22,
    VOX_AUDIO_ALLOPHONE_G = 23,
    VOX_AUDIO_ALLOPHONE_H = 24,
    VOX_AUDIO_ALLOPHONE_J = 25,
    VOX_AUDIO_ALLOPHONE_K = 26,
    VOX_AUDIO_ALLOPHONE_L = 27,
    VOX_AUDIO_ALLOPHONE_M = 28,
    VOX_AUDIO_ALLOPHONE_N = 29,
    VOX_AUDIO_ALLOPHONE_P = 30,
    VOX_AUDIO_ALLOPHONE_R = 31,
    VOX_AUDIO_ALLOPHONE_S = 32,
    VOX_AUDIO_ALLOPHONE_SH = 33,
    VOX_AUDIO_ALLOPHONE_T = 34,
    VOX_AUDIO_ALLOPHONE_TH = 35,
    VOX_AUDIO_ALLOPHONE_V = 36,
    VOX_AUDIO_ALLOPHONE_W = 37,
    VOX_AUDIO_ALLOPHONE_Y = 38,
    VOX_AUDIO_ALLOPHONE_Z = 39
} vox_audio_allophone;

typedef enum vox_audio_ambience {
    VOX_AUDIO_AMBIENCE_WIND = 0,
    VOX_AUDIO_AMBIENCE_WATER = 1,
    VOX_AUDIO_AMBIENCE_LAVA = 2
} vox_audio_ambience;

#define VOX_AUDIO_STOP_VOICES 1U
#define VOX_AUDIO_STOP_NOTES 2U
#define VOX_AUDIO_STOP_SPEECH 4U
#define VOX_AUDIO_STOP_AMBIENCE 8U
#define VOX_AUDIO_STOP_ALL 15U

typedef struct vox_audio_config {
    vox_u32 sample_rate;
    vox_u32 seed;
    vox_u16 master_gain_q15;
    vox_u16 master_ramp_ms;
} vox_audio_config;

typedef struct vox_audio_note {
    vox_u32 delay_frames;
    vox_u32 event_id;
    vox_u16 frequency_hz;
    vox_u16 duration_ms;
    vox_u16 gain_q15;
    vox_i16 pan_q15;
    vox_u8 waveform;
    vox_u8 bus;
    vox_u8 priority;
    vox_u8 reserved;
} vox_audio_note;

typedef struct vox_audio_speech {
    const vox_u8 *allophones;
    vox_u32 event_id;
    vox_u16 allophone_count;
    vox_u16 gain_q15;
    vox_i16 pan_q15;
    vox_u8 profile;
    vox_u8 priority;
} vox_audio_speech;

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

typedef struct vox_audio_note_slot {
    vox_audio_note note;
    vox_u32 serial;
    vox_u8 active;
    vox_u8 padding[3];
} vox_audio_note_slot;

typedef struct vox_audio_phrase_slot {
    vox_u32 event_id;
    vox_u32 serial;
    vox_u16 allophone_count;
    vox_u16 gain_q15;
    vox_i16 pan_q15;
    vox_u8 profile;
    vox_u8 priority;
    vox_u8 state;
    vox_u8 padding;
    vox_u8 allophones[VOX_AUDIO_SPEECH_TOKEN_CAPACITY];
} vox_audio_phrase_slot;

typedef struct vox_audio_ambience_state {
    vox_u32 phase;
    vox_u32 phase_secondary;
    vox_u32 step;
    vox_u32 step_secondary;
    vox_u32 lfsr;
    vox_u32 gain_step;
    vox_u32 gain_remainder;
    vox_u32 gain_error;
    vox_u32 gain_denominator;
    vox_u32 gain_samples_left;
    vox_u16 gain_q15;
    vox_u16 target_gain_q15;
    vox_i16 pan_q15;
    vox_u8 gain_up;
    vox_u8 padding;
    vox_i32 filtered_sample;
} vox_audio_ambience_state;

typedef struct vox_audio_engine {
    vox_u32 sample_rate;
    vox_u32 seed;
    vox_u32 event_serial;
    vox_u32 rendered_frames;
    vox_u32 voice_cursor;
    vox_u32 note_count;
    vox_u32 note_serial;
    vox_u32 phrase_serial;
    vox_u32 master_step;
    vox_u32 master_remainder;
    vox_u32 master_error;
    vox_u32 master_denominator;
    vox_u32 master_samples_left;
    vox_u32 speech_token_sample;
    vox_u32 speech_token_samples;
    vox_u32 speech_pitch_phase;
    vox_u32 speech_pitch_step;
    vox_u32 speech_formant_phase[3];
    vox_u32 speech_formant_step[3];
    vox_u32 speech_lfsr;
    vox_u16 master_gain_q15;
    vox_u16 master_target_q15;
    vox_u16 master_ramp_ms;
    vox_u16 speech_token_index;
    vox_u8 master_gain_up;
    vox_u8 active_phrase;
    vox_u8 padding[2];
    vox_audio_voice voices[VOX_AUDIO_VOICE_COUNT];
    vox_audio_note_slot notes[VOX_AUDIO_NOTE_CAPACITY];
    vox_audio_phrase_slot phrases[VOX_AUDIO_SPEECH_CAPACITY];
    vox_audio_ambience_state ambience[VOX_AUDIO_AMBIENCE_COUNT];
} vox_audio_engine;

typedef char vox_audio_engine_size_contract[
    (sizeof(vox_audio_engine) <= VOX_AUDIO_ENGINE_BYTES_MAX) ? 1 : -1];

/* Populate an event with safe defaults (center, unity gain, no pitch shift). */
void vox_audio_event_init(vox_audio_event *event, vox_u16 preset);
void vox_audio_config_init(vox_audio_config *config, vox_u32 sample_rate,
                           vox_u32 seed);
void vox_audio_note_init(vox_audio_note *note, vox_u16 frequency_hz,
                         vox_u16 duration_ms);
void vox_audio_speech_init(vox_audio_speech *speech,
                           const vox_u8 *allophones,
                           vox_u16 allophone_count);

/* Initialize or reset caller-owned state.  No allocation or device I/O occurs. */
vox_result vox_audio_init(vox_audio_engine *engine, vox_u32 sample_rate,
                          vox_u32 seed);
vox_result vox_audio_init_ex(vox_audio_engine *engine,
                             const vox_audio_config *config);
vox_result vox_audio_set_master_gain(vox_audio_engine *engine,
                                     vox_u16 gain_q15);
vox_u16 vox_audio_master_gain(const vox_audio_engine *engine);
vox_u32 vox_audio_sample_clock(const vox_audio_engine *engine);
vox_u32 vox_audio_ms_to_frames(const vox_audio_engine *engine,
                               vox_u32 milliseconds);
vox_result vox_audio_schedule_note(vox_audio_engine *engine,
                                   const vox_audio_note *note);
vox_result vox_audio_speak(vox_audio_engine *engine,
                           const vox_audio_speech *speech);
vox_result vox_audio_set_ambience(vox_audio_engine *engine,
                                  vox_u8 ambience,
                                  vox_u16 gain_q15,
                                  vox_i16 pan_q15);
void vox_audio_stop(vox_audio_engine *engine, vox_u16 stop_mask);
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
int vox_audio_has_pending(const vox_audio_engine *engine);
vox_u32 vox_audio_state_hash(const vox_audio_engine *engine);

#ifdef __cplusplus
}
#endif

#endif

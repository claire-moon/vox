/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_audio.h"

#include <string.h>

#define VOX_AUDIO_ENV_ATTACK 1U
#define VOX_AUDIO_ENV_DECAY 2U
#define VOX_AUDIO_ENV_HOLD 3U
#define VOX_AUDIO_ENV_RELEASE 4U

typedef struct vox_audio_patch {
    vox_u16 preset;
    vox_u8 waveform;
    vox_u8 priority;
    vox_u16 start_hz;
    vox_u16 end_hz;
    vox_u16 jitter_hz;
    vox_u16 attack_ms;
    vox_u16 decay_ms;
    vox_u16 hold_ms;
    vox_u16 release_ms;
    vox_u16 peak_q15;
    vox_u16 sustain_q15;
    vox_i16 pan_offset_q15;
} vox_audio_patch;

/* Original GPLv3+ patches.  Values evoke, but do not emulate, any one chip. */
static const vox_audio_patch vox_audio_patches[] = {
    {VOX_AUDIO_PRESET_FIRE, VOX_AUDIO_WAVE_POLY9, 140U,
     1900U, 330U, 180U, 0U, 12U, 34U, 90U,
     27000U, 17000U, -3800},
    {VOX_AUDIO_PRESET_FIRE, VOX_AUDIO_WAVE_TONE, 138U,
     240U, 95U, 22U, 0U, 8U, 22U, 75U,
     17500U, 9000U, 4200},
    {VOX_AUDIO_PRESET_HIT, VOX_AUDIO_WAVE_POLY4, 165U,
     1250U, 210U, 210U, 0U, 8U, 18U, 70U,
     28500U, 14000U, 0},
    {VOX_AUDIO_PRESET_HIT, VOX_AUDIO_WAVE_TONE, 162U,
     175U, 72U, 18U, 0U, 6U, 12U, 52U,
     15000U, 8000U, 0},
    {VOX_AUDIO_PRESET_EXPLOSION, VOX_AUDIO_WAVE_POLY17, 220U,
     820U, 43U, 95U, 0U, 55U, 250U, 520U,
     32767U, 20500U, -5200},
    {VOX_AUDIO_PRESET_EXPLOSION, VOX_AUDIO_WAVE_POLY9, 218U,
     3100U, 120U, 360U, 0U, 30U, 120U, 310U,
     28500U, 14000U, 5400},
    {VOX_AUDIO_PRESET_EXPLOSION, VOX_AUDIO_WAVE_TONE, 215U,
     112U, 48U, 12U, 2U, 70U, 140U, 360U,
     24500U, 13500U, 0},
    {VOX_AUDIO_PRESET_KILL, VOX_AUDIO_WAVE_POLY17, 245U,
     610U, 52U, 70U, 0U, 30U, 150U, 360U,
     31000U, 18000U, -4600},
    {VOX_AUDIO_PRESET_KILL, VOX_AUDIO_WAVE_TONE, 244U,
     520U, 96U, 34U, 0U, 45U, 75U, 280U,
     27500U, 15000U, 0},
    {VOX_AUDIO_PRESET_KILL, VOX_AUDIO_WAVE_POLY4, 242U,
     2200U, 360U, 240U, 0U, 18U, 80U, 175U,
     23000U, 10500U, 5200},
    {VOX_AUDIO_PRESET_SPAWN, VOX_AUDIO_WAVE_TONE, 195U,
     205U, 970U, 14U, 4U, 20U, 55U, 95U,
     22000U, 15000U, -3000},
    {VOX_AUDIO_PRESET_SPAWN, VOX_AUDIO_WAVE_POLY4, 192U,
     980U, 2600U, 90U, 0U, 15U, 40U, 85U,
     17000U, 9000U, 3300},
    {VOX_AUDIO_PRESET_ROPE_THROW, VOX_AUDIO_WAVE_POLY9, 172U,
     2400U, 510U, 150U, 0U, 12U, 38U, 90U,
     22000U, 12000U, 0},
    {VOX_AUDIO_PRESET_ROPE_ATTACH, VOX_AUDIO_WAVE_TONE, 180U,
     1280U, 430U, 30U, 0U, 8U, 18U, 70U,
     25000U, 14000U, 0},
    {VOX_AUDIO_PRESET_ROPE_ATTACH, VOX_AUDIO_WAVE_POLY4, 178U,
     2500U, 900U, 170U, 0U, 4U, 12U, 44U,
     14500U, 7000U, 0},
    {VOX_AUDIO_PRESET_ROPE_BREAK, VOX_AUDIO_WAVE_POLY4, 188U,
     1750U, 125U, 165U, 0U, 8U, 28U, 125U,
     27000U, 14500U, 0},
    {VOX_AUDIO_PRESET_UI_MOVE, VOX_AUDIO_WAVE_TONE, 110U,
     760U, 760U, 0U, 0U, 0U, 18U, 18U,
     11500U, 11500U, 0},
    {VOX_AUDIO_PRESET_UI_ACCEPT, VOX_AUDIO_WAVE_TONE, 125U,
     620U, 1240U, 0U, 0U, 0U, 42U, 35U,
     14500U, 14500U, -2200},
    {VOX_AUDIO_PRESET_UI_ACCEPT, VOX_AUDIO_WAVE_TONE, 124U,
     930U, 1860U, 0U, 20U, 0U, 34U, 35U,
     11500U, 11500U, 2200},
    {VOX_AUDIO_PRESET_UI_BACK, VOX_AUDIO_WAVE_TONE, 122U,
     720U, 280U, 0U, 0U, 0U, 40U, 45U,
     13500U, 13500U, 0},
    {VOX_AUDIO_PRESET_BARK_ALERT, VOX_AUDIO_WAVE_TONE, 205U,
     340U, 760U, 24U, 2U, 18U, 32U, 55U,
     22000U, 15000U, -3500},
    {VOX_AUDIO_PRESET_BARK_ALERT, VOX_AUDIO_WAVE_TONE, 204U,
     510U, 1140U, 28U, 38U, 16U, 36U, 60U,
     19000U, 12500U, 3500},
    {VOX_AUDIO_PRESET_BARK_ALERT, VOX_AUDIO_WAVE_POLY4, 202U,
     2100U, 740U, 175U, 0U, 12U, 55U, 85U,
     12000U, 6500U, 0},
    {VOX_AUDIO_PRESET_BARK_HURT, VOX_AUDIO_WAVE_POLY9, 212U,
     1780U, 190U, 225U, 0U, 12U, 42U, 130U,
     25500U, 13500U, -2800},
    {VOX_AUDIO_PRESET_BARK_HURT, VOX_AUDIO_WAVE_TONE, 210U,
     270U, 82U, 25U, 0U, 18U, 28U, 110U,
     20500U, 11000U, 2800},
    {VOX_AUDIO_PRESET_BARK_KILL, VOX_AUDIO_WAVE_TONE, 230U,
     310U, 930U, 20U, 2U, 28U, 55U, 85U,
     24500U, 17000U, -4200},
    {VOX_AUDIO_PRESET_BARK_KILL, VOX_AUDIO_WAVE_TONE, 229U,
     465U, 1395U, 30U, 45U, 24U, 65U, 95U,
     22000U, 14500U, 4200},
    {VOX_AUDIO_PRESET_BARK_KILL, VOX_AUDIO_WAVE_POLY4, 226U,
     2600U, 820U, 220U, 0U, 16U, 75U, 115U,
     13500U, 7000U, 0}
};

static vox_u32 vox_audio_hash_mix(vox_u32 hash, vox_u32 value)
{
    hash ^= value;
    hash *= 16777619U;
    return hash;
}

static vox_u32 vox_audio_random(vox_u32 *state)
{
    *state = *state * 1664525U + 1013904223U;
    return *state;
}

static vox_u32 vox_audio_milliseconds(vox_u32 rate, vox_u32 milliseconds)
{
    vox_u32 samples;

    samples = (rate / 1000U) * milliseconds;
    samples += ((rate % 1000U) * milliseconds) / 1000U;
    if (milliseconds != 0U && samples == 0U) {
        samples = 1U;
    }
    return samples;
}

static vox_u32 vox_audio_frequency_step(vox_u32 rate, vox_u32 frequency)
{
    vox_u32 quotient;
    vox_u32 remainder;

    if (frequency == 0U) {
        frequency = 1U;
    }
    if (frequency >= rate / 2U) {
        frequency = rate / 2U - 1U;
    }
    quotient = 0xffffffffU / rate;
    remainder = 0xffffffffU % rate;
    return quotient * frequency + (remainder * frequency) / rate;
}

static vox_u32 vox_audio_pitch_frequency(vox_u32 frequency,
                                         vox_i16 pitch_percent)
{
    vox_i32 scale;
    vox_u32 result;

    scale = 100L + (vox_i32)pitch_percent;
    if (scale < 25L) {
        scale = 25L;
    } else if (scale > 300L) {
        scale = 300L;
    }
    result = (frequency * (vox_u32)scale) / 100U;
    return result == 0U ? 1U : result;
}

static vox_i16 vox_audio_pan(vox_i16 event_pan, vox_i16 patch_pan)
{
    vox_i32 pan;

    pan = (vox_i32)event_pan + (vox_i32)patch_pan;
    if (pan < -32767L) {
        pan = -32767L;
    } else if (pan > 32767L) {
        pan = 32767L;
    }
    return (vox_i16)pan;
}

static vox_u16 vox_audio_scaled_gain(vox_u16 patch_gain,
                                     vox_u16 event_gain)
{
    vox_u32 product;

    product = (vox_u32)patch_gain * (vox_u32)event_gain;
    return (vox_u16)(product / VOX_AUDIO_GAIN_MAX);
}

static vox_u32 vox_audio_jitter(vox_u32 base, vox_u32 amount,
                                vox_u32 *random_state)
{
    vox_u32 range;
    vox_i32 offset;
    vox_i32 value;

    if (amount == 0U) {
        return base;
    }
    range = amount * 2U + 1U;
    offset = (vox_i32)(vox_audio_random(random_state) % range) -
             (vox_i32)amount;
    value = (vox_i32)base + offset;
    return value < 1L ? 1U : (vox_u32)value;
}

static vox_audio_voice *vox_audio_allocate_voice(vox_audio_engine *engine,
                                                  vox_u8 priority)
{
    vox_u32 offset;
    vox_u32 selected;

    selected = engine->voice_cursor;
    for (offset = 0U; offset < VOX_AUDIO_VOICE_COUNT; ++offset) {
        vox_u32 index;

        index = (engine->voice_cursor + offset) % VOX_AUDIO_VOICE_COUNT;
        if (!engine->voices[index].active) {
            selected = index;
            engine->voice_cursor = (index + 1U) % VOX_AUDIO_VOICE_COUNT;
            return &engine->voices[index];
        }
    }

    for (offset = 1U; offset < VOX_AUDIO_VOICE_COUNT; ++offset) {
        vox_u32 index;
        const vox_audio_voice *candidate;
        const vox_audio_voice *current;

        index = (engine->voice_cursor + offset) % VOX_AUDIO_VOICE_COUNT;
        candidate = &engine->voices[index];
        current = &engine->voices[selected];
        if (candidate->priority < current->priority ||
            (candidate->priority == current->priority &&
             candidate->age_samples > current->age_samples)) {
            selected = index;
        }
    }
    if (engine->voices[selected].priority > priority) {
        return 0;
    }
    engine->voice_cursor = (selected + 1U) % VOX_AUDIO_VOICE_COUNT;
    return &engine->voices[selected];
}

static void vox_audio_begin_transition(vox_audio_voice *voice,
                                       vox_u8 stage, vox_u16 target,
                                       vox_u32 samples)
{
    vox_u32 difference;

    voice->env_stage = stage;
    voice->env_target_q15 = target;
    voice->env_samples_left = samples;
    voice->env_denominator = samples;
    voice->env_error = 0U;
    if (voice->env_level_q15 <= target) {
        difference = (vox_u32)target - (vox_u32)voice->env_level_q15;
        voice->env_direction = 1U;
    } else {
        difference = (vox_u32)voice->env_level_q15 - (vox_u32)target;
        voice->env_direction = 0U;
    }
    voice->env_step = difference / samples;
    voice->env_remainder = difference % samples;
}

static void vox_audio_begin_release(vox_audio_voice *voice)
{
    if (voice->env_release_samples == 0U || voice->env_level_q15 == 0U) {
        voice->env_level_q15 = 0U;
        voice->active = 0U;
        return;
    }
    vox_audio_begin_transition(voice, VOX_AUDIO_ENV_RELEASE, 0U,
                               voice->env_release_samples);
}

static void vox_audio_begin_hold(vox_audio_voice *voice)
{
    voice->env_stage = VOX_AUDIO_ENV_HOLD;
    voice->env_samples_left = voice->env_hold_samples;
    if (voice->env_samples_left == 0U) {
        vox_audio_begin_release(voice);
    }
}

static void vox_audio_begin_decay(vox_audio_voice *voice)
{
    if (voice->env_decay_samples == 0U ||
        voice->env_level_q15 == voice->env_sustain_q15) {
        voice->env_level_q15 = voice->env_sustain_q15;
        vox_audio_begin_hold(voice);
        return;
    }
    vox_audio_begin_transition(voice, VOX_AUDIO_ENV_DECAY,
                               voice->env_sustain_q15,
                               voice->env_decay_samples);
}

static void vox_audio_envelope_advance(vox_audio_voice *voice)
{
    vox_u32 amount;

    if (!voice->active) {
        return;
    }
    if (voice->env_stage == VOX_AUDIO_ENV_HOLD) {
        if (voice->env_samples_left > 0U) {
            voice->env_samples_left--;
        }
        if (voice->env_samples_left == 0U) {
            vox_audio_begin_release(voice);
        }
        return;
    }
    if (voice->env_samples_left == 0U) {
        return;
    }

    amount = voice->env_step;
    voice->env_error += voice->env_remainder;
    if (voice->env_error >= voice->env_denominator) {
        voice->env_error -= voice->env_denominator;
        amount++;
    }
    if (voice->env_direction) {
        voice->env_level_q15 = (vox_u16)
            ((vox_u32)voice->env_level_q15 + amount);
    } else if (amount >= (vox_u32)voice->env_level_q15) {
        voice->env_level_q15 = 0U;
    } else {
        voice->env_level_q15 = (vox_u16)
            ((vox_u32)voice->env_level_q15 - amount);
    }

    voice->env_samples_left--;
    if (voice->env_samples_left == 0U) {
        voice->env_level_q15 = voice->env_target_q15;
        if (voice->env_stage == VOX_AUDIO_ENV_ATTACK) {
            vox_audio_begin_decay(voice);
        } else if (voice->env_stage == VOX_AUDIO_ENV_DECAY) {
            vox_audio_begin_hold(voice);
        } else {
            voice->active = 0U;
        }
    }
}

static vox_u32 vox_audio_polynomial_mask(vox_u8 bits)
{
    if (bits == 4U) {
        return 0x0fU;
    }
    if (bits == 9U) {
        return 0x01ffU;
    }
    return 0x01ffffU;
}

static void vox_audio_polynomial_advance(vox_audio_voice *voice)
{
    vox_u32 tap;
    vox_u32 feedback;
    vox_u32 mask;

    tap = voice->polynomial_bits == 9U ? 4U :
          (voice->polynomial_bits == 17U ? 3U : 1U);
    feedback = (voice->lfsr ^ (voice->lfsr >> tap)) & 1U;
    voice->lfsr = (voice->lfsr >> 1U) |
                  (feedback << (voice->polynomial_bits - 1U));
    mask = vox_audio_polynomial_mask(voice->polynomial_bits);
    voice->lfsr &= mask;
    if (voice->lfsr == 0U) {
        voice->lfsr = mask;
    }
}

static void vox_audio_oscillator_advance(vox_audio_voice *voice)
{
    vox_u32 old_phase;
    vox_u32 amount;

    old_phase = voice->phase;
    voice->phase += voice->step;
    if (voice->waveform != VOX_AUDIO_WAVE_TONE &&
        voice->phase < old_phase) {
        vox_audio_polynomial_advance(voice);
    }

    if (voice->slide_samples_left > 0U) {
        amount = voice->step_delta;
        voice->step_error += voice->step_remainder;
        if (voice->step_error >= voice->slide_denominator) {
            voice->step_error -= voice->slide_denominator;
            amount++;
        }
        if (voice->slide_up) {
            voice->step += amount;
            if (voice->step > voice->step_target) {
                voice->step = voice->step_target;
            }
        } else if (amount >= voice->step) {
            voice->step = voice->step_target;
        } else {
            voice->step -= amount;
            if (voice->step < voice->step_target) {
                voice->step = voice->step_target;
            }
        }
        voice->slide_samples_left--;
        if (voice->slide_samples_left == 0U) {
            voice->step = voice->step_target;
        }
    }
    voice->age_samples++;
}

static void vox_audio_start_patch(vox_audio_engine *engine,
                                  const vox_audio_event *event,
                                  const vox_audio_patch *patch,
                                  vox_u32 patch_index,
                                  vox_u32 event_seed)
{
    vox_audio_voice *voice;
    vox_u32 random_state;
    vox_u32 start_frequency;
    vox_u32 end_frequency;
    vox_u32 slide_samples;
    vox_u32 difference;
    vox_u16 peak;
    vox_u16 sustain;

    random_state = vox_audio_hash_mix(event_seed, patch_index + 1U);
    start_frequency = vox_audio_jitter(patch->start_hz, patch->jitter_hz,
                                       &random_state);
    end_frequency = vox_audio_jitter(patch->end_hz,
                                     patch->jitter_hz / 3U,
                                     &random_state);
    start_frequency = vox_audio_pitch_frequency(start_frequency,
                                                event->pitch_percent);
    end_frequency = vox_audio_pitch_frequency(end_frequency,
                                              event->pitch_percent);
    peak = vox_audio_scaled_gain(patch->peak_q15, event->gain_q15);
    sustain = vox_audio_scaled_gain(patch->sustain_q15, event->gain_q15);

    voice = vox_audio_allocate_voice(engine, patch->priority);
    if (voice == 0) {
        return;
    }
    (void)memset(voice, 0, sizeof(*voice));
    voice->active = 1U;
    voice->waveform = patch->waveform;
    voice->priority = patch->priority;
    voice->bank = (vox_u8)(((vox_u32)(voice - engine->voices)) / 4U);
    voice->pan_q15 = vox_audio_pan(event->pan_q15,
                                  patch->pan_offset_q15);
    voice->phase = vox_audio_random(&random_state);
    voice->step = vox_audio_frequency_step(engine->sample_rate,
                                           start_frequency);
    voice->step_target = vox_audio_frequency_step(engine->sample_rate,
                                                  end_frequency);
    voice->polynomial_bits = patch->waveform == VOX_AUDIO_WAVE_POLY4 ? 4U :
                             (patch->waveform == VOX_AUDIO_WAVE_POLY9 ? 9U :
                              17U);
    voice->lfsr = vox_audio_random(&random_state) &
                  vox_audio_polynomial_mask(voice->polynomial_bits);
    if (voice->lfsr == 0U) {
        voice->lfsr = 1U;
    }

    slide_samples = vox_audio_milliseconds(engine->sample_rate,
                                           (vox_u32)patch->attack_ms +
                                           (vox_u32)patch->decay_ms +
                                           (vox_u32)patch->hold_ms);
    if (slide_samples == 0U) {
        slide_samples = 1U;
    }
    voice->slide_samples_left = slide_samples;
    voice->slide_denominator = slide_samples;
    if (voice->step <= voice->step_target) {
        difference = voice->step_target - voice->step;
        voice->slide_up = 1U;
    } else {
        difference = voice->step - voice->step_target;
        voice->slide_up = 0U;
    }
    voice->step_delta = difference / slide_samples;
    voice->step_remainder = difference % slide_samples;

    voice->env_peak_q15 = peak;
    voice->env_sustain_q15 = sustain;
    voice->env_decay_samples = vox_audio_milliseconds(
        engine->sample_rate, patch->decay_ms);
    voice->env_hold_samples = vox_audio_milliseconds(
        engine->sample_rate, patch->hold_ms);
    voice->env_release_samples = vox_audio_milliseconds(
        engine->sample_rate, patch->release_ms);
    if (patch->attack_ms != 0U) {
        vox_audio_begin_transition(voice, VOX_AUDIO_ENV_ATTACK, peak,
                                   vox_audio_milliseconds(engine->sample_rate,
                                                          patch->attack_ms));
    } else {
        voice->env_level_q15 = peak;
        vox_audio_begin_decay(voice);
    }
}

void vox_audio_event_init(vox_audio_event *event, vox_u16 preset)
{
    if (event == 0) {
        return;
    }
    (void)memset(event, 0, sizeof(*event));
    event->preset = preset;
    event->pan_q15 = VOX_AUDIO_PAN_CENTER;
    event->gain_q15 = VOX_AUDIO_GAIN_MAX;
}

vox_result vox_audio_init(vox_audio_engine *engine, vox_u32 sample_rate,
                          vox_u32 seed)
{
    if (engine == 0 || sample_rate < VOX_AUDIO_RATE_MIN ||
        sample_rate > VOX_AUDIO_RATE_MAX) {
        return VOX_ERR_INVALID;
    }
    (void)memset(engine, 0, sizeof(*engine));
    engine->sample_rate = sample_rate;
    engine->seed = seed == 0U ? 0x564f5801U : seed;
    return VOX_OK;
}

void vox_audio_stop_all(vox_audio_engine *engine)
{
    if (engine == 0) {
        return;
    }
    (void)memset(engine->voices, 0, sizeof(engine->voices));
    engine->voice_cursor = 0U;
}

vox_result vox_audio_emit(vox_audio_engine *engine,
                          const vox_audio_event *event)
{
    vox_u32 event_seed;
    vox_u32 index;
    int found;

    if (engine == 0 || event == 0 ||
        engine->sample_rate < VOX_AUDIO_RATE_MIN ||
        engine->sample_rate > VOX_AUDIO_RATE_MAX ||
        event->preset <= VOX_AUDIO_PRESET_NONE ||
        event->preset >= VOX_AUDIO_PRESET_COUNT ||
        event->gain_q15 > VOX_AUDIO_GAIN_MAX ||
        event->pitch_percent < -75 || event->pitch_percent > 200) {
        return VOX_ERR_INVALID;
    }

    if (event->gain_q15 == 0U) {
        engine->event_serial++;
        return VOX_OK;
    }

    event_seed = engine->seed;
    event_seed = vox_audio_hash_mix(event_seed, engine->event_serial);
    event_seed = vox_audio_hash_mix(event_seed, event->event_id);
    event_seed = vox_audio_hash_mix(event_seed, (vox_u32)event->preset);
    event_seed = vox_audio_hash_mix(event_seed, (vox_u32)event->variant);
    found = 0;
    for (index = 0U;
         index < (vox_u32)(sizeof(vox_audio_patches) /
                           sizeof(vox_audio_patches[0]));
         ++index) {
        if (vox_audio_patches[index].preset == event->preset) {
            vox_audio_start_patch(engine, event, &vox_audio_patches[index],
                                  index, event_seed);
            found = 1;
        }
    }
    engine->event_serial++;
    return found ? VOX_OK : VOX_ERR_INVALID;
}

static vox_i32 vox_audio_voice_sample(const vox_audio_voice *voice)
{
    int positive;

    if (voice->waveform == VOX_AUDIO_WAVE_TONE) {
        positive = (voice->phase & 0x80000000U) != 0U;
    } else {
        positive = (voice->lfsr & 1U) != 0U;
    }
    return positive ? (vox_i32)voice->env_level_q15 :
                      -(vox_i32)voice->env_level_q15;
}

static vox_i16 vox_audio_saturate(vox_i32 sample)
{
    if (sample > 32767L) {
        return (vox_i16)32767;
    }
    if (sample < -32768L) {
        return (vox_i16)-32768;
    }
    return (vox_i16)sample;
}

vox_result vox_audio_render(vox_audio_engine *engine,
                            vox_i16 *interleaved_stereo,
                            vox_u32 frame_count)
{
    vox_u32 frame;

    if (engine == 0 || engine->sample_rate < VOX_AUDIO_RATE_MIN ||
        engine->sample_rate > VOX_AUDIO_RATE_MAX ||
        frame_count > 0x7fffffffU ||
        (interleaved_stereo == 0 && frame_count != 0U)) {
        return VOX_ERR_INVALID;
    }

    for (frame = 0U; frame < frame_count; ++frame) {
        vox_i32 mix_left;
        vox_i32 mix_right;
        vox_u32 index;

        mix_left = 0L;
        mix_right = 0L;
        for (index = 0U; index < VOX_AUDIO_VOICE_COUNT; ++index) {
            vox_audio_voice *voice;
            vox_i32 sample;
            vox_i32 left_gain;
            vox_i32 right_gain;

            voice = &engine->voices[index];
            if (!voice->active) {
                continue;
            }
            sample = vox_audio_voice_sample(voice);
            if (voice->pan_q15 <= 0) {
                left_gain = 32767L;
                right_gain = 32767L + (vox_i32)voice->pan_q15;
            } else {
                left_gain = 32767L - (vox_i32)voice->pan_q15;
                right_gain = 32767L;
            }
            if (sample < 0L) {
                vox_i32 magnitude;

                magnitude = -sample;
                mix_left -= (magnitude * left_gain) / 32767L;
                mix_right -= (magnitude * right_gain) / 32767L;
            } else {
                mix_left += (sample * left_gain) / 32767L;
                mix_right += (sample * right_gain) / 32767L;
            }
            vox_audio_oscillator_advance(voice);
            vox_audio_envelope_advance(voice);
        }
        interleaved_stereo[frame * 2U] = vox_audio_saturate(mix_left);
        interleaved_stereo[frame * 2U + 1U] =
            vox_audio_saturate(mix_right);
        engine->rendered_frames++;
    }
    return VOX_OK;
}

vox_u32 vox_audio_active_voice_count(const vox_audio_engine *engine)
{
    vox_u32 active;
    vox_u32 index;

    if (engine == 0) {
        return 0U;
    }
    active = 0U;
    for (index = 0U; index < VOX_AUDIO_VOICE_COUNT; ++index) {
        if (engine->voices[index].active) {
            active++;
        }
    }
    return active;
}

vox_u32 vox_audio_state_hash(const vox_audio_engine *engine)
{
    vox_u32 hash;
    vox_u32 index;

    if (engine == 0) {
        return 0U;
    }
    hash = 2166136261U;
    hash = vox_audio_hash_mix(hash, engine->sample_rate);
    hash = vox_audio_hash_mix(hash, engine->seed);
    hash = vox_audio_hash_mix(hash, engine->event_serial);
    hash = vox_audio_hash_mix(hash, engine->rendered_frames);
    hash = vox_audio_hash_mix(hash, engine->voice_cursor);
    for (index = 0U; index < VOX_AUDIO_VOICE_COUNT; ++index) {
        const vox_audio_voice *voice;

        voice = &engine->voices[index];
        hash = vox_audio_hash_mix(hash, voice->phase);
        hash = vox_audio_hash_mix(hash, voice->step);
        hash = vox_audio_hash_mix(hash, voice->step_target);
        hash = vox_audio_hash_mix(hash, voice->lfsr);
        hash = vox_audio_hash_mix(hash, voice->age_samples);
        hash = vox_audio_hash_mix(hash, voice->env_level_q15);
        hash = vox_audio_hash_mix(hash, (vox_u32)(vox_u16)voice->pan_q15);
        hash = vox_audio_hash_mix(hash, voice->active);
        hash = vox_audio_hash_mix(hash, voice->waveform);
        hash = vox_audio_hash_mix(hash, voice->priority);
        hash = vox_audio_hash_mix(hash, voice->bank);
    }
    return hash;
}

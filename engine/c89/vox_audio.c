/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_audio.h"

#include <string.h>

#define VOX_AUDIO_ENV_ATTACK 1U
#define VOX_AUDIO_ENV_DECAY 2U
#define VOX_AUDIO_ENV_HOLD 3U
#define VOX_AUDIO_ENV_RELEASE 4U
#define VOX_AUDIO_PHRASE_QUEUED 1U
#define VOX_AUDIO_PHRASE_PLAYING 2U
#define VOX_AUDIO_NO_PHRASE 255U
#define VOX_AUDIO_DEFAULT_MASTER_RAMP_MS 10U
#define VOX_AUDIO_AMBIENCE_RAMP_MS 100U

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

typedef struct vox_audio_allophone_def {
    vox_u16 duration_ms;
    vox_u16 formant_hz[3];
    vox_u8 voiced;
    vox_u8 noise_q8;
} vox_audio_allophone_def;

/*
 * Original compact formant targets.  They are intentionally stylized rather
 * than recordings or an emulation of a commercial speech chip.
 */
static const vox_audio_allophone_def vox_audio_allophones[] = {
    {60U, {0U, 0U, 0U}, 0U, 0U},
    {105U, {730U, 1090U, 2440U}, 1U, 5U},
    {95U, {530U, 1840U, 2480U}, 1U, 4U},
    {90U, {390U, 1990U, 2550U}, 1U, 3U},
    {110U, {570U, 840U, 2410U}, 1U, 4U},
    {95U, {440U, 1020U, 2240U}, 1U, 4U},
    {105U, {660U, 1720U, 2410U}, 1U, 5U},
    {95U, {680U, 1190U, 2480U}, 1U, 5U},
    {125U, {650U, 1020U, 2420U}, 1U, 5U},
    {125U, {600U, 1650U, 2500U}, 1U, 4U},
    {90U, {610U, 1900U, 2570U}, 1U, 4U},
    {105U, {490U, 1350U, 1690U}, 1U, 4U},
    {120U, {500U, 2050U, 2600U}, 1U, 3U},
    {85U, {400U, 1750U, 2450U}, 1U, 3U},
    {105U, {300U, 2300U, 3000U}, 1U, 2U},
    {125U, {500U, 900U, 2450U}, 1U, 4U},
    {130U, {520U, 1300U, 2450U}, 1U, 4U},
    {90U, {460U, 1160U, 2300U}, 1U, 4U},
    {110U, {310U, 1000U, 2350U}, 1U, 3U},
    {52U, {300U, 900U, 2100U}, 1U, 45U},
    {74U, {410U, 2050U, 2750U}, 0U, 220U},
    {55U, {420U, 1450U, 2500U}, 1U, 38U},
    {82U, {350U, 2100U, 2850U}, 0U, 255U},
    {62U, {340U, 1250U, 2200U}, 1U, 46U},
    {75U, {650U, 1350U, 2350U}, 0U, 145U},
    {72U, {450U, 1900U, 2750U}, 1U, 150U},
    {70U, {330U, 1550U, 2600U}, 0U, 205U},
    {78U, {380U, 1250U, 2450U}, 1U, 18U},
    {92U, {260U, 950U, 2100U}, 1U, 12U},
    {88U, {280U, 1650U, 2500U}, 1U, 16U},
    {48U, {280U, 850U, 2100U}, 0U, 190U},
    {90U, {390U, 1120U, 1700U}, 1U, 18U},
    {105U, {310U, 2500U, 3300U}, 0U, 255U},
    {108U, {420U, 2200U, 3000U}, 0U, 235U},
    {48U, {350U, 1800U, 2800U}, 0U, 205U},
    {90U, {350U, 2250U, 3150U}, 0U, 230U},
    {80U, {320U, 1250U, 2300U}, 1U, 90U},
    {88U, {300U, 800U, 2200U}, 1U, 12U},
    {82U, {310U, 2150U, 2950U}, 1U, 9U},
    {98U, {330U, 2350U, 3200U}, 1U, 125U}
};

typedef char vox_audio_allophone_table_contract[
    (sizeof(vox_audio_allophones) / sizeof(vox_audio_allophones[0]) ==
     VOX_AUDIO_ALLOPHONE_COUNT) ? 1 : -1];

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
     13500U, 7000U, 0},
    {VOX_AUDIO_PRESET_ZOOM_CLICK, VOX_AUDIO_WAVE_POLY4, 105U,
     2400U, 950U, 45U, 0U, 0U, 4U, 12U,
     4800U, 4800U, 0},
    {VOX_AUDIO_PRESET_HIT_CONFIRM, VOX_AUDIO_WAVE_TONE, 190U,
     880U, 1320U, 18U, 0U, 4U, 26U, 34U,
     12500U, 9300U, 0},
    {VOX_AUDIO_PRESET_KILL_CONFIRM, VOX_AUDIO_WAVE_TONE, 236U,
     440U, 880U, 12U, 0U, 14U, 58U, 70U,
     18500U, 12500U, -1800},
    {VOX_AUDIO_PRESET_KILL_CONFIRM, VOX_AUDIO_WAVE_TONE, 235U,
     660U, 1320U, 12U, 18U, 12U, 48U, 65U,
     14500U, 10000U, 1800},
    {VOX_AUDIO_PRESET_AMBIENCE_WIND, VOX_AUDIO_WAVE_POLY17, 32U,
     95U, 47U, 18U, 25U, 90U, 180U, 260U,
     3800U, 2600U, 0},
    {VOX_AUDIO_PRESET_AMBIENCE_WATER, VOX_AUDIO_WAVE_POLY9, 34U,
     760U, 180U, 80U, 5U, 35U, 120U, 180U,
     4200U, 2500U, 0},
    {VOX_AUDIO_PRESET_AMBIENCE_LAVA, VOX_AUDIO_WAVE_POLY17, 36U,
     180U, 54U, 45U, 5U, 55U, 160U, 240U,
     5100U, 3300U, 0}
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
    vox_u32 whole_rate;
    vox_u32 fractional_rate;
    vox_u32 fractional_samples;

    whole_rate = rate / 1000U;
    fractional_rate = rate % 1000U;
    if (whole_rate != 0U &&
        milliseconds > 0xffffffffU / whole_rate) {
        return 0xffffffffU;
    }
    samples = whole_rate * milliseconds;
    fractional_samples = (milliseconds / 1000U) * fractional_rate;
    fractional_samples += ((milliseconds % 1000U) * fractional_rate) /
                          1000U;
    if (fractional_samples > 0xffffffffU - samples) {
        return 0xffffffffU;
    }
    samples += fractional_samples;
    if (milliseconds != 0U && samples == 0U) {
        samples = 1U;
    }
    return samples;
}

static vox_u32 vox_audio_frequency_step(vox_u32 rate, vox_u32 frequency)
{
    vox_u32 quotient;
    vox_u32 remainder;
    vox_u32 fractional;
    vox_u32 product_remainder;
    vox_u32 bit;

    if (frequency == 0U) {
        frequency = 1U;
    }
    if (frequency >= rate / 2U) {
        frequency = rate / 2U - 1U;
    }
    quotient = 0xffffffffU / rate;
    remainder = 0xffffffffU % rate;
    /* Exact (remainder * frequency) / rate without requiring a 64-bit type. */
    fractional = 0U;
    product_remainder = 0U;
    bit = 0x80000000U;
    while (bit != 0U) {
        fractional <<= 1U;
        product_remainder <<= 1U;
        if ((frequency & bit) != 0U) {
            product_remainder += remainder;
        }
        while (product_remainder >= rate) {
            product_remainder -= rate;
            fractional++;
        }
        bit >>= 1U;
    }
    return quotient * frequency + fractional;
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

void vox_audio_config_init(vox_audio_config *config, vox_u32 sample_rate,
                           vox_u32 seed)
{
    if (config == 0) {
        return;
    }
    (void)memset(config, 0, sizeof(*config));
    config->sample_rate = sample_rate;
    config->seed = seed;
    config->master_gain_q15 = VOX_AUDIO_GAIN_MAX;
    config->master_ramp_ms = VOX_AUDIO_DEFAULT_MASTER_RAMP_MS;
}

void vox_audio_note_init(vox_audio_note *note, vox_u16 frequency_hz,
                         vox_u16 duration_ms)
{
    if (note == 0) {
        return;
    }
    (void)memset(note, 0, sizeof(*note));
    note->frequency_hz = frequency_hz;
    note->duration_ms = duration_ms;
    note->gain_q15 = 11500U;
    note->pan_q15 = VOX_AUDIO_PAN_CENTER;
    note->waveform = VOX_AUDIO_WAVE_TONE;
    note->bus = VOX_AUDIO_BUS_UI;
    note->priority = VOX_AUDIO_PRIORITY_UI;
}

void vox_audio_speech_init(vox_audio_speech *speech,
                           const vox_u8 *allophones,
                           vox_u16 allophone_count)
{
    if (speech == 0) {
        return;
    }
    (void)memset(speech, 0, sizeof(*speech));
    speech->allophones = allophones;
    speech->allophone_count = allophone_count;
    speech->gain_q15 = 23000U;
    speech->pan_q15 = VOX_AUDIO_PAN_CENTER;
    speech->profile = VOX_AUDIO_SPEECH_HIGH;
    speech->priority = VOX_AUDIO_PRIORITY_BARK;
}

vox_result vox_audio_init_ex(vox_audio_engine *engine,
                             const vox_audio_config *config)
{
    vox_u32 index;

    if (engine == 0 || config == 0 ||
        config->sample_rate < VOX_AUDIO_RATE_MIN ||
        config->sample_rate > VOX_AUDIO_RATE_MAX ||
        config->master_gain_q15 > VOX_AUDIO_GAIN_MAX ||
        config->master_ramp_ms > 1000U) {
        return VOX_ERR_INVALID;
    }
    (void)memset(engine, 0, sizeof(*engine));
    engine->sample_rate = config->sample_rate;
    engine->seed = config->seed == 0U ? 0x564f5801U : config->seed;
    engine->master_gain_q15 = config->master_gain_q15;
    engine->master_target_q15 = config->master_gain_q15;
    engine->master_ramp_ms = config->master_ramp_ms;
    engine->active_phrase = VOX_AUDIO_NO_PHRASE;
    engine->speech_lfsr = engine->seed ^ 0x53504545U;
    if (engine->speech_lfsr == 0U) {
        engine->speech_lfsr = 1U;
    }
    for (index = 0U; index < VOX_AUDIO_AMBIENCE_COUNT; ++index) {
        engine->ambience[index].lfsr =
            (engine->seed ^ (0x414d4201U + index * 0x10203U)) |
            1U;
        engine->ambience[index].step = vox_audio_frequency_step(
            engine->sample_rate,
            index == VOX_AUDIO_AMBIENCE_WIND ? 900U :
            (index == VOX_AUDIO_AMBIENCE_WATER ? 1500U : 480U));
        engine->ambience[index].step_secondary = vox_audio_frequency_step(
            engine->sample_rate,
            index == VOX_AUDIO_AMBIENCE_WIND ? 37U :
            (index == VOX_AUDIO_AMBIENCE_WATER ? 410U : 65U));
    }
    return VOX_OK;
}

vox_result vox_audio_init(vox_audio_engine *engine, vox_u32 sample_rate,
                          vox_u32 seed)
{
    vox_audio_config config;

    vox_audio_config_init(&config, sample_rate, seed);
    return vox_audio_init_ex(engine, &config);
}

vox_result vox_audio_set_master_gain(vox_audio_engine *engine,
                                     vox_u16 gain_q15)
{
    vox_u32 samples;
    vox_u32 difference;

    if (engine == 0 || engine->sample_rate < VOX_AUDIO_RATE_MIN ||
        engine->sample_rate > VOX_AUDIO_RATE_MAX ||
        gain_q15 > VOX_AUDIO_GAIN_MAX) {
        return VOX_ERR_INVALID;
    }
    if (gain_q15 == engine->master_target_q15) {
        return VOX_OK;
    }
    engine->master_target_q15 = gain_q15;
    samples = vox_audio_milliseconds(engine->sample_rate,
                                     engine->master_ramp_ms);
    if (samples == 0U || gain_q15 == engine->master_gain_q15) {
        engine->master_gain_q15 = gain_q15;
        engine->master_samples_left = 0U;
        engine->master_step = 0U;
        engine->master_remainder = 0U;
        engine->master_error = 0U;
        engine->master_denominator = 0U;
        return VOX_OK;
    }
    if (engine->master_gain_q15 < gain_q15) {
        difference = (vox_u32)gain_q15 -
                     (vox_u32)engine->master_gain_q15;
        engine->master_gain_up = 1U;
    } else {
        difference = (vox_u32)engine->master_gain_q15 -
                     (vox_u32)gain_q15;
        engine->master_gain_up = 0U;
    }
    engine->master_samples_left = samples;
    engine->master_denominator = samples;
    engine->master_step = difference / samples;
    engine->master_remainder = difference % samples;
    engine->master_error = 0U;
    return VOX_OK;
}

vox_u16 vox_audio_master_gain(const vox_audio_engine *engine)
{
    return engine == 0 ? 0U : engine->master_target_q15;
}

vox_u32 vox_audio_sample_clock(const vox_audio_engine *engine)
{
    return engine == 0 ? 0U : engine->rendered_frames;
}

vox_u32 vox_audio_ms_to_frames(const vox_audio_engine *engine,
                               vox_u32 milliseconds)
{
    if (engine == 0 || engine->sample_rate < VOX_AUDIO_RATE_MIN ||
        engine->sample_rate > VOX_AUDIO_RATE_MAX) {
        return 0U;
    }
    return vox_audio_milliseconds(engine->sample_rate, milliseconds);
}

void vox_audio_stop(vox_audio_engine *engine, vox_u16 stop_mask)
{
    if (engine == 0) {
        return;
    }
    if ((stop_mask & VOX_AUDIO_STOP_VOICES) != 0U) {
        (void)memset(engine->voices, 0, sizeof(engine->voices));
        engine->voice_cursor = 0U;
    }
    if ((stop_mask & VOX_AUDIO_STOP_NOTES) != 0U) {
        (void)memset(engine->notes, 0, sizeof(engine->notes));
        engine->note_count = 0U;
        engine->note_serial = 0U;
    }
    if ((stop_mask & VOX_AUDIO_STOP_SPEECH) != 0U) {
        (void)memset(engine->phrases, 0, sizeof(engine->phrases));
        engine->active_phrase = VOX_AUDIO_NO_PHRASE;
        engine->speech_token_index = 0U;
        engine->speech_token_sample = 0U;
        engine->speech_token_samples = 0U;
    }
    if ((stop_mask & VOX_AUDIO_STOP_AMBIENCE) != 0U) {
        vox_u32 index;

        for (index = 0U; index < VOX_AUDIO_AMBIENCE_COUNT; ++index) {
            engine->ambience[index].gain_q15 = 0U;
            engine->ambience[index].target_gain_q15 = 0U;
            engine->ambience[index].gain_samples_left = 0U;
            engine->ambience[index].filtered_sample = 0L;
        }
    }
}

void vox_audio_stop_all(vox_audio_engine *engine)
{
    vox_audio_stop(engine, VOX_AUDIO_STOP_ALL);
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
        event->pan_q15 < VOX_AUDIO_PAN_LEFT ||
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

static void vox_audio_start_note(vox_audio_engine *engine,
                                 const vox_audio_note *note,
                                 vox_u32 slot_index)
{
    vox_audio_patch patch;
    vox_audio_event event;
    vox_u16 attack_ms;
    vox_u16 release_ms;
    vox_u16 hold_ms;
    vox_u32 event_seed;

    attack_ms = note->duration_ms >= 8U ? 2U : 0U;
    release_ms = note->duration_ms >= 12U ? 8U :
                 (note->duration_ms > 2U ?
                  (vox_u16)(note->duration_ms / 2U) : 1U);
    if ((vox_u32)attack_ms + (vox_u32)release_ms >=
        (vox_u32)note->duration_ms) {
        hold_ms = 0U;
    } else {
        hold_ms = (vox_u16)(note->duration_ms - attack_ms - release_ms);
    }
    (void)memset(&patch, 0, sizeof(patch));
    patch.waveform = note->waveform;
    patch.priority = note->priority;
    patch.start_hz = note->frequency_hz;
    patch.end_hz = note->frequency_hz;
    patch.attack_ms = attack_ms;
    patch.hold_ms = hold_ms;
    patch.release_ms = release_ms;
    patch.peak_q15 = VOX_AUDIO_GAIN_MAX;
    patch.sustain_q15 = VOX_AUDIO_GAIN_MAX;

    vox_audio_event_init(&event, VOX_AUDIO_PRESET_UI_MOVE);
    event.gain_q15 = note->gain_q15;
    event.pan_q15 = note->pan_q15;
    event.event_id = note->event_id;
    event_seed = vox_audio_hash_mix(engine->seed, note->event_id);
    event_seed = vox_audio_hash_mix(event_seed, slot_index);
    vox_audio_start_patch(engine, &event, &patch, slot_index, event_seed);
}

vox_result vox_audio_schedule_note(vox_audio_engine *engine,
                                   const vox_audio_note *note)
{
    vox_u32 index;

    if (engine == 0 || note == 0 ||
        engine->sample_rate < VOX_AUDIO_RATE_MIN ||
        engine->sample_rate > VOX_AUDIO_RATE_MAX ||
        note->frequency_hz == 0U ||
        (vox_u32)note->frequency_hz >= engine->sample_rate / 2U ||
        note->duration_ms == 0U || note->gain_q15 > VOX_AUDIO_GAIN_MAX ||
        note->pan_q15 < VOX_AUDIO_PAN_LEFT ||
        note->waveform > VOX_AUDIO_WAVE_POLY17 ||
        note->bus >= VOX_AUDIO_BUS_COUNT ||
        note->delay_frames > 0x7fffffffU) {
        return VOX_ERR_INVALID;
    }
    if (note->gain_q15 == 0U) {
        engine->event_serial++;
        return VOX_OK;
    }
    if (engine->note_count >= VOX_AUDIO_NOTE_CAPACITY) {
        return VOX_ERR_CAPACITY;
    }
    if (engine->note_count == 0U) {
        engine->note_serial = 0U;
    }
    for (index = 0U; index < VOX_AUDIO_NOTE_CAPACITY; ++index) {
        if (!engine->notes[index].active) {
            engine->notes[index].note = *note;
            engine->notes[index].serial = engine->note_serial++;
            engine->notes[index].active = 1U;
            engine->note_count++;
            engine->event_serial++;
            return VOX_OK;
        }
    }
    return VOX_ERR_CAPACITY;
}

static void vox_audio_select_phrase(vox_audio_engine *engine)
{
    vox_u32 index;
    vox_u32 selected;

    if (engine->active_phrase != VOX_AUDIO_NO_PHRASE) {
        return;
    }
    selected = VOX_AUDIO_SPEECH_CAPACITY;
    for (index = 0U; index < VOX_AUDIO_SPEECH_CAPACITY; ++index) {
        const vox_audio_phrase_slot *candidate;

        candidate = &engine->phrases[index];
        if (candidate->state != VOX_AUDIO_PHRASE_QUEUED) {
            continue;
        }
        if (selected == VOX_AUDIO_SPEECH_CAPACITY ||
            candidate->priority > engine->phrases[selected].priority ||
            (candidate->priority == engine->phrases[selected].priority &&
             candidate->serial < engine->phrases[selected].serial)) {
            selected = index;
        }
    }
    if (selected == VOX_AUDIO_SPEECH_CAPACITY) {
        return;
    }
    engine->active_phrase = (vox_u8)selected;
    engine->phrases[selected].state = VOX_AUDIO_PHRASE_PLAYING;
    engine->speech_token_index = 0U;
    engine->speech_token_sample = 0U;
    engine->speech_token_samples = 0U;
    engine->speech_pitch_phase = 0U;
    engine->speech_formant_phase[0] = 0U;
    engine->speech_formant_phase[1] = 0x40000000U;
    engine->speech_formant_phase[2] = 0x80000000U;
    engine->speech_lfsr = vox_audio_hash_mix(
        engine->seed ^ 0x53504545U,
        engine->phrases[selected].event_id) | 1U;
}

vox_result vox_audio_speak(vox_audio_engine *engine,
                           const vox_audio_speech *speech)
{
    vox_u32 index;
    vox_u32 selected;

    if (engine == 0 || speech == 0 || speech->allophones == 0 ||
        engine->sample_rate < VOX_AUDIO_RATE_MIN ||
        engine->sample_rate > VOX_AUDIO_RATE_MAX ||
        speech->allophone_count == 0U ||
        speech->allophone_count > VOX_AUDIO_SPEECH_TOKEN_CAPACITY ||
        speech->gain_q15 > VOX_AUDIO_GAIN_MAX ||
        speech->pan_q15 < VOX_AUDIO_PAN_LEFT ||
        speech->profile >= VOX_AUDIO_SPEECH_PROFILE_COUNT) {
        return VOX_ERR_INVALID;
    }
    for (index = 0U; index < (vox_u32)speech->allophone_count; ++index) {
        if (speech->allophones[index] >= VOX_AUDIO_ALLOPHONE_COUNT) {
            return VOX_ERR_INVALID;
        }
    }
    if (speech->gain_q15 == 0U) {
        engine->event_serial++;
        return VOX_OK;
    }

    selected = VOX_AUDIO_SPEECH_CAPACITY;
    if (engine->active_phrase != VOX_AUDIO_NO_PHRASE &&
        speech->priority >
        engine->phrases[engine->active_phrase].priority) {
        selected = engine->active_phrase;
        engine->phrases[selected].state = 0U;
        engine->active_phrase = VOX_AUDIO_NO_PHRASE;
    }
    if (selected == VOX_AUDIO_SPEECH_CAPACITY) {
        for (index = 0U; index < VOX_AUDIO_SPEECH_CAPACITY; ++index) {
            if (engine->phrases[index].state == 0U) {
                selected = index;
                break;
            }
        }
    }
    if (selected == VOX_AUDIO_SPEECH_CAPACITY) {
        for (index = 0U; index < VOX_AUDIO_SPEECH_CAPACITY; ++index) {
            if (engine->phrases[index].state ==
                    VOX_AUDIO_PHRASE_QUEUED &&
                (selected == VOX_AUDIO_SPEECH_CAPACITY ||
                 engine->phrases[index].priority <
                    engine->phrases[selected].priority ||
                 (engine->phrases[index].priority ==
                    engine->phrases[selected].priority &&
                  engine->phrases[index].serial >
                    engine->phrases[selected].serial))) {
                selected = index;
            }
        }
        if (selected == VOX_AUDIO_SPEECH_CAPACITY ||
            speech->priority <= engine->phrases[selected].priority) {
            return VOX_ERR_CAPACITY;
        }
    }

    (void)memset(&engine->phrases[selected], 0,
                 sizeof(engine->phrases[selected]));
    engine->phrases[selected].event_id = speech->event_id;
    engine->phrases[selected].serial = engine->phrase_serial++;
    engine->phrases[selected].allophone_count = speech->allophone_count;
    engine->phrases[selected].gain_q15 = speech->gain_q15;
    engine->phrases[selected].pan_q15 = speech->pan_q15;
    engine->phrases[selected].profile = speech->profile;
    engine->phrases[selected].priority = speech->priority;
    engine->phrases[selected].state = VOX_AUDIO_PHRASE_QUEUED;
    (void)memcpy(engine->phrases[selected].allophones,
                 speech->allophones, speech->allophone_count);
    engine->event_serial++;
    vox_audio_select_phrase(engine);
    return VOX_OK;
}

vox_result vox_audio_set_ambience(vox_audio_engine *engine,
                                  vox_u8 ambience,
                                  vox_u16 gain_q15,
                                  vox_i16 pan_q15)
{
    vox_audio_ambience_state *state;
    vox_u32 samples;
    vox_u32 difference;

    if (engine == 0 || engine->sample_rate < VOX_AUDIO_RATE_MIN ||
        engine->sample_rate > VOX_AUDIO_RATE_MAX ||
        ambience >= VOX_AUDIO_AMBIENCE_COUNT ||
        gain_q15 > VOX_AUDIO_GAIN_MAX ||
        pan_q15 < VOX_AUDIO_PAN_LEFT) {
        return VOX_ERR_INVALID;
    }
    state = &engine->ambience[ambience];
    state->pan_q15 = pan_q15;
    if (state->target_gain_q15 == gain_q15) {
        return VOX_OK;
    }
    state->target_gain_q15 = gain_q15;
    samples = vox_audio_milliseconds(engine->sample_rate,
                                     VOX_AUDIO_AMBIENCE_RAMP_MS);
    if (state->gain_q15 == gain_q15 || samples == 0U) {
        state->gain_q15 = gain_q15;
        state->gain_samples_left = 0U;
        return VOX_OK;
    }
    if (state->gain_q15 < gain_q15) {
        difference = (vox_u32)gain_q15 - (vox_u32)state->gain_q15;
        state->gain_up = 1U;
    } else {
        difference = (vox_u32)state->gain_q15 - (vox_u32)gain_q15;
        state->gain_up = 0U;
    }
    state->gain_samples_left = samples;
    state->gain_denominator = samples;
    state->gain_step = difference / samples;
    state->gain_remainder = difference % samples;
    state->gain_error = 0U;
    return VOX_OK;
}

static void vox_audio_master_advance(vox_audio_engine *engine)
{
    vox_u32 amount;

    if (engine->master_samples_left == 0U) {
        return;
    }
    amount = engine->master_step;
    engine->master_error += engine->master_remainder;
    if (engine->master_error >= engine->master_denominator) {
        engine->master_error -= engine->master_denominator;
        amount++;
    }
    if (engine->master_gain_up) {
        engine->master_gain_q15 = (vox_u16)
            ((vox_u32)engine->master_gain_q15 + amount);
    } else if (amount >= (vox_u32)engine->master_gain_q15) {
        engine->master_gain_q15 = 0U;
    } else {
        engine->master_gain_q15 = (vox_u16)
            ((vox_u32)engine->master_gain_q15 - amount);
    }
    engine->master_samples_left--;
    if (engine->master_samples_left == 0U) {
        engine->master_gain_q15 = engine->master_target_q15;
    }
}

static void vox_audio_process_notes(vox_audio_engine *engine)
{
    vox_u32 index;
    vox_u32 selected;

    for (;;) {
        selected = VOX_AUDIO_NOTE_CAPACITY;
        for (index = 0U; index < VOX_AUDIO_NOTE_CAPACITY; ++index) {
            const vox_audio_note_slot *candidate;

            candidate = &engine->notes[index];
            if (candidate->active && candidate->note.delay_frames == 0U &&
                (selected == VOX_AUDIO_NOTE_CAPACITY ||
                 candidate->serial < engine->notes[selected].serial)) {
                selected = index;
            }
        }
        if (selected == VOX_AUDIO_NOTE_CAPACITY) {
            break;
        }
        vox_audio_start_note(engine, &engine->notes[selected].note,
                             engine->notes[selected].serial);
        (void)memset(&engine->notes[selected], 0,
                     sizeof(engine->notes[selected]));
        engine->note_count--;
    }
    for (index = 0U; index < VOX_AUDIO_NOTE_CAPACITY; ++index) {
        vox_audio_note_slot *slot;

        slot = &engine->notes[index];
        if (slot->active && slot->note.delay_frames > 0U) {
            slot->note.delay_frames--;
        }
    }
}

static vox_i32 vox_audio_scale_signed(vox_i32 sample, vox_u16 gain_q15)
{
    vox_u32 magnitude;
    vox_u32 scaled;

    if (sample < 0L) {
        magnitude = (vox_u32)(-sample);
    } else {
        magnitude = (vox_u32)sample;
    }
    scaled = (magnitude / VOX_AUDIO_GAIN_MAX) * (vox_u32)gain_q15;
    scaled += ((magnitude % VOX_AUDIO_GAIN_MAX) *
               (vox_u32)gain_q15) / VOX_AUDIO_GAIN_MAX;
    return sample < 0L ? -(vox_i32)scaled : (vox_i32)scaled;
}

static int vox_audio_prepare_speech_token(vox_audio_engine *engine)
{
    vox_audio_phrase_slot *phrase;
    const vox_audio_allophone_def *definition;
    vox_u32 duration;
    vox_u32 pitch_hz;
    vox_u32 formant_scale;
    vox_u32 formant;

    vox_audio_select_phrase(engine);
    while (engine->active_phrase != VOX_AUDIO_NO_PHRASE) {
        phrase = &engine->phrases[engine->active_phrase];
        if (engine->speech_token_index < phrase->allophone_count) {
            break;
        }
        (void)memset(phrase, 0, sizeof(*phrase));
        engine->active_phrase = VOX_AUDIO_NO_PHRASE;
        engine->speech_token_index = 0U;
        engine->speech_token_sample = 0U;
        engine->speech_token_samples = 0U;
        vox_audio_select_phrase(engine);
    }
    if (engine->active_phrase == VOX_AUDIO_NO_PHRASE) {
        return 0;
    }
    if (engine->speech_token_samples != 0U) {
        return 1;
    }
    phrase = &engine->phrases[engine->active_phrase];
    definition = &vox_audio_allophones[
        phrase->allophones[engine->speech_token_index]];
    duration = definition->duration_ms;
    if (phrase->profile == VOX_AUDIO_SPEECH_DEEP) {
        duration = (duration * 115U) / 100U;
    } else {
        duration = (duration * 88U) / 100U;
    }
    if (duration == 0U) {
        duration = 1U;
    }
    engine->speech_token_samples = vox_audio_milliseconds(
        engine->sample_rate, duration);
    engine->speech_token_sample = 0U;
    pitch_hz = phrase->profile == VOX_AUDIO_SPEECH_DEEP ? 92U : 210U;
    formant_scale = phrase->profile == VOX_AUDIO_SPEECH_DEEP ? 90U : 118U;
    engine->speech_pitch_step = vox_audio_frequency_step(
        engine->sample_rate, pitch_hz);
    for (formant = 0U; formant < 3U; ++formant) {
        duration = ((vox_u32)definition->formant_hz[formant] *
                    formant_scale) / 100U;
        engine->speech_formant_step[formant] = duration == 0U ? 0U :
            vox_audio_frequency_step(engine->sample_rate, duration);
    }
    return 1;
}

static vox_i32 vox_audio_speech_sample(vox_audio_engine *engine,
                                       vox_i16 *pan_q15)
{
    vox_audio_phrase_slot *phrase;
    const vox_audio_allophone_def *definition;
    vox_u32 token;
    vox_u32 index;
    vox_u32 noise_state;
    vox_i32 sample;
    vox_i32 component;
    vox_i32 noise;

    if (!vox_audio_prepare_speech_token(engine)) {
        *pan_q15 = VOX_AUDIO_PAN_CENTER;
        return 0L;
    }
    phrase = &engine->phrases[engine->active_phrase];
    token = phrase->allophones[engine->speech_token_index];
    definition = &vox_audio_allophones[token];
    *pan_q15 = phrase->pan_q15;
    engine->speech_pitch_phase += engine->speech_pitch_step;

    sample = 0L;
    for (index = 0U; index < 3U; ++index) {
        if (engine->speech_formant_step[index] != 0U) {
            engine->speech_formant_phase[index] +=
                engine->speech_formant_step[index];
            component =
                (engine->speech_formant_phase[index] & 0x80000000U) != 0U ?
                (index == 0U ? 5600L : (index == 1U ? 3200L : 1800L)) :
                -(index == 0U ? 5600L : (index == 1U ? 3200L : 1800L));
            if (definition->voiced &&
                (engine->speech_pitch_phase & 0x80000000U) == 0U) {
                component /= 2L;
            }
            sample += component;
        }
    }
    noise_state = engine->speech_lfsr;
    noise_state ^= noise_state << 13U;
    noise_state ^= noise_state >> 17U;
    noise_state ^= noise_state << 5U;
    engine->speech_lfsr = noise_state == 0U ? 1U : noise_state;
    noise = (engine->speech_lfsr & 1U) != 0U ?
            (vox_i32)definition->noise_q8 * 20L :
            -(vox_i32)definition->noise_q8 * 20L;
    sample += noise;
    sample = vox_audio_scale_signed(sample, phrase->gain_q15);

    engine->speech_token_sample++;
    if (engine->speech_token_sample >= engine->speech_token_samples) {
        engine->speech_token_index++;
        engine->speech_token_sample = 0U;
        engine->speech_token_samples = 0U;
    }
    return sample;
}

static void vox_audio_ambience_gain_advance(
    vox_audio_ambience_state *state)
{
    vox_u32 amount;

    if (state->gain_samples_left == 0U) {
        return;
    }
    amount = state->gain_step;
    state->gain_error += state->gain_remainder;
    if (state->gain_error >= state->gain_denominator) {
        state->gain_error -= state->gain_denominator;
        amount++;
    }
    if (state->gain_up) {
        state->gain_q15 = (vox_u16)((vox_u32)state->gain_q15 + amount);
    } else if (amount >= (vox_u32)state->gain_q15) {
        state->gain_q15 = 0U;
    } else {
        state->gain_q15 = (vox_u16)((vox_u32)state->gain_q15 - amount);
    }
    state->gain_samples_left--;
    if (state->gain_samples_left == 0U) {
        state->gain_q15 = state->target_gain_q15;
    }
}

static void vox_audio_ambience_lfsr(vox_audio_ambience_state *state)
{
    vox_u32 feedback;

    feedback = (state->lfsr ^ (state->lfsr >> 3U)) & 1U;
    state->lfsr = (state->lfsr >> 1U) | (feedback << 16U);
    state->lfsr &= 0x01ffffU;
    if (state->lfsr == 0U) {
        state->lfsr = 0x01ffffU;
    }
}

static vox_i32 vox_audio_ambience_sample(vox_audio_engine *engine,
                                         vox_u32 ambience)
{
    vox_audio_ambience_state *state;
    vox_u32 old_phase;
    vox_i32 raw;
    vox_i32 tone;
    vox_i32 divisor;

    state = &engine->ambience[ambience];
    vox_audio_ambience_gain_advance(state);
    if (state->gain_q15 == 0U) {
        return 0L;
    }
    old_phase = state->phase;
    state->phase += state->step;
    if (state->phase < old_phase) {
        vox_audio_ambience_lfsr(state);
    }
    state->phase_secondary += state->step_secondary;
    raw = (state->lfsr & 1U) != 0U ? 7600L : -7600L;
    tone = (state->phase_secondary & 0x80000000U) != 0U ? 2200L : -2200L;
    if (ambience == VOX_AUDIO_AMBIENCE_WIND) {
        raw += tone / 4L;
        divisor = 32L;
    } else if (ambience == VOX_AUDIO_AMBIENCE_WATER) {
        raw = raw / 2L + tone;
        divisor = 8L;
    } else {
        raw += tone;
        divisor = 16L;
    }
    state->filtered_sample += (raw - state->filtered_sample) / divisor;
    return vox_audio_scale_signed(state->filtered_sample,
                                  state->gain_q15);
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

static void vox_audio_mix_pan(vox_i32 sample, vox_i16 pan_q15,
                              vox_i32 *mix_left, vox_i32 *mix_right)
{
    vox_i32 left_gain;
    vox_i32 right_gain;
    vox_i32 magnitude;

    if (pan_q15 <= 0) {
        left_gain = 32767L;
        right_gain = 32767L + (vox_i32)pan_q15;
    } else {
        left_gain = 32767L - (vox_i32)pan_q15;
        right_gain = 32767L;
    }
    if (sample < 0L) {
        magnitude = -sample;
        *mix_left -= (magnitude * left_gain) / 32767L;
        *mix_right -= (magnitude * right_gain) / 32767L;
    } else {
        *mix_left += (sample * left_gain) / 32767L;
        *mix_right += (sample * right_gain) / 32767L;
    }
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
        vox_i32 extra_sample;
        vox_i16 extra_pan;

        vox_audio_process_notes(engine);
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

        extra_sample = vox_audio_speech_sample(engine, &extra_pan);
        if (extra_sample != 0L) {
            vox_audio_mix_pan(extra_sample, extra_pan,
                              &mix_left, &mix_right);
        }
        for (index = 0U; index < VOX_AUDIO_AMBIENCE_COUNT; ++index) {
            extra_sample = vox_audio_ambience_sample(engine, index);
            if (extra_sample != 0L) {
                vox_audio_mix_pan(extra_sample,
                                  engine->ambience[index].pan_q15,
                                  &mix_left, &mix_right);
            }
        }
        if (engine->master_gain_q15 != VOX_AUDIO_GAIN_MAX) {
            mix_left = vox_audio_scale_signed(mix_left,
                                              engine->master_gain_q15);
            mix_right = vox_audio_scale_signed(mix_right,
                                               engine->master_gain_q15);
        }
        interleaved_stereo[frame * 2U] = vox_audio_saturate(mix_left);
        interleaved_stereo[frame * 2U + 1U] =
            vox_audio_saturate(mix_right);
        vox_audio_master_advance(engine);
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

int vox_audio_has_pending(const vox_audio_engine *engine)
{
    vox_u32 index;

    if (engine == 0) {
        return 0;
    }
    if (engine->note_count != 0U ||
        engine->active_phrase != VOX_AUDIO_NO_PHRASE ||
        engine->master_samples_left != 0U ||
        vox_audio_active_voice_count(engine) != 0U) {
        return 1;
    }
    for (index = 0U; index < VOX_AUDIO_SPEECH_CAPACITY; ++index) {
        if (engine->phrases[index].state != 0U) {
            return 1;
        }
    }
    for (index = 0U; index < VOX_AUDIO_AMBIENCE_COUNT; ++index) {
        if (engine->ambience[index].gain_q15 != 0U ||
            engine->ambience[index].target_gain_q15 != 0U ||
            engine->ambience[index].gain_samples_left != 0U) {
            return 1;
        }
    }
    return 0;
}

vox_u32 vox_audio_state_hash(const vox_audio_engine *engine)
{
    vox_u32 hash;
    vox_u32 index;

    if (engine == 0) {
        return 0U;
    }
    hash = 2166136261U;
    hash = vox_audio_hash_mix(hash, VOX_AUDIO_VERSION);
    hash = vox_audio_hash_mix(hash, engine->sample_rate);
    hash = vox_audio_hash_mix(hash, engine->seed);
    hash = vox_audio_hash_mix(hash, engine->event_serial);
    hash = vox_audio_hash_mix(hash, engine->rendered_frames);
    hash = vox_audio_hash_mix(hash, engine->voice_cursor);
    hash = vox_audio_hash_mix(hash, engine->note_count);
    hash = vox_audio_hash_mix(hash, engine->note_serial);
    hash = vox_audio_hash_mix(hash, engine->phrase_serial);
    hash = vox_audio_hash_mix(hash, engine->master_gain_q15);
    hash = vox_audio_hash_mix(hash, engine->master_target_q15);
    hash = vox_audio_hash_mix(hash, engine->master_ramp_ms);
    hash = vox_audio_hash_mix(hash, engine->master_step);
    hash = vox_audio_hash_mix(hash, engine->master_remainder);
    hash = vox_audio_hash_mix(hash, engine->master_error);
    hash = vox_audio_hash_mix(hash, engine->master_denominator);
    hash = vox_audio_hash_mix(hash, engine->master_samples_left);
    hash = vox_audio_hash_mix(hash, engine->master_gain_up);
    hash = vox_audio_hash_mix(hash, engine->speech_token_index);
    hash = vox_audio_hash_mix(hash, engine->speech_token_sample);
    hash = vox_audio_hash_mix(hash, engine->speech_token_samples);
    hash = vox_audio_hash_mix(hash, engine->speech_pitch_phase);
    hash = vox_audio_hash_mix(hash, engine->speech_pitch_step);
    hash = vox_audio_hash_mix(hash, engine->speech_lfsr);
    hash = vox_audio_hash_mix(hash, engine->active_phrase);
    for (index = 0U; index < 3U; ++index) {
        hash = vox_audio_hash_mix(hash, engine->speech_formant_phase[index]);
        hash = vox_audio_hash_mix(hash, engine->speech_formant_step[index]);
    }
    for (index = 0U; index < VOX_AUDIO_VOICE_COUNT; ++index) {
        const vox_audio_voice *voice;

        voice = &engine->voices[index];
        hash = vox_audio_hash_mix(hash, voice->phase);
        hash = vox_audio_hash_mix(hash, voice->step);
        hash = vox_audio_hash_mix(hash, voice->step_target);
        hash = vox_audio_hash_mix(hash, voice->step_delta);
        hash = vox_audio_hash_mix(hash, voice->step_remainder);
        hash = vox_audio_hash_mix(hash, voice->step_error);
        hash = vox_audio_hash_mix(hash, voice->slide_denominator);
        hash = vox_audio_hash_mix(hash, voice->slide_samples_left);
        hash = vox_audio_hash_mix(hash, voice->lfsr);
        hash = vox_audio_hash_mix(hash, voice->age_samples);
        hash = vox_audio_hash_mix(hash, voice->env_step);
        hash = vox_audio_hash_mix(hash, voice->env_remainder);
        hash = vox_audio_hash_mix(hash, voice->env_error);
        hash = vox_audio_hash_mix(hash, voice->env_denominator);
        hash = vox_audio_hash_mix(hash, voice->env_samples_left);
        hash = vox_audio_hash_mix(hash, voice->env_decay_samples);
        hash = vox_audio_hash_mix(hash, voice->env_hold_samples);
        hash = vox_audio_hash_mix(hash, voice->env_release_samples);
        hash = vox_audio_hash_mix(hash, voice->env_level_q15);
        hash = vox_audio_hash_mix(hash, voice->env_target_q15);
        hash = vox_audio_hash_mix(hash, voice->env_peak_q15);
        hash = vox_audio_hash_mix(hash, voice->env_sustain_q15);
        hash = vox_audio_hash_mix(hash, (vox_u32)(vox_u16)voice->pan_q15);
        hash = vox_audio_hash_mix(hash, voice->active);
        hash = vox_audio_hash_mix(hash, voice->waveform);
        hash = vox_audio_hash_mix(hash, voice->polynomial_bits);
        hash = vox_audio_hash_mix(hash, voice->priority);
        hash = vox_audio_hash_mix(hash, voice->slide_up);
        hash = vox_audio_hash_mix(hash, voice->env_stage);
        hash = vox_audio_hash_mix(hash, voice->env_direction);
        hash = vox_audio_hash_mix(hash, voice->bank);
    }
    for (index = 0U; index < VOX_AUDIO_NOTE_CAPACITY; ++index) {
        const vox_audio_note_slot *slot;

        slot = &engine->notes[index];
        hash = vox_audio_hash_mix(hash, slot->active);
        if (slot->active) {
            hash = vox_audio_hash_mix(hash, slot->serial);
            hash = vox_audio_hash_mix(hash, slot->note.delay_frames);
            hash = vox_audio_hash_mix(hash, slot->note.event_id);
            hash = vox_audio_hash_mix(hash, slot->note.frequency_hz);
            hash = vox_audio_hash_mix(hash, slot->note.duration_ms);
            hash = vox_audio_hash_mix(hash, slot->note.gain_q15);
            hash = vox_audio_hash_mix(hash,
                (vox_u32)(vox_u16)slot->note.pan_q15);
            hash = vox_audio_hash_mix(hash, slot->note.waveform);
            hash = vox_audio_hash_mix(hash, slot->note.bus);
            hash = vox_audio_hash_mix(hash, slot->note.priority);
        }
    }
    for (index = 0U; index < VOX_AUDIO_SPEECH_CAPACITY; ++index) {
        const vox_audio_phrase_slot *phrase;
        vox_u32 token;

        phrase = &engine->phrases[index];
        hash = vox_audio_hash_mix(hash, phrase->state);
        hash = vox_audio_hash_mix(hash, phrase->serial);
        hash = vox_audio_hash_mix(hash, phrase->event_id);
        hash = vox_audio_hash_mix(hash, phrase->allophone_count);
        hash = vox_audio_hash_mix(hash, phrase->gain_q15);
        hash = vox_audio_hash_mix(hash,
            (vox_u32)(vox_u16)phrase->pan_q15);
        hash = vox_audio_hash_mix(hash, phrase->profile);
        hash = vox_audio_hash_mix(hash, phrase->priority);
        for (token = 0U; token < phrase->allophone_count; ++token) {
            hash = vox_audio_hash_mix(hash, phrase->allophones[token]);
        }
    }
    for (index = 0U; index < VOX_AUDIO_AMBIENCE_COUNT; ++index) {
        const vox_audio_ambience_state *ambience;

        ambience = &engine->ambience[index];
        hash = vox_audio_hash_mix(hash, ambience->phase);
        hash = vox_audio_hash_mix(hash, ambience->phase_secondary);
        hash = vox_audio_hash_mix(hash, ambience->step);
        hash = vox_audio_hash_mix(hash, ambience->step_secondary);
        hash = vox_audio_hash_mix(hash, ambience->lfsr);
        hash = vox_audio_hash_mix(hash, ambience->gain_step);
        hash = vox_audio_hash_mix(hash, ambience->gain_remainder);
        hash = vox_audio_hash_mix(hash, ambience->gain_error);
        hash = vox_audio_hash_mix(hash, ambience->gain_denominator);
        hash = vox_audio_hash_mix(hash, ambience->gain_samples_left);
        hash = vox_audio_hash_mix(hash, ambience->gain_q15);
        hash = vox_audio_hash_mix(hash, ambience->target_gain_q15);
        hash = vox_audio_hash_mix(hash,
            (vox_u32)(vox_u16)ambience->pan_q15);
        hash = vox_audio_hash_mix(hash, ambience->gain_up);
        hash = vox_audio_hash_mix(hash,
            (vox_u32)ambience->filtered_sample);
    }
    return hash;
}

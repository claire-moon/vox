/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_audio.h"

#include <stdio.h>
#include <string.h>

#define TEST_RATE 48000U
#define TEST_FRAMES 4096U
#define TEST_REPLAY_PCM_HASH 0x58059244U

static vox_u32 hash_pcm(const vox_i16 *samples, vox_u32 count)
{
    vox_u32 hash;
    vox_u32 index;

    hash = 2166136261U;
    for (index = 0U; index < count; ++index) {
        vox_u16 value;

        value = (vox_u16)samples[index];
        hash ^= (vox_u32)(value & 0xffU);
        hash *= 16777619U;
        hash ^= (vox_u32)(value >> 8U);
        hash *= 16777619U;
    }
    return hash;
}

static int emit(vox_audio_engine *engine, vox_u16 preset,
                vox_u16 variant, vox_i16 pan, vox_u32 id)
{
    vox_audio_event event;

    vox_audio_event_init(&event, preset);
    event.variant = variant;
    event.pan_q15 = pan;
    event.event_id = id;
    return vox_audio_emit(engine, &event) == VOX_OK;
}

static int check_invalid_contract(void)
{
    vox_audio_engine engine;
    vox_audio_engine boundary;
    vox_audio_event event;
    vox_i16 sample[2];

    vox_audio_event_init(&event, VOX_AUDIO_PRESET_FIRE);
    if (vox_audio_init(0, TEST_RATE, 1U) != VOX_ERR_INVALID ||
        vox_audio_init(&engine, VOX_AUDIO_RATE_MIN - 1U, 1U) !=
            VOX_ERR_INVALID ||
        vox_audio_init(&engine, VOX_AUDIO_RATE_MAX + 1U, 1U) !=
            VOX_ERR_INVALID ||
        vox_audio_init(&engine, TEST_RATE, 1U) != VOX_OK ||
        vox_audio_emit(0, &event) != VOX_ERR_INVALID ||
        vox_audio_emit(&engine, 0) != VOX_ERR_INVALID ||
        vox_audio_render(&engine, 0, 1U) != VOX_ERR_INVALID ||
        vox_audio_render(&engine, 0, 0U) != VOX_OK) {
        fprintf(stderr, "audio invalid-input contract failed\n");
        return 0;
    }
    event.preset = VOX_AUDIO_PRESET_COUNT;
    if (vox_audio_emit(&engine, &event) != VOX_ERR_INVALID) {
        fprintf(stderr, "audio invalid-preset contract failed\n");
        return 0;
    }
    event.preset = VOX_AUDIO_PRESET_FIRE;
    event.gain_q15 = VOX_AUDIO_GAIN_MAX + 1U;
    if (vox_audio_emit(&engine, &event) != VOX_ERR_INVALID ||
        vox_audio_render(0, sample, 1U) != VOX_ERR_INVALID ||
        vox_audio_render(&engine, sample, 0x80000000U) !=
            VOX_ERR_INVALID) {
        fprintf(stderr, "audio range contract failed\n");
        return 0;
    }
    if (vox_audio_init(&boundary, VOX_AUDIO_RATE_MIN, 2U) != VOX_OK ||
        !emit(&boundary, VOX_AUDIO_PRESET_UI_MOVE, 0U, 0, 1U) ||
        vox_audio_render(&boundary, sample, 1U) != VOX_OK ||
        vox_audio_init(&boundary, VOX_AUDIO_RATE_MAX, 2U) != VOX_OK ||
        !emit(&boundary, VOX_AUDIO_PRESET_UI_MOVE, 0U, 0, 1U) ||
        vox_audio_render(&boundary, sample, 1U) != VOX_OK) {
        fprintf(stderr, "audio boundary sample-rate contract failed\n");
        return 0;
    }
    return 1;
}

static int check_zero_gain_is_silent(void)
{
    vox_audio_engine engine;
    vox_audio_event event;

    (void)vox_audio_init(&engine, TEST_RATE, 3U);
    vox_audio_event_init(&event, VOX_AUDIO_PRESET_EXPLOSION);
    event.gain_q15 = 0U;
    if (vox_audio_emit(&engine, &event) != VOX_OK ||
        vox_audio_active_voice_count(&engine) != 0U ||
        engine.event_serial != 1U) {
        fprintf(stderr, "zero-gain audio event consumed voices\n");
        return 0;
    }
    return 1;
}

static int populate_sequence(vox_audio_engine *engine)
{
    return emit(engine, VOX_AUDIO_PRESET_FIRE, 2U, -12000, 100U) &&
           emit(engine, VOX_AUDIO_PRESET_HIT, 7U, 9000, 101U) &&
           emit(engine, VOX_AUDIO_PRESET_EXPLOSION, 4U, 0, 102U);
}

static int check_replay_determinism(void)
{
    vox_audio_engine first;
    vox_audio_engine second;
    vox_i16 first_pcm[TEST_FRAMES * 2U];
    vox_i16 second_pcm[TEST_FRAMES * 2U];
    vox_u32 first_hash;
    vox_u32 second_hash;

    if (vox_audio_init(&first, TEST_RATE, 0x12345678U) != VOX_OK ||
        vox_audio_init(&second, TEST_RATE, 0x12345678U) != VOX_OK ||
        !populate_sequence(&first) || !populate_sequence(&second) ||
        vox_audio_render(&first, first_pcm, TEST_FRAMES) != VOX_OK ||
        vox_audio_render(&second, second_pcm, TEST_FRAMES) != VOX_OK) {
        fprintf(stderr, "audio deterministic setup failed\n");
        return 0;
    }
    first_hash = hash_pcm(first_pcm, TEST_FRAMES * 2U);
    second_hash = hash_pcm(second_pcm, TEST_FRAMES * 2U);
    if (first_hash != second_hash ||
        memcmp(first_pcm, second_pcm, sizeof(first_pcm)) != 0 ||
        vox_audio_state_hash(&first) != vox_audio_state_hash(&second)) {
        fprintf(stderr, "audio replay differed: %08lx != %08lx\n",
                (unsigned long)first_hash, (unsigned long)second_hash);
        return 0;
    }
    if (first_hash != TEST_REPLAY_PCM_HASH) {
        fprintf(stderr, "audio golden PCM changed: got %08lx\n",
                (unsigned long)first_hash);
        return 0;
    }
    return 1;
}

static int check_variant_changes_pcm(void)
{
    vox_audio_engine first;
    vox_audio_engine second;
    vox_i16 first_pcm[1024U * 2U];
    vox_i16 second_pcm[1024U * 2U];

    (void)vox_audio_init(&first, TEST_RATE, 0xfeed1234U);
    (void)vox_audio_init(&second, TEST_RATE, 0xfeed1234U);
    if (!emit(&first, VOX_AUDIO_PRESET_EXPLOSION, 1U, 0, 77U) ||
        !emit(&second, VOX_AUDIO_PRESET_EXPLOSION, 2U, 0, 77U)) {
        return 0;
    }
    (void)vox_audio_render(&first, first_pcm, 1024U);
    (void)vox_audio_render(&second, second_pcm, 1024U);
    if (memcmp(first_pcm, second_pcm, sizeof(first_pcm)) == 0) {
        fprintf(stderr, "audio explosion variants produced identical PCM\n");
        return 0;
    }
    return 1;
}

static int check_stereo_pan(void)
{
    vox_audio_engine engine;
    vox_audio_event event;
    vox_i16 pcm[512U * 2U];
    vox_u32 index;
    vox_u32 left_energy;
    vox_u32 right_energy;

    (void)vox_audio_init(&engine, TEST_RATE, 9U);
    vox_audio_event_init(&event, VOX_AUDIO_PRESET_UI_MOVE);
    event.pan_q15 = VOX_AUDIO_PAN_LEFT;
    if (vox_audio_emit(&engine, &event) != VOX_OK) {
        return 0;
    }
    (void)vox_audio_render(&engine, pcm, 512U);
    left_energy = 0U;
    right_energy = 0U;
    for (index = 0U; index < 512U; ++index) {
        vox_i32 left;
        vox_i32 right;

        left = pcm[index * 2U];
        right = pcm[index * 2U + 1U];
        left_energy += (vox_u32)(left < 0L ? -left : left);
        right_energy += (vox_u32)(right < 0L ? -right : right);
    }
    if (left_energy == 0U || right_energy * 8U >= left_energy) {
        fprintf(stderr, "audio pan did not favor left: %lu/%lu\n",
                (unsigned long)left_energy, (unsigned long)right_energy);
        return 0;
    }
    return 1;
}

static int check_voice_limit_and_stop(void)
{
    vox_audio_engine engine;
    vox_u32 index;
    vox_u32 high_priority_hash;

    (void)vox_audio_init(&engine, TEST_RATE, 22U);
    for (index = 0U; index < 12U; ++index) {
        if (!emit(&engine, VOX_AUDIO_PRESET_EXPLOSION,
                  (vox_u16)index, 0, index)) {
            return 0;
        }
        if (vox_audio_active_voice_count(&engine) >
            VOX_AUDIO_VOICE_COUNT) {
            fprintf(stderr, "audio exceeded eight voices\n");
            return 0;
        }
    }
    if (vox_audio_active_voice_count(&engine) != VOX_AUDIO_VOICE_COUNT) {
        fprintf(stderr, "audio voice allocator did not fill eight voices\n");
        return 0;
    }
    for (index = 0U; index < VOX_AUDIO_VOICE_COUNT; ++index) {
        vox_u8 expected_bank;

        expected_bank = index < 4U ? 0U : 1U;
        if (engine.voices[index].bank != expected_bank) {
            fprintf(stderr, "audio voice %lu was assigned to wrong bank\n",
                    (unsigned long)index);
            return 0;
        }
    }
    high_priority_hash = vox_audio_state_hash(&engine);
    if (!emit(&engine, VOX_AUDIO_PRESET_UI_MOVE, 0U, 0, 99U)) {
        return 0;
    }
    if (vox_audio_state_hash(&engine) == high_priority_hash) {
        /* The serial advances, but high-priority voices themselves stay live. */
        fprintf(stderr, "audio event serial did not advance\n");
        return 0;
    }
    for (index = 0U; index < VOX_AUDIO_VOICE_COUNT; ++index) {
        if (engine.voices[index].priority < 200U) {
            fprintf(stderr, "low-priority UI sound stole an explosion voice\n");
            return 0;
        }
    }
    vox_audio_stop_all(&engine);
    if (vox_audio_active_voice_count(&engine) != 0U) {
        fprintf(stderr, "audio stop-all left active voices\n");
        return 0;
    }
    return 1;
}

static int check_saturation(void)
{
    vox_audio_engine engine;
    vox_i16 pcm[2];
    vox_u32 index;

    (void)vox_audio_init(&engine, TEST_RATE, 55U);
    for (index = 0U; index < VOX_AUDIO_VOICE_COUNT; ++index) {
        vox_audio_voice *voice;

        voice = &engine.voices[index];
        voice->active = 1U;
        voice->waveform = VOX_AUDIO_WAVE_TONE;
        voice->phase = 0x80000000U;
        voice->env_level_q15 = VOX_AUDIO_GAIN_MAX;
        voice->env_stage = 3U;
        voice->env_samples_left = 2U;
        voice->env_hold_samples = 2U;
        voice->pan_q15 = VOX_AUDIO_PAN_CENTER;
    }
    if (vox_audio_render(&engine, pcm, 1U) != VOX_OK ||
        pcm[0] != 32767 || pcm[1] != 32767) {
        fprintf(stderr, "audio positive saturation failed: %d/%d\n",
                (int)pcm[0], (int)pcm[1]);
        return 0;
    }
    for (index = 0U; index < VOX_AUDIO_VOICE_COUNT; ++index) {
        engine.voices[index].phase = 0U;
    }
    if (vox_audio_render(&engine, pcm, 1U) != VOX_OK ||
        pcm[0] != -32768 || pcm[1] != -32768) {
        fprintf(stderr, "audio negative saturation failed: %d/%d\n",
                (int)pcm[0], (int)pcm[1]);
        return 0;
    }
    return 1;
}

static int check_effects_finish(void)
{
    vox_audio_engine engine;
    vox_i16 pcm[1024U * 2U];
    vox_u32 block;
    vox_u32 index;

    (void)vox_audio_init(&engine, TEST_RATE, 44U);
    if (!emit(&engine, VOX_AUDIO_PRESET_KILL, 8U, 0, 14U)) {
        return 0;
    }
    for (block = 0U; block < 96U; ++block) {
        (void)vox_audio_render(&engine, pcm, 1024U);
    }
    if (vox_audio_active_voice_count(&engine) != 0U) {
        fprintf(stderr, "audio effect failed to release all voices\n");
        return 0;
    }
    (void)vox_audio_render(&engine, pcm, 1024U);
    for (index = 0U; index < 1024U * 2U; ++index) {
        if (pcm[index] != 0) {
            fprintf(stderr, "audio inactive engine was not silent\n");
            return 0;
        }
    }
    return 1;
}

static int check_all_presets(void)
{
    vox_audio_engine engine;
    vox_u16 preset;

    (void)vox_audio_init(&engine, TEST_RATE, 101U);
    for (preset = VOX_AUDIO_PRESET_FIRE;
         preset < VOX_AUDIO_PRESET_COUNT; ++preset) {
        if (!emit(&engine, preset, preset, 0, preset)) {
            fprintf(stderr, "audio preset %u was not implemented\n",
                    (unsigned int)preset);
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    if (!check_invalid_contract() ||
        !check_zero_gain_is_silent() ||
        !check_replay_determinism() ||
        !check_variant_changes_pcm() ||
        !check_stereo_pan() ||
        !check_voice_limit_and_stop() ||
        !check_saturation() ||
        !check_effects_finish() ||
        !check_all_presets()) {
        return 1;
    }
    printf("VOX dual-bank 8-voice audio contract passed\n");
    return 0;
}

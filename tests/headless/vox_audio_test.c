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

static int check_weapon_palette(void)
{
    static const vox_u16 presets[] = {
        VOX_AUDIO_PRESET_PULASKI,
        VOX_AUDIO_PRESET_POPPER,
        VOX_AUDIO_PRESET_SMOKER,
        VOX_AUDIO_PRESET_HOT_RAIL,
        VOX_AUDIO_PRESET_HYDROSHOT,
        VOX_AUDIO_PRESET_GIANT_HAMMER,
        VOX_AUDIO_PRESET_BOLT_ACTION,
        VOX_AUDIO_PRESET_SCATTERBRAIN,
        VOX_AUDIO_PRESET_FIRECRACKER,
        VOX_AUDIO_PRESET_BORE_DRILL
    };
    vox_u32 hashes[sizeof(presets) / sizeof(presets[0])];
    vox_i16 pcm[1024U * 2U];
    vox_u32 preset_index;
    vox_u32 compare_index;

    for (preset_index = 0U;
         preset_index < (vox_u32)(sizeof(presets) / sizeof(presets[0]));
         ++preset_index) {
        vox_audio_engine engine;
        vox_u32 sample;
        vox_u32 energy;

        if (vox_audio_init(&engine, TEST_RATE, 0x4404U) != VOX_OK ||
            !emit(&engine, presets[preset_index], 9U, 0,
                  0x9000U + preset_index) ||
            vox_audio_render(&engine, pcm, 1024U) != VOX_OK) {
            fprintf(stderr, "weapon palette setup failed at %lu\n",
                    (unsigned long)preset_index);
            return 0;
        }
        energy = 0U;
        for (sample = 0U; sample < 1024U * 2U; ++sample) {
            vox_i32 value;

            value = pcm[sample];
            energy += (vox_u32)(value < 0L ? -value : value);
        }
        if (energy == 0U) {
            fprintf(stderr, "weapon palette preset %u was silent\n",
                    (unsigned int)presets[preset_index]);
            return 0;
        }
        hashes[preset_index] = hash_pcm(pcm, 1024U * 2U);
        for (compare_index = 0U; compare_index < preset_index;
             ++compare_index) {
            if (hashes[compare_index] == hashes[preset_index]) {
                fprintf(stderr,
                        "weapon palette presets %u and %u matched\n",
                        (unsigned int)presets[compare_index],
                        (unsigned int)presets[preset_index]);
                return 0;
            }
        }
    }
    return 1;
}

static int check_v3_contract(void)
{
    vox_audio_engine engine;
    vox_audio_config config;
    vox_audio_note note;
    vox_audio_speech speech;
    vox_u8 token;

    if (VOX_AUDIO_VERSION != 3U ||
        VOX_AUDIO_PRESET_FIRE != 1 ||
        VOX_AUDIO_PRESET_BARK_KILL != 14 ||
        VOX_AUDIO_PRESET_ZOOM_CLICK != 15 ||
        VOX_AUDIO_PRESET_PULASKI != 21 ||
        VOX_AUDIO_PRESET_BORE_DRILL != 30 ||
        VOX_AUDIO_PRESET_COUNT != 31 ||
        sizeof(engine) > VOX_AUDIO_ENGINE_BYTES_MAX) {
        fprintf(stderr, "audio v3 ABI constants changed\n");
        return 0;
    }
    vox_audio_config_init(&config, TEST_RATE, 0x901U);
    if (config.master_gain_q15 != VOX_AUDIO_GAIN_MAX ||
        config.master_ramp_ms == 0U ||
        vox_audio_init_ex(&engine, &config) != VOX_OK ||
        vox_audio_master_gain(&engine) != VOX_AUDIO_GAIN_MAX ||
        vox_audio_sample_clock(&engine) != 0U ||
        vox_audio_ms_to_frames(&engine, 10U) != 480U) {
        fprintf(stderr, "audio v2 initialization contract failed\n");
        return 0;
    }
    config.master_gain_q15 = VOX_AUDIO_GAIN_MAX + 1U;
    if (vox_audio_init_ex(&engine, &config) != VOX_ERR_INVALID ||
        vox_audio_init_ex(&engine, 0) != VOX_ERR_INVALID) {
        fprintf(stderr, "audio v2 invalid config was accepted\n");
        return 0;
    }
    vox_audio_note_init(&note, 440U, 40U);
    if (note.gain_q15 == 0U || note.bus != VOX_AUDIO_BUS_UI ||
        note.priority != VOX_AUDIO_PRIORITY_UI) {
        fprintf(stderr, "audio note defaults failed\n");
        return 0;
    }
    token = VOX_AUDIO_ALLOPHONE_A;
    vox_audio_speech_init(&speech, &token, 1U);
    if (speech.profile != VOX_AUDIO_SPEECH_HIGH ||
        speech.priority != VOX_AUDIO_PRIORITY_PLAYER_BARK ||
        VOX_AUDIO_PRIORITY_BOT_BARK >=
            VOX_AUDIO_PRIORITY_PLAYER_BARK ||
        VOX_AUDIO_PRIORITY_PLAYER_BARK >=
            VOX_AUDIO_PRIORITY_ANNOUNCER ||
        speech.gain_q15 == 0U) {
        fprintf(stderr, "audio speech defaults failed\n");
        return 0;
    }
    return 1;
}

static int check_note_scheduler(void)
{
    vox_audio_engine first;
    vox_audio_engine second;
    vox_audio_note note;
    vox_i16 first_pcm[96U * 2U];
    vox_i16 second_pcm[96U * 2U];
    vox_u32 index;

    (void)vox_audio_init(&first, TEST_RATE, 0x110U);
    vox_audio_note_init(&note, 523U, 35U);
    note.delay_frames = 3U;
    note.event_id = 77U;
    if (vox_audio_schedule_note(&first, &note) != VOX_OK ||
        !vox_audio_has_pending(&first) || first.note_count != 1U ||
        vox_audio_render(&first, first_pcm, 3U) != VOX_OK ||
        vox_audio_active_voice_count(&first) != 0U ||
        vox_audio_render(&first, first_pcm + 6U, 1U) != VOX_OK ||
        vox_audio_active_voice_count(&first) != 1U ||
        first.note_count != 0U || vox_audio_sample_clock(&first) != 4U) {
        fprintf(stderr, "audio delayed-note timing failed\n");
        return 0;
    }

    (void)vox_audio_init(&first, TEST_RATE, 0x220U);
    (void)vox_audio_init(&second, TEST_RATE, 0x220U);
    vox_audio_note_init(&note, 659U, 55U);
    note.delay_frames = 17U;
    note.event_id = 91U;
    if (vox_audio_schedule_note(&first, &note) != VOX_OK ||
        vox_audio_schedule_note(&second, &note) != VOX_OK ||
        vox_audio_render(&first, first_pcm, 96U) != VOX_OK ||
        vox_audio_render(&second, second_pcm, 31U) != VOX_OK ||
        vox_audio_render(&second, second_pcm + 62U, 65U) != VOX_OK ||
        memcmp(first_pcm, second_pcm, sizeof(first_pcm)) != 0 ||
        vox_audio_state_hash(&first) != vox_audio_state_hash(&second)) {
        fprintf(stderr, "audio note scheduling depended on buffer size\n");
        return 0;
    }

    /* Reusing a low-numbered hole must not overtake an older due note. */
    (void)vox_audio_init(&first, TEST_RATE, 0x221U);
    vox_audio_note_init(&note, 220U, 40U);
    note.delay_frames = 0U;
    note.event_id = 92U;
    if (vox_audio_schedule_note(&first, &note) != VOX_OK) return 0;
    note.frequency_hz = 440U;
    note.delay_frames = 1U;
    note.event_id = 93U;
    if (vox_audio_schedule_note(&first, &note) != VOX_OK ||
        vox_audio_render(&first, first_pcm, 1U) != VOX_OK) return 0;
    note.frequency_hz = 880U;
    note.delay_frames = 0U;
    note.event_id = 94U;
    if (vox_audio_schedule_note(&first, &note) != VOX_OK ||
        vox_audio_render(&first, first_pcm, 1U) != VOX_OK ||
        first.voices[1].step >= first.voices[2].step) {
        fprintf(stderr, "audio same-frame note FIFO order failed\n");
        return 0;
    }

    vox_audio_stop_all(&first);
    vox_audio_note_init(&note, 330U, 20U);
    note.delay_frames = 100U;
    for (index = 0U; index < VOX_AUDIO_NOTE_CAPACITY; ++index) {
        note.event_id = index;
        if (vox_audio_schedule_note(&first, &note) != VOX_OK) {
            fprintf(stderr, "audio note queue filled early\n");
            return 0;
        }
    }
    if (vox_audio_schedule_note(&first, &note) != VOX_ERR_CAPACITY ||
        first.note_count != VOX_AUDIO_NOTE_CAPACITY) {
        fprintf(stderr, "audio note capacity was not enforced\n");
        return 0;
    }
    vox_audio_stop(&first, VOX_AUDIO_STOP_NOTES);
    if (first.note_count != 0U || vox_audio_has_pending(&first)) {
        fprintf(stderr, "audio note stop semantics failed\n");
        return 0;
    }
    return 1;
}

static int check_master_volume(void)
{
    vox_audio_engine engine;
    vox_audio_config config;
    vox_i16 pcm[512U * 2U];
    vox_u32 ramp_frames;
    vox_u32 index;

    vox_audio_config_init(&config, TEST_RATE, 0x330U);
    config.master_ramp_ms = 10U;
    if (vox_audio_init_ex(&engine, &config) != VOX_OK ||
        !emit(&engine, VOX_AUDIO_PRESET_UI_MOVE, 0U, 0, 2U) ||
        vox_audio_set_master_gain(&engine, 0U) != VOX_OK) {
        return 0;
    }
    ramp_frames = vox_audio_ms_to_frames(&engine, config.master_ramp_ms);
    if (ramp_frames != 480U ||
        vox_audio_render(&engine, pcm, 1U) != VOX_OK ||
        engine.master_samples_left != ramp_frames - 1U ||
        vox_audio_set_master_gain(&engine, 0U) != VOX_OK ||
        engine.master_samples_left != ramp_frames - 1U ||
        vox_audio_render(&engine, pcm + 2U, ramp_frames) != VOX_OK ||
        pcm[0] == 0 || engine.master_gain_q15 != 0U ||
        pcm[ramp_frames * 2U] != 0 ||
        pcm[ramp_frames * 2U + 1U] != 0) {
        fprintf(stderr, "audio master gain did not ramp to silence\n");
        return 0;
    }
    if (vox_audio_set_master_gain(&engine,
                                  VOX_AUDIO_GAIN_MAX + 1U) !=
        VOX_ERR_INVALID) {
        fprintf(stderr, "audio invalid master gain was accepted\n");
        return 0;
    }

    vox_audio_config_init(&config, TEST_RATE, 0x331U);
    config.master_gain_q15 = 0U;
    config.master_ramp_ms = 0U;
    if (vox_audio_init_ex(&engine, &config) != VOX_OK ||
        !emit(&engine, VOX_AUDIO_PRESET_EXPLOSION, 0U, 0, 3U) ||
        vox_audio_render(&engine, pcm, 512U) != VOX_OK) {
        return 0;
    }
    for (index = 0U; index < 512U * 2U; ++index) {
        if (pcm[index] != 0) {
            fprintf(stderr, "zero master gain emitted PCM\n");
            return 0;
        }
    }
    return 1;
}

static int check_speech_queue(void)
{
    static const vox_u8 original_phrase[] = {
        VOX_AUDIO_ALLOPHONE_D, VOX_AUDIO_ALLOPHONE_IH,
        VOX_AUDIO_ALLOPHONE_G, VOX_AUDIO_ALLOPHONE_Z,
        VOX_AUDIO_ALLOPHONE_SILENCE
    };
    vox_u8 copied_phrase[sizeof(original_phrase)];
    vox_u8 invalid_phrase[1];
    vox_audio_engine first;
    vox_audio_engine second;
    vox_audio_speech speech;
    vox_i16 first_pcm[2048U * 2U];
    vox_i16 second_pcm[2048U * 2U];
    vox_u32 index;

    (void)memcpy(copied_phrase, original_phrase, sizeof(original_phrase));
    (void)vox_audio_init(&first, TEST_RATE, 0x440U);
    (void)vox_audio_init(&second, TEST_RATE, 0x440U);
    vox_audio_speech_init(&speech, copied_phrase,
                          (vox_u16)sizeof(copied_phrase));
    speech.event_id = 19U;
    if (vox_audio_speak(&first, &speech) != VOX_OK) {
        return 0;
    }
    copied_phrase[0] = VOX_AUDIO_ALLOPHONE_F;
    vox_audio_speech_init(&speech, original_phrase,
                          (vox_u16)sizeof(original_phrase));
    speech.event_id = 19U;
    if (vox_audio_speak(&second, &speech) != VOX_OK ||
        vox_audio_render(&first, first_pcm, 2048U) != VOX_OK ||
        vox_audio_render(&second, second_pcm, 701U) != VOX_OK ||
        vox_audio_render(&second, second_pcm + 1402U, 1347U) != VOX_OK ||
        memcmp(first_pcm, second_pcm, sizeof(first_pcm)) != 0 ||
        vox_audio_state_hash(&first) != vox_audio_state_hash(&second)) {
        fprintf(stderr, "audio speech tokens were not copied/deterministic\n");
        return 0;
    }

    vox_audio_stop_all(&first);
    vox_audio_speech_init(&speech, original_phrase,
                          (vox_u16)sizeof(original_phrase));
    speech.priority = VOX_AUDIO_PRIORITY_BARK;
    if (vox_audio_speak(&first, &speech) != VOX_OK) {
        return 0;
    }
    speech.profile = VOX_AUDIO_SPEECH_DEEP;
    speech.priority = VOX_AUDIO_PRIORITY_ANNOUNCER;
    speech.event_id = 20U;
    if (vox_audio_speak(&first, &speech) != VOX_OK ||
        first.active_phrase == 255U ||
        first.phrases[first.active_phrase].priority !=
            VOX_AUDIO_PRIORITY_ANNOUNCER) {
        fprintf(stderr, "audio announcer did not preempt bark\n");
        return 0;
    }
    speech.priority = VOX_AUDIO_PRIORITY_BARK;
    for (index = 0U; index < VOX_AUDIO_SPEECH_CAPACITY - 1U; ++index) {
        speech.event_id = 30U + index;
        if (vox_audio_speak(&first, &speech) != VOX_OK) {
            fprintf(stderr, "audio speech queue filled early\n");
            return 0;
        }
    }
    if (vox_audio_speak(&first, &speech) != VOX_ERR_CAPACITY) {
        fprintf(stderr, "audio speech capacity was not enforced\n");
        return 0;
    }
    invalid_phrase[0] = VOX_AUDIO_ALLOPHONE_COUNT;
    vox_audio_speech_init(&speech, invalid_phrase, 1U);
    if (vox_audio_speak(&second, &speech) != VOX_ERR_INVALID) {
        fprintf(stderr, "audio invalid allophone was accepted\n");
        return 0;
    }
    vox_audio_stop(&first, VOX_AUDIO_STOP_SPEECH);
    if (first.active_phrase != 255U || vox_audio_has_pending(&first)) {
        fprintf(stderr, "audio speech stop semantics failed\n");
        return 0;
    }
    return 1;
}

static int check_speech_rate_boundaries(void)
{
    static const vox_u8 phrase[] = {
        VOX_AUDIO_ALLOPHONE_G, VOX_AUDIO_ALLOPHONE_EH,
        VOX_AUDIO_ALLOPHONE_T, VOX_AUDIO_ALLOPHONE_SILENCE
    };
    vox_audio_engine engine;
    vox_audio_speech speech;
    vox_i16 pcm[512U * 2U];
    vox_u32 rates[2];
    vox_u32 rate_index;

    rates[0] = VOX_AUDIO_RATE_MIN;
    rates[1] = VOX_AUDIO_RATE_MAX;
    for (rate_index = 0U; rate_index < 2U; ++rate_index) {
        vox_u32 block;
        vox_u32 energy;

        (void)vox_audio_init(&engine, rates[rate_index], 0x550U);
        vox_audio_speech_init(&speech, phrase,
                              (vox_u16)sizeof(phrase));
        if (vox_audio_speak(&engine, &speech) != VOX_OK) {
            return 0;
        }
        energy = 0U;
        for (block = 0U; block < 512U &&
                         vox_audio_has_pending(&engine); ++block) {
            vox_u32 sample;

            (void)vox_audio_render(&engine, pcm, 512U);
            for (sample = 0U; sample < 512U * 2U; ++sample) {
                vox_i32 value;

                value = pcm[sample];
                energy += (vox_u32)(value < 0L ? -value : value);
            }
        }
        if (energy == 0U || vox_audio_has_pending(&engine)) {
            fprintf(stderr, "audio speech failed at rate %lu\n",
                    (unsigned long)rates[rate_index]);
            return 0;
        }
    }
    return 1;
}

static int check_ambience(void)
{
    vox_audio_engine engine;
    vox_i16 pcm[512U * 2U];
    vox_u32 block;
    vox_u32 sample;
    vox_u32 energy;
    vox_u32 remaining;

    (void)vox_audio_init(&engine, TEST_RATE, 0x660U);
    if (vox_audio_set_ambience(&engine, VOX_AUDIO_AMBIENCE_WIND,
                              9000U, VOX_AUDIO_PAN_LEFT) != VOX_OK ||
        vox_audio_set_ambience(&engine, VOX_AUDIO_AMBIENCE_WATER,
                              6000U, VOX_AUDIO_PAN_CENTER) != VOX_OK ||
        vox_audio_set_ambience(&engine, VOX_AUDIO_AMBIENCE_LAVA,
                              7000U, VOX_AUDIO_PAN_RIGHT) != VOX_OK ||
        !vox_audio_has_pending(&engine)) {
        return 0;
    }
    (void)vox_audio_render(&engine, pcm, 1U);
    remaining = engine.ambience[VOX_AUDIO_AMBIENCE_WIND].gain_samples_left;
    if (vox_audio_set_ambience(&engine, VOX_AUDIO_AMBIENCE_WIND,
                              9000U, VOX_AUDIO_PAN_RIGHT) != VOX_OK ||
        engine.ambience[VOX_AUDIO_AMBIENCE_WIND].gain_samples_left !=
            remaining ||
        engine.ambience[VOX_AUDIO_AMBIENCE_WIND].pan_q15 !=
            VOX_AUDIO_PAN_RIGHT) {
        fprintf(stderr, "audio ambience target assertion restarted ramp\n");
        return 0;
    }
    energy = 0U;
    for (block = 0U; block < 12U; ++block) {
        (void)vox_audio_render(&engine, pcm, 512U);
        for (sample = 0U; sample < 512U * 2U; ++sample) {
            vox_i32 value;

            value = pcm[sample];
            energy += (vox_u32)(value < 0L ? -value : value);
        }
    }
    if (energy == 0U ||
        vox_audio_set_ambience(&engine, VOX_AUDIO_AMBIENCE_WIND,
                              0U, VOX_AUDIO_PAN_CENTER) != VOX_OK ||
        vox_audio_set_ambience(&engine, VOX_AUDIO_AMBIENCE_WATER,
                              0U, VOX_AUDIO_PAN_CENTER) != VOX_OK ||
        vox_audio_set_ambience(&engine, VOX_AUDIO_AMBIENCE_LAVA,
                              0U, VOX_AUDIO_PAN_CENTER) != VOX_OK) {
        fprintf(stderr, "audio ambience emitted no PCM\n");
        return 0;
    }
    for (block = 0U; block < 12U; ++block) {
        (void)vox_audio_render(&engine, pcm, 512U);
    }
    if (vox_audio_has_pending(&engine) ||
        vox_audio_set_ambience(&engine, VOX_AUDIO_AMBIENCE_COUNT,
                              1U, 0) != VOX_ERR_INVALID) {
        fprintf(stderr, "audio ambience failed to ramp out\n");
        return 0;
    }
    return 1;
}

static int check_state_hash_contract(void)
{
    vox_audio_engine first;
    vox_audio_engine second;
    vox_audio_event event;
    vox_audio_note note;
    vox_audio_speech speech;
    vox_u8 token;

    token = VOX_AUDIO_ALLOPHONE_A;
    (void)vox_audio_init(&first, TEST_RATE, 0x770U);
    vox_audio_event_init(&event, VOX_AUDIO_PRESET_EXPLOSION);
    if (vox_audio_emit(&first, &event) != VOX_OK) return 0;
    second = first;
    second.voices[0].step_remainder ^= 1U;
    if (vox_audio_state_hash(&first) == vox_audio_state_hash(&second)) {
        fprintf(stderr, "audio hash omitted voice transition state\n");
        return 0;
    }
    second = first;
    second.ambience[0].gain_up ^= 1U;
    if (vox_audio_state_hash(&first) == vox_audio_state_hash(&second)) {
        fprintf(stderr, "audio hash omitted ambience transition state\n");
        return 0;
    }

    event.pan_q15 = (vox_i16)-32768L;
    vox_audio_note_init(&note, 440U, 20U);
    note.pan_q15 = (vox_i16)-32768L;
    vox_audio_speech_init(&speech, &token, 1U);
    speech.pan_q15 = (vox_i16)-32768L;
    if (vox_audio_emit(&first, &event) != VOX_ERR_INVALID ||
        vox_audio_schedule_note(&first, &note) != VOX_ERR_INVALID ||
        vox_audio_speak(&first, &speech) != VOX_ERR_INVALID ||
        vox_audio_set_ambience(&first, VOX_AUDIO_AMBIENCE_WIND,
                              1U, (vox_i16)-32768L) != VOX_ERR_INVALID) {
        fprintf(stderr, "audio accepted pan below its documented range\n");
        return 0;
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
        !check_all_presets() ||
        !check_weapon_palette() ||
        !check_v3_contract() ||
        !check_note_scheduler() ||
        !check_master_volume() ||
        !check_speech_queue() ||
        !check_speech_rate_boundaries() ||
        !check_ambience() ||
        !check_state_hash_contract()) {
        return 1;
    }
    printf("VOX Audio v3 deterministic synthesis contract passed\n");
    return 0;
}

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# VOX Audio v2 contract

VOX Audio is an allocation-free strict-C89 synthesizer.  It owns no operating
system device, thread, file, or callback: a host serializes event and render
calls and sends the returned interleaved signed-16-bit stereo frames to its
adapter.  Device failure therefore remains presentation-only and nonfatal.

The public `vox_audio_engine` is caller-owned and bounded to 8192 bytes.  Eight
voices cover immediate chip patches, scheduled notes, speech, and ambience.
Legacy preset IDs and `vox_audio_init` remain source-compatible; v2 adds an
extended initializer and appends new preset IDs without renumbering the v1
family. `VOX_AUDIO_VERSION` is a compile-time API version: because the public
state is caller-owned, consumers rebuild when its layout changes rather than
mixing audio object layouts across binary versions.

## Queues and ordering

- 32 note slots are ordered by due sample frame and insertion serial.
- Four speech phrases may own at most 192 copied allophone tokens in total.
- Forty stable allophones feed high-miner and deep-announcer profiles.
- Wind, water, and lava retain three virtual ambience states; the strongest
  bounded layers yield to higher-priority effects and speech.
- All queues use fixed arrays. Capacity or validation failure returns an error
  without partial mutation or delayed heap work.

`vox_audio_schedule_note` copies a note with a relative sample delay.
`vox_audio_speak` copies the supplied token stream, so caller storage may be
released immediately.  Announcer speech preempts lower-priority phrases;
manual miner speech takes priority over bot chatter. `vox_audio_has_pending`
includes voices, future notes, speech, and audible/fading ambience so a host
can stop feeding silence when the engine is idle.

## Master gain and mixing

`vox_audio_set_master_gain` accepts Q15 gain and ramps changes to avoid clicks.
Zero gain writes silence while sample clocks, envelopes, note due times,
speech, and ambience continue to advance. DIGS exposes this as its only user
volume setting. Internal patch priority and balance remain fixed so ports hear
the same authored mix for the same events.

## Speech boundary

The engine synthesizes supplied allophone IDs. DIGS combines authored phrases
with a small deterministic letter-pattern converter and spelled fallback for
unknown custom names. The result deliberately retains a period Speak & Spell
character rather than claiming natural speech. The implementation and phrase
data are original GPL-3.0-or-later work and contain no recorded samples or
imported speech tables.

The SDL adapter renders continuously from the audio-device callback. Main
thread event and configuration changes are serialized with the device lock;
presentation frame rate therefore cannot alter sample pitch, phrase duration,
menu cadence, or ambience time.

## Determinism and tests

Event IDs seed oscillator/noise variation. Rendering one frame at a time or in
irregular blocks must yield identical PCM and state to a single large render.
Tests cover legacy PCM compatibility, note due ordering and capacity, speech
copy/preemption/capacity, profile distinction, ambience, master gain, stop
semantics, saturation, and the supported 8000--192000 Hz range. Audio state is
deterministic for a fixed audio event stream but remains outside the
authoritative DIGS match hash.

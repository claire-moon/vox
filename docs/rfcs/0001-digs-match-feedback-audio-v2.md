<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# RFC 0001: DIGS match feedback and VOX Audio v2

- Status: Proposed
- Target: VOX + DIGS v0.0.2
- Author: Pinnacle Point Development
- Review requirement: one qualified reviewer other than the implementing
  maintainer

## Problem

The v0.0.2 demo has four connected gaps.  A screen-space lava footer disagrees
with the world camera, projectiles begin inside their owner, respawn and match
outcomes are partly hard-coded in the SDL host, and all interface feedback is
routed through a small immediate sound-preset API.  Player identity, hit and
kill confirmation, contextual dialogue, and deterministic match awards cannot
be added reliably without defining which state is authoritative.

## Constraints

- Simulation remains deterministic at 60 Hz in strict C89/C++98 code.
- Hot paths remain allocation-free and retain bounded caller-owned storage.
- Presentation rate, audio-device availability, player names, and cosmetic
  feedback must not change canonical match hashes.
- Existing public structs are caller-owned, so changing their layouts requires
  an explicit ABI break and source recompilation.
- Audio and dialogue must be original GPL-3.0-or-later implementation and data;
  no proprietary samples, copied character lines, or external runtime TTS.
- The scalar portable path is authoritative.  NASM remains an optional proven
  leaf rather than a requirement for gameplay correctness.

## Decision

1. Advance the public C/Rust ABI from 7 to 8.  Add configurable respawn rules,
   authoritative match results, bounded projectile owner clearance, and an
   explicit respawn request API.  Canonical additions participate in the
   deterministic hash.
2. Keep player names, kill-chain awards, notification queues, speech bubbles,
   and dialogue selection in a separate caller-owned DIGS feedback state.
   That state consumes existing match events after each simulation step and is
   excluded from the match hash and AI event ring.
3. Remove the screen-space lava substitute.  The renderer shows only the
   projected authoritative world horizon; offscreen lava has no fake footer.
4. Advance VOX Audio from API version 1 to 2.  Retain immediate presets and the
   legacy initializer while adding bounded sample-time notes, fixed allophone
   speech, persistent ambience state, master gain, and pending-work queries.
5. Use two fixed speech profiles and authored phoneme streams.  Runtime
   arbitrary text-to-speech and spoken player names are deferred.
6. Migrate SDL preferences from input-settings schema 1 to unified settings
   schema 2.  Migration preserves valid legacy input fields and supplies
   defaults independently for new settings.

## Deterministic order

Each tick applies submitted input, advances players/projectiles/materials,
records all damage and kills, updates lava, and then resolves score/time end
conditions once.  This lets simultaneous final-tick outcomes resolve as a
draw.  The SDL host drains presentation-relevant events into feedback state
after each completed match step; it never injects combo or dialogue events
back into the authoritative event ring.

Projectile clearance records the firing body envelope and lasts no more than
six ticks.  The projectile ignores its owner and terrain inside that envelope,
clears protection as soon as it exits, and otherwise uses normal collision and
self-damage rules.

Audio scheduling is ordered by due sample frame and insertion serial.  A due
event that cannot obtain a voice at its priority is dropped rather than
delayed.  Speech priority is announcer, local player, then bot.  All queues and
token stores have compile-time capacities.

## Compatibility and migration

- ABI 7 callers must rebuild against ABI 8; runtime validators reject mixed
  layouts.  The Rust host constant advances in the same change.
- `vox_audio_init` remains source-compatible and initializes v2 at full gain.
  New hosts use the extended configuration. `VOX_AUDIO_VERSION` is a
  compile-time API guard; caller-owned audio state requires source
  recompilation and is not binary-compatible across layout changes.
- Existing preset numeric values remain stable; new presets append to the
  enumeration.
- Settings schema 1 preserves P1/P2 input tuning on first v2 load.  Unknown
  future schemas are read without automatic overwrite.
- Map generator version 4 and Lua catalog schema 1 remain unchanged.

## Rejected alternatives

- Keeping a screen-space lava footer was rejected because it fabricates a
  hazard at a camera-dependent location.
- Disabling all self-damage was rejected because it removes close-range risk
  and rocket-jump-style systemic play.
- Adding combo and bark events to the match event ring was rejected because
  ring pressure can change AI hearing and event visibility.
- Recorded samples and external TTS were rejected because they increase
  licensing, storage, platform, and old-toolchain requirements.
- Per-bus user volume sliders were rejected for this iteration; only one
  master control is exposed while internal balances stay deterministic.

## Acceptance evidence

- Strict C89/C++98 build, CTests, Rust tests, scalar/NASM parity, smoke, input,
  camera, and headless proofs pass from a clean checkout.
- Match hashes repeat across time limits, respawn modes, score-cap outcomes,
  projectile-clearance cases, and render frame caps.
- Audio PCM/state tests cover 8 kHz and 192 kHz, queue boundaries, speech
  preemption, ambience selection, and zero master volume.
- Manual Linux testing covers keyboard, mouse, Logitech F310 X/D modes, two
  local players, all frame caps, audio-device failure, and the GTX 1660 Ti
  laptop while retaining the CPU-renderer limitation in the compatibility
  record.

## Rollback

Before release, the feature branch can revert the ABI/audio commits and retain
the prior v0.0.2 data without migration loss.  After an ABI 8 release, fixes
must preserve ABI 8 layouts or advance the ABI again; an ABI 7 binary must not
be relabeled as ABI 8.  Settings migration always retains the last readable
file and writes through a temporary replacement, so rollback can ignore
unknown v2 keys safely.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# RFC 0003: DIGS bot memory and conversation

- Status: Accepted for implementation
- Target: VOX + DIGS v0.0.4
- Native ABI: 10
- Settings schema: 5
- Chronicle format: 1

## Problem

Through v0.0.3 the three miners were opponents with aim. They spoke, but the
lines were assembled at runtime from fragments, which is the documented source
of the nonsense the previous playtest reported, and nothing they said or did
outlived the match. A player could spend fifty matches with RIVET and RIVET
would meet them for the first time in every one.

v0.0.4 makes them characters: they remember the player between sessions, hold
conversations with each other and with the player, and carry the results
forward. The release is named for it.

Three things about the existing design make this harder than it sounds, and
they are the constraints this RFC is written against:

1. **The simulation is authoritative and deterministic.** A fixed 60 Hz step,
   integer-only, no allocation, no wall clock, one noise function. Anything
   that remembers is by definition state, and state that reaches the
   simulation reaches the canonical hash.
2. **ISO C90, and a 1.44 MB ceiling on the playable payload.** A conversation
   system carries a corpus, and a corpus is bytes.
3. **The relationship has no UI.** This was decided during the phase and is
   recorded here because it raises the bar on everything else: no nameplate
   tint, no standings screen, no addressing markers. What a miner thinks of
   you is conveyed by what it says and nothing else.

## Decision

### The saved layer sits outside the match hash

Memory, the inbox, and the log live in a versioned save layer owned by the
port, not the simulation. `vox_digs_bot_memory` is handed to the match at
init through `vox_digs_match_init_ex` and read back with
`vox_digs_match_export_memory`; the simulation treats it as an opening
condition, exactly like a seed.

Two matches started from the same seed and the same memory snapshot produce
the same canonical hash on any host, and `--load-self-test 600` prints the
same hash on a pref path with a long history as on a fresh one. VOX-QA-094
tests precisely this.

This boundary is easy to breach by accident and was breached once during
implementation: the memory digest folded `launch_counter` and `elapsed_coarse`,
both of which the port fills from `time(0)`, into a hash that is itself folded
into `vox_digs_hash`. Two matches one launch apart then had identical world
hashes, scores and deaths, and different canonical hashes from tick zero. No
gate caught it, because every gate initialises from a canonical snapshot in
which those counters are zero; only the shipping demo carries a clock in. The
two fields are now excluded, with the reason recorded at the exclusion site,
and `test_the_clock_stays_out_of_the_hash` fails if either returns.

The general rule, which is the part worth keeping: **a wall-clock value may be
read by the port and may be written to the chronicle, but must never reach a
digest that the canonical hash consumes.**

### Relationships are pairwise, and shift slowly

Six unordered pairs across four slots, each holding a valence in ±1000 and one
of eight tones from FEUD to BONDED. Twenty-seven stimuli move valence.
Severity is expressed by *which* stimulus fires, never by scaling a single
one, so the mapping from event to feeling stays inspectable.

Tone changes are hysteretic: a dwell of 180 ticks plus a margin of 40 either
side of each boundary. Without it a pair sitting on a threshold flickers
between tones and the miners read as unstable rather than as changing their
minds. Valence decays toward zero, so a grudge fades if it is not fed.

### Speech is priced, addressed, and floored

A line is chosen from a written corpus, never assembled. `digs_lines_pool`
resolves (voice, tone, stimulus) through exact → voice → generic, so an
unwritten combination is a hole in the writing that shows up as silence rather
than as a blank bubble or a mad-lib.

- **Audience** is SELF, ONE or ALL. Aim decides who ONE is, which makes the
  single bark button a way to choose who you needle.
- **Exchanges** have heat, moved by the valence sign of each line, so a row
  escalates or cools and closes on a line that fits how it ended. With no
  relationship UI, this is the main thing telling a player where they stand.
- **Two floors** separate lines: 42 ticks inside an exchange, 480 between
  them. One bubble is on screen at a time, and matches have quiet stretches.
- **Replies are priced against the line they answer.** `digs_lines_length`
  exists so the simulation can do this without pulling `string.h` into a
  translation unit that is deliberately free of it. The delay is a fraction of
  that say-time set by the speaker's `patience`, plus personality-scaled
  jitter: RIVET nearly always lets you finish, CINDER often does not, FLAMEY
  is inconsistent. Interruption stays available to the player as a way to talk
  over somebody; what was removed is that a bot *always* did it.

The delay must be clamped to the exchange it belongs to. A patient archetype
could otherwise queue a reply past the life of its exchange, at which point
the between-exchange floor held it and the line was never spoken — the reply
was orphaned. This is fixed and tested; it is called out because the failure
is silent, and reads as "the bots are a bit quiet" rather than as a defect.

### The player only speaks when asked to

Every player line comes from a press of the bark button. This was briefly
automatic during the phase and was wrong in play: the player character
narrating itself is noise, and it takes away the one thing the player controls.

Playing alone, the player's own last line becomes the context for the next
press, so repeated presses build a train of thought rather than restarting
from unrelated openers. Miners are always listening, including to themselves.

## Compatibility and migration

**Native ABI 9 → 10.** `vox_digs_match` gains contract, speech and memory
state; `vox_game.h` gains `vox_digs_tone`, `vox_digs_stimulus`,
`vox_digs_audience`, `vox_digs_contract`, `vox_digs_bot_memory`,
`VOX_DIGS_ACTION_BARK` and `vox_digs_speech_duration`. Every struct carrying
`abi_version` continues to reject a mismatch, so a v0.0.3 consumer fails
loudly rather than reading a shifted layout. There is no in-process
compatibility shim: the ABI is not stable across releases and this is stated
in the compatibility matrix.

**Settings schema 3 → 5.** Schemas 1 through 5 are all readable and migrate
with safe defaults. The accepted-version list must name every schema that can
still be read; during implementation a bump left the outgoing number off that
list, which would have silently discarded every existing player's settings on
upgrade. That is a one-line mistake with no visible symptom until it ships,
and the list is now commented to say so.

**Chronicle format 1** is new. It is a text format with an FNV-1a checksum,
written atomically (temporary file, rename, previous kept aside) so a failed
replacement restores rather than destroys. A file failing its checksum is
repaired from the last good copy and raises a TAMPERED flag, which the miners
are told about in fiction rather than the player being shown a file error.
An absent chronicle is a first launch, not an error.

**The canonical hash moves.** v0.0.4 reports different 600-tick counters and a
different state hash from v0.0.3, principally because bots now dig. The
release evidence log is the reference; a v0.0.3 expectation applied to a
v0.0.4 binary will look like a failure and is not one.

## Rejected alternatives

**Memory inside the simulation.** Simplest to write, and it would have made
the canonical hash depend on the player's save file — replays unreproducible
across machines and desync detection meaningless. Rejected on determinism
grounds; the boundary above exists because of it.

**A runtime line generator.** Cheaper in bytes than a written corpus, and it
is what v0.0.3 did. It is the documented cause of lines that read as nonsense.
The quality filter is now the index rather than a runtime check.

**A relationship UI.** A nameplate tint or a standings page would make
relationships legible immediately and cheaply. Declined deliberately: being
told a number is not the same as being disliked. This is the decision most
likely to need revisiting — if relationships read as invisible in play, the
nameplate tint is the cheapest remedy and should be reconsidered before
anything more elaborate.

**No reset at all.** The design intent is that the history between the player
and these three is theirs and unrepeatable; a reset makes it a save file.
The released build nevertheless has a two-press confirmed reset in Options, at
the lead's explicit request, made after this trade-off was put to them. It is
recorded here as a deliberate exception rather than an oversight, so that a
future reader does not "fix" it back.

## Acceptance evidence

- `./dev.sh check`: 22 deterministic tests, the size budget, the work-counter
  benchmark, and optimisation invariance.
- `--load-self-test 600` reproduces the canonical counters and hash, and does
  so identically on histories of any length (VOX-QA-094).
- `--chronicle-self-test` round-trips a chronicle and refuses a damaged one.
- `--menu-self-test` walks every screen and fails on anything drawn outside
  its frame.
- Targeted determinism tests: `test_the_clock_stays_out_of_the_hash`,
  `test_init_leaves_nothing_uninitialised`, `test_patience_decides_who_interrupts`,
  `test_alone_you_talk_to_yourself`, `test_overhearing_takes_sides`,
  `test_talk_arrives_in_exchanges`.
- Human lane: `qa/V0.0.4-QUICK-FEEDBACK.txt` sections 2, 5 and 8, which are
  the parts that cannot be automated because they are judged by ear.

## Rollback

The conversation and memory systems are additive and sit behind the save
layer. Reverting the branch restores v0.0.3 behaviour; an existing chronicle
is then simply unread, since a v0.0.3 binary never looks for one. No player
data is destroyed by rolling back, and none is migrated on the way forward
beyond the settings schema.

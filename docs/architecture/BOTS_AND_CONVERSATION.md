<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Bots, relationships, and conversation

How the three miners decide what to feel and what to say. This describes the
mechanism as built; `docs/rfcs/0003-digs-bot-memory-and-conversation.md` records
why it was built that way and which alternatives were rejected.

Everything here is inside the authoritative simulation: integer-only, no
allocation, no wall clock, and the only randomness is
`digs_noise(seed, tick, player, salt)`. The saved layer that carries it between
sessions is owned by the port and is described at the end.

## Identity is an ordinal, not a slot

A bot's identity is its ordinal among the bots, so the first bot in a match is
always RIVET whichever player slot it occupies. `bot_mask` is already folded
into the canonical hash, so this needs no extra authoritative state.

| Archetype | Aggression | Patience | Caution | Grudge | Sociability |
|---|---:|---:|---:|---:|---:|
| RIVET | 90 | 210 | 190 | 220 | 70 |
| CINDER | 245 | 40 | 45 | 120 | 200 |
| FLAMEY | 150 | 110 | 120 | 80 | 235 |
| the player's miner | 128 | 128 | 128 | 128 | 128 |

The player's miner has no archetype, so `vox_digs_personality_get` returns null
for it. Speech pacing needs a temperament for *every* speaker, which is why the
middling row exists — omitting it caused three segfaults during development.

Traits drift by a bounded amount per match, per archetype
(`digs_drift_rate` = 3, 9, 6), clamped to 40..235. RIVET revises slowly and
remembers; CINDER swings and forgets. The clamp matters: a bot that drifts far
enough stops being the character the player learned, which is the opposite of
the point.

## Relationships are pairwise

Four slots make six unordered pairs (`VOX_DIGS_MAX_PAIRS`). Each holds a
signed valence in ±1000 and one of eight tones:

```
FEUD  HOSTILE  NEEDLING  NEUTRAL  WARY  THAWING  TRUCE  BONDED
  0      1         2        3      4      5        6      7
```

Twenty-seven stimuli (`VOX_DIGS_STIMULUS_COUNT`) move valence. **Severity is
expressed by which stimulus fires, never by scaling one**, so the mapping from
event to feeling stays inspectable rather than becoming a tuning curve.

Two mechanisms keep tone from being noisy:

- **Hysteresis.** A tone change needs `DIGS_CONTRACT_TONE_DWELL_TICKS` (180) of
  dwell plus a `DIGS_CONTRACT_TONE_MARGIN` (40) bias either side of each
  boundary. Without it a pair sitting on a threshold flickers and the miners
  read as unstable rather than as changing their minds.
- **Decay.** Valence moves `DIGS_CONTRACT_DECAY_STEP` (4) toward zero every
  `DIGS_CONTRACT_DECAY_TICKS` (300), so a grudge fades if it is not fed.

## Choosing a line

`digs_lines_pool(voice, tone, stimulus)` resolves through a fallback chain:
the exact cell, then that voice's take on the stimulus, then whatever anyone
would say about it. **The quality filter is the index, not a runtime check** —
if nobody wrote a line for a combination, the result is silence, which reads as
a quiet miner rather than as a blank bubble or an assembled mad-lib.

The corpus is 1,798 written lines. Nothing is assembled at runtime; the only
substitution is `%T` for whoever is being talked about, and
`digs_lines_addresses` lets a miner talking to itself avoid lines that name
somebody.

Line ids pack `set * DIGS_LINE_SET_STRIDE + index` into a `vox_u16`.
`digs_lines_stride_is_sound` guards the packing — an earlier stride of 256
overflowed past set 255 and silently aliased set 282 onto set 26.

## Pacing

| Constant | Value | Meaning |
|---|---:|---|
| `DIGS_SPEECH_FLOOR_IN_EXCHANGE` | 42 | Minimum ticks between lines inside a row |
| `DIGS_SPEECH_FLOOR_BETWEEN` | 480 | Minimum ticks between separate rows |
| `DIGS_SPEECH_EXCHANGE_MAX` | 4 | Lines before an exchange must close |

One bubble is on screen at a time and matches have quiet stretches. An
exchange carries **heat**, moved by the valence sign of each line, so a row
escalates or cools and closes on a line that fits how it ended. With no
relationship UI, that arc is the main thing telling a player where they stand.

**Audience** is SELF, ONE, or ALL. Aim decides who ONE is, which is what makes
one bark button a way to choose who you needle.

**Replies are priced against the line they answer.** `digs_lines_length` exists
so the simulation can do this without pulling `string.h` into a translation
unit that is deliberately free of it. The delay is a fraction of that say-time
scaled by the speaker's `patience`, plus personality-scaled jitter — so RIVET
nearly always lets you finish and CINDER often does not.

The delay is clamped to the exchange it belongs to. Without the clamp a patient
archetype queues a reply past the life of its exchange, the between-exchange
floor then holds it, and the line is never spoken. That failure is silent and
reads as "the bots are a bit quiet" rather than as a defect.

## The player's voice

Every player line comes from a press of the bark button; the player character
never speaks unprompted. Playing alone, the player's own last line becomes the
context for the next press, so repeated presses build a train of thought.
Miners are always listening — including to themselves.

## What crosses between sessions

`vox_digs_bot_memory` holds regard per pair and a record per identity (traits,
matches played, wins, kills, deaths). It enters through
`vox_digs_match_init_ex` and leaves through `vox_digs_match_export_memory`.
The simulation treats it as an opening condition, exactly like a seed: it never
opens a file and never reads a clock.

A foreign or mismatched snapshot is discarded rather than reinterpreted. A
snapshot read the wrong way round would produce plausible traits and a wrong
match, which is worse than no memory at all.

Carried memory *does* reach the canonical hash — a match opened from a played
chronicle does not simulate identically to a first meeting, which is the entire
point. What must never reach it is a wall-clock value. `launch_counter` and
`elapsed_coarse` are filled from `time(0)` by the port and are deliberately
excluded from `vox_digs_memory_hash`; including them once put `time(0)` into
the authoritative hash and made two identical matches look like a desync.
`test_the_clock_stays_out_of_the_hash` fails if either returns.

The file format, atomic write, checksum repair, and inbox live in the port —
see `ports/sdl2/digs_chronicle.h`.

## Tests

| Test | What it holds down |
|---|---|
| `digs_memory_across_sessions` | Memory survives a real process boundary and changes the match |
| `digs_chronicle_persistence` | The save layer round-trips and refuses a damaged file |
| `test_the_clock_stays_out_of_the_hash` | No wall clock in the canonical digest |
| `test_patience_decides_who_interrupts` | Interruption is a trait, not a rule |
| `test_alone_you_talk_to_yourself` | Repeated barks chain instead of restarting |
| `test_overhearing_takes_sides` | Miners react to conversations they are not in |
| `test_talk_arrives_in_exchanges` | Lines cluster into rows rather than trickling |
| `test_init_leaves_nothing_uninitialised` | `vox_digs_match_init` has no `memset` and needs none |

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# RFC 0004: DIGS completion candidate

Status: implemented for review on `feat/digs-completion`; not accepted for a
public release. This extends the locked v0.0.5 scope without changing the
four-slot roster, settings schema 6, chronicle format 1, or memory version 1.

## Problem and constraints

The prior candidate's local support heuristic can treat a floating column as
an anchor. Fluid exhaustion can prevent an otherwise valid downward move.
Drowning can damage a miner standing in shallow water, and death presentation
does not consistently use the actual damage class. The host's destruction
qualification also prints zero tick debt without measuring it.

Complete these mechanisms with integer-only fixed work, explicit initialization,
stable pool ordering, and no allocation or wall clock in authoritative state.
Retain strict C89, the scalar oracle, a 60 Hz step, the 1,474,560-byte playable
payload ceiling, and the authored personalities and persistent identities.

## Design

Connected terrain support uses a resumable component audit with a world-sized
visited bitmap and a bounded queue. Work continues over fixed ticks, including
components larger than a debris fragment. Contacts must reach a genuine anchor;
ordinary terrain touching another floating voxel is insufficient. Physical
extraction and rigid debris stay bounded and material accounting remains
explicit. Audits must handle terrain edits during traversal without trusting a
stale positive support result.

Fluid moves commit volume only after a destination is available. At capacity,
a wholly vacated source can be moved in place; a failed deposit cannot consume
an active slot or delete liquid. Head immersion determines drowning. Bots use
the same hazard geometry as miners rather than treating every foot-level pool
as lethal or ignoring persistent fluid.

Fatal entry and damage class drive severing, blast disconnection, and heat
melting. Procedural death/grunt sounds and phonetic articulation use the
existing audio engine. Articulated miner segments, distinct held-tool profiles,
muzzle positions, air-only background layers, and camera choreography are
derived by presentation and do not enter canonical state or saved memory.

The stress probe times input/tool submission as well as match stepping. A
virtual 60 Hz service queue accumulates measured excess service time and spends
later spare budget clearing it. Record p99, maximum service time, final and
maximum service debt, and late ticks. This qualifies simulation service on the
named machine; it does not measure interactive frame pacing, display, audio,
controllers, or a human soak.

Extracted-package checks run before freezing the final binary SBOM and archive.
Their stdout, stderr, and exit statuses must be inside the delivered archive.
The source archive includes the exact source used for the executable.

## Compatibility and migration

ABI 11 remains the identifier of this untagged source candidate. Expanded
match-owned support state changes in-memory structure sizes: rebuild every
consumer together. No compatibility with an older ABI-11 binary is promised.
No match snapshots are persisted as raw structs. Settings and chronicle readers
retain their existing readable versions; history is not reset by upgrading.
Canonical hashes and deterministic work baselines change intentionally with
material and damage behavior and must be recorded with these changes after
independent optimization-level agreement.

## Alternatives and limits

Particle-chain ragdolls, removing persistent fluids, declaring fragment-local
support to be whole-component support, and weakening the size/determinism gates
would reduce the locked scope and are rejected. Background rendering must never
hide material silhouettes. The exact commercial DOOM palette still needs an
approved provenance decision; repository-authored colors cannot be labeled as
that asset. Automated observations cannot close human acceptance checkpoints.

## Acceptance and rollback

Run the strict native tests, focused material/death/presentation regressions,
scalar/NASM parity, compiler optimization invariance, 600-tick load, cross-process
memory, 18,000-tick destruction counters, payload gate, named-machine timing,
and the extracted-package/source/SBOM checks. Retain actual outputs for this
candidate. Record every missing physical, hosted, and human lane as unrun.

Review this branch before normal protected-main integration. If rejected,
revert the completion commit and its benchmark changes together; existing
settings and chronicle files need no migration or destructive rollback.

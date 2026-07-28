<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# RFC 0002: DIGS stability and systemic-control pass

- Status: Accepted for implementation
- Target: VOX + DIGS v0.0.3
- Native ABI: 9
- Settings schema: 3

## Decision

v0.0.3 makes the fixed 60 Hz simulation authoritative independently of
presentation and audio. A selected presentation cap is a measured contract:
unsupported caps are identified by the local qualification path rather than
silently slowing simulation. Laptop Mode may reduce or cache presentation
work, but never changes match rules, the FX budget, bots, material behavior, or
the canonical hash.

The update is stability-first:

1. Embedded miners receive a bounded deterministic local depenetration. A
   failed recovery becomes one ordinary crush/environment death; it never
   invokes the spawn search or teleports a living miner.
2. Projectiles, rope hooks, and deposited effects use swept fixed-point
   contact. Combat resolves against visible anatomy hurtboxes. Spawn shields
   emit distinct block feedback rather than a normal zero-damage hit.
3. TTL removes an airborne effect without creating terrain. Debris can deposit
   only at a physical impact, remains granular/loose where declared, and may
   not materialize inside a living body.
4. Rope states are Idle, Casting, and Attached. The hook is swept, terrain is
   allowed to block it, and the attached segmented cable wraps and unwraps in
   bounded deterministic fashion. A hook strike against an unshielded enemy
   sets aggregate health to exactly one, applies bounded knockback, and ends
   the cast; it does not sever anatomy or kill. Cable contact is non-damaging.
5. The steampack supplies gentle lift-and-glide control below the physics
   solver's speed limit. Its small flame and exhaust are globally lit visual
   voxels and have no gameplay collision, heat, deposit, or damage.
6. The charged Mining Rail is weapon 11 without renumbering the original ten.
   Its deterministic ray loses energy through soft terrain and each aligned
   body, stops on sufficient stone or bedrock, uses exact anatomy, and is
   precision-lethal at full charge. It belongs to Full Works only.

## Host behavior

Settings schema 3 adds persistent 15 FPS, Laptop Mode, per-player Hold/Toggle
rope policy, Dummy Mode, and Off/Low/Normal/Heavy haptics. Schema 1 and 2 files
migrate with safe defaults; future schemas remain read-only.

The SDL host renders audio from the device clock, not the render loop. Standard
SDL low/high-motor envelopes provide local, distance-weighted feedback for
shots, rope, hits, explosions, kills, and collapse, with graceful fallback on
controllers that expose less capability. Nintendo, Xbox, PlayStation, and
generic controller prompts show the detected physical labels and current
binding. Switch Pro acceptance covers USB and Bluetooth.

Mouse-wheel zoom remains available. On a controller, holding ZL/left trigger
and moving the right stick vertically changes the shared camera zoom. The HUD
stays on the 320 x 200 pixel grid; gameplay is sampled from the native visible
voxel/lightfield scene so close zoom reveals cells discarded by the old
full-world 320 x 200 prepass. One-player matches process only that camera's
visible region; two local players use the union of the shared camera.

Miner speech uses curated PG-13 lines plus deterministic grammar pools keyed
by miner, seed, state, rival, weapon, material, and hazard. Human barks have a
three-second cooldown; Dummy Mode removes it only for local humans. Bots react
sparingly under a global spacing rule. Unknown custom names use deterministic
letter-to-sound or spelled fallback, preserving the original Speak & Spell
character without sampled voices or an external TTS runtime.

## Compatibility and verification

ABI 8 hosts must rebuild for ABI 9. Replays/hashes from v0.0.2 are not relabeled
as v0.0.3. The scalar C89 implementation remains the correctness oracle;
C++98, Rust, NASM, SDL, and future adapters must validate their boundaries
against it.

Acceptance requires deterministic overlap, tunnelling, anatomy, rope,
deposition, rail, steampack, audio-cadence, settings-migration, camera-detail,
controller-label, and haptic tests. Every host runs the 600-tick deterministic
four-miner load regression and must reproduce its canonical activity plus state
hash without applying wall-clock limits. The separate named-bench
`--performance-self-test 600` qualification must keep the 60 Hz simulation
below 5 ms average, 8 ms p95, and 16.67 ms maximum on the GTX 1660 Ti laptop's
i7-10750H CPU while it is in power-saver mode. Shared CI runner timing is not
substituted for this hardware qualification. Manual device behavior is not
promoted beyond BUILDS until the USB/Bluetooth and audible checks are recorded
by a tester.

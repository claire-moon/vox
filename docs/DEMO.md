<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# DIGS v0.0.4 demo guide

DIGS is a deterministic voxel-destruction deathmatch. The same bounded voxel
world drives terrain destruction, granular motion, liquids, gases, heat,
hazards, lighting, projectile impacts, and transient effects, and every one of
those feeds the authoritative match rather than being presentation-only.

v0.0.4 is "THE BOTS REMEMBER". The three miners you fight — RIVET, CINDER and
FLAMEY — carry attitudes between sessions, hold conversations with each other
and with you, and keep grudges. That is the release's headline change and the
part of it least visible in a single match.

This guide describes what the current build actually does. It replaces a
v0.0.1 document that had gone comprehensively out of date: different menus,
ten weapons instead of eleven, and a claim that the bots were "target-selection
demonstrators, not navigation" agents, which stopped being true when they
learned to cut their way out of terrain.

The checkout still identifies itself as v0.0.4 and has not been tagged or
published as v0.0.5. It also contains an untagged v0.0.5 physics/gameplay
increment. The status below separates those verified source changes from the
human release work and presentation features that remain open.

## Untagged v0.0.5 increment

The strict-C90 build now includes bounded, integer-authoritative pools for
persistent water/lava/blood volumes, oriented rigid bodies and joints, corpse
assemblies, blast debris, structural cluster extraction, always-toggle grapple
targeting, forward dash and invulnerability, headshots, kill healing, awards,
the moving dropship, and a render-only best-kill replay ledger. Fluid flow uses
gravity-increasing y coordinates and exact 3-D terrain blocking; fixture blasts
emit metal scrap, and submerged miners receive hashed drowning damage. The new
headless gates cover conservation, deterministic body order, cluster debris,
fixture and drowning events, dropship route transitions, and replay selection.
The current 600-tick SDL load gate begins the interactive dropship sequence and
records fired 20, explosions 16, crushes 0, effects 876, awake 3882, and
canonical hash `8d98c1d0`; the same result was
recaptured in strict `-O0` and `-O3` builds.

This is engineering evidence, not a v0.0.5 release claim. The current source
now carries wide unsupported roofs through bounded support-frontier fragments
and settles compact debris back as loose material with explicit accounting for
the bounded remainder. It still needs a whole-world support/load model beyond
those bounded fragments, large-world fluid-capacity qualification, and manual
acceptance of the fluid, cave-in,
grapple, bot, replay, and dropship scenarios. The SDL2 port now has a
restrained air-only fog/moon pass, paced replay framing, compact live
rigid-body marks, and simple miner motion poses. The noisier parallax and
cloud treatment is deliberately deferred until it can preserve the material
read of the v0.0.4 renderer; it still does not ship the exact imported DOOM
palette or a full skeletal-animation system.

## The title screen

| Row | What it opens |
|---|---|
| **BEGIN** | Match setup: bot count, map, arsenal, and game customization |
| **INBOX (N)** | Messages the miners left you between sessions. `(N)` is the unread count |
| **LOG** | Everything anyone has said, oldest first, across every session |
| **PRACTICE** | A no-bot sandbox with the same authoritative material and weapon rules |
| **CONTROLS** | Rebind every action per device |
| **OPTIONS** | Frame cap, Lightfield tier, volume, Laptop Mode, haptics, and the memory reset |
| **QUIT** | Leave |

Maps are Coal Ridge, Deepworks, and Furnace Yard. Arsenals are Full Works
(all eleven weapons), Miner Kit (direct mining and firearms), and Powder Keg
(thrown explosives and material deposits).

The interface uses a repository-owned 5 x 7 BIOS-style bitmap alphabet with
VGA-era drop shadows and the fixed IBM-PC 16-color values. It loads no host
font, so glyph shape, spacing, palette, and licensing remain identical across
platform adapters.

Every screen draws through one window widget with a title, a scrolling content
region, a scrollbar when the content is taller than the region, and a footer.
`digs_demo --menu-self-test` walks all twelve screens and fails if anything draws
outside its frame.

## Controls

Every action below is rebindable from the CONTROLS screen, per device. These
are the defaults; the screen is authoritative if they ever disagree.

| Action | Player 1 | Player 2 | Pad |
|---|---|---|---|
| Move left / right | `A` / `D` | left / right | stick or dpad |
| Jump | `Space` | up | A |
| Steam | `Left Shift` | `Right Shift` | X |
| Rope | middle mouse, always-toggle | `/` | LB |
| Fire | `E` or left mouse | `Right Ctrl` | RB |
| Previous / next weapon | `Z` / `X` | `,` / `.` | Y / B |
| Bark | `C` | `M` | right stick click |

Holding jump after leaving the ground engages the steampack. Aim is the mouse
for player 1 and the right stick on a pad. The mouse wheel zooms the
player-locked camera; `F1` toggles the debug overlay and `F11` fullscreen.

**Bark is the whole conversational interface.** The player character never
speaks unprompted. One press produces exactly one line, chosen to fit what is
happening, and aiming at a miner when you press it makes that miner the
subject and the most likely to answer. Playing alone, repeated presses build a
train of thought rather than restarting from unrelated openers.

## Full Works arsenal

Every weapon enters the authoritative game path. Melee edits, projectiles,
deposited material, blast damage, cooldowns, kills, and resulting effects feed
the deterministic match state.

| Slot | Weapon | Demonstrated system |
|---|---|---|
| 1 | PULASKI | Precise close-range excavation and melee damage |
| 2 | POPPER | Fused gravity projectile, terrain blast, and high damage |
| 3 | SMOKER | Fused gravity projectile that deposits buoyant smoke |
| 4 | HOT RAIL | Fused gravity projectile that deposits hot lava and starts heat reactions |
| 5 | HYDROSHOT | Fast water stream for displacement, damage, cooling, and steam |
| 6 | GIANT FUCKING HAMMER | Broad close-range terrain and player impact |
| 7 | BOLT ACTION | Fast direct projectile and compact impact effect |
| 8 | SCATTERBRAIN | Short-lived multi-projectile spread |
| 9 | FIRECRACKER | Fused gravity projectile with the widest terrain shock |
| 10 | BORING DRILL | Fused explosive with blast damage and nail-like voxel effects |
| 11 | RAILSHOT | Charged piercing ray that loses energy through terrain and bodies |

Numeric slots are stable across releases; the names are v0.0.4 gameplay names
layered over them, so a saved binding or a replay does not break when a weapon
is renamed.

The pools are fixed and bounded: up to 64 authoritative projectiles and 768
transient voxel effects. A stable slot order, integer positions and velocities,
and bounded projectile substeps make exhaustion and collision behavior
repeatable.

## The miners

RIVET, CINDER and FLAMEY are fixed identities, not skins over one AI. Each has
an archetype that decides how it fights and how it talks:

| Miner | Reads as | In conversation |
|---|---|---|
| RIVET | Deliberate, methodical, slow to revise | Nearly always lets you finish |
| CINDER | Aggressive, impatient, quick to swing and quick to forget | Talks over you often |
| FLAMEY | Erratic and sociable | Inconsistent — sometimes waits, sometimes cuts in |

They breach walls with a cutting tool rather than grinding against terrain, so
a walled-in bot digs its way out.

**Relationships are pairwise and deliberately have no UI.** There is no
nameplate tint and no standings screen. What a miner thinks of you is carried
entirely by what it says. Six pairs across four slots each hold a valence and
one of eight tones from FEUD to BONDED, moved by thirty-seven distinct
stimuli. Tone changes are hysteretic so a pair sitting on a threshold does not
flicker, and valence decays toward zero, so a grudge fades if you stop feeding
it.

Lines come from a written corpus, never assembled at runtime. An unwritten
combination produces silence rather than a blank bubble or a mad-lib.

## What survives closing the game

Memory, the inbox, and the conversation log live in a versioned save layer
beside `settings.cfg` in the pref path. It is written atomically — temporary
file, rename, previous copy kept aside — so a failed replacement restores
rather than destroys. A file that fails its checksum is repaired from the last
good copy, and the miners are told about the damage in fiction rather than the
player being shown an error.

The saved layer is an opening condition handed to the match like a seed. It
does reach the simulation — a match opened from a played chronicle does not
simulate identically to a first meeting, which is the whole point — but no
wall-clock value ever enters a digest the canonical hash consumes.

`OPTIONS` carries a memory reset behind a two-press confirm. RFC 0003 records
why that is a deliberate exception: the design intent is that the history
between you and these three is unrepeatable, and a reset makes it a save file
instead. It exists at the project lead's explicit request.

## Systemic material playground

The demo uses fourteen stable material IDs: air, bedrock, stone, soil, coal,
biomass, sand, water, lava, metal, flesh, blood, smoke, and firedamp.

- Sand, water, lava, and blood fall; the untagged v0.0.5 core also keeps bounded
  fixed-point fluid volumes that pool and equalize laterally; smoke, firedamp,
  and gaseous water rise.
- Water touching lava converts the lava contact to stone and emits hot smoke
  as the demo's steam representation.
- Lava or sufficiently hot neighbors ignite flammable biomass, coal, and
  firedamp. Burning solid material accumulates damage and becomes smoke.
- Ignited firedamp triggers a bounded terrain blast.
- Bedrock anchors the arena and cannot be removed by ordinary weapons.
- Rising lava is inserted into the bottom of the world and damages miners that
  contact the hazard.

Reactions are intentionally local, ordered, integer-only, and bounded. This is
a systemic vocabulary, not a chemically complete simulation.

## Lightfield and presentation

The software renderer scans each ten-cell depth column once, caching its
frontmost occupied material while collecting RGB emission from every layer. It
then propagates skylight and lava/hot-cell emission through a `512 x 320`
world-space field. Occupied cells attenuate light more strongly than air. The
cached surface and field are sampled while writing a caller-owned `320 x 200`
RGB24 framebuffer.

The option changes propagation work, not simulation:

| Tier | Propagation passes | Intended use |
|---|---:|---|
| Compatibility | 1 | Lowest-cost CPU presentation and future constrained adapters |
| Balanced | 3 | Default desktop setting |
| Showcase | 5 | Wider glow where CPU headroom permits |

The host builds a render-only snapshot for miners and projectiles so they
receive the same Lightfield treatment without mutating authoritative terrain or
replay hashes. Transient gore, smoke, sparks, and dust are then blended as
compact screen-space marks, so they cannot overwrite terrain as opaque blocks.
Blood uses the restored v0.0.4 visual baseline while its newer systemic path is
rebuilt: a broad, long-lived ballistic burst leaves non-blocking blood residue
where drops land and also deposits into the authoritative fluid volume. The
renderer no longer substitutes a faint surface-stain pass for the established
readable spray. SDL2 uploads the completed RGB texture and scales it to the
window with letterboxing.

`MY MINER` also exposes outfit and helmet colours for each local human slot.
They are presentation-only schema-6 preferences: bots retain their authored
looks, and changing either colour never reaches the match hash or chronicle
format.

The SDL2 port also overlays a compact voxel dropship hull and each miner's held
tool silhouette from existing authoritative position, velocity, selected-tool,
and aim state. These overlays are rebuilt every frame and never become terrain,
physics, or replay-hash state.

When BEGIN starts an interactive match, the SDL2 host stages the miners on that
authoritative hull before tick zero. FIRE launches them; bots use their normal
input stream; and the route releases any holdouts at its far in-world endpoint.
The ground spawn chosen during initialization remains the deterministic respawn
target rather than a hidden second vehicle simulation. The deck begins below
the HUD-safe top edge, and FIRE ejects a deck rider through the lower hold with
a full authoritative cell of clearance before gravity takes over; a real
post-launch collision is still lethal. The opening announcer says
`FIRE TO LAUNCH!`; lava start uses the same deep speech path for the pilot
line `PILOT: LAVA RISING. EXTRACT NOW!` before its alarm.
`vox_digs_match_init` itself leaves the hull departed: core scenarios that do
not explicitly begin the launch sequence have no virtual ship movement or
collision surface.

The renderer currently uses repository-owned palette data. An exact imported
DOOM palette is not shipped because the repository's asset-provenance policy
does not permit copying commercial game assets without an approved source and
license decision.

The selectable `15 LOW`, `30`, `60`, `90`, `120`, `144`, and `UNLIMITED` caps
govern presentation only. A fixed-step accumulator advances the game in exact
one-sixtieth-second authoritative steps, with at most eight catch-up steps per
presented frame. If a host is overwhelmed, it drops excess wall-clock debt
rather than using a variable timestep: the match may run slower than real time,
but simulated step size and ordering remain unchanged. Laptop Mode may reduce
presentation work and never changes match rules, FX counts, bot decisions, or
the hash.

Audio is generated procedurally as short queued signed-16-bit mono effects at
22,050 Hz. There is no sampled sound dependency. If SDL2 cannot open an audio
device, gameplay and verification continue silently.

## Non-windowed acceptance

After configuring with `-DVOX_BUILD_SDL2_DEMO=ON`:

```sh
./build/digs_demo --smoke-test /tmp/digs-demo-smoke.ppm
./build/digs_demo --load-self-test 600
./build/digs_demo --chronicle-self-test
./build/digs_demo --menu-self-test
./build/digs_demo --benchmark 240
```

The load self-test is the portable determinism gate: 600 authoritative ticks
reproducing exact activity counters and state hash, with no wall-clock
assertion. Those counters moved from v0.0.3 because bots now dig, so a v0.0.3
expectation applied to a v0.0.4 binary reads as a failure and is not one — use
this release's evidence log.

`--chronicle-self-test` round-trips the save layer and confirms a damaged file
is refused by its checksum. `--menu-self-test` walks every screen.

For the complete local gate:

```sh
tools/vox-verify.sh          # strict ISO C90 build, tests, self-tests, gates
./dev.sh check               # the same, plus size budget and optimisation invariance
```

`tools/vox-session-evidence.sh` is the one lane that crosses a process
boundary: it plays a match in one process, writes the chronicle, and opens a
match from it in another, proving that carried memory reaches the simulation.
Every other test runs inside a single process, and memory that survives a
single process is a variable rather than memory.

Interactive release acceptance still covers what no automated lane can: menu
navigation, all eleven weapon bindings, bot combat, damage/death/respawn, the
three map styles, fixed-step behavior at every cap, Lightfield differences,
fullscreen transitions, pause/results flow, audio fallback, rising-lava damage
— and, new in this release, whether the miners read as characters. See
`qa/V0.0.4-QUICK-FEEDBACK.txt`.

## Testing cockpit and feedback

The repository and Linux tester bundle contain `qa/VOX_QA_FEEDBACK.xlsx`,
whose Checkpoints, Issues, and Environment sheets turn the manual acceptance
list into structured evidence. Preserve an untouched template and give each
tester their own copy. The workbook is generated deterministically from
`qa/VOX_QA_CHECKPOINTS.csv` and regenerates byte-identically under the pinned
CPython 3.12.3 / openpyxl 3.1.2 / lxml 5.2.1.

With xleak installed, combine one or more completed workbooks and run the
automated checks against the same binary:

```sh
tools/vox-test-cockpit.sh --binary ./build/digs_demo \
  tester-claire.xlsx tester-b.xlsx
```

Review the generated `REPORT.md`, copied workbooks, logs, and screenshots for
credentials or private machine information before sharing. The chronicle
records what you and the miners said to each other; treat it as you would any
other personal file. The cockpit is local-only and performs no upload.

## Deliberate limits and open v0.0.5 work

- The world is a bounded slab, not the planned large or streamed profile.
- The v0.0.5 rigid pool is bounded and deterministic; it is not a general
  convex-body engine. The match-owned support scheduler carries wide roofs
  through bounded fragments, and sleeping debris reintroduces up to sixteen
  loose cells in stable order while hashing every explicit remainder. The SDL2
  port draws compact, capped live rigid marks and renderer-only miner motion;
  whole-world support/load analysis and a full skeletal-animation system remain
  open.
- Persistent fluid state is implemented and tested, but dam/lake, magma-spill,
  and blood-basin behavior still require manual QA in the host.
- Headshots, forward dash, awards, dropship state, and replay selection are in
  the untagged core. The SDL2 host paces the bounded best-kill ledger and
  frames its killer/victim exchange, while the full interactive dropship and
  replay experience remains acceptance-gated.
- The SDL2 host is a Linux-first acceptance path. The only NASM code is an
  optional Linux x86-64 FNV-1a contract probe; it does not accelerate gameplay
  or rendering. Windows, macOS, historical APIs, production SIMD/assembly, and
  GPU renderers remain evidence-gated work.
- There is no network multiplayer, replay save-file format, asset pack, mod
  loader, or editor, and no compatibility promise across ABI changes.

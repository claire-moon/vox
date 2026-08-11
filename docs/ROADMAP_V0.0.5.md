<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# v0.0.5 — "MEAT AND WEATHER"

The plan for the release after v0.0.4. Written to be picked up cold: every item
says what the lead asked for, what exists today, and what "done" looks like.

Source of intent is `v004changes.txt` — the lead's own notes. Items below quote
or paraphrase it and cite the line of thinking, so nothing here is invented.
Where an item conflicts with a standing constraint, the conflict is stated
rather than quietly resolved.

---

## Where v0.0.4 left things

| | |
|---|---|
| Tests | 23, all green |
| Payload | 514,063 B — **34.8 %** of the 1,474,560 B ceiling |
| Headroom | **960,497 B** |
| ABI | 11 |
| Settings schema | 6 |
| Chronicle format | 1 |
| Canonical 600-tick hash | `1acec253` |

**Two things are carried over, not finished.** Neither blocks starting v0.0.5,
but both belong to v0.0.4 and should not be quietly forgotten:

1. **The fifteen-minute soak (VOX-QA-084) has no v0.0.4 run.** The 600-tick
   named-bench qualification passed (avg 3.07 ms, p95 5.29 ms, max 9.62 ms
   against 5 / 8 / 16.67) but it is not a smaller version of the soak, which
   needs two humans, a controller, and fifteen minutes.
2. **Every human QA checkpoint is unrecorded.** VOX-QA-089–093 (memory across
   sessions) and 096–100 (conversation pacing) are release-blocking. The
   mechanism is proven by `digs_memory_across_sessions`; whether any of it
   *reads* as a grudge is unmeasured.

The table above is the historical v0.0.4 baseline. The current checkout is an
untagged implementation increment and is not a published v0.0.5 release.

## Current contract and implementation status

The following decisions are locked for v0.0.5:

- Four total slots remain: up to two humans plus RIVET, CINDER, and FLAMEY.
  Six relationship pairs and four persistent identities remain unchanged.
- Settings schema is 6; chronicle format and memory version remain 1. The old
  player-facing seed row and old rope Hold/Toggle choice are removed. Internal
  deterministic seeds remain in rules, diagnostics, and replay metadata.
- Grapple is always-toggle: press to cast/attach, press again to retarget,
  jump to cancel, and arbitrary solid terrain is valid. Metal fixtures are a
  special target class and destroyed fixtures produce non-anchor scrap.
- The authoritative core now has bounded fixed-point fluid, oriented rigid-body,
  structural-cluster, award, replay-selection, dash, and dropship interfaces.
  Water, lava, and blood share the persistent fluid path; corpses and detached
  debris share the rigid pool; and headshots, kill healing, cave-ins, grapple
  targeting, and dropship events are hashed through the match state where
  appropriate. Authored fixtures are explicitly tagged rather than inferred
  from ordinary metal, so only FIRECRACKER/POPPER explosive fractures may
  turn them into non-anchor scrap.
- The current implementation increment is covered by strict C90 builds, 31
  CTest tests including the long deterministic match gate, focused
  fluid/rigid/cluster/gameplay tests, and SDL settings/chronicle/haptic
  self-tests. Those are source/build gates, not a substitute for human QA.
- Feedback hardening keeps jointed corpse segments out of their own contact
  pairs, so they settle rather than jittering as suspended organs.  Blood and
  flesh effects now have a short, heavy ballistic pass: blood deposits into
  the authoritative fluid world on impact and neither material remains a
  long-lived airborne cloud.  `CRACKING...` is debug-only; ordinary play
  reserves its alert text for an actual `CAVE-IN!`.

Still open before calling v0.0.5 complete: complete connected-volume
support/load analysis beyond the bounded cascade fragments, the recorded
named-machine p99 result from `digs_demo --performance-stress-self-test`, and
the remaining palette/animation art pass. The reproducible counterpart is
`tools/vox-destruction-stress.sh`, which runs 18,000 fixed ticks of four-miner
destruction and compares only canonical work counters. An exact imported
DOOM palette is additionally blocked until its asset provenance is approved
under the repository policy. Payload/performance qualification after all
changes and the manual scenarios listed below also remain required.

---

## Standing constraints — these do not relax

From the lead's notes, unchanged since v0.0.4:

- **ISO C only.** Strict C89 for every first-party translation unit, including
  ports, tools and tests. No C++, no Rust, no embedded scripting.
- **The game must not exceed 1.44 MB.** The gate is enforced inside
  `tools/package-linux-demo.sh`, so a bundle that outgrows it fails rather
  than ships.
- **Performance must hold at all framerates regardless of device.**
- **Determinism is the foundation.** Fixed 60 Hz, integer-only, no allocation
  in the sim, no wall clock, one noise function. A wall-clock value may be read
  by the port and written to the chronicle, but must never reach a digest the
  canonical hash consumes.

**The size budget is the binding constraint on this release.** v0.0.5 is the
heaviest feature list the project has had — fluid simulation, ragdolls,
skeletal animation, a raymarched sky, a 256-colour palette, voxel weapon models
— against 960 KB of headroom. Budget per feature before writing it, and measure
with `tools/vox-size-report.sh` as you go rather than at the end.

---

## Phase A — Feedback and violence

The lead's largest single theme: *"we want every action towards either a miner
or the terrain to have a john woo esque poppy-ness to it"*. This is the release
name. Do it first, because it is what the release is judged on.

### A1. Liquid overhaul

- **Asked for:** water that actually propagates so you can build lakes and
  rivers; lava that drips out when a magma pocket is popped; blood as a real
  simulated volume rather than a firework.
- **Today:** the untagged core has bounded, fixed-point water/lava/blood cells
  with deterministic downward flow, lateral equalization, pressure/head,
  terrain-aware blocking, and water/lava reaction accounting. The dam, lake,
  magma-spill, and blood-basin claims still need host-level manual acceptance.
- **Done looks like:** a player can dam a channel and fill it; popping a magma
  pocket produces a running spill; blood pools and runs downhill.
- **Watch:** this is the item most likely to break determinism or the tick
  budget. Lateral flow is a per-cell neighbour pass over a 1.6 M-cell world.
  Bound it hard and keep it in the sleeping/awake frontier that already exists.

### A2. Dismemberment replaces gibbing

- **Asked for:** *"remove full gibbing and focus purely on actual
  dismemberment"* — every kill severs from the entry point of the attack.
  Fire melts, explosions gib; everything else dismembers.
- **Today:** anatomy supports entry-point severing, corpse assemblies, blood
  deposits, explosive/heat damage flags, and headshot handling. Weapon-specific
  dismemberment/gib policy still needs broader scenario coverage.
- **Done looks like:** weapon and damage type decide the death. Scatterbrain
  usually splits into pieces and rarely full-gibs; bolt action severs cleanly.

### A3. Guts, bones, and ragdolls

- **Asked for:** Havok-like ragdolls, People Playground physics, Soldier of
  Fortune visceral detail, Halo 2/3 corpse physics. Actual guts and bones.
- **Today:** the untagged core has fixed-point oriented rectangular rigid bodies,
  bounded anatomy joints, stable contacts, fluid drag/buoyancy sampling, and
  deterministic sleep/recycling. It remains a bounded solver rather than a
  general convex rigid-body engine.
- **Done looks like:** severed parts and corpses tumble with momentum inherited
  from the killing blow, so two deaths from the same weapon look different.
- **Watch:** this is the largest engineering item in the release and the one
  most likely to blow both the size and the tick budget. Angular rigid bodies
  with joints, integer-only and deterministic, is a research task. The locked
  contract requires bounded angular rigid segments; a particle-chain
  substitution is not an acceptable scope reduction.

### A4. Headshots

- **Asked for:** any tool including the steam hook can land one; treated like
  a DOUBLE KILL / TRIPLE KILL with an on-screen `HEADSHOT!`; the goriest kill.
- **Today:** headshots are a fatal authoritative damage class with a distinct
  hashed event, award stimulus, and replay-selection weight. Host popup/audio
  acceptance remains separate.

### A5. Weapon feedback pass

Each weapon has specific notes in `v004changes.txt`. Summarised:

| Weapon | Asked for |
|---|---|
| PULASKI | Cut through terrain and miners like butter; bigger spinning-axe graphic; nasty deaths |
| POPPER | Semi-auto explosive-bullet pistol; poor damage, tears terrain; 3–4 shots pop a limb; good for digging |
| SMOKER | Trail of smoke; explosion obscures the battlefield; higher cooldown; stop bouncing after a direct hit; thud/clink sounds; chargeable throw |
| HOT RAIL | Click = low-damage, high-burn bullet; hold = safe tunnel bore that heats but never leaves lava under its user |
| HYDROSHOT | Subtle kickback usable as extra steampack fuel; much more water; drowning |
| GIANT FUCKING HAMMER | Red Faction Guerrilla scale; throws soil in every direction; tumbles landscapes |
| BOLT ACTION | R8-style revolver; hold one second, powerful shot, knockback, smoke puff; visible hot-steel bolt that impales and severs |
| SCATTERBRAIN | Muzzle flash, reload sound; paints the landscape red; melts/scalds terrain with steam from exit craters |
| FIRECRACKER | Hot glowing projectile with a followable trail; varied chunks; repeated use starts tunnel fires; significant knockback |
| BORING DRILL | Wider impact; drives the player toward the ground in mid-air |
| RAILSHOT | Far more kick and reach — blast a line through a mountain; smouldering craters; beefier sound; charging pans the camera *toward* the crosshair for sniping |

Plus: **muzzle flashes for all projectile weapons**, each a small particle
explosion with smoke wisps and a light flash on the shooter.

The first mechanics tranche implements the HOT RAIL split above.  Its public
single-fire API and a tap now stop at the first solid target and scorch it;
only continuous fire enters the bounded excavation path.  The headless gate
proves the tap leaves the seam intact, the hold opens it, and neither path
creates lava in the bore.

### A6. Red Faction-style cluster collapse

- **Asked for:** landscape collapses into itself; miners can cave each other
  in, or kill by blowing a chunk out of a ceiling. Voxel *groups* carry their
  own state so clusters behave as clusters. Craters should pool debris rather
  than leaving Worms-style circles.
- **Today:** direct cuts add low, hashed strain and are amortised across
  structural thresholds; a sound chunk can therefore support a normal narrow
  tunnel without repeatedly scanning or collapsing it. Blasts cross the
  critical threshold immediately. Once a chunk is actually unsafe, every new
  disturbance evaluates it, warnings precede the physical result, and only a
  detached group of sixteen or more cells produces the `CAVE-IN!` banner.
  Detached material becomes bounded rigid debris, so only actual debris or a
  trap can hurt a miner. Oversized fragments deliberately re-arm their edge
  frontier at blast strength, preserving a deterministic cross-map cascade
  without escalating small collapses into a whole-world scan. Detached bodies
  settle up to sixteen loose cells of their recorded material and hash any
  explicit remainder.

---

## Phase B — Movement and traversal

### B1. STEAM HOOK (rope overhaul)

- **Asked for:** rename to STEAM HOOK; auto-aim to whatever is pointed at, so
  players feel good at it; pulls the miner in for escapes and flings; can grab
  landscape (not just grid) at the cost of the hook burrowing; puff of smoke
  from launcher and landscape; impales miners to surfaces.
- **Today:** the rope is always-toggle (cast, press again to retarget, jump to
  cancel), auto-aims arbitrary terrain or metal fixtures, and remains attached
  while other actions run. Host audio/camera feedback remains acceptance work.

### B2. Forward dash

- **Asked for:** works in mid-air and on the ground, grants temporary i-frames
  so attacks can be dodged and saves made. Available to NPCs too.
- **Today:** players and bots share a forward fixed-point dash with cooldown and
  a bounded invulnerability window.

### B3. Kill heals

- **Asked for:** a kill regenerates 50 % of health (or 50 HP — whichever is
  saner for the engine).

---

## Phase C — Presentation

### C1. 256-colour DOOM palette

- **Asked for:** revamp to the DOOM palette with closest-possible conversion,
  to evoke DOS-era games and to be accurate when backported to older systems.
- **Watch:** this changes every colour in the game and interacts with the
  Lightfield. Do it before C2 and C3, not after.

### C2. Parallax skybox

- **Asked for:** Worms-style parallax layers; onion layers of other terrain
  rendered from the same seed with fog to separate them; real-time raymarched
  clouds; no sun but a small white moon on the top layer; a puppet-theatre feel
  where the world unfurls behind the action.
- **Today:** the SDL2 port has a restrained air-only fog bias and a
  seed-derived white moon placed below the HUD-safe top strip. The prior
  parallax silhouettes and per-cell cloud
  dither are deliberately deferred: at the target resolution they competed
  with terrain and made the new physical debris read as visual noise. This is
  a compatible fallback, not a claim that the requested full raymarched
  skybox is complete.

### C3. Lighting and global illumination

- **Asked for:** shadows and possibly GI — *"subtle though, nothing
  overstated"*, enough that voxels plus lighting make emergent paintings, but
  not so much that it fights the palette.

### C4. Camera dynamism

- **Asked for:** gentle pull-out when moving or throwing; pull-in for close
  weapons; shift opposite to a charging shot then slingshot toward it on
  release; slow zoom out on death; gentle sway when moving.
- **Today:** the existing player camera has movement lead, fitting, death hold,
  shake, and event trauma. Results replay now holds each captured frame for a
  bounded slow-motion cadence and frames the ledger's killer/victim exchange
  with headshot and multi-kill close-in treatment. More weapon-specific camera
  choreography remains presentation work.

### C5. QWOP-style skeletal animation

- **Asked for:** walk cycles, jump, steampack, pain and firing states. Not more
  fidelity — more fluidity, *"like N++ or super meat boy"*.
- **Today:** the renderer derives compact walk, jump, steam, pain, and fire
  poses from authoritative body/input state without adding animation state to
  the match. Live rigid bodies also render as compact, capped corpse, debris,
  and scrap marks rather than broad terrain-coloured bars. A fuller skeletal
  system remains optional
  presentation work.

### C6. Voxel weapon models

- **Asked for:** low-poly voxel models the miners visibly carry and aim, so
  muzzle flashes fire out of something.
- **Today:** the SDL2 port overlays a bounded voxel tool silhouette from the
  selected weapon and authoritative aim. It is deliberately render-only; more
  distinct per-tool forms remain presentation work.

---

## Phase D — The DROPSHIP

A self-contained feature, and the most novel thing in the release.

- **Asked for:** a ship crossing the sky at match start to drop the miners —
  press fire to launch out, giving varied first spawns. A comet with a blue
  exhaust glow, showcasing the voxel engine (cf. Jane Orr's ship in Dark
  Forces). At match end, as lava rises, the pilot broadcasts over the top of
  the screen in a deeper, crunchier voice using the same bark system, telling
  everyone to prepare for extraction. An alarm follows the broadcast. Miners
  can land on it by jumping/steampack **or** grapple onto it with the hook and
  be swung out of the match to wait out the rising lava.
- **Note:** this reuses the v0.0.4 bark and voice systems rather than adding a
  parallel one. The pilot is a fourth voice, not a fourth miner.
- **Today:** the authoritative route, launch, collision, grapple, extraction,
  and alarm states are implemented. SDL2 begins an interactive match by staging
  miners on the authoritative hull before tick zero; Fire releases a miner and
  the far route endpoint auto-releases any holdouts. It draws a render-only
  voxel hull with directional exhaust below the HUD-safe top edge. A deck
  launch ejects below the hull before the next collision check; the core and
  host regressions both step through that formerly lethal post-Fire tick,
  while an intentionally intersecting launched miner is still splattered. Its
  default core state is departed until the host calls `vox_digs_dropship_begin`,
  so ground-based deterministic tools do not acquire an invisible ship. Its
  deep announcer opens each route with `OVER AND OUT!`, and the lava alarm
  delivers `PILOT: LAVA RISING. EXTRACT NOW!`
  through the same bark/audio path. Physical acceptance remains open.

---

## Phase E — Bots, voice, and the conversation

**Continued refinement of the conversation system is explicitly a v0.0.5
sub-task**, not a finished thing inherited from v0.0.4.

### E1. Lower-level AI states

- **Asked for:** more states and far larger action pools per state, so random
  behaviours (bots killing themselves, giving up) stop happening by accident.
  *"If a bot kills itself it should be because of its personality and not an
  error in its pathfinding."* Three archetypes standing for typical player
  behaviours — one strategic and burrowing, one aggressive with the heaviest
  tools, and a third to identify.
- **Today:** RIVET / CINDER / FLAMEY submit the same movement, tool, and fire
  actions as players. Their explicit tunnel states cover planning, excavation,
  ambush, escape, trap, collapse risk, drowning, extraction, and recovery;
  focused scenarios distinguish RIVET's route planning, CINDER's direct breach,
  and FLAMEY's ambush/trap choice.

### E2. Death sounds and voice definition

- **Asked for:** garbled, crunchy, game-over-style death moans with comedic
  kick; grunts; better phonetic definition so words are more intelligible —
  closer to a rudimentary text-to-speech — while keeping the Speak & Spell
  character. Distinct pitch, tenor and speed per bot.
- **Today:** voice pitch/tone/speed already exist and are customisable.

### E3. Fixed roster and authored player voice

The v0.0.5 contract retains four total slots: up to two humans plus RIVET,
CINDER, and FLAMEY. There is no fourth bot, fifth slot, or ten-pair
relationship expansion. The six pair records and four persistent identities
remain compatible with chronicle format 1. Player expression comes from an
implemented authored contextual bark corpus, not a selectable persona system.

### E4. Awards, medals, and the replay

- **Asked for:** a Worms-style slow-motion replay of the match's best kill,
  chosen by killstreak / most kills in one shot / similar. Post-game awards
  feed back into the bark and memory systems so bots remember what you won and
  what they won, and rivalries or favour form from it.
- **Locked diction:** `FIREBRAND!`, `REAPER!`, `HOTSHOT!`, `MOLERAT!`, and
  `SKYJOCKEY!` replace the earlier displayed names. `MOLERAT!` is behavioral:
  it is awarded only after successful direct use of PULASKI, HOT RAIL, GIANT
  HAMMER, or BORING DRILL for more than half a finite match, or 7,200 ticks in
  an unlimited one. Count at most once per tick; misses, explosions, combat,
  and cave-in collateral never count.
- **Note:** E4 is the natural bridge between v0.0.4's memory system and
  v0.0.5's spectacle. It is listed last but it is the item most likely to make
  the memory system *feel* present, which is the open question v0.0.4 left.

---

## Phase F — Interface and cleanup

Small and cheap. Three of the lead's interface notes are **already done** and
are listed here only so nobody re-investigates them:

- ~~Remove modes entirely~~ — **done.** Miners vs Machines is gone from the
  menus *and* from the rules; there is no mode enum anywhere in the engine or
  the game.
- ~~More time-limit options including unlimited~~ — **done.** 1:00, 2:00,
  3:00, 5:00, 10:00 and UNLIMITED.
- ~~Mouse in menus~~ — **done.** Every screen registers its rows each frame and
  hit-tests the pointer against them, offset by the scroll position.

Still outstanding:

- **Fonts:** replace the gold/yellow stock font with arcade/90s DOS-appropriate
  faces using the existing font engine.
- **Text spacing:** the lead asked for a pixel or two more between glyphs;
  `VOX_UI_DOS_ADVANCE` went 5 → 6 in v0.0.4. Confirm it reads right now.
- **Fix random deaths while digging.** Called out twice in the notes. Related
  to the HOT RAIL tunnel bug in A5.

---

## Suggested order

```
A2 A5  →  A1 A6  →  A3 A4  →  B1 B2 B3  →  C1  →  C2 C3 C4  →  C5 C6  →  D  →  E  →  F
```

Reasoning:

- **A2 and A5 first.** Cheapest route to the release's stated feel, and they
  need no new engine systems. They also make everything after them look better.
- **A1 and A6 next.** Both are material-simulation work and share the same
  bounded-neighbour-pass problem; doing them together avoids solving it twice.
- **A3 before A4.** Headshots want ragdolls to land properly.
- **C1 before the rest of C.** The palette changes every colour; doing skybox
  and lighting first means doing them twice.
- **E4 late, but do not drop it.** It is the item most likely to answer whether
  the memory system reads as present.
- **F throughout** — these are small and make good filler between large items.

---

## Resolved release decisions

The locked v0.0.5 contract resolves the scope questions that were present in
the earlier exploratory roadmap:

1. **Ragdolls use full bounded angular rigid segments.** A particle-chain
   approximation is not an acceptable substitute.
2. **The fixed four-slot roster is part of the compatibility contract.** New
   gameplay must not expand the relationship matrix or chronicle identity
   count.
3. **Relationships still have no direct UI.** v0.0.4 deliberately shipped with
   none. If it reads as invisible in play, the nameplate tint is the cheapest
   remedy and should be reconsidered before anything more elaborate. The human
   QA checkpoints are what answer this.
4. **The third archetype is FLAMEY:** short trap tunnels, ambush routes,
   smoke/fire setups, and opportunistic escapes.
5. **The 1.44 MB ceiling remains decisive.** 960 KB of headroom for
   fluids, ragdolls, skeletal animation, a raymarched sky and voxel weapon
   models is tight. If it comes to a choice, which features are the release and
   which are negotiable?

---

## Where to look

| Thing | Where |
|---|---|
| Traps and unproven claims | `CONCERNS.md` — read before starting |
| The lead's original notes | `v004changes.txt` |
| What the current build does | `docs/DEMO.md` |
| How the miners work | `docs/architecture/BOTS_AND_CONVERSATION.md` |
| Why the memory boundary is shaped this way | `docs/rfcs/0003-digs-bot-memory-and-conversation.md` |
| What has actually been measured | `docs/compatibility/MATRIX.md` |
| What v0.0.4 still owes | `docs/releasing/V0.0.4_RELEASE_CHECKLIST.md` |
| The gates | `./dev.sh check`, `tools/vox-verify.sh` |
| The size budget | `tools/vox-size-report.sh` |

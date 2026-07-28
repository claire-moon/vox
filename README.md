# VOX engine / DIGS game

VOX is a CPU-authoritative mini-voxel engine for deterministic, systemic
worlds. DIGS is its first game: a real-time, side-view steampunk-miner
deathmatch in which terrain, projectiles, hazards, and effects share the same
material simulation.

`v0.0.3` is the active development demo. It retains the v0.0.2 match while
making collision recovery, swept anatomy hits, loose debris, reaction waking,
rope traversal, audio timing, and frame budgets explicit contracts. It adds a
charged Mining Rail, controllable lift-and-glide steampack, contextual speech,
Switch Pro haptics, controller-aware prompts, Laptop Mode, and close-zoom
native voxel detail. It is still a vertical slice, not a claim that the complete
cross-generation engine is finished. Ports become supported only after their
own reproducible acceptance evidence is recorded.

The tagged `v0.0.1` release remains the first verified Linux x86-64 CPU
baseline. Its release checklist, evidence record, hashes, and deliberate limits
remain under `docs/releasing/` and are not retroactively changed by this work.

## v0.0.3 development-demo highlights

- A bounded `512 x 320 x 10` mini-voxel world: 1,638,400 simulated cells,
  exactly 40x the original demo volume and four times the prior dense profile,
  partitioned into 640
  `16 x 16 x 10` active/sleeping chunks.
- Four match slots with one or two independent local humans and zero to two
  deterministic bots. Free-for-All and Miners vs Machines expose team and
  friendly-fire rules without giving bots hidden physics advantages. Normal
  matches use a selectable two- or three-minute clock, optional score cap,
  named players, authoritative winner/draw results, and configurable AUTO or
  ON FIRE respawn timing.
- Three topology-specific maps generated from a visible seed: Coal Ridge's
  rolling seams and drifts, Deepworks' connected chambers and shafts, and
  Furnace Yard's industrial terraces and contained hot pockets. Every style
  can resolve into an archipelago, continent, or twin-hill macro landform.
  Sparse rope fixtures sit high above the complete jump envelope and never
  place support legs across the ground route.
- Eleven steampunk tools and weapons: Pick, Blast Charge, Smoke Pot, Cinder Flask,
  Pressure Hose, Sledge, Nail Gun, Boiler Shotgun, Concussion Grenade, and Nail
  Bomb, plus the charged, energy-piercing Mining Rail.
- Accelerated/coyote/buffered movement, a swept casting hook and stable
  terrain-wrapping segmented rope with reel/break rules, lift-and-glide steam
  traversal, fixed-pool projectiles, destructible and collapsing terrain,
  rising lava, cooling, fire, smoke, and firedamp.
- Bounded local depenetration replaces living-player spawn teleports. Swept
  contact resolves visible anatomy instead of undersized point samples, while
  impact-only loose deposition prevents airborne TTL solids and debris towers.
- Fifteen-part miner anatomy, bleeding/cautery/severing state, five-second
  attack-cancelled spawn shields, deterministic event variants, and selectable
  `768`/`1536`/`3072` simulated FX-voxel budgets. The selected budget is an
  explicit deterministic match profile, not a presentation-only switch.
- Exclusive per-player AUTO/keyboard/controller ownership, persisted input
  tuning, circular deadzones, player-relative tool-range aim, raw joystick
  fallback, SDL GameController hotplug and deterministic claims for two pads,
  runtime rebinding, controller-family pixel prompts, shared dynamic camera
  framing, hardware-pointer-correct mouse aim, distance-weighted haptics,
  local-only flashes, and damage-number controls.
- A sandboxed Lua 5.1 catalog containing 87 materials, weapons, entities,
  reactions, anatomy parts, modes, AI states, and system entries. The in-game
  Miner's Index displays six entries at a time; F5 uses transactional reload.
- An original allocation-free VOX Audio engine with eight bounded stereo
  voices driven by the audio-device clock, sample-time menu notes, hit/kill
  confirms, world-aware wind/water/lava ambience, and deterministic Speak &
  Spell-style speech built from curated and seeded lexical pools. No sampled
  sound asset or external speech runtime is required.
- A CPU RGB Lightfield with Compatibility, Balanced, and Showcase quality
  tiers. One depth scan caches each visible surface while collecting emission
  from all ten layers; rendering and frame pacing never change the
  authoritative 60 Hz simulation.
- An opt-in Linux System V x86-64 NASM FNV-1a leaf with a strict C89 fallback
  and exhaustive scalar/assembly parity over the acceptance vectors. It is a
  dispatch contract probe, not a renderer or simulation speed claim.
- A DIGS-only title screen, setup, Foundry Lab, How To Play, Miner's Index,
  Controls, Options, QA, pause/results screens, 15/30/60/90/120/144/Unlimited
  caps, presentation-only Laptop Mode, fullscreen, and a debug overlay.

The demo renders the simulation through a software framebuffer and asks SDL2
only for the window, texture presentation, input, timing, and audio device. It
does not yet contain a GPU renderer, networking, a general rigid-body solver,
or the historical platform adapters described by [ROADMAP.txt](ROADMAP.txt).
See [the v0.0.3 quick-start and test guide](CG-README.TXT) for exact controls,
rope, controller assignment, script-runtime, and manual acceptance behavior.

## Build the portable core

The default build is headless and does not require SDL2. It is the graceful
path for build servers, bring-up work, and platforms whose host adapter does
not exist yet.

Requirements are CMake 3.16 or newer, a C89 compiler, a C++98 compiler, and
Rust/Cargo for the small modern-host boundary test.

```sh
cmake -S . -B build -DVOX_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cargo test --workspace
```

The standalone software-render proof writes a PPM without opening a window:

```sh
./build/vox_render_demo /tmp/vox-render.ppm
```

## Build and run the graphical demo

The SDL2 host is opt-in. It requires SDL 2.0.10 or newer; SDL 2.30.0 is the
version used on the initial Linux test machine. On Debian, Ubuntu, or Linux
Mint, install it with:

```sh
sudo apt-get install cmake build-essential cargo libsdl2-dev
# Optional on Linux x86-64, for the assembly contract probe:
sudo apt-get install nasm
```

Then configure the demo explicitly:

```sh
cmake -S . -B build-demo \
  -DVOX_BUILD_TESTS=ON \
  -DVOX_BUILD_SDL2_DEMO=ON
cmake --build build-demo --parallel
ctest --test-dir build-demo --output-on-failure
./build-demo/digs_demo
```

On Linux System V x86-64, append `-DVOX_BUILD_NASM_ACCEL=ON` to the CMake
configure command to build and test the optional assembly contract probe.
Thermally constrained machines can cap the project scripts without changing
test coverage, for example `VOX_BUILD_JOBS=1 tools/vox-verify.sh` or
`VOX_BUILD_JOBS=2 tools/package-linux-demo.sh`.

The host links to the system SDL2 library. No SDL source or binary is vendored.
A pinned zlib-licensed SDL GameControllerDB data snapshot is bundled so known
pads are normalized before enumeration; unknown pads use the raw-joystick
fallback. See [THIRD_PARTY.md](THIRD_PARTY.md).

## Run the Linux tester bundle

The current easy-run artifact is a Linux x86-64 bundle. Extract it and start
the launcher; it checks the dynamically linked SDL2 runtime and prints
distribution-specific installation help if the library is missing:

To obtain a current development bundle, open the repository's
[GitHub Actions runs](https://github.com/claire-moon/vox/actions), select the
latest successful `foundation-ci` run for the branch or pull request being
tested, and download the `vox-digs-v0.0.3-linux-x86_64` artifact. Tagged
release bundles will also appear on the repository's Releases page after the
candidate completes manual QA; an Actions artifact is not a final release.

For a complete plain-text walkthrough written for the first local test pass,
open [CG-README.TXT](CG-README.TXT).

```sh
tar -xzf vox-digs-v0.0.3-linux-x86_64.tar.gz
cd vox-digs-v0.0.3-linux-x86_64
./run-digs.sh
```

`smoke-test.sh`, `benchmark.sh`, the `digs_script` validator, and both headless
proof binaries are included beside the game. The packaged runtime data lives
under `share/digs/`, including the Lua catalog and
`controllers/gamecontrollerdb.txt`; keep that directory with the executable.
The matching GPL
Corresponding Source archive and
`SHA256SUMS` ship next to the binary archive. Both archives contain an SPDX
2.3 JSON SBOM. The binary bundle also preserves the package-time CTest, Rust,
headless, smoke, renderer, and benchmark streams under `evidence/` so its
claims can be audited against the exact executable. Maintainers create all
three files from a clean checkout with:

```sh
tools/package-linux-demo.sh
(cd dist && sha256sum -c SHA256SUMS)
```

The release packager also requires Python 3 with `openpyxl`; it refuses to ship
a stale QA workbook. On Debian-family systems install `python3-openpyxl`.

This bundle is intentionally labeled for Linux x86-64; it is not evidence for
the planned Windows, macOS, historical, or GPU adapters.

## Playtest feedback

The title screen's **QA Feedback** entry points testers to the versioned
88-checkpoint workbook and cockpit. Fill `qa/VOX_QA_FEEDBACK.xlsx`, then combine
one or more
tester workbooks with xleak and optionally exercise the exact demo binary:

Start with [`qa/V0.0.3-QUICK-FEEDBACK.txt`](qa/V0.0.3-QUICK-FEEDBACK.txt) for a
guided 20-minute keyboard/mouse regression lane. Its F310 and Switch Pro
USB/Bluetooth additions are separate observations, and it names the exact
workbook rows for collision, damage, debris, rope, rail, steampack, zoom,
audio/caps, Laptop Mode, haptics, two-player bots, and hashes. It is a rapid
feedback path, not a substitute for the complete release pass.

```sh
tools/vox-test-cockpit.sh --binary ./build-demo/digs_demo \
  qa/VOX_QA_FEEDBACK.xlsx
```

The command produces a Markdown report, raw sheet exports, logs, copied
workbooks, and a reviewable `.tar.gz` evidence packet under `qa/out/`; it never
uploads anything. See [qa/README.md](qa/README.md) for result/severity rules and
privacy checks, then submit a reviewed packet through the
[DIGS demo feedback form](https://github.com/claire-moon/vox/issues/new?template=demo-feedback.yml).

## Automated smoke and benchmark modes

The smoke path does not initialize a window or audio device. It runs a fixed,
bounded match scenario, renders the final state, writes a `320 x 200` PPM, and
prints simulation and framebuffer hashes:

```sh
./build-demo/digs_demo --smoke-test /tmp/digs-demo-smoke.ppm
./build-demo/digs_demo --input-self-test
./build-demo/digs_demo --cap-self-test
./build-demo/digs_demo --audio-cadence-self-test
./build-demo/digs_demo --bark-self-test
./build-demo/digs_demo --haptic-self-test
./build-demo/digs_demo --load-self-test 600
./build-demo/digs_demo --performance-self-test 600
./build-demo/digs_demo --settings-self-test /tmp/digs-settings-test.cfg
./build-demo/digs_demo --camera-self-test
./build-demo/digs_demo --render-miner-icon-xpm /tmp/digs-miner.xpm
```

The input self-test exercises controller radial normalization, precision, and
weapon-aware reach without opening a window. Cap qualification, audio cadence,
bark/G2P coverage, haptic mixing, the canonical four-player simulation load,
settings migration, and camera transforms also have non-windowed deterministic
checks. `--load-self-test 600` is the host-independent correctness gate: it
checks exact activity and state hash `f3924b81` without judging wall-clock
speed. `--performance-self-test 600` retains the strict 5 ms average, 8 ms p95,
and 16.67 ms maximum qualification and is release evidence only on the named
i7-10750H laptop bench. Hosted CI never treats shared-runner timing as that
hardware qualification.
The icon command emits the exact
transparent, nearest-neighbor miner
used by the runtime renderer; release packaging compares it byte-for-byte with
the reviewed canonical XPM.

The renderer benchmark measures each Lightfield tier on the same deterministic
scene. The optional argument is the number of frames per tier:

```sh
./build-demo/digs_demo --benchmark 240
```

`tools/vox-verify.sh` configures strict `-std=c89`/`-std=c++98` builds out of
tree, runs the native determinism tests and the Rust workspace, executes both
headless proofs, and runs the graphical host's non-windowed smoke scenario.
It expects the SDL2 development package because it deliberately verifies the
opt-in demo as well as the default core. On Linux x86-64 it enables the NASM
parity probe automatically when `nasm` is present; set `VOX_NASM_ACCEL=OFF` to
exercise the identical scalar fallback explicitly.

## Controls

- Menus: arrow keys or controller D-pad/left stick move; `Enter`, controller
  A, or Start confirms; `Esc` or controller B goes back.
- Player 1 keyboard/mouse: `A`/`D` run, `Space` jumps, left `Shift` uses the
  steam pack, `Q` operates the rope, `W`/`S` reel an attached rope, mouse aims,
  `C` barks, and `E` or left mouse fires. Number keys `1` through `0` select
  the original tools and `-` selects the Mining Rail.
- Player 2 keyboard: left/right run, up jumps, right `Shift` uses steam, `/`
  holds the rope, right `Ctrl` fires, `I`/`J`/`K`/`L` aims, and `,`/`.` cycles
  tools; `M` barks.
- Normalized controller: left stick moves/reels; right stick uses radial,
  player-relative aim; the detected face-button prompts identify jump and
  steam; left shoulder operates the rope; right shoulder or ZR/right trigger
  fires; the displayed buttons cycle tools; R3 barks; and Start/Plus pauses.
- Controller zoom: hold ZL/left trigger and move the right stick vertically.
  Mouse wheel remains the keyboard/mouse zoom control.
- Match/global: the mouse wheel biases camera zoom, Shift+wheel cycles P1's
  arsenal, `Esc` pauses, `R` restarts, `F1` toggles diagnostics, `F5` reloads
  validated Lua data transactionally, and `F11` toggles fullscreen.

The Controls screen supports action rebinding and conflict swaps.
Options > Input & Controller selects exclusive AUTO, keyboard, or controller
ownership per player and persists sensitivity, deadzone, aim slowdown, and
Hold/Toggle rope policy. See [CG-README.TXT](CG-README.TXT) for F310 X/D-mode
and Switch Pro USB/Bluetooth testing, controller assignment, rope behavior,
script validation, the arsenal, and the complete v0.0.3 acceptance procedure.
[docs/DEMO.md](docs/DEMO.md) remains the historical v0.0.1 demo guide.

## Architecture and portability policy

The scalar C89 world is the behavioral oracle. C++98 is isolated behind a C
ABI for fixed-point body collision. Rust currently provides a dependency-free,
versioned modern-host capability boundary; it does not own simulation rules.
The first NASM leaf proves optional build-time dispatch for one FNV-1a byte-hash
contract on Linux System V x86-64. It is not wired into the canonical world
hash and carries no game-performance claim. Future NASM/SIMD, GPU, and
operating-system-specific paths remain optional and must prove equivalence
against the scalar path. See
[docs/architecture/ACCELERATION.md](docs/architecture/ACCELERATION.md).

The initial automated acceptance host is Linux x86-64 on an Intel Core
i7-10750H using the CPU software renderer. The test laptop is reported to
contain a GTX 1660 Ti, but no working NVIDIA driver/device node is available to
the verification process: `nvidia-smi` cannot communicate with the driver and
`/dev/dri` is unavailable. Therefore `v0.0.1` makes no GPU or GTX 1660 Ti
performance claim. Exact evidence and the evidence-gated adapter tiers are
recorded in [docs/compatibility/MATRIX.md](docs/compatibility/MATRIX.md).

Windows, macOS, earlier Unix-like systems, DOS, and classic Mac targets remain
planned adapters. “Cross-generation” is a design constraint, not a blanket
support badge: compile, boot, input, render, match completion, determinism, and
performance evidence are required per platform. The future lab pairs QEMU for
automated guest smoke tests with 86Box for period-PC device fidelity, then uses
native hardware for final performance evidence.

## License and contribution model

Original VOX and DIGS code, documentation, and assets are licensed under the
GNU General Public License version 3 or later. See [LICENSE](LICENSE),
[LICENSES/](LICENSES), and [THIRD_PARTY.md](THIRD_PARTY.md).

The project uses Developer Certificate of Origin sign-offs and does not require
a Contributor License Agreement. Contributors retain copyright in their work,
receive release-note credit, and can earn review and maintainer responsibility
through sustained, constructive participation. The upstream-first governance
loop and portability evidence rules are described in
[CONTRIBUTING.md](CONTRIBUTING.md) and [docs/GOVERNANCE.md](docs/GOVERNANCE.md).

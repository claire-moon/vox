# VOX engine / DIGS game

VOX is a CPU-authoritative mini-voxel engine for deterministic, systemic
worlds. DIGS is its first game: a real-time, side-view steampunk-miner
deathmatch in which terrain, projectiles, hazards, and effects share the same
material simulation.

`v0.0.1` is a focused graphical demonstration, not a claim that the complete
cross-generation engine is finished. It establishes a strict C89 simulation
oracle, a strict C++98 fixed-point body solver, a CPU software Lightfield
renderer, a versioned C ABI, and an SDL2 desktop host. Ports become supported
only after their own reproducible acceptance evidence is recorded.

## Demo highlights

- A bounded `256 x 160 x 10` mini-voxel world: 409,600 simulated cells,
  exactly 10x the original demo volume, partitioned into 160
  `16 x 16 x 10` active/sleeping chunks.
- Four-player Classic FFA with one local player and zero to three deterministic
  bots, health, damage, scoring, death effects, respawns, and match results.
- Coal Ridge, Deepworks, and Furnace Yard maps generated from a visible seed.
- Ten steampunk tools and weapons: Pick, Blast Charge, Smoke Pot, Cinder Flask,
  Pressure Hose, Sledge, Nail Gun, Boiler Shotgun, Concussion Grenade, and Nail
  Bomb.
- Fixed-pool projectiles and voxel effects, destructible terrain, rising lava,
  water/lava cooling, steam and smoke, burning biomass/coal, and firedamp
  ignition.
- A CPU RGB Lightfield with Compatibility, Balanced, and Showcase quality
  tiers. Rendering and frame pacing never change the authoritative 60 Hz
  simulation.
- An opt-in Linux System V x86-64 NASM FNV-1a leaf with a strict C89 fallback
  and exhaustive scalar/assembly parity over the acceptance vectors. It is a
  dispatch contract probe, not a renderer or simulation speed claim.
- A title screen, match setup, Foundry Lab sandbox, pause/results screens,
  frame caps (`30`, `60`, `90`, `120`, `144`, and `Unlimited`), fullscreen,
  a debug overlay, and procedural queued audio generated without sound assets.

The demo renders the simulation through a software framebuffer and asks SDL2
only for the window, texture presentation, input, timing, and audio device. It
does not yet contain a GPU renderer, networking, a general rigid-body solver,
or the historical platform adapters described by [ROADMAP.txt](ROADMAP.txt).

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

The host links to the system SDL2 library. No SDL source or binary is vendored
in this repository; see [THIRD_PARTY.md](THIRD_PARTY.md).

## Run the Linux tester bundle

The current easy-run artifact is a Linux x86-64 bundle. Extract it and start
the launcher; it checks the dynamically linked SDL2 runtime and prints
distribution-specific installation help if the library is missing:

For a complete plain-text walkthrough written for the first local test pass,
open [CG-README.TXT](CG-README.TXT).

```sh
tar -xzf vox-digs-v0.0.1-linux-x86_64.tar.gz
cd vox-digs-v0.0.1-linux-x86_64
./run-digs.sh
```

`smoke-test.sh`, `benchmark.sh`, and both headless proof binaries are included
beside the game. The matching GPL Corresponding Source archive and
`SHA256SUMS` ship next to the binary archive. Both archives contain an SPDX
2.3 JSON SBOM. The binary bundle also preserves the package-time CTest, Rust,
headless, smoke, renderer, and benchmark streams under `evidence/` so its
claims can be audited against the exact executable. Maintainers create all
three files from a clean checkout with:

```sh
tools/package-linux-demo.sh
(cd dist && sha256sum -c SHA256SUMS)
```

This bundle is intentionally labeled for Linux x86-64; it is not evidence for
the planned Windows, macOS, historical, or GPU adapters.

## Playtest feedback

The title screen's **QA Feedback** entry points testers to the versioned
workbook and cockpit. Fill `qa/VOX_QA_FEEDBACK.xlsx`, then combine one or more
tester workbooks with xleak and optionally exercise the exact demo binary:

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
```

The renderer benchmark measures each Lightfield tier on the same deterministic
scene. The optional argument is the number of frames per tier:

```sh
./build-demo/digs_demo --benchmark 240
```

`tools/vox-verify.sh` configures strict `-std=c89`/`-std=c++98` builds out of
tree, runs all native determinism tests and the Rust workspace, executes both
headless proofs, and runs the graphical host's non-windowed smoke scenario.
It expects the SDL2 development package because it deliberately verifies the
opt-in demo as well as the default core. On Linux x86-64 it enables the NASM
parity probe automatically when `nasm` is present; set `VOX_NASM_ACCEL=OFF` to
exercise the identical scalar fallback explicitly.

## Controls

- Menus: arrow keys change the selection/value, `Enter` confirms, and `Esc`
  goes back. On the seed row, `R` generates another deterministic seed.
- Movement: `A`/`D` or left/right arrows run; `W`, up arrow, or `Space` jumps;
  left or right `Shift` engages the rechargeable steam jet.
- Combat: aim with the mouse and fire with the left mouse button. Number keys
  `1` through `0` select the ten weapons. The mouse wheel zooms the
  player-locked camera; Shift+wheel cycles the active arsenal.
- Match: `Esc` pauses and `R` restarts the current map and mode.
- Diagnostics: `F1` toggles the live FPS/tick/hash/material overlay and `F11`
  toggles desktop fullscreen.

See [docs/DEMO.md](docs/DEMO.md) for the arsenal, material interactions,
menus, and acceptance procedure.

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

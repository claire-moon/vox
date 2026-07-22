<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Compatibility and acceptance matrix

VOX treats portability as measured evidence. “Cross-generation” describes the
architecture and adapter roadmap; it does not mean one untested binary supports
every machine released since 1990.

| State | Meaning |
|---|---|
| PLANNED | Design target only; no support claim |
| BUILDS | Reproducible compile/link evidence exists |
| RUNS | Boots and completes the defined smoke scenario |
| VERIFIED | Native or reproducible acceptance evidence is recorded |
| UNSUPPORTED | Explicitly outside the current profile |

## v0.0.2 development-candidate evidence

| Surface | Exact environment | State | Evidence and boundary |
|---|---|---|---|
| Strict portable core, ABI 8, and headless match | Ageless Linux 0.1.1 (Linux Mint 22.3 base), Linux 6.17.0-35 x86-64; Intel Core i7-10750H; GCC/G++ 13.3.0; CMake 3.28.3; Cargo 1.95.0 | RUNS | Extension-free strict C89/C++98 scalar and NASM-enabled suites, Rust tests, audio/input/camera/settings proofs, deterministic headless scenarios, and focused AddressSanitizer/UndefinedBehaviorSanitizer runs pass on the development branch; the clean package records the final commit and logs |
| SDL2 non-windowed demo smoke | Same host; SDL2 2.30.0; Release CPU RGB24 renderer | RUNS | Two latest-source runs match at state `06fb9a04`, frame `78e5e49a`, and PPM SHA-256 `53c0f9d8765d52d753f19163543d7305d18d2e3a7fcda60d8fa4fcd09efcb37c`; this proves the scripted smoke path, not desktop input or audible output |
| Release CPU Lightfield sample | Same host on the `balanced` power profile; 60 frames per tier | RUNS | Compatibility 7.708 ms/frame (129.7 FPS), Balanced 10.900 ms/frame (91.7 FPS), and Showcase 14.098 ms/frame (70.9 FPS), with stable hashes `0412d9ae`, `a5e05931`, and `b5979fc2`; timing is a short local sample, not a cross-platform guarantee |
| SDL desktop, Logitech F310, two-player input, and procedural audio | Code-complete SDL2 host on the same laptop | BUILDS | Automated input normalization, camera, settings migration, and silent-device paths pass; the 65-point workbook still requires an interactive X/D-mode, audio, display, and couch-play pass before promotion to VERIFIED |
| NVIDIA GTX 1660 Ti acceleration | GPU is present in the laptop but v0.0.2 still uploads a CPU-rendered SDL texture | PLANNED | No GPU backend, driver-performance result, or GPU acceleration claim is made for this candidate |

These rows describe a development candidate, not a tagged release. RFC review,
manual QA, a clean packaged soak, and governance checks remain release gates.

## v0.0.1 evidence

| Surface | Exact environment | State | Evidence and boundary |
|---|---|---|---|
| Strict portable core and headless tests | Ageless Linux 0.1.1 (Linux Mint 22.3 base), Linux 6.17.0-35 x86-64; Intel Core i7-10750H (6 cores/12 threads); GCC/G++ 13.3.0; CMake 3.28.3; Cargo 1.95.0 | VERIFIED | Extension-free strict C89/C++98 configure/build, eight native CTests, Rust workspace tests, and both deterministic headless proofs complete on CPU |
| Optional NASM contract probe | Same Linux System V x86-64 host; NASM 2.16.01; `VOX_BUILD_NASM_ACCEL=ON` | VERIFIED | Selected FNV-1a leaf matches the C89 oracle for known vectors, invalid input, and every prefix length 0..1024; this is dispatch evidence, not simulation/renderer acceleration or a speed claim |
| SDL2 software demo smoke | Same host; system SDL2 2.30.0; CPU RGB24 renderer | VERIFIED | Opt-in SDL2 build and fixed non-windowed smoke scenario complete and produce a nonempty `320 x 200` PPM; canonical hashes are recorded only from the frozen release commit |
| SDL2 interactive desktop host | Same host and CPU renderer | BUILDS | Window/menu/input/texture/audio path compiles; the current non-windowed verification environment cannot complete the manual display/audio checklist in `docs/DEMO.md` |
| Lightfield CPU benchmark | Same host; Compatibility/Balanced/Showcase tiers | VERIFIED | Deterministic benchmark mode completes for all three tiers and reports distinct stable frame hashes; timing is machine/load/power specific and is not a cross-platform guarantee |
| NVIDIA GTX 1660 Ti GPU path | Laptop is reported to contain this GPU, but the verification environment has no `/dev/dri` and `nvidia-smi` cannot communicate with a driver | PLANNED | No GPU backend exists in v0.0.1, SDL2 presents a CPU-rendered texture, and no device, driver, VRAM, or GTX performance claim is made |

“VERIFIED” above applies only to the named command surface and evidence. It does
not promote untested input/audio hardware, a different compiler, another Linux
distribution, or a GPU path.

The candidate's commands, deterministic hashes, measured CPU Lightfield
sample, and manual boundary are recorded in
[v0.0.1 Linux x86-64 acceptance evidence](../releasing/V0.0.1_LINUX_EVIDENCE.md).

## Evidence-gated adapter tiers

| Tier | Intended systems | Required shape | v0.0.1 status |
|---|---|---|---|
| A: Desktop SDL2 | Current Linux, Windows, and macOS | C89/C++98 core, CPU renderer, SDL2 host; optional future GPU adapter | Linux x86-64 only; other OS adapters PLANNED |
| B: Legacy 32-bit | Windows XP/7, older i686 Linux, older Intel macOS | Same scalar oracle, reduced presentation/world profile as measured, period-appropriate host API | PLANNED |
| C: Retro native | DOS, Windows 3.1/95/98/NT4/2000, period Linux, classic Mac 68k/PPC | Explicit fixed-width type policy, reduced bounded profile, software framebuffer, platform input/audio/timer adapter | PLANNED |
| D: Modern accelerated | Cross-generation discrete/integrated GPUs | Renderer-neutral snapshot plus OpenGL/D3D/Metal or compute adapter; software fallback | PLANNED |

A tier is a test policy, not one code path forced onto every machine. Older
ports may lower world dimensions, color depth, Lightfield passes, bot count, or
presentation rate when the profile is versioned and does not silently claim
canonical equivalence with a different profile. Hardware-specific NASM is an
optional measured optimization inside a tier, never the only correctness path.

## Future compatibility lab

QEMU is the planned automation layer for reproducible guest installation,
build, boot, headless hashes, and basic framebuffer/input smoke across CPU and
OS families. 86Box is the planned period-PC layer for chipset-, BIOS-, bus-,
sound-, and display-adapter behavior that a generic virtual machine does not
model faithfully. Native hardware remains the final performance and
device-driver authority.

Neither emulator is a v0.0.1 build/runtime dependency. Do not commit firmware,
ROMs, installation media, product keys, or mutable VM disks. A lab record names
the emulator and version, machine definition, firmware/media origin and hashes,
guest patches, clock/speed controls, configuration, and commands needed to
reproduce the result. Only redistributable fixtures may accompany the source.

## Platform evidence record

Before changing a row to `VERIFIED`, attach or publish all applicable fields:

1. exact VOX commit/tag and source archive identity;
2. OS/distribution/version, kernel, architecture, CPU, memory, and power state;
3. GPU model, driver, graphics API, and device visibility, or “CPU software”;
4. compiler/linker/build-tool versions and exact configuration command;
5. build logs plus strict-language/determinism test results;
6. boot/window, input, audio, fullscreen, and clean shutdown result;
7. smoke state/frame hashes and match-completion result;
8. benchmark command, scene, frame count, raw timing, and variance; and
9. known limitations and any reduced simulation/presentation profile.

Compile evidence alone is `BUILDS`. Emulator evidence is labeled as such and
does not substitute for native-hardware performance evidence.

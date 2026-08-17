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

## v0.0.5 pre-release source snapshot

This is local source evidence collected before the v0.0.5 release-foundation
candidate. It is not a published release or a replacement for the carried
human-QA lanes below. The exact release candidate must recapture the relevant
automated, package, hosted, and manual evidence.

| Surface | Exact environment | State | Evidence and boundary |
|---|---|---|---|
| Strict portable core and current gameplay increment | Current local checkout; strict C90 CMake build | RUNS | `ctest --test-dir build-sdl-v005 --output-on-failure` completed 31/31, including the 479-second deterministic match regression, fluid/rigid/cluster gates, wide-cascade and fixture-overflow scenarios, dropship start safety, particle overlay, and cosmetic art determinism. This is native Linux source evidence only. |
| SDL deterministic load | Current local checkout; clean strict SDL2 builds at `-O0` and `-O2` | RUNS | Both optimisation levels reproduced the 600-tick stream including the interactive dropship start: fired 23, explosions 16, crushes 0, effects 978, awake 4616, canonical hash `c53b59d9`. No timing threshold was applied. |
| Memory/hash boundary | Controlled load and cross-session process lanes | RUNS | The controlled 600-tick load test starts from a canonical snapshot and must ignore port wall-clock metadata. A match opened from carried memory intentionally has a different opening state and may have a different canonical hash; that is not a desync. |
| SDL settings and menu migration | Current local SDL2 host | RUNS | Settings, input, bark, chronicle, camera, fixed-step, cap, haptic, audio-cadence, smoke, particle overlay, dropship-start, and menu self-tests pass. The menu gate now renders and hit-tests 12 screens, including both Input & Controller pages; this is not physical controller, display, or audio acceptance. |
| Current-source test package | Explicitly allowed dirty local checkout; clean Release+NASM package build | RUNS | Package CTest completed 31/31. The temporary package produced binary and Corresponding Source archives, validated SPDX SBOMs and SHA-256 checksums, and a 560,087-byte staged payload (37.9% of the 1,474,560-byte ceiling; 914,473 bytes headroom). Its historical feedback-test name makes it non-release evidence, not an artifact to publish. The package was made before this evidence-row wording refresh. |
| Linux and Windows package preflight | Checked-in GitHub Actions workflows | PLANNED | Both platforms build release-shaped `-ci` tester artifacts without publishing. They must be green for the exact candidate, but hosted output still does not replace fresh-package manual acceptance. |
| Current manual/release acceptance | No human or release-package evidence | PLANNED | The dam, magma, blood, cave-in, grapple, bot-tunnelling, replay, dropship, platform-performance, source-archive, SBOM, and checksum lanes still require a clean release candidate and recorded human acceptance. Carried human QA remains separate and blocking for public publication. |

## v0.0.4 development-candidate evidence

> **Erratum:** the historical "Saved history excluded" row records a controlled
> load-test result, not a claim that carried memory never reaches the match
> hash. Carried memory is an intentional simulation input; only wall-clock
> metadata must stay out. See `docs/releasing/V0.0.4_SUPERSESSION.md`.

| Surface | Exact environment | State | Evidence and boundary |
|---|---|---|---|
| Strict portable core, ABI 10, and deterministic match | Linux Mint/Ageless Linux x86-64; Intel Core i7-10750H; GCC 13.3 and Clang 18.1; CMake 3.28 | RUNS | Working-tree `./dev.sh check` passes 22/22 with `vox_headless` at `3ca4d8b0` and `digs_headless` at `43c03b9d`. v0.0.4 is ISO C only: the Rust and C++ boundaries of previous releases are gone, so no cargo lane appears in this table. Clean-package and hosted-platform evidence remain separate gates. |
| Optimisation invariance | Same laptop; GCC 13.3 at `-O0`, `-O1`, `-O2` and `-Os` | RUNS | All four optimisation levels reproduce `vox_headless=3ca4d8b0` and `digs_headless=43c03b9d`. This is a determinism result, not a speed result. |
| SDL2 visible-region software host and device-clock audio | Same laptop; SDL2 2.30; CPU RGB renderer | RUNS | Non-windowed smoke passes at state `845478cb` and frame `2604ef6a`; input, cap, audio cadence, bark, chronicle, menu, settings, camera-detail, and fixed-step debt self-tests pass. This does not promote the interactive display, audible output, or physical controller paths beyond their manual gates. |
| Four-miner deterministic load | Portable SDL2 non-windowed host; 600 authoritative ticks | RUNS | Working-tree verification reproduces fired 43, explosions 16, crushes 1, effects 474, awake 10754, and state hash `1acec253`. These counters differ from v0.0.3 principally because bots now breach walls; a v0.0.3 expectation applied here reads as a failure and is not one. No wall-clock assertion is applied, so this is a correctness/load result rather than a speed claim. |
| Saved history excluded from the canonical hash | Same host; pref paths with and without an accumulated chronicle | RUNS | `--load-self-test 600` prints `1acec253` regardless of how much history the pref path holds, and `test_the_clock_stays_out_of_the_hash` fails if a wall-clock value re-enters the digest. This is the determinism boundary RFC 0003 rests on; VOX-QA-094 is its human lane. |
| Chronicle durability | Same host | RUNS | `--chronicle-self-test` round-trips a chronicle and refuses a damaged one by checksum. Recovery from a genuinely corrupted file on disk is VOX-QA-092 and is not automated. |
| Menu and window layout | Same host; 12 screens | RUNS | `--menu-self-test` walks every screen and fails on content drawn outside its frame. Scrollbar behaviour, wrapping and colour coding are judged by eye in VOX-QA-101 and 102. |
| Playable payload budget | Staged Release tree | RUNS | 505,735 bytes, 34.2% of the 1,474,560-byte ceiling, 968,825 bytes of headroom. The budget covers the binary plus the staged `share/` tree; documentation, licences and evidence are tester-archive material and excluded. |
| Four-miner destruction performance | Named bench: i7-10750H in `power-saver`; GTX 1660 Ti laptop; CPU-authoritative simulation | RUNS | Clean-tree package qualification with `VOX_NAMED_BENCH_QUALIFY=1` records avg 3.074 ms, p95 5.287 ms, max 9.620 ms against the 5 / 8 / 16.67 ms gate, with canonical activity and hash `1acec253`. Five further runs on the same bench held every limit (avg 2.927-3.439, p95 4.999-6.792, max 7.518-13.171), so `max` is the variable metric and the narrowest margin. Recorded with ordinary desktop work running, which makes the result conservative rather than flattering. This is a 600-tick qualification, not the 15-minute soak. |
| Bot memory across sessions | Same host; three separate processes over one chronicle file | RUNS | `tools/vox-session-evidence.sh` plays a match in one process, writes the chronicle, and opens a match from it in another. The carried accounts arrive intact (regard total 592, three pairs met), the second match opens at valence -136 where a first meeting opens at 0, and its state hash differs from a first meeting's -- so memory reaches the simulation rather than merely being stored. A missing or checksum-failed chronicle is refused rather than reported as a first meeting, which would pass the carry test for the wrong reason. Verified by reintroducing the defect: a build that ignores loaded memory fails exactly the two load-bearing assertions. This proves the mechanism, NOT that any of it reads as a grudge; VOX-QA-089 through 093 remain the human evidence and are still unrecorded. |
| NVIDIA GTX 1660 Ti rendering | Same laptop | PLANNED | v0.0.4 remains a CPU renderer presented by SDL2. GPU presence is test-bench context, not evidence of graphics acceleration. |

These rows preserve the v0.0.4 development-candidate record.
The v0.0.4 release checklist, a clean package, the QA workbook, and physical
controller/audio evidence remain authoritative promotion gates.

One gap is deliberately still open. The cross-session row proves the
mechanism and nothing about how it feels: a number carried across a relaunch
is not a grudge a player notices. With no relationship UI by design, whether
the miners read as characters is carried entirely by the writing and the
pacing, and only a person can say. That is what `qa/V0.0.4-QUICK-FEEDBACK.txt`
sections 2, 5 and 8 are for, and no automated result substitutes for them.

## v0.0.4 test lanes

| Lane | Required commands or artifact | What a successful result can prove | What it cannot prove |
|---|---|---|---|
| Linux x86-64 full candidate | `tools/vox-verify.sh`, clean `tools/package-linux-demo.sh`, package evidence, and `qa/V0.0.4-QUICK-FEEDBACK.txt` followed by the full workbook | Strict scalar/NASM build parity, automated host behavior, packaged data/source identity, the size budget, and the explicitly observed laptop/device paths | GPU acceleration, bot memory across sessions, or any untested controller, transport, OS, driver, display, or historical target |
| Conversation and memory | `qa/V0.0.4-QUICK-FEEDBACK.txt` sections 2, 5 and 8 with logs attached | That the miners carry attitudes across a relaunch, that pacing and interruption read correctly by ear, and that the Options reset does what it says | Anything about a different seed, personality mix, or session history than the one recorded |
| Windows x64 portable core | `portable-core / Windows x64 MSVC` Actions job | Compile/link plus native CTest for the scalar/headless boundary on the named hosted image | SDL2 gameplay host, Win32 input/audio/haptics, installer, historical Windows, or physical performance |
| macOS Intel portable core | `portable-core / macOS 15 Intel Clang` Actions job | Compile/link plus native CTest for the scalar/headless boundary on the named hosted image | SDL2/Quartz gameplay host, CoreAudio, controllers, app bundle, Apple Silicon, or physical performance |
| Linux i686 portability probe | `portable core / Linux i686 multilib` Actions job | 32-bit compile/link and test execution for the portable scalar/headless boundary | A historical distribution, 32-bit SDL host, memory-budget fitness, period drivers, or period CPU speed |

The portable-core matrix is now the whole of the ISO C portability claim.
Through v0.0.3 a Rust boundary and a C++98 translation unit were also
exercised; v0.0.4 has neither, so these three lanes are the only mechanical
evidence that the core compiles and runs as ISO C90 off this laptop.

## v0.0.3 development-candidate evidence

| Surface | Exact environment | State | Evidence and boundary |
|---|---|---|---|
| Strict portable core, ABI 9, and deterministic match | Linux Mint/Ageless Linux x86-64; Intel Core i7-10750H; GCC/G++ 13.3 and Clang 18.1; CMake 3.28; Rust/Cargo host boundary | RUNS | Frozen-tree strict GCC/NASM and Clang/scalar suites pass 20/20 with match hash `5b3ceef6`; full ASan and UBSan suites pass 20/20 with leak detection disabled only for the ptraced ASan environment; Rust and headless proofs pass. Clean-package and hosted-platform evidence remain separate gates. |
| SDL2 visible-region software host and device-clock audio | Same laptop; SDL2 2.30; CPU RGB renderer | RUNS | Non-windowed smoke passes at state `57987c09` and frame `71e7fb86`; input, cap, audio cadence, bark/G2P, haptic mixer, settings, camera-detail, and fixed-step debt self-tests pass. This does not promote the interactive display, audible output, or physical controller paths beyond their manual gates. |
| Nintendo Switch Pro Controller | USB and Bluetooth through SDL GameController/raw fallback | PLANNED | The accepted test surface includes correct physical prompts and Off/Low/Normal/Heavy standard SDL low/high-motor feedback. Neither transport is VERIFIED until a tester records mapping and vibration evidence. |
| Four-miner deterministic load | Portable SDL2 non-windowed host; 600 authoritative ticks | RUNS | Frozen-tree verification reproduces fired 20, explosions 16, crushes 0, effects 1153, awake 6975, and state hash `c37e44b6`. (Corrected during v0.0.4 planning; the originally recorded `f3924b81` predates commit `47c109c`, which changed simulation behaviour before the v0.0.3 tag. See the erratum in `docs/releasing/V0.0.3_RELEASE_CHECKLIST.md`.) The default CTest, verifier, cockpit, package, and hosted CI paths apply no shared-host wall-clock assertion, so this is a correctness/load result rather than a speed claim. |
| Four-miner destruction performance | i7-10750H in `power-saver`; GTX 1660 Ti laptop, CPU-authoritative simulation | BUILDS | `--performance-self-test 600` alone retains the 5 ms average/8 ms p95/16.67 ms maximum gate. Package evidence captures it only with `VOX_NAMED_BENCH_QUALIFY=1`; final frozen-tree qualification and 15-minute audio results are still required. |
| NVIDIA GTX 1660 Ti rendering | Same laptop | PLANNED | v0.0.3 remains a CPU renderer presented by SDL2. GPU presence is test-bench context, not evidence of graphics acceleration. |
| Windows x64 portable core CI | GitHub-hosted Windows Server 2022; MSVC; scalar core; SDL2 and NASM disabled | RUNS | Main commit `4869cbe` configures, compiles, links, and completes native CTest plus Cargo in [foundation-ci run 30359668668](https://github.com/claire-moon/vox/actions/runs/30359668668). This excludes the SDL window, audio, input, haptics, installer, historical Windows, and player-perceived performance. |
| macOS Intel portable core CI | GitHub-hosted `macos-15-intel`; Clang; scalar core; SDL2 and NASM disabled | RUNS | Main commit `4869cbe` configures, compiles, links, and completes native CTest plus Cargo in [foundation-ci run 30359668668](https://github.com/claire-moon/vox/actions/runs/30359668668). This is not Apple Silicon, Quartz, audio, controller, app-bundle, or physical-performance evidence. |
| Linux i686 portable core CI | Ubuntu 24.04 x86-64 host using GCC/G++ multilib and Rust i686 target; 32-bit process | RUNS | Main commit `4869cbe` compiles and executes the scalar C/C++ tests plus Rust boundary as an i686 process in [foundation-ci run 30359668668](https://github.com/claire-moon/vox/actions/runs/30359668668). This does not imply Windows XP, a period Linux distribution, 32-bit SDL, or low-spec performance support. |

These rows are implementation-candidate boundaries. The v0.0.3 release
checklist, clean package, QA workbook, and physical controller/audio evidence
remain authoritative promotion gates.

## v0.0.3 test lanes

| Lane | Required commands or artifact | What a successful result can prove | What it cannot prove |
|---|---|---|---|
| Linux x86-64 full candidate | `tools/vox-verify.sh`, clean `tools/package-linux-demo.sh`, package evidence, and `qa/V0.0.3-QUICK-FEEDBACK.txt` followed by the full workbook | Strict scalar/NASM build parity, automated host behavior, packaged data/source identity, and the explicitly observed laptop/device paths | GPU acceleration or any untested controller, transport, OS, driver, display, or historical target |
| Windows x64 portable core | `portable-core / Windows x64 MSVC` Actions job | Compile/link plus native CTest and Rust results for the scalar/headless boundary on the named hosted image | SDL2 gameplay host, Win32 input/audio/haptics, installer, historical Windows, or physical performance |
| macOS Intel portable core | `portable-core / macOS 15 Intel Clang` Actions job | Compile/link plus native CTest and Rust results for the scalar/headless boundary on the named hosted image | SDL2/Quartz gameplay host, CoreAudio, controllers, app bundle, Apple Silicon, or physical performance |
| Linux i686 portability probe | `portable core / Linux i686 multilib` Actions job | 32-bit compile/link and test execution for the portable scalar/headless boundary | A historical distribution, 32-bit SDL host, memory-budget fitness, period drivers, or period CPU speed |
| Historical QEMU/86Box/native | Versioned future lab record with legal media/firmware provenance, exact machine configuration, hashes, boot/input/audio/display results, and native follow-up | Only the exact recorded guest/emulator or native machine | Any other historical system or a blanket 1990-to-now claim |

The quick-feedback guide deliberately tells testers which workbook IDs to fill
for collision/teleport, swept damage and dismemberment, debris towers, rope,
rail, steampack, close zoom, audio cadence, cap qualification, Laptop Mode,
controller families/haptics, two-player bots, and deterministic hashes. A quick
pass finds regressions; it does not satisfy the full release checklist.

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

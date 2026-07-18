<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Contributing to VOX + DIGS

VOX + DIGS is an upstream-first GPL project. The goal is to make improvements
valuable to share: portable scalar behavior is reviewed in public, optimized
backends compete on measured evidence, and reusable work returns to the common
engine instead of becoming an opaque platform fork.

## License, copyright, and sign-off

Contributions are accepted under `GPL-3.0-or-later`, the repository's license.
Contributors retain copyright in their work. The project does not require a
Contributor License Agreement and does not ask contributors to assign
copyright or grant a private relicensing right.

Every commit must carry a
[Developer Certificate of Origin](https://developercertificate.org/) sign-off:

```sh
git commit -s
```

The `Signed-off-by` line certifies that you have the right to submit the work
under the project's license. Do not submit proprietary assets, ripped game
content, secrets or access tokens, ROMs, VM images, generated runtime state,
or code whose provenance you cannot establish.

Third-party source and assets must retain their original notices and be entered
in `THIRD_PARTY.md` before merge. New dependencies need exact version/source,
license compatibility, modifications, distribution mode, and release
obligations. “Open source” alone is not a sufficient license review.

## Change workflow

1. Start with an issue for a bug, bounded feature, port, or measurement.
2. Use a short-lived branch from `main` and keep the change reviewable.
3. Add the smallest deterministic test that distinguishes the new behavior.
4. Run the relevant local gates and include exact commands/results in the pull
   request. Do not label a platform supported from a compile alone.
5. Sign every commit and open a pull request using the repository template.
6. Address review in the branch; maintainers squash only when authorship,
   sign-off, and useful history remain clear.

An RFC is required before changing the C ABI, canonical hashes, rules/replay or
data-pack formats, determinism policy, world profile, dependency policy,
support tiers, licensing, or governance. An RFC should define the problem,
constraints, rejected alternatives, compatibility/migration plan, acceptance
evidence, and rollback path. Design discussion is not an implementation claim.

## Local verification

The default build proves that the core remains useful without SDL2:

```sh
cmake -S . -B build -DVOX_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cargo test --workspace
```

When SDL2 2.0.10 or newer is installed, exercise the complete demo gate:

```sh
VOX_BUILD_DIR="$PWD/build-demo" tools/vox-verify.sh
```

On Linux x86-64 the script enables the optional NASM probe when `nasm` is
installed. Run once with `VOX_NASM_ACCEL=OFF` when changing its scalar fallback
or dispatch boundary.

For renderer or performance work, also capture a reproducible baseline and
candidate result:

```sh
./build-demo/digs_demo --benchmark 240
./build-demo/digs_demo --smoke-test /tmp/digs-demo-smoke.ppm
```

Record OS, CPU, compiler, build type, SDL version, power state, command, frame
count, hashes, and raw output. A benchmark change must preserve deterministic
tests and explain warm-up, measurement, and variance. One machine's faster
result is evidence for that machine, not a universal performance claim.

Interactive host changes also check title/setup/options/pause/results flow,
keyboard and mouse input, all ten weapons, bots, damage/respawn, materials,
lava, audio fallback, fullscreen, debug overlay, and every frame cap.

## Engineering contracts

- C89 owns authoritative voxel/material/game rules. Avoid implementation-
  defined behavior, hidden allocation, floating-point state, and unbounded work
  in the deterministic path.
- C++98 may implement isolated systems behind the versioned C ABI. C++ types,
  exceptions, and RTTI must not cross that boundary.
- Rust hosts or tools consume the C ABI and must not duplicate authoritative
  rules.
- Scalar code is the oracle. NASM, intrinsics, worker, GPU, and platform
  specializations are optional adapters with a scalar fallback and equivalence
  tests.
- The v0.0.1 NASM FNV-1a leaf is a build/ABI/parity probe only. Do not describe
  it as simulation, renderer, or whole-game acceleration; profile first before
  proposing another specialized leaf.
- Presentation frame rate, Lightfield tier, audio, window state, and debug UI
  must not alter the fixed 60 Hz state sequence or canonical hash.
- Fixed pools must define deterministic capacity behavior. Never make gameplay
  depend on allocator timing or an unordered container.
- A port owns presentation and platform services, not a divergent game fork.
  Keep host-specific code behind narrow interfaces.

Use `apply_patch`-sized changes when possible, preserve unrelated work, and
never weaken a strict-language or deterministic test merely to make a backend
pass.

## Recognition and responsibility

Merged contributors receive authorship in Git history and credit in release
notes for substantive work. Review, testing on uncommon hardware, reproducible
bug reports, documentation, accessibility, and license/provenance work count as
first-class contributions alongside code.

Sustained contributors may earn triage, review, subsystem ownership, and
maintainer responsibility through demonstrated care for users, deterministic
contracts, portability evidence, licensing, and other contributors. Authority
follows accountable work and can be reduced after inactivity or misuse; it is
not purchased, tied to employment, or conditioned on copyright assignment.

See `docs/GOVERNANCE.md` for the public proposal-to-release loop, decision
rules, credits, and conflict handling.

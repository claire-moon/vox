<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# v0.0.1 release checklist

## Source and reproducibility

- [ ] Release candidate is a reviewed commit on protected `main`; worktree is clean.
- [ ] Versioned C ABI, map generator, rules, canonical hash inputs, and known schema limits are documented.
- [ ] `tools/vox-verify.sh` passes from a clean checkout with strict C89/C++98 flags.
- [ ] Default `VOX_BUILD_SDL2_DEMO=OFF` headless configure/build/test passes without SDL2.
- [ ] Opt-in `VOX_BUILD_SDL2_DEMO=ON` configure/build/test passes with SDL2 2.0.10 or newer.
- [ ] Default `VOX_BUILD_NASM_ACCEL=OFF` C89 FNV-1a fallback passes.
- [ ] Opt-in Linux x86-64 NASM leaf passes known vectors and scalar parity for lengths 0..1024; no renderer/game speed claim is inferred.
- [ ] Native CTests, Rust workspace, `vox_headless`, and `digs_headless` pass twice with matching deterministic output.

## Graphical demo

- [ ] `digs_demo --smoke-test` completes from a non-windowed environment, writes a nonempty `320 x 200` PPM, and repeats state/frame hashes.
- [ ] `digs_demo --benchmark 240` completes for Compatibility, Balanced, and Showcase; raw output and exact host evidence are archived.
- [ ] Title, match setup, Foundry Lab, options, pause, and results screens work.
- [ ] Bot counts 0-3, all three maps, visible seed editing/randomization, and Full Works/Miner Kit/Powder Keg arsenals work.
- [ ] Human movement/jump/steam, deterministic bot combat, health, damage, deaths, scoring, respawn, and match end work.
- [ ] All ten keyboard weapon selections, mouse wheel, mouse aim, cooldowns, projectile capacity, and effects work.
- [ ] Terrain destruction, sand/liquid/gas movement, water/lava stone-and-steam reaction, ignition/firedamp, and physical rising-lava damage are visible.
- [ ] Frame caps 30/60/90/120/144/Unlimited preserve the fixed 60 Hz simulation sequence.
- [ ] All Lightfield tiers are visibly distinct; transient miner/projectile/effect rendering does not change authoritative hashes.
- [ ] Debug overlay, restart, pause/resume, fullscreen transition, resize/letterbox mapping, and clean quit work.
- [ ] Procedural audio works where a device exists and fails silently without changing gameplay where it does not.

## Compatibility and licensing

- [ ] Linux CPU acceptance record in `docs/compatibility/MATRIX.md` matches the release machine and frozen commit.
- [ ] GPU/device limitations are explicit; no GTX 1660 Ti or cross-platform performance claim is inferred from CPU software evidence.
- [ ] Each additional platform claim includes build, boot, input, render, match, determinism, and performance evidence.
- [ ] `THIRD_PARTY.md` records system-linked SDL2 provenance, tested version, zlib license, and distribution decision.
- [ ] `THIRD_PARTY.md` records system-tool NASM provenance/version/license when the optional leaf is included.
- [ ] SPDX identifiers, third-party notices, source offer/Corresponding Source, and SPDX SBOM are complete for every distributed archive.
- [ ] No credentials, proprietary assets, ROMs/VM images, build directories, generated smoke images, or unreviewed binaries are committed.
- [ ] DCO sign-offs and contributor/reviewer credits are complete; no CLA or copyright assignment is requested.

## Publication

- [ ] Known limitations match `docs/DEMO.md`; deferred GPU, networking, large-world, rigid-body, and historical-adapter work is not described as complete.
- [ ] Source archive and compiled game payload reproduce from the frozen tree/toolchain and include GPL license/notices; any intentional archive variance from preserved raw timing evidence is documented and the published artifact is checksummed.
- [ ] `SHA256SUMS` is generated from final artifacts and checked independently.
- [ ] `v0.0.1-rc.1` receives a clean-checkout soak and manual demo pass.
- [ ] Final release notes include exact smoke hashes, benchmark context, compatibility state, contributors, and upgrade/ABI warning.
- [ ] Signed `v0.0.1` tag is created from the accepted protected-`main` commit only after every applicable item above passes.

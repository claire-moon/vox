<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
## Outcome and scope

<!-- What user-visible or engineering outcome changed? What is explicitly deferred? -->

## Contract impact

- [ ] No C ABI/canonical hash/rules/map/replay/profile change
- [ ] Determinism and fixed-capacity behavior reviewed
- [ ] Presentation remains independent of the fixed 60 Hz simulation
- [ ] Scalar fallback/oracle remains available for any optimized path
- [ ] NASM/SIMD claim is limited to the exact tested leaf and does not imply whole-game acceleration

<!-- Link the accepted RFC when any item above changes. -->

## Validation evidence

- [ ] Default headless build succeeds without SDL2
- [ ] `tools/vox-verify.sh`
- [ ] `digs_demo --smoke-test /tmp/digs-demo-smoke.ppm` repeated with matching hashes
- [ ] Interactive demo checklist completed when host/input/render/audio changed
- [ ] Before/after `digs_demo --benchmark` evidence attached when performance is claimed
- [ ] Exact OS/CPU/GPU-or-software/compiler/SDL/build context recorded for a platform claim

<!-- Paste concise command results or link durable raw evidence. Never paste credentials. -->

## License and provenance

- [ ] Original work is submitted under `GPL-3.0-or-later`
- [ ] Dependency/asset/source provenance and GPL compatibility checked
- [ ] `THIRD_PARTY.md`, notices, and SBOM inputs updated where required
- [ ] No proprietary content, secrets, generated runtime state, ROM, or VM image included

## Contributor sign-off

By submitting this pull request, I certify that every commit I contributed
carries the Developer Certificate of Origin sign-off (`git commit -s`). VOX +
DIGS does not require a Contributor License Agreement or copyright assignment.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Contributing to VOX + DIGS

Use a short-lived branch from `main`, keep changes focused, and include the
smallest reproducible test for every behavior change. The C89 kernel is the
portable simulation authority; Rust host code must not duplicate simulation
rules; optimized NASM/SIMD paths require a scalar oracle.

Before opening a pull request:

```text
cmake -S . -B build -DVOX_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
cargo test --workspace
```

Sign commits with `git commit -s` to certify the Developer Certificate of
Origin. Do not submit proprietary assets, ripped game content, secrets, VM
images, ROMs, generated runtime state, or code without provenance.

RFCs are required for ABI changes, rules/replay formats, determinism,
platform support tiers, licensing, and governance. Every dependency or
vendored file must be recorded in `THIRD_PARTY.md` before merge.

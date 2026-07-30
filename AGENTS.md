# VOX repository instructions

## Scope

This is the independent VOX + DIGS repository. Do not copy PPD-COM runtime
state, private documents, credentials, generated logs, VM images, ROMs, or
unreviewed assets into this tree.

## Branches

- `main` is protected and releasable.
- Use short-lived `feat/`, `fix/`, `perf/`, `port/`, and `docs/` branches.
- Require review/checks before merging; never force-push `main` or release tags.

## Language boundaries

- v0.0.4 is ISO C only. Every first-party translation unit is strict C89.
- `engine/c89` is strict C89 and OS-independent. No C++, no Rust, no Lua.
- A port owns presentation and platform services. Third-party host libraries
  (SDL2) are permitted there; they are not first-party code.
- NASM is optional x86 leaf optimization with a scalar C oracle.

## Validation

```text
cmake -S . -B build -DVOX_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check
```

Every optimized path must match the scalar state hash. Every distributed
binary must have corresponding source, notices, an SPDX SBOM, and a recorded
compatibility/performance result.

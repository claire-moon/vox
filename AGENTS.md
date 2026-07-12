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

- `engine/c89` is strict C89 and OS-independent.
- `engine/cpp98` is strict C++98 behind the C ABI; no exceptions, RTTI, or
  STL types cross the ABI.
- Rust is for modern host/tools and never owns authoritative simulation state.
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

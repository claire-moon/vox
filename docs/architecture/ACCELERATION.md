<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Optional acceleration contract

VOX never requires assembly for correctness. The scalar C89 implementation is
the oracle and every specialized leaf must have the same input validation,
result, capacity behavior, and test vectors.

`v0.0.1` includes one deliberately small contract probe:

- `vox_fnv1a32_scalar` implements a 32-bit FNV-1a byte hash in strict C89;
- `vox_fnv1a32` is the public selector and uses the scalar function by default;
- `vox_fnv1a32_nasm` is an optional NASM implementation for the Linux System V
  AMD64 ABI; and
- `vox_accel_nasm_enabled` reports which implementation was selected at build
  time.

The NASM leaf uses only baseline x86-64 integer instructions. The build does
not perform runtime CPUID dispatch because the selected instructions do not
need an optional feature bit. The function is a leaf, allocates nothing, and
does not call into C. Null input is valid only with zero length; invalid input
is rejected by the C selector before the assembly boundary.

Configure it explicitly with:

```sh
cmake -S . -B build-nasm \
  -DVOX_BUILD_TESTS=ON \
  -DVOX_BUILD_NASM_ACCEL=ON
cmake --build build-nasm --parallel
ctest --test-dir build-nasm --output-on-failure
```

The option is accepted only for Linux x86-64 with an ELF64-capable NASM
assembler. Unsupported systems fail configuration when the option is forced;
leaving it `OFF` always builds the scalar selector. `tools/vox-verify.sh`
selects it automatically only when all three conditions are visible, and
`VOX_NASM_ACCEL=OFF` forces scalar acceptance.

The parity test checks standard FNV-1a vectors, invalid input, and every prefix
length from 0 through 1024 bytes against the scalar oracle. The regular test
target runs in both configurations, so the fallback is not an untested code
path.

This probe is not used by the canonical world/match hashes and is not evidence
that the renderer, material step, physics, or whole game became faster. It
exists to prove the source layout, C ABI, conditional build, linker hygiene,
fallback, and parity policy before profiling identifies a meaningful hot leaf.

A future accelerated routine must additionally provide:

1. a measured scalar hotspot and representative benchmark;
2. a caller-visible alignment/overlap/length contract;
3. scalar-specialized equivalence across edge and randomized vectors;
4. deterministic behavior on every supported CPU and optimization level;
5. runtime feature dispatch when instructions exceed the platform baseline;
6. no illegal-instruction path when detection or linkage is unavailable; and
7. before/after evidence that includes the full workload, not only a favorable
   microbenchmark.

Assembly source is original VOX code under `GPL-3.0-or-later`. NASM itself is a
system build tool under the two-clause BSD license and is neither linked into
nor distributed with VOX; its provenance is recorded in `THIRD_PARTY.md`.

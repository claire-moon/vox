#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR=${VOX_BUILD_DIR:-/tmp/vox-verify-build}
CARGO_TARGET_DIR=${VOX_CARGO_TARGET_DIR:-/tmp/vox-cargo-target}

cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DVOX_BUILD_TESTS=ON \
    -DCMAKE_C_FLAGS="${VOX_C_FLAGS:--std=c89 -pedantic-errors -Wall -Wextra -Werror}" \
    -DCMAKE_CXX_FLAGS="${VOX_CXX_FLAGS:--std=c++98 -pedantic-errors -Wall -Wextra -Werror -fno-exceptions -fno-rtti}"
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure
CARGO_TARGET_DIR="$CARGO_TARGET_DIR" cargo test --manifest-path "$ROOT/Cargo.toml" --workspace
"$BUILD_DIR/vox_headless"
"$BUILD_DIR/digs_headless"

#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR=${VOX_BUILD_DIR:-/tmp/vox-verify-build}
CARGO_TARGET_DIR=${VOX_CARGO_TARGET_DIR:-/tmp/vox-cargo-target}
SMOKE_IMAGE=${VOX_SMOKE_IMAGE:-/tmp/vox-digs-demo-smoke.ppm}
MINER_ICON=${VOX_MINER_ICON:-/tmp/vox-digs-miner.xpm}
NASM_ACCEL=${VOX_NASM_ACCEL:-AUTO}
BUILD_JOBS=${VOX_BUILD_JOBS:-}

if [ "$NASM_ACCEL" = AUTO ]; then
    NASM_ACCEL=OFF
    if [ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ] &&
        command -v nasm >/dev/null 2>&1; then
        NASM_ACCEL=ON
    fi
fi
case "$NASM_ACCEL" in
    ON|OFF) ;;
    *)
        echo "VOX_NASM_ACCEL must be AUTO, ON, or OFF" >&2
        exit 2
        ;;
esac
case "$BUILD_JOBS" in
    '') ;;
    *[!0-9]*|0|0*) echo "VOX_BUILD_JOBS must be a positive integer" >&2; exit 2 ;;
esac

cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DVOX_BUILD_TESTS=ON \
    -DVOX_BUILD_SDL2_DEMO=ON \
    -DVOX_BUILD_NASM_ACCEL="$NASM_ACCEL" \
    -DCMAKE_C_FLAGS="${VOX_C_FLAGS:--std=c89 -pedantic-errors -Wall -Wextra -Werror}" \
    -DCMAKE_CXX_FLAGS="${VOX_CXX_FLAGS:--std=c++98 -pedantic-errors -Wall -Wextra -Werror -fno-exceptions -fno-rtti}"
if [ -n "$BUILD_JOBS" ]; then
    cmake --build "$BUILD_DIR" --parallel "$BUILD_JOBS"
else
    cmake --build "$BUILD_DIR" --parallel
fi
ctest --test-dir "$BUILD_DIR" --output-on-failure
CARGO_TARGET_DIR="$CARGO_TARGET_DIR" cargo test --manifest-path "$ROOT/Cargo.toml" --workspace
"$BUILD_DIR/vox_headless"
"$BUILD_DIR/digs_headless"
"$BUILD_DIR/digs_demo" --input-self-test
"$BUILD_DIR/digs_demo" --settings-self-test \
    "$BUILD_DIR/digs-settings-self-test.cfg"
"$BUILD_DIR/digs_demo" --camera-self-test
"$BUILD_DIR/digs_demo" --render-miner-icon-xpm "$MINER_ICON"
cmp "$MINER_ICON" "$BUILD_DIR/share/digs/icons/digs-miner.xpm"
"$BUILD_DIR/digs_demo" --smoke-test "$SMOKE_IMAGE"
test -s "$SMOKE_IMAGE"

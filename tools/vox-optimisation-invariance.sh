#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Prove that the authoritative simulation produces bit-identical state
# hashes regardless of optimisation level.
#
# The v0.0.4 size diet wants -Os, -ffunction-sections/--gc-sections, and
# strip.  Those are only safe to adopt if the compiler cannot change the
# result of a match.  The simulation is pure integer Q16.16 with no float,
# no unsequenced side effects, and no reliance on implementation-defined
# negative division (vox_physics_div_trunc_positive and
# digs_div_trunc_positive normalise it), so the hashes should be identical.
# This asserts it rather than assuming it.
#
# Usage:  tools/vox-optimisation-invariance.sh
#
# Environment:
#   VOX_INVARIANCE_DIR    scratch build root (default: a mktemp directory)
#   VOX_INVARIANCE_LEVELS optimisation levels (default: "-O0 -O1 -O2 -Os")
#   VOX_BUILD_JOBS        parallel build jobs
#
# Exit status:
#   0  every optimisation level agreed
#   1  a hash diverged
#   2  usage or environment error
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
LEVELS=${VOX_INVARIANCE_LEVELS:--O0 -O1 -O2 -Os}
BUILD_JOBS=${VOX_BUILD_JOBS:-}

case "$BUILD_JOBS" in
    '') ;;
    *[!0-9]*|0|0*)
        echo "VOX_BUILD_JOBS must be a positive integer" >&2
        exit 2
        ;;
esac

WORK_DIR=${VOX_INVARIANCE_DIR:-}
CLEAN_WORK_DIR=0
if [ -z "$WORK_DIR" ]; then
    WORK_DIR=$(mktemp -d 2>/dev/null) || {
        echo "could not create a scratch directory" >&2
        exit 2
    }
    CLEAN_WORK_DIR=1
fi
cleanup() {
    if [ "$CLEAN_WORK_DIR" = 1 ] && [ -d "$WORK_DIR" ]; then
        rm -rf "$WORK_DIR"
    fi
}
trap cleanup EXIT INT TERM

# Every probe is a headless binary that ends by printing "... hash=xxxxxxxx".
# Extracting only that field keeps the comparison insensitive to the
# activity counters, which are informative but not the contract here.
extract_hash() {
    sed -n 's/.*hash=\([0-9a-f][0-9a-f]*\).*/\1/p'
}

REFERENCE_LEVEL=
REFERENCE_VOX=
REFERENCE_DIGS=
STATUS=0

for level in $LEVELS; do
    build_dir=$WORK_DIR/opt$(echo "$level" | tr -d '-')
    # CMAKE_BUILD_TYPE is deliberately empty: a configured build type would
    # append its own -O flag after CMAKE_C_FLAGS and silently win.
    cmake -S "$ROOT" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE= \
        -DVOX_BUILD_TESTS=OFF \
        -DVOX_BUILD_SDL2_DEMO=OFF \
        -DVOX_BUILD_NASM_ACCEL=OFF \
        -DCMAKE_C_FLAGS="-std=c89 -pedantic-errors -Wall -Wextra -Werror $level" \
        -DCMAKE_CXX_FLAGS="-std=c++98 -pedantic-errors -Wall -Wextra -Werror -fno-exceptions -fno-rtti $level" \
        >"$build_dir.configure.log" 2>&1 || {
            echo "configure failed at $level; see $build_dir.configure.log" >&2
            exit 2
        }
    if [ -n "$BUILD_JOBS" ]; then
        cmake --build "$build_dir" --target vox_headless digs_headless \
            --parallel "$BUILD_JOBS" >"$build_dir.build.log" 2>&1 || {
                echo "build failed at $level; see $build_dir.build.log" >&2
                exit 2
            }
    else
        cmake --build "$build_dir" --target vox_headless digs_headless \
            --parallel >"$build_dir.build.log" 2>&1 || {
                echo "build failed at $level; see $build_dir.build.log" >&2
                exit 2
            }
    fi

    vox_hash=$("$build_dir/vox_headless" | extract_hash)
    digs_hash=$("$build_dir/digs_headless" | extract_hash)
    if [ -z "$vox_hash" ] || [ -z "$digs_hash" ]; then
        echo "could not read a state hash at $level" >&2
        exit 2
    fi
    printf '%-4s vox_headless=%s digs_headless=%s\n' \
        "$level" "$vox_hash" "$digs_hash"

    if [ -z "$REFERENCE_LEVEL" ]; then
        REFERENCE_LEVEL=$level
        REFERENCE_VOX=$vox_hash
        REFERENCE_DIGS=$digs_hash
        continue
    fi
    if [ "$vox_hash" != "$REFERENCE_VOX" ]; then
        echo "FAIL: vox_headless hash $vox_hash at $level does not match $REFERENCE_VOX at $REFERENCE_LEVEL" >&2
        STATUS=1
    fi
    if [ "$digs_hash" != "$REFERENCE_DIGS" ]; then
        echo "FAIL: digs_headless hash $digs_hash at $level does not match $REFERENCE_DIGS at $REFERENCE_LEVEL" >&2
        STATUS=1
    fi
done

if [ -z "$REFERENCE_LEVEL" ]; then
    echo "no optimisation levels were tested" >&2
    exit 2
fi
if [ "$STATUS" = 0 ]; then
    echo "OK: simulation hashes are invariant across $LEVELS"
fi
exit "$STATUS"

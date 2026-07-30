#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Run the deterministic simulation benchmark and compare it to the
# committed baseline.
#
# Two classes of signal, deliberately treated differently:
#
#   hashes and scenario identity  must match exactly.  A change means the
#                                 authoritative simulation behaves
#                                 differently, which is sometimes correct
#                                 but is never incidental -- it has to be
#                                 re-baselined by a human in the same
#                                 commit that caused it.
#   work counters                 may drift within a tolerance.  Growth
#                                 beyond it means a system started doing
#                                 more work per tick, which is what the
#                                 v0.0.4 performance constraint forbids.
#   cpu_* timings                 recorded, never enforced.  Shared CI
#                                 runners cannot measure speed honestly.
#
# Usage:
#   tools/vox-bench.sh [build-dir]            compare against the baseline
#   tools/vox-bench.sh --update [build-dir]   rewrite the baseline
#
# Environment:
#   VOX_BENCH_TICKS      simulation ticks (default 600)
#   VOX_BENCH_BASELINE   baseline path (default benchmarks/baseline.txt)
#   VOX_BENCH_TOLERANCE  permitted work-counter drift, percent (default 10)
#
# Exit status:
#   0  within tolerance
#   1  a work counter regressed beyond tolerance
#   2  simulation behaviour changed, or a usage/environment error
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
UPDATE=0
if [ "${1:-}" = "--update" ]; then
    UPDATE=1
    shift
fi
BUILD_DIR=${1:-${VOX_BUILD_DIR:-$ROOT/build}}
BASELINE=${VOX_BENCH_BASELINE:-$ROOT/benchmarks/baseline.txt}
TICKS=${VOX_BENCH_TICKS:-600}
TOLERANCE=${VOX_BENCH_TOLERANCE:-10}

case "$TICKS" in
    ''|*[!0-9]*) echo "VOX_BENCH_TICKS must be a positive integer" >&2; exit 2 ;;
esac
case "$TOLERANCE" in
    ''|*[!0-9]*) echo "VOX_BENCH_TOLERANCE must be a non-negative integer" >&2; exit 2 ;;
esac

BENCH_BIN=$BUILD_DIR/vox_bench
if [ ! -x "$BENCH_BIN" ]; then
    echo "vox_bench not found: $BENCH_BIN" >&2
    echo "build it with: cmake --build $BUILD_DIR --target vox_bench" >&2
    exit 2
fi

CURRENT=$(mktemp 2>/dev/null) || { echo "could not create a temp file" >&2; exit 2; }
trap 'rm -f "$CURRENT"' EXIT INT TERM
"$BENCH_BIN" "$TICKS" >"$CURRENT"

if [ "$UPDATE" = 1 ]; then
    mkdir -p "$(dirname "$BASELINE")"
    # Timings are host-specific, so they never enter the committed baseline.
    grep -v '^cpu_' <"$CURRENT" >"$BASELINE"
    echo "baseline updated: $BASELINE"
    exit 0
fi

if [ ! -f "$BASELINE" ]; then
    echo "no baseline at $BASELINE" >&2
    echo "create one with: tools/vox-bench.sh --update $BUILD_DIR" >&2
    exit 2
fi

awk -v tolerance="$TOLERANCE" '
FNR == NR {
    if ($0 ~ /^#/ || $0 !~ /=/) { next }
    key = $0; sub(/=.*$/, "", key)
    value = $0; sub(/^[^=]*=/, "", value)
    base[key] = value
    next
}
{
    if ($0 ~ /^#/ || $0 !~ /=/) { next }
    key = $0; sub(/=.*$/, "", key)
    value = $0; sub(/^[^=]*=/, "", value)
    if (key ~ /^cpu_/) { next }
    if (!(key in base)) {
        printf "NEW      %s = %s\n", key, value
        added++
        next
    }
    previous = base[key]
    seen[key] = 1
    if (key ~ /hash$/ || key == "scenario" || key == "seed") {
        if (value != previous) {
            printf "BEHAVIOUR %s: %s -> %s\n", key, previous, value
            behaviour++
        }
        next
    }
    before = previous + 0
    after = value + 0
    if (before == 0) {
        if (after != 0) {
            printf "DRIFT    %s: 0 -> %s\n", key, value
            regressed++
        }
        next
    }
    delta = (after - before) * 100.0 / before
    if (delta > tolerance) {
        printf "REGRESS  %s: %s -> %s (%+.1f%%)\n", key, previous, value, delta
        regressed++
    } else if (delta < -tolerance) {
        printf "IMPROVE  %s: %s -> %s (%+.1f%%)\n", key, previous, value, delta
        improved++
    }
}
END {
    for (key in base) {
        if (!(key in seen)) {
            printf "MISSING  %s (was %s)\n", key, base[key]
            missing++
        }
    }
    if (behaviour > 0) {
        printf "\nFAIL: simulation behaviour changed in %d field(s).\n", behaviour
        printf "If the change was intended, re-baseline in the same commit:\n"
        printf "  tools/vox-bench.sh --update\n"
        exit 2
    }
    if (regressed > 0 || missing > 0) {
        printf "\nFAIL: %d work counter(s) regressed beyond %d%%.\n", regressed + missing, tolerance
        exit 1
    }
    if (improved > 0) {
        printf "\nOK: within tolerance (%d counter(s) improved beyond %d%%).\n", improved, tolerance
    } else if (added > 0) {
        printf "\nOK: within tolerance (%d new counter(s); re-baseline to record them).\n", added
    } else {
        printf "OK: within %d%% of the baseline.\n", tolerance
    }
}
' "$BASELINE" "$CURRENT"

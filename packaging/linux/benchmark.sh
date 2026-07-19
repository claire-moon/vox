#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

ROOT=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=/dev/null
source "$ROOT/libexec/vox-runtime.sh"
BINARY="$ROOT/bin/digs_demo"
FRAMES=${1:-240}
OUT_DIR="$ROOT/qa/out"
STAMP=$(date -u +%Y%m%dT%H%M%SZ)
REPORT="$OUT_DIR/benchmark-$STAMP.txt"

if [[ ! "$FRAMES" =~ ^[0-9]+$ ]] || (( FRAMES < 1 || FRAMES > 100000 )); then
    vox_die 'benchmark frame count must be an integer from 1 through 100000'
fi
vox_require_executable "$BINARY"
vox_require_runtime_libraries "$BINARY"
mkdir -p -- "$OUT_DIR"
{
    printf 'DIGS v0.0.2 Linux benchmark\n'
    printf 'UTC: %s\n' "$STAMP"
    printf 'Kernel: %s\n' "$(uname -srmo 2>/dev/null || uname -a)"
    if [[ -r /proc/cpuinfo ]]; then
        printf 'CPU: %s\n' "$(awk -F ': ' '/^model name/{print $2; exit}' /proc/cpuinfo)"
    fi
    printf 'Frames per Lightfield tier: %s\n\n' "$FRAMES"
    "$BINARY" --benchmark "$FRAMES"
} | tee "$REPORT"
printf 'Benchmark report: %s\n' "$REPORT"

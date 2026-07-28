#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

ROOT=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=/dev/null
source "$ROOT/libexec/vox-runtime.sh"
BINARY="$ROOT/bin/digs_demo"
OUTPUT=${1:-"$ROOT/qa/out/digs-demo-smoke.ppm"}
NAMED_BENCH_QUALIFY=${VOX_NAMED_BENCH_QUALIFY:-0}

case "$NAMED_BENCH_QUALIFY" in
    0|1) ;;
    *) vox_die "VOX_NAMED_BENCH_QUALIFY must be 0 or 1" ;;
esac

vox_require_executable "$BINARY"
vox_require_runtime_libraries "$BINARY"
mkdir -p -- "$(dirname -- "$OUTPUT")"
"$BINARY" --input-self-test
"$BINARY" --cap-self-test
"$BINARY" --audio-cadence-self-test
"$BINARY" --bark-self-test
"$BINARY" --haptic-self-test
"$BINARY" --load-self-test 600
if [[ "$NAMED_BENCH_QUALIFY" == 1 ]]; then
    "$BINARY" --performance-self-test 600
fi
"$BINARY" --settings-self-test "$ROOT/qa/out/digs-settings-self-test.cfg"
"$BINARY" --camera-self-test
"$BINARY" --smoke-test "$OUTPUT"
[[ -s "$OUTPUT" ]] || vox_die "smoke test did not create $OUTPUT"
printf 'Smoke image: %s\n' "$OUTPUT"

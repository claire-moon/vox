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
# v0.0.5: the save layer and the window widget. A tester running only this
# script should still find out if either is broken on their machine.
"$BINARY" --chronicle-self-test "$ROOT/qa/out/digs-chronicle-self-test.dat"
"$BINARY" --menu-self-test
# Does what the miners learn actually survive closing the game? This is
# the one claim that needs more than one process to test.
"$ROOT/session-evidence.sh" "$BINARY"
"$BINARY" --smoke-test "$OUTPUT"
[[ -s "$OUTPUT" ]] || vox_die "smoke test did not create $OUTPUT"
printf 'Smoke image: %s\n' "$OUTPUT"

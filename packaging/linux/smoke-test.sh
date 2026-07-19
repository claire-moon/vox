#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

ROOT=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=/dev/null
source "$ROOT/libexec/vox-runtime.sh"
BINARY="$ROOT/bin/digs_demo"
OUTPUT=${1:-"$ROOT/qa/out/digs-demo-smoke.ppm"}

vox_require_executable "$BINARY"
vox_require_runtime_libraries "$BINARY"
mkdir -p -- "$(dirname -- "$OUTPUT")"
"$BINARY" --input-self-test
"$BINARY" --camera-self-test
"$BINARY" --smoke-test "$OUTPUT"
[[ -s "$OUTPUT" ]] || vox_die "smoke test did not create $OUTPUT"
printf 'Smoke image: %s\n' "$OUTPUT"

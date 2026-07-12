#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

ROOT=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=/dev/null
source "$ROOT/libexec/vox-runtime.sh"
BINARY="$ROOT/bin/digs_demo"

vox_require_executable "$BINARY"
vox_require_runtime_libraries "$BINARY"
exec "$BINARY" "$@"

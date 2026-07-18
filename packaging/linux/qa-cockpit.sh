#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

ROOT=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
COCKPIT="$ROOT/tools/vox-test-cockpit.sh"
WORKBOOK="$ROOT/qa/VOX_QA_FEEDBACK.xlsx"
BINARY="$ROOT/bin/digs_demo"

if [[ ! -x "$COCKPIT" ]]; then
    printf 'DIGS: the QA cockpit is missing from this archive.\n' >&2
    exit 1
fi
if (( $# == 0 )); then
    if [[ ! -f "$WORKBOOK" ]]; then
        printf 'DIGS: the default QA workbook is missing: %s\n' \
            "$WORKBOOK" >&2
        exit 1
    fi
    exec "$COCKPIT" --binary "$BINARY" "$WORKBOOK"
fi
exec "$COCKPIT" "$@"

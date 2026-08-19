#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Measure the shipped DIGS payload against the active size budget.
#
# The budgeted payload is exactly what a player needs in order to boot and
# play: the digs_demo binary plus every file under the staged share/ tree.
# Documentation, licences, QA material, and the evidence bundle are part of
# the tester archive, not the game, and are deliberately excluded.
#
# Usage:  tools/vox-size-report.sh [build-dir]
#
# Environment:
#   VOX_BUILD_DIR      build directory (default: build, or $1 when given)
#   VOX_SIZE_BINARY    game binary      (default: $BUILD_DIR/digs_demo)
#   VOX_SIZE_SHARE     runtime data dir (default: $BUILD_DIR/share)
#   VOX_SIZE_REPORT    write the machine-readable report here as well
#   VOX_SIZE_TARGET    target payload size     (default 6291456)
#   VOX_SIZE_WARN      warning threshold       (default 8388608)
#   VOX_SIZE_CEILING   hard fail threshold     (default 10485760 = 10 MiB)
#
# Exit status:
#   0  payload is at or below the hard ceiling (may still warn)
#   1  payload exceeds the hard ceiling
#   2  usage or environment error
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR=${1:-${VOX_BUILD_DIR:-$ROOT/build}}
SIZE_REPORT=${VOX_SIZE_REPORT:-}
SIZE_TARGET=${VOX_SIZE_TARGET:-6291456}
SIZE_WARN=${VOX_SIZE_WARN:-8388608}
SIZE_CEILING=${VOX_SIZE_CEILING:-10485760}

for threshold in "$SIZE_TARGET" "$SIZE_WARN" "$SIZE_CEILING"; do
    case "$threshold" in
        ''|*[!0-9]*)
            echo "size thresholds must be non-negative integers" >&2
            exit 2
            ;;
    esac
done
if [ "$SIZE_TARGET" -gt "$SIZE_WARN" ] || [ "$SIZE_WARN" -gt "$SIZE_CEILING" ]; then
    echo "expected VOX_SIZE_TARGET <= VOX_SIZE_WARN <= VOX_SIZE_CEILING" >&2
    exit 2
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo "build directory not found: $BUILD_DIR" >&2
    exit 2
fi

BINARY=${VOX_SIZE_BINARY:-$BUILD_DIR/digs_demo}
if [ ! -f "$BINARY" ]; then
    echo "game binary not found: $BINARY" >&2
    echo "configure with -DVOX_BUILD_SDL2_DEMO=ON and build first" >&2
    exit 2
fi
SHARE_DIR=${VOX_SIZE_SHARE:-$BUILD_DIR/share}

# Sum file sizes without relying on GNU stat or du extensions.  One wc per
# file keeps the output free of the aggregate "total" rows that `wc -c {} +`
# would otherwise interleave.
sum_bytes() {
    if [ ! -e "$1" ]; then
        echo 0
        return 0
    fi
    find "$1" -type f -exec wc -c {} \; |
        awk '{ total += $1 } END { printf "%d\n", total + 0 }'
}

BINARY_BYTES=$(sum_bytes "$BINARY")
SHARE_BYTES=$(sum_bytes "$SHARE_DIR")
PAYLOAD_BYTES=$((BINARY_BYTES + SHARE_BYTES))

if [ "$PAYLOAD_BYTES" -gt "$SIZE_CEILING" ]; then
    STATUS=OVER_CEILING
elif [ "$PAYLOAD_BYTES" -gt "$SIZE_WARN" ]; then
    STATUS=OVER_WARN
elif [ "$PAYLOAD_BYTES" -gt "$SIZE_TARGET" ]; then
    STATUS=OVER_TARGET
else
    STATUS=WITHIN_TARGET
fi

# Tenths of a percent without floating point, so the report stays stable.
PCT_TENTHS=$((PAYLOAD_BYTES * 1000 / SIZE_CEILING))
PCT=$((PCT_TENTHS / 10)).$((PCT_TENTHS % 10))
HEADROOM=$((SIZE_CEILING - PAYLOAD_BYTES))

emit_report() {
    echo "# VOX + DIGS payload size report"
    echo "build_dir=$BUILD_DIR"
    echo "binary_path=${BINARY##*/}"
    echo "binary_bytes=$BINARY_BYTES"
    echo "share_bytes=$SHARE_BYTES"
    echo "payload_bytes=$PAYLOAD_BYTES"
    echo "target_bytes=$SIZE_TARGET"
    echo "warn_bytes=$SIZE_WARN"
    echo "ceiling_bytes=$SIZE_CEILING"
    echo "headroom_bytes=$HEADROOM"
    echo "payload_pct_ceiling=$PCT"
    echo "status=$STATUS"
    echo "# largest payload files, bytes first"
    {
        echo "$BINARY_BYTES ${BINARY##*/}"
        if [ -d "$SHARE_DIR" ]; then
            share_parent=${SHARE_DIR%/*}
            find "$SHARE_DIR" -type f -exec wc -c {} \; |
                while read -r bytes path; do
                    echo "$bytes ${path#"$share_parent"/}"
                done
        fi
    } | sort -rn | awk '{ printf "file=%s bytes=%s\n", $2, $1 }'
    if command -v size >/dev/null 2>&1; then
        echo "# binary sections"
        size "$BINARY" 2>/dev/null |
            awk 'NR == 2 { printf "text_bytes=%s\ndata_bytes=%s\nbss_bytes=%s\n", $1, $2, $3 }'
    fi
}

emit_report
if [ -n "$SIZE_REPORT" ]; then
    emit_report >"$SIZE_REPORT"
fi

case "$STATUS" in
    OVER_CEILING)
        echo "FAIL: payload $PAYLOAD_BYTES B exceeds the $SIZE_CEILING B ceiling" >&2
        exit 1
        ;;
    OVER_WARN)
        echo "WARN: payload $PAYLOAD_BYTES B is above the $SIZE_WARN B warning line" >&2
        ;;
    OVER_TARGET)
        echo "NOTE: payload $PAYLOAD_BYTES B is above the $SIZE_TARGET B v0.0.4 target" >&2
        ;;
esac
exit 0

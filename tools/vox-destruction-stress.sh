#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Deterministic five-minute, four-miner destruction work gate.  This checks
# canonical counters only; run the SDL named-machine stress qualification for
# the p99 frame-time and zero-tick-debt acceptance evidence.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
VOX_BENCH_STRESS=1 "$ROOT/tools/vox-bench.sh" "$@"

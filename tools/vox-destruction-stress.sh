#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Deterministic 1,800-tick, four-miner destruction workload gate.  This checks
# canonical counters only; run the SDL named-machine stress qualification for
# the p99 frame-time and zero-tick-debt acceptance evidence.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
"$ROOT/tools/vox-bench.sh" "$@"

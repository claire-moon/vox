#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Prove that what one match teaches the miners, the next launch starts with.
#
# This is the one claim in v0.0.4 that no other lane touches. Every other test
# runs inside a single process, and memory that survives a single process is
# not memory -- it is a variable. So each phase here is a separate invocation
# of the binary, with a real file between them, which is as close to quitting
# and coming back as a script can get.
#
# What it establishes, in the order the assertions run:
#
#   1. A launch with no history opens neutral.
#   2. A played match writes something back.
#   3. A LATER PROCESS reads it and opens from it, not from nothing.
#   4. That difference reaches the simulation -- the carried match and the
#      neutral one do not produce the same hash. Memory is load-bearing
#      rather than decorative.
#   5. A missing or damaged chronicle is refused rather than quietly reported
#      as a first meeting, which would pass step 3 for the wrong reason.
#
# What it does NOT establish, and cannot: whether any of it reads well. That
# a miner opens at valence -136 is mechanical; that it sounds like somebody
# holding a grudge is VOX-QA-089 and needs a person.
#
# Usage:  tools/vox-session-evidence.sh [path-to-digs_demo]
#
# Exit status:
#   0  every assertion held
#   1  an assertion failed
#   2  usage or environment error
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
DEMO=${1:-${VOX_SESSION_DEMO:-$ROOT/build-dev/digs_demo}}

if [ ! -x "$DEMO" ]; then
    printf 'vox-session-evidence: no digs_demo at %s\n' "$DEMO" >&2
    exit 2
fi

WORK=${TMPDIR:-/tmp}/vox-session-evidence.$$
CHRONICLE=$WORK/chronicle.dat
mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT INT TERM

failures=0

field() {
    # field <line> <key>  ->  the value, or empty
    printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"
}

check() {
    # check <description> <actual> <operator> <expected>
    description=$1
    actual=$2
    operator=$3
    expected=$4
    ok=0
    case $operator in
        eq) [ "$actual" = "$expected" ] && ok=1 ;;
        ne) [ "$actual" != "$expected" ] && ok=1 ;;
        gt) [ "$actual" -gt "$expected" ] 2>/dev/null && ok=1 ;;
    esac
    if [ "$ok" -eq 1 ]; then
        printf '  ok    %s\n' "$description"
    else
        printf '  FAIL  %s (got %s, wanted %s %s)\n' \
            "$description" "$actual" "$operator" "$expected"
        failures=$((failures + 1))
    fi
}

printf 'Cross-session memory evidence, using %s\n' "$DEMO"

# ---------------------------------------------------------------------------
# 1. A launch with nothing behind it.
# ---------------------------------------------------------------------------
printf '\nprocess 1: a first launch, no history\n'
fresh=$("$DEMO" --session-self-test "$CHRONICLE" fresh) || {
    printf 'vox-session-evidence: fresh phase failed\n' >&2
    exit 1
}
printf '  %s\n' "$fresh"
fresh_regard=$(field "$fresh" regard_total)
fresh_met=$(field "$fresh" pairs_met)
fresh_played=$(field "$fresh" identity_matches)
fresh_valence=$(field "$fresh" opening_valence)
fresh_contract_total=$(field "$fresh" contract_total)
fresh_hash=$(field "$fresh" hash)
check "opens with no accounts"        "$fresh_regard"  eq 0
check "opens having met nobody"       "$fresh_met"     eq 0
check "opens having played nothing"   "$fresh_played"  eq 0
check "opens neutral toward the first bot" "$fresh_valence" eq 0
check "opens with no carried contract" "$fresh_contract_total" eq 0

# ---------------------------------------------------------------------------
# 2. A match is played and writes back.
# ---------------------------------------------------------------------------
printf '\nprocess 2: play a match and put it away\n'
record=$("$DEMO" --session-self-test "$CHRONICLE" record) || {
    printf 'vox-session-evidence: record phase failed\n' >&2
    exit 1
}
printf '  %s\n' "$record"
record_regard=$(field "$record" regard_total)
record_met=$(field "$record" pairs_met)
record_played=$(field "$record" identity_matches)
check "the match moved the accounts"  "$record_regard" gt 0
check "the miners met each other"     "$record_met"    gt 0
check "the match was counted"         "$record_played" gt 0
[ -s "$CHRONICLE" ] || {
    printf '  FAIL  nothing was written to disk\n'
    failures=$((failures + 1))
}

# ---------------------------------------------------------------------------
# 3 and 4. A later process opens from the file, and it changes the match.
# ---------------------------------------------------------------------------
printf '\nprocess 3: relaunch and open from what is on disk\n'
verify=$("$DEMO" --session-self-test "$CHRONICLE" verify) || {
    printf 'vox-session-evidence: verify phase failed\n' >&2
    exit 1
}
printf '  %s\n' "$verify"
verify_regard=$(field "$verify" regard_total)
verify_met=$(field "$verify" pairs_met)
verify_played=$(field "$verify" identity_matches)
verify_valence=$(field "$verify" opening_valence)
verify_contract_total=$(field "$verify" contract_total)
verify_hash=$(field "$verify" hash)
check "the accounts crossed the process boundary intact" \
    "$verify_regard" eq "$record_regard"
check "so did who has met whom"       "$verify_met"    eq "$record_met"
check "so did the match count"        "$verify_played" eq "$record_played"
check "the new match carries a non-neutral account" \
    "$verify_contract_total" gt 0
check "and therefore does not simulate identically to a first meeting" \
    "$verify_hash" ne "$fresh_hash"

# ---------------------------------------------------------------------------
# 5. The failure modes that would make step 3 pass for the wrong reason.
# ---------------------------------------------------------------------------
printf '\nrefusals: a missing or damaged chronicle must not read as a stranger\n'
if "$DEMO" --session-self-test "$WORK/absent.dat" verify >/dev/null 2>&1; then
    printf '  FAIL  verify accepted a chronicle that does not exist\n'
    failures=$((failures + 1))
else
    printf '  ok    verify refuses a chronicle that does not exist\n'
fi

# Edit a value in place and leave the checksum alone, the way a player poking
# at the file would.
if sed -i.orig 's/-\([0-9][0-9][0-9]\)/-9\19/' "$CHRONICLE" 2>/dev/null &&
   ! cmp -s "$CHRONICLE" "$CHRONICLE.orig"; then
    if "$DEMO" --session-self-test "$CHRONICLE" verify >/dev/null 2>&1; then
        printf '  FAIL  verify accepted a chronicle that failed its checksum\n'
        failures=$((failures + 1))
    else
        printf '  ok    verify refuses a chronicle that failed its checksum\n'
    fi
else
    # Not a failure of the game -- a failure to construct the test. Say so
    # rather than reporting a pass nobody earned.
    printf '  SKIP  could not damage the chronicle to test the refusal\n'
fi

printf '\n'
if [ "$failures" -ne 0 ]; then
    printf 'vox-session-evidence: %d assertion(s) failed\n' "$failures" >&2
    exit 1
fi
printf 'vox-session-evidence: memory survives a relaunch and changes the match\n'
exit 0

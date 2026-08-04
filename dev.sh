#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Everyday driver for working on DIGS.  Wraps the commands you would
# otherwise have to remember, and always builds with the strict ISO C
# settings the project actually ships with.
#
#   ./dev.sh            build, then launch the game
#   ./dev.sh build      build only
#   ./dev.sh run        launch without rebuilding
#   ./dev.sh test       build, then run the test suite
#   ./dev.sh gates      build, then run the size / benchmark / invariance gates
#   ./dev.sh check      build, tests, and gates -- run this before committing
#   ./dev.sh rebase     accept the current simulation behaviour as the new
#                       benchmark baseline (only after a deliberate change)
#   ./dev.sh debug      build and launch an unoptimised build with symbols
#   ./dev.sh clean      delete the build directories
#
# Environment:
#   VOX_DEV_BUILD_DIR   build directory (default: build-dev)
#   VOX_DEV_JOBS        parallel build jobs (default: all cores)
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
BUILD_DIR=${VOX_DEV_BUILD_DIR:-$ROOT/build-dev}
DEBUG_DIR=$ROOT/build-dev-debug
JOBS=${VOX_DEV_JOBS:-}
STRICT_C_FLAGS="-std=c89 -pedantic-errors -Wall -Wextra -Werror"

say() { printf '\n\033[1;36m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31mdev.sh: %s\033[0m\n' "$*" >&2; exit 1; }

configure() {
    dir=$1
    build_type=$2
    cmake -S "$ROOT" -B "$dir" \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DVOX_BUILD_TESTS=ON \
        -DVOX_BUILD_SDL2_DEMO=ON \
        -DCMAKE_C_FLAGS="$STRICT_C_FLAGS" >/dev/null || {
            cat >&2 <<'EOF'

Configure failed.  The playable demo needs SDL2 development headers:
  Debian/Ubuntu/Mint  sudo apt install libsdl2-dev
  Fedora              sudo dnf install SDL2-devel
  Arch                sudo pacman -S sdl2
EOF
            exit 1
        }
}

build() {
    dir=$1
    build_type=${2:-Release}
    say "Building ($build_type)"
    configure "$dir" "$build_type"
    if [ -n "$JOBS" ]; then
        cmake --build "$dir" --parallel "$JOBS"
    else
        cmake --build "$dir" --parallel
    fi
}

require_built() {
    [ -x "$1/digs_demo" ] || die "no build yet -- run './dev.sh build' first"
}

run_tests() {
    say "Tests"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
}

run_gates() {
    say "Size budget"
    "$ROOT/tools/vox-size-report.sh" "$BUILD_DIR" |
        grep -E '^(payload_bytes|payload_pct_ceiling|headroom_bytes|status)='
    say "Benchmark"
    # A changed state hash exits 2 and means the simulation behaves
    # differently.  That is sometimes correct -- './dev.sh rebase' records it.
    # Capture the status directly: inside "if ! cmd", $? is the exit of the
    # negation and is therefore always 0, so the old form reported every
    # behaviour change as a pass and "All green" printed over a failed gate.
    status=0
    "$ROOT/tools/vox-bench.sh" "$BUILD_DIR" || status=$?
    if [ "$status" -ne 0 ]; then
        if [ "$status" = 2 ]; then
            printf '\n\033[1;33mSimulation behaviour changed.\033[0m\n'
            printf 'If that was intended: ./dev.sh rebase\n'
        fi
        return "$status"
    fi
    say "Optimisation invariance"
    "$ROOT/tools/vox-optimisation-invariance.sh"
}

command=${1:-play}
case "$command" in
    play)
        build "$BUILD_DIR"
        say "Launching"
        exec "$BUILD_DIR/digs_demo"
        ;;
    build)
        build "$BUILD_DIR"
        say "Built $BUILD_DIR/digs_demo"
        ;;
    run)
        require_built "$BUILD_DIR"
        exec "$BUILD_DIR/digs_demo"
        ;;
    test)
        build "$BUILD_DIR"
        run_tests
        ;;
    gates)
        build "$BUILD_DIR"
        run_gates
        ;;
    check)
        build "$BUILD_DIR"
        run_tests
        run_gates || exit $?
        say "All green -- safe to commit"
        ;;
    rebase)
        build "$BUILD_DIR"
        "$ROOT/tools/vox-bench.sh" --update "$BUILD_DIR"
        printf '\nBaseline updated.  Commit benchmarks/baseline.txt WITH the\n'
        printf 'change that caused it, so the two never drift apart.\n'
        ;;
    debug)
        build "$DEBUG_DIR" Debug
        say "Launching debug build"
        exec "$DEBUG_DIR/digs_demo"
        ;;
    clean)
        rm -rf "$BUILD_DIR" "$DEBUG_DIR"
        say "Removed $BUILD_DIR and $DEBUG_DIR"
        ;;
    -h|--help|help)
        # Print the header comment and stop at the first line of real code,
        # so this never drifts out of step with the block above.
        awk 'NR > 2 { if ($0 !~ /^#/) exit; sub(/^# ?/, ""); print }' "$0"
        ;;
    *)
        die "unknown command '$command' -- try './dev.sh help'"
        ;;
esac

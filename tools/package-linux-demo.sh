#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

export LC_ALL=C
umask 022

ROOT=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
DIST_DIR=${VOX_PACKAGE_DIST_DIR:-"$ROOT/dist"}
VERSION=${VOX_PACKAGE_VERSION:-v0.0.1}
ARCHIVE_STEM="vox-digs-$VERSION-linux-x86_64"
SOURCE_STEM="vox-digs-$VERSION-source"
BINARY_ARCHIVE="$DIST_DIR/$ARCHIVE_STEM.tar.gz"
SOURCE_ARCHIVE="$DIST_DIR/$SOURCE_STEM.tar.gz"
CHECKSUMS="$DIST_DIR/SHA256SUMS"
ALLOW_DIRTY=${VOX_PACKAGE_ALLOW_DIRTY:-0}
NASM_ACCEL=${VOX_PACKAGE_NASM:-AUTO}
BENCHMARK_FRAMES=${VOX_PACKAGE_BENCHMARK_FRAMES:-120}
WORK_DIR=

die()
{
    printf 'package-linux-demo: %s\n' "$*" >&2
    exit 1
}

cleanup()
{
    if [[ -n "$WORK_DIR" && -d "$WORK_DIR" ]]; then
        rm -rf -- "$WORK_DIR"
    fi
}
trap cleanup EXIT HUP INT TERM

need_command()
{
    command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

copy_file()
{
    local source=$1
    local destination=$2

    install -D -m 0644 -- "$source" "$destination"
}

copy_tree()
{
    local source=$1
    local destination=$2

    mkdir -p -- "$destination"
    cp -RPp -- "$source"/. "$destination"/
}

make_archive()
{
    local parent=$1
    local member=$2
    local output=$3

    tar --sort=name --format=posix \
        --pax-option=delete=atime,delete=ctime \
        --mtime="@$SOURCE_DATE_EPOCH" \
        --owner=0 --group=0 --numeric-owner \
        -C "$parent" -cf - "$member" | gzip -n -9 >"$output"
}

capture_evidence()
{
    local label=$1
    local directory=$2
    local stdout_file="$EVIDENCE_DIR/$label.stdout.txt"
    local stderr_file="$EVIDENCE_DIR/$label.stderr.txt"
    local -a pipeline_status

    shift 2
    printf 'Running evidence command: %s\n' "$label"
    set +e
    (
        cd -- "$directory"
        "$@"
    ) 2>"$stderr_file" | tee "$stdout_file"
    pipeline_status=("${PIPESTATUS[@]}")
    set -e

    # stderr is replayed only after the command so that its archived file is an
    # exact byte-for-byte stream. PIPESTATUS is copied before any other command
    # can overwrite it, preserving both the producer and stdout tee status.
    if ! tee /dev/stderr <"$stderr_file" >/dev/null; then
        die "could not archive stderr for evidence command: $label"
    fi
    if (( pipeline_status[1] != 0 )); then
        die "could not archive stdout for evidence command: $label"
    fi
    printf '%d\n' "${pipeline_status[0]}" >"$EVIDENCE_DIR/$label.exit-status.txt"
    if (( pipeline_status[0] != 0 )); then
        die "evidence command failed ($label): exit ${pipeline_status[0]}"
    fi
}

need_command git
need_command cmake
need_command ctest
need_command cargo
need_command tar
need_command gzip
need_command sha256sum
need_command install
need_command find
need_command touch
need_command python3
need_command tee

[[ $(uname -s) == Linux ]] || die 'this packager targets Linux only'
case "$(uname -m)" in
    x86_64|amd64) ;;
    *) die "this packager targets x86_64, not $(uname -m)" ;;
esac
[[ "$ALLOW_DIRTY" == 0 || "$ALLOW_DIRTY" == 1 ]] || \
    die 'VOX_PACKAGE_ALLOW_DIRTY must be 0 or 1'
[[ "$VERSION" =~ ^[A-Za-z0-9][A-Za-z0-9._+-]*$ ]] || \
    die 'VOX_PACKAGE_VERSION may contain only letters, digits, dot, underscore, plus, and hyphen'
if [[ ! "$BENCHMARK_FRAMES" =~ ^[0-9]+$ ]] ||
    (( BENCHMARK_FRAMES < 1 || BENCHMARK_FRAMES > 100000 )); then
    die 'VOX_PACKAGE_BENCHMARK_FRAMES must be an integer from 1 through 100000'
fi

DIRTY_STATE=$(git -C "$ROOT" status --porcelain=v1 --untracked-files=all)
if [[ -n "$DIRTY_STATE" && "$ALLOW_DIRTY" != 1 ]]; then
    printf '%s\n' "$DIRTY_STATE" >&2
    die 'the source tree is dirty; commit/stash changes or set VOX_PACKAGE_ALLOW_DIRTY=1 for a non-release test package'
fi
if [[ -n "$DIRTY_STATE" ]]; then
    printf '%s\n' \
        'package-linux-demo: WARNING: building an explicitly allowed dirty-tree package.' >&2
fi

if [[ -z ${SOURCE_DATE_EPOCH:-} ]]; then
    SOURCE_DATE_EPOCH=$(git -C "$ROOT" show -s --format=%ct HEAD)
fi
[[ "$SOURCE_DATE_EPOCH" =~ ^[0-9]+$ ]] || \
    die 'SOURCE_DATE_EPOCH must contain a non-negative integer timestamp'
export SOURCE_DATE_EPOCH

case "$NASM_ACCEL" in
    AUTO)
        if command -v nasm >/dev/null 2>&1; then
            NASM_ACCEL=ON
        else
            NASM_ACCEL=OFF
        fi
        ;;
    ON)
        command -v nasm >/dev/null 2>&1 || \
            die 'VOX_PACKAGE_NASM=ON requires nasm'
        ;;
    OFF) ;;
    *) die 'VOX_PACKAGE_NASM must be AUTO, ON, or OFF' ;;
esac

mkdir -p -- "$DIST_DIR"
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/vox-linux-package.XXXXXXXX")
BUILD_DIR="$WORK_DIR/build"
CARGO_TARGET_DIR="$WORK_DIR/cargo-target"
STAGE_DIR="$WORK_DIR/$ARCHIVE_STEM"
SOURCE_STAGE="$WORK_DIR/$SOURCE_STEM"
EVIDENCE_DIR="$STAGE_DIR/evidence"

printf 'Configuring Release build (NASM acceleration: %s)\n' "$NASM_ACCEL"
if ! cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DVOX_BUILD_TESTS=ON \
    -DVOX_BUILD_SDL2_DEMO=ON \
    -DVOX_BUILD_NASM_ACCEL="$NASM_ACCEL"; then
    cat >&2 <<'EOF'

SDL2 development files are required to build the playable demo.
Common packages: libsdl2-dev (Debian/Ubuntu/Mint), SDL2-devel (Fedora),
sdl2 (Arch), or libSDL2-devel (openSUSE).
EOF
    exit 1
fi
cmake --build "$BUILD_DIR" --config Release --parallel

mkdir -p -- "$EVIDENCE_DIR"
capture_evidence ctest "$BUILD_DIR" \
    ctest -C Release --output-on-failure
capture_evidence cargo-test "$ROOT" \
    env CARGO_TARGET_DIR="$CARGO_TARGET_DIR" cargo test \
        --manifest-path "$ROOT/Cargo.toml" --workspace --locked
capture_evidence vox-headless "$EVIDENCE_DIR" "$BUILD_DIR/vox_headless"
capture_evidence digs-headless "$EVIDENCE_DIR" "$BUILD_DIR/digs_headless"
capture_evidence digs-demo-smoke "$EVIDENCE_DIR" \
    "$BUILD_DIR/digs_demo" --smoke-test digs-demo-smoke.ppm
[[ -s "$EVIDENCE_DIR/digs-demo-smoke.ppm" ]] || \
    die 'digs_demo smoke test produced no image'
capture_evidence digs-demo-benchmark "$EVIDENCE_DIR" \
    "$BUILD_DIR/digs_demo" --benchmark "$BENCHMARK_FRAMES"
capture_evidence vox-render-demo "$EVIDENCE_DIR" \
    "$BUILD_DIR/vox_render_demo" vox-render-smoke.ppm
[[ -s "$EVIDENCE_DIR/vox-render-smoke.ppm" ]] || \
    die 'vox_render_demo produced no image'

mkdir -p -- "$STAGE_DIR/bin" "$STAGE_DIR/libexec" "$STAGE_DIR/tools"
for binary in digs_demo vox_headless digs_headless vox_render_demo; do
    install -m 0755 -- "$BUILD_DIR/$binary" "$STAGE_DIR/bin/$binary"
done
install -m 0755 -- "$ROOT/packaging/linux/run-digs.sh" \
    "$STAGE_DIR/run-digs.sh"
install -m 0755 -- "$ROOT/packaging/linux/smoke-test.sh" \
    "$STAGE_DIR/smoke-test.sh"
install -m 0755 -- "$ROOT/packaging/linux/benchmark.sh" \
    "$STAGE_DIR/benchmark.sh"
install -m 0755 -- "$ROOT/packaging/linux/qa-cockpit.sh" \
    "$STAGE_DIR/qa-cockpit.sh"
install -m 0644 -- "$ROOT/packaging/linux/libexec/vox-runtime.sh" \
    "$STAGE_DIR/libexec/vox-runtime.sh"
copy_file "$ROOT/packaging/linux/START-HERE.txt" "$STAGE_DIR/START-HERE.txt"
copy_file "$ROOT/CGReadme.txt" "$STAGE_DIR/CGReadme.txt"

copy_file "$ROOT/LICENSE" "$STAGE_DIR/LICENSE"
copy_file "$ROOT/THIRD_PARTY.md" "$STAGE_DIR/THIRD_PARTY.md"
copy_file "$ROOT/README.md" "$STAGE_DIR/README.md"
copy_file "$ROOT/CONTRIBUTING.md" "$STAGE_DIR/CONTRIBUTING.md"
copy_file "$ROOT/CODE_OF_CONDUCT.md" "$STAGE_DIR/CODE_OF_CONDUCT.md"
copy_file "$ROOT/SECURITY.md" "$STAGE_DIR/SECURITY.md"
copy_file "$ROOT/ROADMAP.txt" "$STAGE_DIR/ROADMAP.txt"
copy_tree "$ROOT/LICENSES" "$STAGE_DIR/LICENSES"
copy_tree "$ROOT/docs" "$STAGE_DIR/docs"

if [[ -d "$ROOT/qa" ]]; then
    copy_tree "$ROOT/qa" "$STAGE_DIR/qa"
    rm -rf -- "$STAGE_DIR/qa/out"
fi
mkdir -p -- "$STAGE_DIR/qa/out"
if [[ -f "$ROOT/tools/vox-test-cockpit.sh" ]]; then
    install -m 0755 -- "$ROOT/tools/vox-test-cockpit.sh" \
        "$STAGE_DIR/tools/vox-test-cockpit.sh"
fi
if [[ -f "$ROOT/tools/build-qa-workbook.py" ]]; then
    copy_file "$ROOT/tools/build-qa-workbook.py" \
        "$STAGE_DIR/tools/build-qa-workbook.py"
fi
if [[ -f "$ROOT/tools/merge-qa-csv.py" ]]; then
    copy_file "$ROOT/tools/merge-qa-csv.py" \
        "$STAGE_DIR/tools/merge-qa-csv.py"
fi
install -m 0755 -- "$ROOT/tools/generate-spdx-sbom.py" \
    "$STAGE_DIR/tools/generate-spdx-sbom.py"

RUNTIME_DEPENDENCIES=
if command -v ldd >/dev/null 2>&1; then
    if ! RUNTIME_DEPENDENCIES=$(LC_ALL=C ldd "$STAGE_DIR/bin/digs_demo" 2>&1); then
        printf '%s\n' "$RUNTIME_DEPENDENCIES" >&2
        die 'could not inspect the packaged demo runtime libraries'
    fi
    if grep -F 'not found' <<<"$RUNTIME_DEPENDENCIES" >/dev/null; then
        printf '%s\n' "$RUNTIME_DEPENDENCIES" >&2
        die 'the packaged demo has an unresolved dynamic runtime library'
    fi
fi

{
    printf 'VOX + DIGS Linux binary-bundle evidence\n'
    printf 'Version: %s\n' "$VERSION"
    printf 'Git commit: %s\n' "$(git -C "$ROOT" rev-parse HEAD)"
    printf 'Dirty source explicitly allowed: %s\n' \
        "$([[ -n "$DIRTY_STATE" ]] && printf yes || printf no)"
    printf 'SOURCE_DATE_EPOCH: %s\n' "$SOURCE_DATE_EPOCH"
    printf 'Host: %s\n' "$(uname -srmo)"
    printf 'CMake: %s\n' "$(cmake --version | sed -n '1p')"
    printf 'C compiler: %s\n' "${CC:-CMake default}"
    printf 'C++ compiler: %s\n' "${CXX:-CMake default}"
    printf 'NASM acceleration: %s\n' "$NASM_ACCEL"
    if command -v nasm >/dev/null 2>&1; then
        printf 'NASM: %s\n' "$(nasm -v)"
    fi
    if [[ -n ${RUNTIME_DEPENDENCIES:-} ]]; then
        printf '\nDynamic runtime dependencies (digs_demo):\n'
        printf '%s\n' "$RUNTIME_DEPENDENCIES" | \
            sed -E 's/[[:space:]]+\(0x[0-9a-fA-F]+\)//g'
    fi
} >"$EVIDENCE_DIR/build-info.txt"

copy_file "$ROOT/packaging/linux/EVIDENCE-README.txt" \
    "$EVIDENCE_DIR/README.txt"

python3 "$ROOT/tools/generate-spdx-sbom.py" \
    --root "$STAGE_DIR" \
    --output "$STAGE_DIR/SBOM.spdx.json" \
    --version "$VERSION" \
    --commit "$(git -C "$ROOT" rev-parse HEAD)" \
    --epoch "$SOURCE_DATE_EPOCH" \
    --document-name "VOX + DIGS $VERSION Linux x86_64 binary bundle" \
    --purpose APPLICATION \
    --namespace-label linux-x86_64-binary

python3 -m json.tool "$STAGE_DIR/SBOM.spdx.json" >/dev/null

# Build the Corresponding Source archive from every tracked or non-ignored
# source file. In a clean release this is exactly the committed tree; the
# explicit dirty-tree override still captures modified and new source files.
mkdir -p -- "$SOURCE_STAGE"
while IFS= read -r -d '' relative_path; do
    mkdir -p -- "$SOURCE_STAGE/$(dirname -- "$relative_path")"
    cp -Pp -- "$ROOT/$relative_path" "$SOURCE_STAGE/$relative_path"
done < <(git -C "$ROOT" ls-files -z --cached --others --exclude-standard)

python3 "$ROOT/tools/generate-spdx-sbom.py" \
    --root "$SOURCE_STAGE" \
    --output "$SOURCE_STAGE/SBOM.spdx.json" \
    --version "$VERSION" \
    --commit "$(git -C "$ROOT" rev-parse HEAD)" \
    --epoch "$SOURCE_DATE_EPOCH" \
    --document-name "VOX + DIGS $VERSION Corresponding Source" \
    --purpose SOURCE \
    --namespace-label corresponding-source

python3 -m json.tool "$SOURCE_STAGE/SBOM.spdx.json" >/dev/null

# Archive metadata is normalized; file contents, executable modes, and the
# source-controlled directory shape are preserved.
find "$STAGE_DIR" "$SOURCE_STAGE" -exec \
    touch -h -d "@$SOURCE_DATE_EPOCH" -- {} +
rm -f -- "$BINARY_ARCHIVE" "$SOURCE_ARCHIVE" "$CHECKSUMS"
make_archive "$WORK_DIR" "$ARCHIVE_STEM" "$BINARY_ARCHIVE"
make_archive "$WORK_DIR" "$SOURCE_STEM" "$SOURCE_ARCHIVE"
(
    cd -- "$DIST_DIR"
    sha256sum "$(basename -- "$BINARY_ARCHIVE")" \
        "$(basename -- "$SOURCE_ARCHIVE")" >"$(basename -- "$CHECKSUMS")"
)

printf '\nCreated tester artifacts:\n'
printf '  %s\n' "$BINARY_ARCHIVE" "$SOURCE_ARCHIVE" "$CHECKSUMS"
printf 'Verify with: (cd %q && sha256sum -c %q)\n' \
    "$DIST_DIR" "$(basename -- "$CHECKSUMS")"

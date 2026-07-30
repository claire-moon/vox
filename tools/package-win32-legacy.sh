#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build a self-contained PE32 addendum for the native legacy Win32 frontend.
# The frontend target itself deliberately lives outside this packaging script:
# it must be supplied as the CMake target digs_win32_legacy.

set -Eeuo pipefail
umask 022

ROOT=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
DIST_DIR=${VOX_PACKAGE_DIST_DIR:-"$ROOT/dist"}
VERSION=${VOX_PACKAGE_VERSION:-v0.0.3}
SOURCE_REF=${VOX_LEGACY_SOURCE_REF:-unknown}
SOURCE_COMMIT=${VOX_LEGACY_SOURCE_COMMIT:-}
TARGET=${VOX_WIN32_LEGACY_TARGET:-digs_win32_legacy}
EXECUTABLE=${VOX_WIN32_LEGACY_EXECUTABLE:-}
CC=${CC:-i686-w64-mingw32-gcc-win32}
CXX=${CXX:-i686-w64-mingw32-g++-win32}
RC=${RC:-i686-w64-mingw32-windres}
OBJDUMP=${OBJDUMP:-i686-w64-mingw32-objdump}
WORK_DIR=

usage()
{
    cat <<'EOF'
Usage: tools/package-win32-legacy.sh

Builds the native legacy Win32 frontend target, packages it as a separate
addendum, creates a matching source archive, and validates its PE32 imports.

Environment:
  VOX_PACKAGE_VERSION              Existing release tag, default v0.0.3
  VOX_LEGACY_SOURCE_REF            Checkout ref recorded in source evidence
  VOX_LEGACY_SOURCE_COMMIT         Required exact checkout commit
  VOX_PACKAGE_DIST_DIR             Output directory, default ./dist
  VOX_WIN32_LEGACY_TARGET          CMake target, default digs_win32_legacy
  VOX_WIN32_LEGACY_EXECUTABLE      Optional already-built .exe path
  CC/CXX/RC/OBJDUMP                i686 MinGW-win32 tool paths
EOF
}

die()
{
    printf 'package-win32-legacy: %s\n' "$*" >&2
    exit 1
}

need_command()
{
    command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

copy_file()
{
    local source=$1
    local destination=$2

    [[ -f "$source" ]] || die "required file is missing: $source"
    install -D -m 0644 -- "$source" "$destination"
}

copy_tree()
{
    local source=$1
    local destination=$2

    [[ -d "$source" ]] || die "required directory is missing: $source"
    mkdir -p -- "$destination"
    cp -RPp -- "$source"/. "$destination"/
}

make_tar_archive()
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

make_zip_archive()
{
    local parent=$1
    local member=$2
    local output=$3

    python3 - "$parent" "$member" "$output" "$SOURCE_DATE_EPOCH" <<'PY'
import os
import stat
import sys
import time
import zipfile

parent, member, output, epoch = sys.argv[1:]
root = os.path.join(parent, member)
timestamp = time.gmtime(int(epoch))[:6]
if timestamp[0] < 1980:
    timestamp = (1980, 1, 1, 0, 0, 0)

with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED,
                     compresslevel=9) as archive:
    for directory, names, files in os.walk(root):
        names.sort()
        files.sort()
        for filename in files:
            path = os.path.join(directory, filename)
            relative = os.path.relpath(path, parent).replace(os.sep, "/")
            info = zipfile.ZipInfo(relative, timestamp)
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | 0o644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(path, "rb") as source:
                archive.writestr(info, source.read())
PY
}

legacy_pe_validate()
{
    local executable=$1
    local report=$2
    local dll
    local info
    local forbidden

    info=$("$OBJDUMP" -p "$executable") || die 'could not inspect PE headers'
    printf '%s\n' "$info" >"$report"
    "$OBJDUMP" -f "$executable" >>"$report" || die 'could not inspect PE format'

    grep -Eiq 'file format[[:space:]]+pei-i386' "$report" || \
        die 'legacy executable is not a PE32 i386 image'
    grep -Eq 'MajorOSystemVersion[[:space:]]+4' "$report" || \
        die 'legacy executable does not declare Windows 4.x OS compatibility'
    grep -Eq 'MinorOSystemVersion[[:space:]]+0' "$report" || \
        die 'legacy executable does not declare a Windows 4.0 OS minor version'
    grep -Eq 'MajorSubsystemVersion[[:space:]]+4' "$report" || \
        die 'legacy executable does not declare a Windows 4.x subsystem'
    grep -Eq 'MinorSubsystemVersion[[:space:]]+0' "$report" || \
        die 'legacy executable does not declare a Windows 4.0 subsystem minor version'

    while IFS= read -r dll; do
        case "${dll,,}" in
            kernel32.dll|user32.dll|gdi32.dll|winmm.dll|msvcrt.dll) ;;
            '') ;;
            *) die "legacy executable imports a non-baseline DLL: $dll" ;;
        esac
    done < <(sed -n 's/^[[:space:]]*DLL Name: //p' "$report")

    for forbidden in \
        api-ms-win kernelbase.dll ucrtbase.dll vcruntime msvcp \
        libgcc libstdc++ libwinpthread \
        AddDllDirectory GetTickCount64 GetSystemTimePreciseAsFileTime \
        GetNativeSystemInfo GetModuleHandleEx InitializeCriticalSectionEx \
        SetThreadErrorMode; do
        if grep -Fqi "$forbidden" "$report"; then
            die "legacy executable imports unsupported legacy-baseline symbol: $forbidden"
        fi
    done
}

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    usage
    exit 0
fi
[[ $# -eq 0 ]] || die "unknown argument: $1"

[[ "$VERSION" =~ ^[A-Za-z0-9][A-Za-z0-9._+-]*$ ]] || \
    die 'VOX_PACKAGE_VERSION may contain only letters, digits, dot, underscore, plus, and hyphen'
need_command git
need_command cmake
need_command tar
need_command gzip
need_command sha256sum
need_command python3
need_command install
need_command "$CC"
need_command "$CXX"
need_command "$RC"
need_command "$OBJDUMP"

DIRTY=$(git -C "$ROOT" status --porcelain=v1 --untracked-files=all)
[[ -z "$DIRTY" ]] || die 'the source checkout is dirty; legacy addenda require an exact committed source tree'
ACTUAL_COMMIT=$(git -C "$ROOT" rev-parse HEAD)
[[ -n "$SOURCE_COMMIT" ]] || die 'VOX_LEGACY_SOURCE_COMMIT is required'
[[ "$SOURCE_COMMIT" == "$ACTUAL_COMMIT" ]] || \
    die "source commit mismatch: checkout is $ACTUAL_COMMIT, expected $SOURCE_COMMIT"

if [[ -z ${SOURCE_DATE_EPOCH:-} ]]; then
    SOURCE_DATE_EPOCH=$(git -C "$ROOT" show -s --format=%ct HEAD)
fi
[[ "$SOURCE_DATE_EPOCH" =~ ^[0-9]+$ ]] || \
    die 'SOURCE_DATE_EPOCH must contain a non-negative integer timestamp'
export SOURCE_DATE_EPOCH

ARCHIVE_STEM="vox-digs-$VERSION-windows-win32-legacy"
SOURCE_STEM="${ARCHIVE_STEM}-source"
BINARY_ARCHIVE="$DIST_DIR/$ARCHIVE_STEM.zip"
SOURCE_ARCHIVE="$DIST_DIR/$SOURCE_STEM.tar.gz"
CHECKSUMS="$DIST_DIR/$ARCHIVE_STEM.SHA256SUMS"
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/vox-win32-legacy-package.XXXXXXXX")
BUILD_DIR="$WORK_DIR/build"
STAGE_DIR="$WORK_DIR/$ARCHIVE_STEM"
SOURCE_STAGE="$WORK_DIR/$SOURCE_STEM"
EVIDENCE_DIR="$STAGE_DIR/EVIDENCE"

cleanup()
{
    if [[ -n "$WORK_DIR" && -d "$WORK_DIR" ]]; then
        rm -rf -- "$WORK_DIR"
    fi
}
trap cleanup EXIT HUP INT TERM

mkdir -p -- "$DIST_DIR"
if [[ -z "$EXECUTABLE" ]]; then
    cmake -S "$ROOT" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_SYSTEM_PROCESSOR=x86 \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        -DCMAKE_RC_COMPILER="$RC" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DVOX_BUILD_TESTS=OFF \
        -DVOX_BUILD_SDL2_DEMO=OFF \
        -DVOX_BUILD_NASM_ACCEL=OFF \
        -DVOX_BUILD_WIN32_LEGACY_DEMO=ON \
        -DVOX_BUILD_LEGACY_PROFILE=ON
    cmake --build "$BUILD_DIR" --config Release --target "$TARGET" --parallel 2 || \
        die "CMake could not build the required native legacy target '$TARGET'"
    for candidate in \
        "$BUILD_DIR/$TARGET.exe" \
        "$BUILD_DIR/Release/$TARGET.exe" \
        "$BUILD_DIR/bin/$TARGET.exe"; do
        if [[ -f "$candidate" ]]; then
            EXECUTABLE=$candidate
            break
        fi
    done
fi

[[ -n "$EXECUTABLE" && -f "$EXECUTABLE" ]] || \
    die "the required native legacy executable was not produced: $TARGET.exe"

mkdir -p -- "$STAGE_DIR/SHARE" "$EVIDENCE_DIR" \
    "$STAGE_DIR/LICENSES"
install -m 0755 -- "$EXECUTABLE" "$STAGE_DIR/DIGS95.EXE"

if [[ -d "$BUILD_DIR/share" ]]; then
    copy_tree "$BUILD_DIR/share" "$STAGE_DIR/SHARE"
else
    mkdir -p -- "$STAGE_DIR/SHARE/digs"
    copy_tree "$ROOT/games/digs/scripts" "$STAGE_DIR/SHARE/digs/scripts"
    copy_tree "$ROOT/games/digs/catalog" "$STAGE_DIR/SHARE/digs/catalog"
    copy_tree "$ROOT/games/digs/icons" "$STAGE_DIR/SHARE/digs/icons"
    mkdir -p -- "$STAGE_DIR/SHARE/digs/controllers"
    copy_file "$ROOT/third_party/SDL_GameControllerDB/gamecontrollerdb.txt" \
        "$STAGE_DIR/SHARE/digs/controllers/gamecontrollerdb.txt"
fi
[[ -f "$STAGE_DIR/SHARE/digs/scripts/manifest.txt" ]] || \
    die 'the legacy package is missing the DIGS script manifest'
[[ -f "$STAGE_DIR/SHARE/digs/controllers/gamecontrollerdb.txt" ]] || \
    die 'the legacy package is missing the controller database'

copy_file "$ROOT/packaging/win32-legacy/RUN-DIGS.BAT" \
    "$STAGE_DIR/RUN-DIGS.BAT"
copy_file "$ROOT/packaging/win32-legacy/START-HERE.TXT" \
    "$STAGE_DIR/START-HERE.TXT"
copy_file "$ROOT/packaging/win32-legacy/THIRD-PARTY.TXT" \
    "$STAGE_DIR/THIRD-PARTY.TXT"
copy_file "$ROOT/LICENSE" "$STAGE_DIR/LICENSE"
copy_file "$ROOT/LICENSES/Lua-5.1.txt" "$STAGE_DIR/LICENSES/Lua-5.1.txt"
copy_file "$ROOT/LICENSES/SDL_GameControllerDB.txt" \
    "$STAGE_DIR/LICENSES/SDL_GameControllerDB.txt"
copy_file "$ROOT/packaging/win32-legacy/EVIDENCE-README.TXT" \
    "$EVIDENCE_DIR/README.TXT"

legacy_pe_validate "$STAGE_DIR/DIGS95.EXE" "$EVIDENCE_DIR/PE-IMPORTS.TXT"
{
    printf 'VOX + DIGS Win32 legacy addendum build evidence\n'
    printf 'Release tag: %s\n' "$VERSION"
    printf 'Source ref: %s\n' "$SOURCE_REF"
    printf 'Source commit: %s\n' "$ACTUAL_COMMIT"
    printf 'SOURCE_DATE_EPOCH: %s\n' "$SOURCE_DATE_EPOCH"
    printf 'C compiler: %s\n' "$("$CC" --version | sed -n '1p')"
    printf 'C++ compiler: %s\n' "$("$CXX" --version | sed -n '1p')"
    printf 'PE inspector: %s\n' "$("$OBJDUMP" --version | sed -n '1p')"
    printf 'PE target: PE32 i386, OS/subsystem version 4.0\n'
    printf 'Allowed imported DLLs: KERNEL32.dll USER32.dll GDI32.dll WINMM.dll MSVCRT.dll\n'
    printf 'Legacy host status: native GDI/WinMM profile; PE import audit passed\n'
    printf 'SDL2: not linked and not distributed by this legacy bundle\n'
} >"$STAGE_DIR/BUILD-INFO.TXT"
copy_file "$STAGE_DIR/BUILD-INFO.TXT" "$EVIDENCE_DIR/BUILD-INFO.TXT"
printf 'Source ref: %s\nSource commit: %s\n' "$SOURCE_REF" "$ACTUAL_COMMIT" \
    >"$STAGE_DIR/SOURCE-COMMIT.TXT"

python3 "$ROOT/tools/generate-spdx-sbom.py" \
    --root "$STAGE_DIR" \
    --output "$STAGE_DIR/SBOM.spdx.json" \
    --version "$VERSION" \
    --commit "$ACTUAL_COMMIT" \
    --epoch "$SOURCE_DATE_EPOCH" \
    --document-name "VOX + DIGS $VERSION Windows Win32 legacy binary addendum" \
    --purpose APPLICATION \
    --namespace-label windows-win32-legacy-binary \
    --no-sdl2
python3 -m json.tool "$STAGE_DIR/SBOM.spdx.json" >/dev/null

git -C "$ROOT" archive --format=tar --prefix="$SOURCE_STEM/" HEAD | \
    tar -xf - -C "$WORK_DIR"
[[ -d "$SOURCE_STAGE" ]] || die 'could not stage the exact source checkout'
printf 'Source ref: %s\nSource commit: %s\n' "$SOURCE_REF" "$ACTUAL_COMMIT" \
    >"$SOURCE_STAGE/SOURCE-COMMIT.txt"
python3 "$ROOT/tools/generate-spdx-sbom.py" \
    --root "$SOURCE_STAGE" \
    --output "$SOURCE_STAGE/SBOM.spdx.json" \
    --version "$VERSION" \
    --commit "$ACTUAL_COMMIT" \
    --epoch "$SOURCE_DATE_EPOCH" \
    --document-name "VOX + DIGS $VERSION Win32 legacy Corresponding Source" \
    --purpose SOURCE \
    --namespace-label windows-win32-legacy-source
python3 -m json.tool "$SOURCE_STAGE/SBOM.spdx.json" >/dev/null

find "$STAGE_DIR" "$SOURCE_STAGE" -exec touch -h -d "@$SOURCE_DATE_EPOCH" -- {} +
rm -f -- "$BINARY_ARCHIVE" "$SOURCE_ARCHIVE" "$CHECKSUMS"
make_zip_archive "$WORK_DIR" "$ARCHIVE_STEM" "$BINARY_ARCHIVE"
make_tar_archive "$WORK_DIR" "$SOURCE_STEM" "$SOURCE_ARCHIVE"
(
    cd -- "$DIST_DIR"
    sha256sum "$(basename -- "$BINARY_ARCHIVE")" \
        "$(basename -- "$SOURCE_ARCHIVE")" >"$(basename -- "$CHECKSUMS")"
    sha256sum -c "$(basename -- "$CHECKSUMS")"
)

python3 - "$BINARY_ARCHIVE" "$ARCHIVE_STEM" <<'PY'
import sys
import zipfile

archive, stem = sys.argv[1:]
expected = {
    stem + "/DIGS95.EXE",
    stem + "/SHARE/digs/scripts/manifest.txt",
    stem + "/RUN-DIGS.BAT",
    stem + "/BUILD-INFO.TXT",
    stem + "/SBOM.spdx.json",
    stem + "/EVIDENCE/PE-IMPORTS.TXT",
    stem + "/SOURCE-COMMIT.TXT",
}
with zipfile.ZipFile(archive) as bundle:
    names = set(bundle.namelist())
missing = sorted(expected - names)
if missing:
    raise SystemExit("legacy ZIP missing: " + ", ".join(missing))
PY
tar -tzf "$SOURCE_ARCHIVE" | grep -Fqx "$SOURCE_STEM/SOURCE-COMMIT.txt" || \
    die 'the legacy Corresponding Source archive is missing SOURCE-COMMIT.txt'

printf 'Created isolated legacy addendum assets:\n'
printf '  %s\n' "$BINARY_ARCHIVE" "$SOURCE_ARCHIVE" "$CHECKSUMS"
printf 'The existing release SHA256SUMS is intentionally not modified.\n'

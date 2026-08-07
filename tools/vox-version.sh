#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Print the version of this tree, read from the one file that holds it.
#
# C code gets the same number through the generated vox/vox_version.h.  Shell
# and PowerShell cannot include a C header, so they come here instead, and
# nothing outside these two paths is allowed to write the number down --
# tools/vox-version-check.sh enforces that.
#
# Usage:
#   tools/vox-version.sh            0.0.4
#   tools/vox-version.sh --tag      v0.0.4
#   tools/vox-version.sh --title    DIGS v0.0.4
#
# Exit status:
#   0  printed
#   2  the VERSION file is missing or malformed
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
VERSION_FILE=$ROOT/VERSION

if [ ! -r "$VERSION_FILE" ]; then
    printf 'vox-version: cannot read %s\n' "$VERSION_FILE" >&2
    exit 2
fi

VERSION=$(tr -d ' \t\r\n' < "$VERSION_FILE")

# The same shape CMake insists on, so the two readers cannot disagree about
# what counts as a valid version.
if ! printf '%s' "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    printf 'vox-version: VERSION must hold a bare MAJOR.MINOR.PATCH, got "%s"\n' \
        "$VERSION" >&2
    exit 2
fi

case ${1:-} in
    ''|--plain) printf '%s\n' "$VERSION" ;;
    --tag)      printf 'v%s\n' "$VERSION" ;;
    --title)    printf 'DIGS v%s\n' "$VERSION" ;;
    *)
        printf 'vox-version: unknown option %s\n' "$1" >&2
        exit 2
        ;;
esac

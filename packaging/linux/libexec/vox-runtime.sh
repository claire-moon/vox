#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later

# Shared runtime checks for the Linux binary-bundle launchers. This file is
# sourced by the scripts at the root of the extracted archive.

vox_die()
{
    printf 'DIGS: %s\n' "$*" >&2
    exit 1
}
vox_require_executable()
{
    local binary=$1

    if [[ ! -x "$binary" ]]; then
        vox_die "missing executable: $binary (extract the complete archive and try again)"
    fi
}

vox_print_sdl2_help()
{
    cat >&2 <<'EOF'
DIGS needs the SDL2 runtime library supplied by your Linux distribution.
Install it, then run this script again. Common commands are:
  Debian / Ubuntu / Mint: sudo apt install libsdl2-2.0-0
  Fedora:                 sudo dnf install SDL2
  Arch Linux:             sudo pacman -S sdl2
  openSUSE:               sudo zypper install libSDL2-2_0-0
EOF
}

vox_require_runtime_libraries()
{
    local binary=$1
    local dependencies

    if ! command -v ldd >/dev/null 2>&1; then
        printf '%s\n' \
            'DIGS: warning: ldd is unavailable; skipping the runtime-library preflight.' >&2
        return 0
    fi

    if ! dependencies=$(LC_ALL=C ldd "$binary" 2>&1); then
        printf '%s\n' "$dependencies" >&2
        if grep -Eiq 'GLIBC_[0-9.]+.*not found|version .*not found' <<<"$dependencies"; then
            vox_die 'this DIGS binary needs a newer Linux runtime; download the release bundle for your system or build from source'
        fi
        vox_die "could not inspect runtime libraries for $binary"
    fi
    if grep -F 'not found' <<<"$dependencies" >/dev/null; then
        printf '%s\n' 'DIGS: one or more runtime libraries are missing:' >&2
        printf '%s\n' "$dependencies" | grep -F 'not found' >&2
        if grep -Eiq 'GLIBC_[0-9.]+.*not found|version .*not found' <<<"$dependencies"; then
            vox_die 'this DIGS binary needs a newer Linux runtime; download the release bundle for your system or build from source'
        fi
        if grep -Eiq 'SDL2|libSDL' <<<"$dependencies"; then
            vox_print_sdl2_help
        fi
        exit 1
    fi
}

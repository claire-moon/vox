#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fail if anything that identifies the product names a version other than the
# one in the VERSION file.
#
# v0.0.3 shipped its number written out by hand in more than twenty files.
# The number now lives in VERSION, reaches C through the generated
# vox/vox_version.h and shell through tools/vox-version.sh, and this gate
# stops a stale literal from creeping back into a surface a player sees.
#
# WHAT IS CHECKED, AND WHAT IS NOT
#
# Only product-identity surfaces: the build system, the packaging scripts and
# the text they ship, CI artefact names, the port sources, and the documents
# that tell a player what to download.  A wrong number in one of those is a
# wrong number on somebody's screen.
#
# Prose that discusses project history is NOT in scope by path, and neither
# are the records of previous releases.  A document explaining what changed
# since v0.0.3 has to be able to say "v0.0.3".  Exemptions live in
# tools/version-exceptions.txt, each with the reason it is there, and a stale
# exemption is reported so the list cannot quietly rot.
#
# Third-party versions are never matched: the pattern only recognises this
# product's own 0.0.x numbering, so Lua 5.1.5 and CPython 3.12.3 are invisible
# to it.
#
# Usage:  tools/vox-version-check.sh [--verbose]
#
# Exit status:
#   0  every product surface agrees with VERSION
#   1  a surface disagrees, or an exemption no longer applies
#   2  usage or environment error
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
VERBOSE=0

case ${1:-} in
    '') ;;
    --verbose) VERBOSE=1 ;;
    *) printf 'vox-version-check: unknown option %s\n' "$1" >&2; exit 2 ;;
esac

VERSION=$("$ROOT/tools/vox-version.sh") || exit 2
EXCEPTIONS=$ROOT/tools/version-exceptions.txt

# Product-identity surfaces.  Directories are searched for tracked files only,
# so a stray build artefact cannot fail the gate.
SCOPE='
CMakeLists.txt
README.md
CG-README.TXT
ROADMAP.txt
THIRD_PARTY.md
tools/package-linux-demo.sh
tools/package-windows-demo.ps1
tools/vox-verify.sh
.github/workflows/ci.yml
.github/workflows/publish-release.yml
packaging
ports
engine
games
qa/README.md
docs/architecture
docs/compatibility
'

# Only this product's numbering.  Anything else in the tree is somebody
# else's version and none of our business.
PATTERN='v\{0,1\}0\.0\.[0-9]\{1,\}'

WORK=${TMPDIR:-/tmp}/vox-version-check.$$
trap 'rm -f "$WORK" "$WORK.hits" "$WORK.used"' EXIT INT TERM
: > "$WORK.hits"
: > "$WORK.used"

# Collect the files in scope that git knows about.
files=''
for entry in $SCOPE; do
    [ -e "$ROOT/$entry" ] || continue
    if [ -d "$ROOT/$entry" ]; then
        listing=$(git -C "$ROOT" ls-files -- "$entry")
    else
        listing=$(git -C "$ROOT" ls-files -- "$entry")
    fi
    [ -n "$listing" ] || continue
    files="$files$listing
"
done

# Every occurrence that is not the current version.
for file in $files; do
    [ -r "$ROOT/$file" ] || continue
    # Binary files have no version literals worth reading.
    case $(file -b --mime-encoding "$ROOT/$file" 2>/dev/null || echo unknown) in
        binary) continue ;;
    esac
    grep -n "$PATTERN" "$ROOT/$file" 2>/dev/null |
    while IFS= read -r hit; do
        line_no=${hit%%:*}
        text=${hit#*:}
        # Strip every mention of the current version, then see if any other
        # version survives.  A line may name both.
        residue=$(printf '%s' "$text" |
                  sed "s/v\{0,1\}$VERSION//g")
        if printf '%s' "$residue" | grep -q "$PATTERN"; then
            printf '%s\t%s\t%s\n' "$file" "$line_no" "$text" >> "$WORK.hits"
        fi
    done
done

# Apply the exemptions, recording which ones were needed.
status=0
if [ -s "$WORK.hits" ]; then
    while IFS="$(printf '\t')" read -r file line_no text; do
        exempt=0
        if [ -r "$EXCEPTIONS" ]; then
            while IFS= read -r rule; do
                # Comments and blank lines.
                case $rule in ''|'#'*) continue ;; esac
                rule_path=${rule%%'::'*}
                rule_path=$(printf '%s' "$rule_path" | sed 's/[[:space:]]*$//')
                [ "$rule_path" = "$file" ] || continue
                if [ "$rule" = "$rule_path" ]; then
                    # Whole-file exemption.
                    exempt=1
                    printf '%s\n' "$rule_path" >> "$WORK.used"
                    break
                fi
                rule_text=${rule#*'::'}
                rule_text=$(printf '%s' "$rule_text" | sed 's/^[[:space:]]*//')
                case $text in
                    *"$rule_text"*)
                        exempt=1
                        printf '%s:: %s\n' "$rule_path" "$rule_text" \
                            >> "$WORK.used"
                        break
                        ;;
                esac
            done < "$EXCEPTIONS"
        fi
        if [ "$exempt" -eq 0 ]; then
            printf '  %s:%s: %s\n' "$file" "$line_no" "$text"
            status=1
        elif [ "$VERBOSE" -eq 1 ]; then
            printf '  exempt  %s:%s\n' "$file" "$line_no" >&2
        fi
    done < "$WORK.hits" > "$WORK"

    if [ -s "$WORK" ]; then
        printf 'vox-version-check: product surfaces disagree with VERSION (%s)\n' \
            "$VERSION" >&2
        cat "$WORK" >&2
        printf '\nEither update the surface, or -- if it is describing history\n' >&2
        printf 'rather than this build -- add it to %s\n' \
            "tools/version-exceptions.txt" >&2
        printf 'with the reason it belongs there.\n' >&2
        exit 1
    fi
fi

# A rule that never fired is describing a problem that no longer exists.
# Leaving it in place would silently exempt a future mistake.
if [ -r "$EXCEPTIONS" ]; then
    stale=0
    while IFS= read -r rule; do
        case $rule in ''|'#'*) continue ;; esac
        rule_path=${rule%%'::'*}
        rule_path=$(printf '%s' "$rule_path" | sed 's/[[:space:]]*$//')
        if [ "$rule" = "$rule_path" ]; then
            key=$rule_path
        else
            rule_text=${rule#*'::'}
            rule_text=$(printf '%s' "$rule_text" | sed 's/^[[:space:]]*//')
            key="$rule_path:: $rule_text"
        fi
        if ! grep -Fqx "$key" "$WORK.used" 2>/dev/null; then
            printf '  %s\n' "$key" >&2
            stale=1
        fi
    done < "$EXCEPTIONS"
    if [ "$stale" -eq 1 ]; then
        printf 'vox-version-check: the rules above exempted nothing.\n' >&2
        printf 'Remove them -- a rule that matches nothing today will exempt\n' >&2
        printf 'the wrong thing tomorrow.\n' >&2
        exit 1
    fi
fi

printf 'vox-version-check: every product surface reads %s\n' "$VERSION"
exit 0

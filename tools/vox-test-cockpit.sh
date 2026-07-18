#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -u

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
OUT_ROOT=$ROOT/qa/out
BINARY=
BENCHMARK_FRAMES=240
WORKBOOK_LIST=
FAILURES=0

usage()
{
    cat <<'EOF'
Usage: tools/vox-test-cockpit.sh [OPTIONS] WORKBOOK.xlsx [WORKBOOK.xlsx ...]

Combine VOX QA workbooks into a reviewable local evidence packet.

Options:
  --binary PATH             Run PATH --smoke-test and --benchmark.
  --benchmark-frames COUNT  Frames per Lightfield tier (default: 240).
  -h, --help                Show this help.

Requires xleak and tar. Nothing is uploaded. Output is written under qa/out/.
Review every generated file for private information before sharing the packet.
EOF
}

die()
{
    echo "vox-test-cockpit: $*" >&2
    exit 2
}

need_command()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        die "missing '$1'. Install it and ensure it is available on PATH."
    fi
}

append_workbook()
{
    if [ -z "$WORKBOOK_LIST" ]; then
        WORKBOOK_LIST=$1
    else
        WORKBOOK_LIST=$WORKBOOK_LIST'
'$1
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --binary)
            [ "$#" -ge 2 ] || die "--binary requires a path"
            BINARY=$2
            shift 2
            ;;
        --benchmark-frames)
            [ "$#" -ge 2 ] || die "--benchmark-frames requires a count"
            BENCHMARK_FRAMES=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            while [ "$#" -gt 0 ]; do
                append_workbook "$1"
                shift
            done
            ;;
        -*)
            die "unknown option: $1"
            ;;
        *)
            append_workbook "$1"
            shift
            ;;
    esac
done

[ -n "$WORKBOOK_LIST" ] || {
    usage >&2
    die "provide at least one .xlsx workbook"
}

case "$BENCHMARK_FRAMES" in
    ''|*[!0-9]*) die "--benchmark-frames must be an integer from 1 to 100000" ;;
esac
if ! { [ "$BENCHMARK_FRAMES" -ge 1 ] 2>/dev/null &&
       [ "$BENCHMARK_FRAMES" -le 100000 ] 2>/dev/null; }; then
    die "--benchmark-frames must be an integer from 1 to 100000"
fi

need_command xleak
need_command tar
need_command awk
need_command sed
need_command uname

if [ -n "$BINARY" ]; then
    [ -f "$BINARY" ] || die "binary does not exist: $BINARY"
    [ -x "$BINARY" ] || die "binary is not executable: $BINARY"
    BINARY_DIR=$(CDPATH='' cd -- "$(dirname "$BINARY")" 2>/dev/null && pwd) ||
        die "cannot resolve binary directory: $BINARY"
    BINARY=$BINARY_DIR/$(basename "$BINARY")
fi

STAMP=$(date -u '+%Y%m%dT%H%M%SZ' 2>/dev/null || date '+%Y%m%dT%H%M%S')
RUN_ID=vox-qa-$STAMP-$$
RUN_DIR=$OUT_ROOT/$RUN_ID
EXPORT_DIR=$RUN_DIR/exports
WORKBOOK_DIR=$RUN_DIR/workbooks
LOG_DIR=$RUN_DIR/logs
REPORT=$RUN_DIR/REPORT.md
SYSTEM=$RUN_DIR/system.txt
mkdir -p "$EXPORT_DIR" "$WORKBOOK_DIR" "$LOG_DIR" ||
    die "cannot create output directory: $RUN_DIR"

printf '%s\n' \
    'VOX QA system profile' \
    'Privacy: host name, user name, network identifiers, environment variables, and serial numbers are intentionally omitted.' \
    "Captured UTC: $STAMP" \
    "Operating system: $(uname -s 2>/dev/null || printf 'unknown')" \
    "Kernel release: $(uname -r 2>/dev/null || printf 'unknown')" \
    "Architecture: $(uname -m 2>/dev/null || printf 'unknown')" >"$SYSTEM"

if [ -r /etc/os-release ]; then
    sed -n 's/^PRETTY_NAME=//p' /etc/os-release | sed 's/^"//; s/"$//' |
        sed 's/^/Distribution: /' >>"$SYSTEM"
fi
if [ -r /proc/cpuinfo ]; then
    sed -n 's/^model name[[:space:]]*:[[:space:]]*/CPU model: /p' /proc/cpuinfo |
        sed -n '1p' >>"$SYSTEM"
fi
if command -v getconf >/dev/null 2>&1; then
    printf 'Logical CPUs: %s\n' "$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf 'unknown')" >>"$SYSTEM"
fi
if [ -r /proc/meminfo ]; then
    sed -n 's/^MemTotal:[[:space:]]*/Memory total: /p' /proc/meminfo >>"$SYSTEM"
fi
if command -v lspci >/dev/null 2>&1; then
    lspci 2>/dev/null | sed -n '/VGA compatible controller:/p; /3D controller:/p' |
        sed 's/^[^ ]* /GPU: /' >>"$SYSTEM"
fi
printf 'xleak: %s\n' "$(xleak --version 2>/dev/null || printf 'unknown')" >>"$SYSTEM"

for combined in checkpoints issues environment; do
    : >"$RUN_DIR/combined-$combined.csv"
done

merge_csv()
{
    csv_source=$1
    csv_input=$2
    csv_output=$3
    csv_header=$4
    csv_temporary=$csv_output.merge.$$
    if awk -v source="$csv_source" -v include_header="$csv_header" '
        function odd_quotes(value, copy) {
            copy = value
            return gsub(/"/, "", copy) % 2
        }
        BEGIN {
            open_record = 0
            record = 0
            emit = 0
        }
        {
            if (!open_record) {
                record++
                emit = (record > 1 || include_header == "yes")
                if (emit) {
                    if (record == 1) {
                        printf "Source Workbook,"
                    } else {
                        printf "%s,", source
                    }
                }
            }
            if (emit) {
                print $0
            }
            if (odd_quotes($0)) {
                open_record = !open_record
            }
        }
        END {
            if (open_record) {
                exit 42
            }
        }
    ' "$csv_input" >"$csv_temporary"; then
        if [ "$csv_header" = yes ]; then
            cp "$csv_temporary" "$csv_output"
        else
            cat "$csv_temporary" >>"$csv_output"
        fi
        rm -f "$csv_temporary"
        return 0
    fi
    rm -f "$csv_temporary"
    return 1
}

WORKBOOK_COUNT=0
OLD_IFS=$IFS
IFS='
'
for workbook in $WORKBOOK_LIST; do
    IFS=$OLD_IFS
    WORKBOOK_COUNT=$((WORKBOOK_COUNT + 1))
    [ -f "$workbook" ] || die "workbook does not exist: $workbook"
    [ -r "$workbook" ] || die "workbook is not readable: $workbook"
    case "$workbook" in
        *.xlsx|*.XLSX) ;;
        *) die "expected an .xlsx workbook: $workbook" ;;
    esac
    workbook_dir=$(CDPATH='' cd -- "$(dirname "$workbook")" 2>/dev/null && pwd) ||
        die "cannot resolve workbook directory: $workbook"
    workbook=$workbook_dir/$(basename "$workbook")
    workbook_name=$(basename "$workbook")
    safe_name=$(printf '%s' "$workbook_name" | tr -c 'A-Za-z0-9._-' '_')
    copy_name=$(printf '%02d-%s' "$WORKBOOK_COUNT" "$safe_name")
    cp "$workbook" "$WORKBOOK_DIR/$copy_name" || die "cannot copy $workbook_name"

    for sheet in Checkpoints Issues Environment; do
        sheet_key=$(printf '%s' "$sheet" | tr '[:upper:]' '[:lower:]')
        export_path=$EXPORT_DIR/${WORKBOOK_COUNT}-${sheet_key}.csv
        error_path=$LOG_DIR/${WORKBOOK_COUNT}-${sheet_key}-xleak.log
        if xleak --sheet "$sheet" --export csv --max-rows 0 "$workbook" >"$export_path" 2>"$error_path"; then
            combined=$RUN_DIR/combined-$sheet_key.csv
            if [ ! -s "$combined" ]; then
                csv_header=yes
            else
                csv_header=no
            fi
            if ! merge_csv "$copy_name" "$export_path" "$combined" "$csv_header"; then
                printf 'CSV merge rejected malformed quoted data in sheet %s from %s\n' "$sheet" "$workbook_name" >>"$error_path"
                FAILURES=$((FAILURES + 1))
            fi
        else
            printf 'xleak could not export sheet %s from %s\n' "$sheet" "$workbook_name" >>"$error_path"
            FAILURES=$((FAILURES + 1))
        fi
    done
    IFS='
'
done
IFS=$OLD_IFS

SMOKE_STATUS="Skipped (no --binary supplied)"
BENCHMARK_STATUS="Skipped (no --binary supplied)"
VOX_HEADLESS_STATUS="Skipped (not found beside demo binary)"
DIGS_HEADLESS_STATUS="Skipped (not found beside demo binary)"

if [ -n "$BINARY" ]; then
    BINARY_BASE=$(basename "$BINARY")
    BINARY_SHA=unavailable
    if command -v sha256sum >/dev/null 2>&1; then
        BINARY_SHA=$(sha256sum "$BINARY" | sed 's/[[:space:]].*$//')
    elif command -v shasum >/dev/null 2>&1; then
        BINARY_SHA=$(shasum -a 256 "$BINARY" | sed 's/[[:space:]].*$//')
    fi
    printf 'Demo binary: %s\nDemo binary SHA-256: %s\n' "$BINARY_BASE" "$BINARY_SHA" >>"$SYSTEM"
    if (cd "$RUN_DIR" && "$BINARY" --smoke-test smoke.ppm) >"$LOG_DIR/smoke.log" 2>&1; then
        if [ -s "$RUN_DIR/smoke.ppm" ]; then
            SMOKE_STATUS=Pass
        else
            SMOKE_STATUS="Fail (no smoke.ppm produced)"
            FAILURES=$((FAILURES + 1))
        fi
    else
        SMOKE_STATUS=Fail
        FAILURES=$((FAILURES + 1))
    fi
    if (cd "$RUN_DIR" && "$BINARY" --benchmark "$BENCHMARK_FRAMES") >"$LOG_DIR/benchmark.log" 2>&1; then
        BENCHMARK_STATUS=Pass
    else
        BENCHMARK_STATUS=Fail
        FAILURES=$((FAILURES + 1))
    fi

    if [ -x "$BINARY_DIR/vox_headless" ]; then
        if "$BINARY_DIR/vox_headless" >"$LOG_DIR/vox-headless.log" 2>&1; then
            VOX_HEADLESS_STATUS=Pass
        else
            VOX_HEADLESS_STATUS=Fail
            FAILURES=$((FAILURES + 1))
        fi
    fi
    if [ -x "$BINARY_DIR/digs_headless" ]; then
        if "$BINARY_DIR/digs_headless" >"$LOG_DIR/digs-headless.log" 2>&1; then
            DIGS_HEADLESS_STATUS=Pass
        else
            DIGS_HEADLESS_STATUS=Fail
            FAILURES=$((FAILURES + 1))
        fi
    fi
fi

{
    printf '%s\n\n' '# VOX + DIGS QA evidence report'
    printf '%s\n\n' '> Review this report, copied workbooks, logs, screenshots, and videos for private information before sharing. This script intentionally does not collect environment variables, host names, user names, network identifiers, or hardware serial numbers.'
    printf '%s\n\n' '## Run summary'
    printf '%s\n' "- Run ID: \`$RUN_ID\`" "- Workbooks: $WORKBOOK_COUNT" "- xleak: \`$(xleak --version 2>/dev/null || printf unknown)\`"
    printf '%s\n' "- Smoke test: $SMOKE_STATUS" "- Benchmark ($BENCHMARK_FRAMES frames per tier): $BENCHMARK_STATUS" "- VOX headless proof: $VOX_HEADLESS_STATUS" "- DIGS headless proof: $DIGS_HEADLESS_STATUS" "- Failed automation/export steps: $FAILURES"
    printf '\n%s\n\n' '## System profile'
    sed 's/^/    /' "$SYSTEM"
    for sheet_key in checkpoints issues environment; do
        case "$sheet_key" in
            checkpoints) title=Checkpoints ;;
            issues) title=Issues ;;
            environment) title=Environment ;;
        esac
        printf '\n## Combined %s export\n\n' "$title"
        if [ -s "$RUN_DIR/combined-$sheet_key.csv" ]; then
            sed 's/^/    /' "$RUN_DIR/combined-$sheet_key.csv"
        else
            printf '%s\n' '_No rows were exported. See xleak logs._'
        fi
    done
    printf '\n%s\n\n' '## Evidence index'
    # Backticks are intentional Markdown, not shell substitutions.
    # shellcheck disable=SC2016
    printf '%s\n' '- `workbooks/`: exact local copies supplied to this run.' '- `exports/`: per-workbook CSV exported by xleak.' '- `combined-*.csv`: merged sheet exports with source workbook labels.' '- `logs/`: xleak diagnostics plus optional smoke, benchmark, and headless output.' '- `smoke.ppm`: deterministic non-windowed frame when the smoke test passed.' '- `system.txt`: privacy-conscious machine and binary profile.'
    printf '\nGenerated locally; no files were uploaded.\n'
} >"$REPORT"

PACKET=$OUT_ROOT/$RUN_ID.tar.gz
if ! tar -czf "$PACKET" -C "$RUN_DIR" .; then
    die "could not create evidence packet: $PACKET"
fi

printf 'VOX QA report: %s\n' "$REPORT"
printf 'VOX QA packet: %s\n' "$PACKET"
if [ "$FAILURES" -ne 0 ]; then
    echo "VOX QA cockpit completed with $FAILURES failed step(s); inspect REPORT.md and logs." >&2
    exit 1
fi
echo "VOX QA cockpit completed successfully."

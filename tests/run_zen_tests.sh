#!/usr/bin/env bash
# ==============================================================
# run_zen_tests.sh — Snapshot test runner for tests/*.zen
#
# Usage:
#   tests/run_zen_tests.sh <zen-binary> [--update]
#
# Behaviour:
#   - For each tests/*.zen file (NOT in tests/snapshot_skip.txt):
#       1. Run with the given binary (combined stdout+stderr).
#       2. Strip trailing whitespace from each line, drop trailing blank
#          lines (output-end normalisation).
#       3. Compare with tests/expected/<name>.out.
#   - Files listed in tests/snapshot_skip.txt are skipped (one
#     basename per line; lines starting with # are comments).
#   - With --update the expected snapshots are (re)generated and
#     no comparison is performed.
#
# Exit code:
#   0  = all snapshots match (or --update succeeded)
#   1  = at least one mismatch
#   2  = usage / setup error
# ==============================================================

set -u

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <zen-binary> [--update]" >&2
    exit 2
fi

BIN="$1"
UPDATE=0
if [[ "${2:-}" == "--update" ]]; then
    UPDATE=1
fi

if [[ ! -x "$BIN" ]]; then
    echo "error: '$BIN' is not an executable file" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TESTS_DIR="$ROOT/tests"
EXPECTED_DIR="$TESTS_DIR/expected"
SKIP_FILE="$TESTS_DIR/snapshot_skip.txt"

mkdir -p "$EXPECTED_DIR"

# Build skip set
declare -A SKIP=()
if [[ -f "$SKIP_FILE" ]]; then
    while IFS= read -r line; do
        line="${line%%#*}"          # strip comments
        line="${line//[$'\t\r ']/}" # strip whitespace
        [[ -z "$line" ]] && continue
        SKIP["$line"]=1
    done < "$SKIP_FILE"
fi

# Normalisation: rstrip each line, drop trailing blank lines.
normalize() {
    sed -E 's/[[:space:]]+$//' | awk '
        { buf[NR] = $0; lastnonblank = ($0 == "" ? lastnonblank : NR) }
        END { for (i = 1; i <= lastnonblank; i++) print buf[i] }
    '
}

PASS=0
FAIL=0
SKIPPED=0
UPDATED=0
FAIL_NAMES=""

shopt -s nullglob
for src in "$TESTS_DIR"/*.zen; do
    name="$(basename "$src" .zen)"
    base="$(basename "$src")"

    if [[ -n "${SKIP[$base]:-}" ]]; then
        printf '  [SKIP] %s\n' "$name"
        ((SKIPPED++))
        continue
    fi

    expected_path="$EXPECTED_DIR/$name.out"
    actual="$("$BIN" "$src" 2>&1 < /dev/null | normalize)"

    if [[ $UPDATE -eq 1 ]]; then
        printf '%s\n' "$actual" > "$expected_path"
        printf '  [UPD ] %s\n' "$name"
        ((UPDATED++))
        continue
    fi

    if [[ ! -f "$expected_path" ]]; then
        printf '  [MISS] %s (no expected snapshot — run with --update)\n' "$name"
        ((FAIL++))
        FAIL_NAMES+="    $name (missing snapshot)"$'\n'
        continue
    fi

    expected="$(cat "$expected_path" | normalize)"
    if [[ "$actual" == "$expected" ]]; then
        ((PASS++))
        printf '  [ OK ] %s\n' "$name"
    else
        ((FAIL++))
        FAIL_NAMES+="    $name"$'\n'
        printf '  [FAIL] %s\n' "$name"
        if [[ -n "${ZEN_TEST_VERBOSE:-}" ]]; then
            diff <(printf '%s\n' "$expected") <(printf '%s\n' "$actual") | head -40 | sed 's/^/        /'
        fi
    fi
done
shopt -u nullglob

echo
if [[ $UPDATE -eq 1 ]]; then
    echo "Updated $UPDATED snapshot(s); skipped $SKIPPED."
    exit 0
fi

echo "passed=$PASS  failed=$FAIL  skipped=$SKIPPED"
if [[ $FAIL -gt 0 ]]; then
    echo "Failures:"
    printf '%s' "$FAIL_NAMES"
    echo "(set ZEN_TEST_VERBOSE=1 to see diffs; or run with --update to refresh snapshots)"
    exit 1
fi
exit 0

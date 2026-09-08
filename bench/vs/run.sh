#!/usr/bin/env bash
# run.sh — Where do zenvm and zenpy spend their time on the same algorithm?
#
# The two VMs share an interpreter but diverged in the compiler, so a gap
# between them is a codegen difference, not an engine one. This runs the same
# workload on both under the per-opcode profiler and prints the two profiles
# side by side, plus the dispatch totals — which is the number that says
# whether one is emitting more work than the other.
#
#   ./run.sh [case]        # default: all cases in cases/
#
# Needs both VMs built with -DZEN_OPCODE_PROFILE. Build them with:
#   cmake -B build-p -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-DZEN_OPCODE_PROFILE
# The profiler's rdtsc costs roughly 2x, so the cycle columns are only
# meaningful against each other, never as absolute timings.
cd "$(dirname "$0")"
export LC_ALL=C

ZENVM="${ZENVM:-/media/projectos/projects/cpp/zenvm/bin/zen}"
ZENPY="${ZENPY:-/media/projectos/projects/cpp/zenpy/bin/zen}"

for bin in "$ZENVM" "$ZENPY"; do
    [[ -x "$bin" ]] || { echo "missing: $bin" >&2; exit 2; }
done

profile() { # binary source -> "op count" lines
    "$1" "$2" 2>&1 >/dev/null | awk '/^[A-Z_]+ +[0-9]/ {print $1, $2, $4}'
}

total() { "$1" "$2" 2>&1 >/dev/null | awk '/^total dispatches:/ {print $3}'; }

run_case() {
    local name="$1"
    local zsrc="cases/$name.zen" psrc="cases/$name.py"
    [[ -f "$zsrc" && -f "$psrc" ]] || { echo "skip $name (missing source)"; return; }

    # Correctness first: the two must agree, or the comparison is meaningless.
    local zout pout
    zout=$("$ZENVM" "$zsrc" 2>/dev/null | head -1)
    pout=$("$ZENPY" "$psrc" 2>/dev/null | head -1)
    printf '\n=== %s ===\n' "$name"
    if [[ "$zout" != "$pout" ]]; then
        printf '  MISMATCH: zenvm=%s zenpy=%s — profiles below are not comparable\n' "$zout" "$pout"
    else
        printf '  both produce: %s\n' "$zout"
    fi

    local zt pt
    zt=$(total "$ZENVM" "$zsrc"); pt=$(total "$ZENPY" "$psrc")
    printf '  dispatches: zenvm=%s  zenpy=%s  (%+.1f%%)\n' "$zt" "$pt" \
        "$(awk -v a="$zt" -v b="$pt" 'BEGIN{print (b-a)/a*100}')"

    printf '\n  %-18s %12s %6s   | %-18s %12s %6s\n' \
        "zenvm opcode" "count" "%" "zenpy opcode" "count" "%"
    paste <(profile "$ZENVM" "$zsrc" | head -12) \
          <(profile "$ZENPY" "$psrc" | head -12) |
    awk -F'\t' '{split($1,a," "); split($2,b," ");
                 printf "  %-18s %12s %5s   | %-18s %12s %5s\n",
                        a[1], a[2], a[3], b[1], b[2], b[3]}'
}

if [[ $# -gt 0 ]]; then
    for c in "$@"; do run_case "$c"; done
else
    for f in cases/*.zen; do run_case "$(basename "$f" .zen)"; done
fi

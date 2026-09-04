#!/usr/bin/env bash
# Run the zen-vs-python comparison suite and print a ratio table.
# Usage: bench/compare/run.sh [zen-binary] [runs]
set -u
BIN="${1:-bin/zen}"
RUNS="${2:-3}"
DIR="$(cd "$(dirname "$0")" && pwd)"

collect() { # $1 = command...; best-of-RUNS per bench name
    local -A best=()
    for ((r = 0; r < RUNS; r++)); do
        while read -r tag name secs; do
            [[ "$tag" == "BENCH" ]] || continue
            local cur="${best[$name]:-}"
            if [[ -z "$cur" ]] || awk "BEGIN{exit !($secs < $cur)}"; then
                best[$name]="$secs"
            fi
        done < <("$@")
    done
    for name in "${!best[@]}"; do echo "$name ${best[$name]}"; done
}

echo "zen: $BIN   python: $(python3 --version 2>&1)   best of $RUNS runs"
zen_out="$(collect "$BIN" "$DIR/bench.zen")"
py_out="$(collect python3 "$DIR/bench.py")"

printf '%-16s %10s %10s %8s\n' "bench" "zen(s)" "python(s)" "zen/py"
join <(sort <<<"$zen_out") <(sort <<<"$py_out") | while read -r name z p; do
    awk -v n="$name" -v z="$z" -v p="$p" \
        'BEGIN{printf "%-16s %10.4f %10.4f %7.2fx\n", n, z, p, z/p}'
done

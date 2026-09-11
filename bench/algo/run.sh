#!/usr/bin/env bash
# Runs every algorithm benchmark in zen, Lua, Wren (and zenpy/CPython if
# present), best of 3, and prints one table. Every language must print the
# same checksum for a phase — that is the correctness check.
#   ./run.sh [zen-binary]
cd "$(dirname "$0")"
export LC_ALL=C   # decimal point, not comma, in printf and awk
ZEN="${1:-../../bin/zen}"
WREN="${WREN:-/media/projectos/projects/languages/wren-0.4.0/bin/wren_test}"
ZENPY="${ZENPY:-/media/projectos/projects/cpp/zenpy/build_release/bin/zen}"
PYDIR="${PYDIR:-/media/projectos/projects/cpp/zenpy/tests/manual/algo_bench}"
PHASES="astar dijkstra quadtree octree hanoi floodfill"

declare -A best
declare -A sums
tmp=$(mktemp)
run3() { # label cmd...
  local label=$1; shift
  for i in 1 2 3; do
    "$@" 2>/dev/null | tr '\t' ' ' > "$tmp" || continue
    while read -r name rest; do
      case " $PHASES " in
        *" $name "*)
          if [[ "$rest" == checksum* ]]; then
            sums["$label/$name"]="${rest#checksum }"
          else
            local key="$label/$name"
            if [[ -z "${best[$key]:-}" ]] || awk "BEGIN{exit !($rest < ${best[$key]})}"; then
              best[$key]=$rest
            fi
          fi
          ;;
      esac
    done < "$tmp"
  done
}

for f in pathfind spatial hanoi; do
  run3 zen  "$ZEN" $f.zen
  run3 lua  lua "$PYDIR/lua/$f.lua"
  [[ -x "$WREN" ]] && run3 wren "$WREN" "$PYDIR/wren/$f.wren"
  [[ -x "$ZENPY" ]] && run3 zenpy "$ZENPY" "$PYDIR/zen_typed/$f.py"
  command -v python3 >/dev/null && run3 python python3 "$PYDIR/py/$f.py"
done
rm -f "$tmp"

printf "%-8s" "seconds"; for p in $PHASES; do printf "%10s" "$p"; done; echo
for l in zen lua wren zenpy python; do
  [[ -z "${best[$l/hanoi]:-}${best[$l/astar]:-}" ]] && continue
  printf "%-8s" "$l"
  for p in $PHASES; do v=${best[$l/$p]:-}; printf "%10s" "${v:+$(printf %.3f "$v")}"; done
  echo
done

echo
echo "checksums (must be identical down each column):"
printf "%-8s" ""; for p in $PHASES; do printf "%12s" "$p"; done; echo
for l in zen lua wren zenpy python; do
  [[ -z "${sums[$l/hanoi]:-}${sums[$l/astar]:-}" ]] && continue
  printf "%-8s" "$l"
  for p in $PHASES; do printf "%12s" "${sums[$l/$p]:-}"; done
  echo
done

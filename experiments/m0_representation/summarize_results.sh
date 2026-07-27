#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
source_file="$root/experiments/m0_representation/m0_analyze.c"
build_dir="$root/build/m0_representation"
compiler=${CC:-gcc}
results=${1:-"$root/performance/m0_representation/results.csv"}
summary=${2:-"$root/performance/m0_representation/summary.md"}
binary="$build_dir/m0_analyze"

mkdir -p "$build_dir" "$(dirname "$summary")"
"$compiler" -std=c17 -O2 -Wall -Wextra -Werror \
    "$source_file" -lm -o "$binary"
"$binary" "$results" >"$summary"

test -s "$summary"
echo "M0 benchmark summary created: $summary"

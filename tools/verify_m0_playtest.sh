#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
build_dir="$root/build/m0_playtest_verify"
output_dir=${1:-"$root/performance/local/m0_playtest"}
compiler=${CC:-gcc}
binary="$build_dir/m0_movement_verify"

mkdir -p "$build_dir" "$output_dir"

"$compiler" -std=c17 -O2 -g \
    -Wall -Wextra -Wpedantic -Werror \
    "$root/experiments/m0_playtest/movement_model.c" \
    "$root/experiments/m0_playtest/movement_verify.c" \
    -lm -o "$binary"

"$binary" >"$output_dir/verification.txt" \
    2>"$output_dir/diagnostics.txt"

grep -q '^self-test=pass cases=7 ' "$output_dir/verification.txt"

echo "M0 movement playtest verification passed: $output_dir"

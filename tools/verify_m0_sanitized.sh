#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
build_dir="$root/build/m0_representation"
output_dir=${1:-"$root/performance/local/sanitized"}
compiler=${CC:-gcc}
binary="$build_dir/m0_bench_sanitized"

mkdir -p "$build_dir" "$output_dir"

"$compiler" -std=c17 -O1 -g \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -Wall -Wextra -Werror \
    "$root/experiments/m0_representation/m0_bench.c" \
    -lm -o "$binary"

# LeakSanitizer cannot enumerate threads under the Work Mode ptrace/container
# environment. AddressSanitizer and UndefinedBehaviorSanitizer remain enabled;
# leak checking is restored on normal CI hosts in M1.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
    "$binary" --mode smoke \
    >"$output_dir/results.csv" \
    2>"$output_dir/diagnostics.txt"

grep -q '^self-test=pass ' "$output_dir/diagnostics.txt"
grep -q '^benchmark=complete ' "$output_dir/diagnostics.txt"

{
    echo "status=pass"
    echo "address_sanitizer=enabled"
    echo "undefined_behavior_sanitizer=enabled"
    echo "leak_sanitizer=disabled_container_incompatible"
    "$compiler" --version | sed -n '1p'
} >"$output_dir/manifest.txt"

echo "M0 sanitizer verification passed: $output_dir"

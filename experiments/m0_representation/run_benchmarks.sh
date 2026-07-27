#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
source_file="$root/experiments/m0_representation/m0_bench.c"
build_dir="$root/build/m0_representation"
mode=${M0_BENCH_MODE:-commit}
if [ "$#" -gt 0 ]; then
    case "$1" in
        smoke|commit|milestone)
            mode=$1
            shift
            ;;
    esac
fi
output_dir=${1:-"$root/performance/local/manual"}
compiler=${CC:-gcc}
binary="$build_dir/m0_bench"
affinity=unavailable
tree_status=$(git -C "$root" status --porcelain --untracked-files=normal)

case "$mode" in
    smoke|commit|milestone)
        ;;
    *)
        echo "invalid M0_BENCH_MODE: $mode" >&2
        exit 2
        ;;
esac

mkdir -p "$build_dir" "$output_dir"

cflags="-std=c17 -O3 -march=native -DNDEBUG -Wall -Wextra -Werror"
"$compiler" $cflags "$source_file" -lm -o "$binary"

if command -v taskset >/dev/null 2>&1 && taskset -c 0 true 2>/dev/null; then
    affinity=cpu0
fi

metadata="$output_dir/metadata.txt"
results="$output_dir/results.csv"
diagnostics="$output_dir/diagnostics.txt"

{
    echo "mode=$mode"
    echo "commit=$(git -C "$root" rev-parse HEAD)"
    if [ -z "$tree_status" ]; then
        echo "dirty=false"
    else
        echo "dirty=true"
    fi
    echo "affinity=$affinity"
    echo "frequency_control=container_unavailable"
    echo "thermal_control=container_unavailable"
    echo "compiler=$compiler"
    echo "cflags=$cflags"
    "$compiler" --version | sed -n '1p'
    uname -srvm
    if command -v lscpu >/dev/null 2>&1; then
        lscpu | sed -n \
            '/^Architecture:/p;/^Model name:/p;/^CPU(s):/p;/^Thread(s) per core:/p;/^Hypervisor vendor:/p'
    fi
} >"$metadata"

run_binary()
{
    "$binary" --mode "$mode" >"$results" 2>"$diagnostics"
}

if [ "$affinity" = cpu0 ]; then
    taskset -c 0 "$binary" --mode "$mode" >"$results" 2>"$diagnostics"
else
    run_binary
fi

test -s "$results"
test -s "$diagnostics"
grep -q '^self-test=pass ' "$diagnostics"
grep -q '^benchmark=complete ' "$diagnostics"

echo "M0 benchmark $mode passed: $results"

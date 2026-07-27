#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m1_foundation"}
compiler=${CC:-cc}
direct_dir="$output_dir/direct"
cmake_dir="$output_dir/cmake"

mkdir -p "$direct_dir" "$cmake_dir"

"$compiler" -std=c17 -O2 -g \
    -Wall -Wextra -Wpedantic -Werror -Wconversion -Wformat=2 \
    -Wmissing-prototypes -Wshadow -Wstrict-prototypes -Wundef \
    -Wwrite-strings \
    -I"$root/include" \
    "$root/src/sim/sim.c" \
    "$root/tests/sim/test_sim_contract.c" \
    -o "$direct_dir/sim_contract_test"

"$direct_dir/sim_contract_test" >"$direct_dir/sim_contract.txt"
grep -q '^sim-contract=pass abi=1 tick_hz=60$' \
    "$direct_dir/sim_contract.txt"

"$compiler" -std=c17 -O2 -g \
    -Wall -Wextra -Wpedantic -Werror -Wconversion -Wformat=2 \
    -Wmissing-prototypes -Wshadow -Wstrict-prototypes -Wundef \
    -Wwrite-strings \
    -I"$root/include" \
    "$root/src/sim/sim.c" \
    "$root/src/headless/main.c" \
    -o "$direct_dir/headless"

"$direct_dir/headless" --smoke >"$direct_dir/headless.txt"
grep -q '^headless-smoke=pass sim_abi=1 tick_hz=60$' \
    "$direct_dir/headless.txt"

cmake_command=${CMAKE_COMMAND:-}
if [ -z "$cmake_command" ] && command -v cmake >/dev/null 2>&1; then
    cmake_command=$(command -v cmake)
fi

if [ -n "$cmake_command" ]; then
    if [ ! -x "$cmake_command" ]; then
        echo "M1 foundation verification failed: CMake is not executable: $cmake_command" >&2
        exit 1
    fi

    "$cmake_command" -S "$root" -B "$cmake_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=ON \
        -DPF_BUILD_HEADLESS=ON
    "$cmake_command" --build "$cmake_dir" --parallel

    ctest_command=$(dirname "$cmake_command")/ctest
    if [ ! -x "$ctest_command" ]; then
        if command -v ctest >/dev/null 2>&1; then
            ctest_command=$(command -v ctest)
        else
            echo "M1 foundation verification failed: matching ctest was not found" >&2
            exit 1
        fi
    fi
    "$ctest_command" --test-dir "$cmake_dir" --output-on-failure
    echo "cmake-validation=pass"
else
    echo "cmake-validation=skipped reason=cmake-not-on-path"
fi

echo "M1 foundation verification passed: $output_dir"

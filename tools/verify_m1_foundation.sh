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
    "$root/src/sim/sim_combat.c" \
    "$root/src/sim/sim_content.c" \
    "$root/src/sim/sim_event.c" \
    "$root/src/sim/sim_item.c" \
    "$root/src/sim/sim_projectile.c" \
    "$root/src/sim/sim_movement.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_rl.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/tests/sim/test_sim_contract.c" \
    -o "$direct_dir/sim_contract_test"

"$direct_dir/sim_contract_test" >"$direct_dir/sim_contract.txt"
grep -q '^sim-contract=pass abi=4 tick_hz=60$' \
    "$direct_dir/sim_contract.txt"

"$compiler" -std=c17 -O2 -g \
    -Wall -Wextra -Wpedantic -Werror -Wconversion -Wformat=2 \
    -Wmissing-prototypes -Wshadow -Wstrict-prototypes -Wundef \
    -Wwrite-strings \
    -I"$root/include" \
    "$root/src/sim/sim.c" \
    "$root/src/sim/sim_combat.c" \
    "$root/src/sim/sim_content.c" \
    "$root/src/sim/sim_event.c" \
    "$root/src/sim/sim_item.c" \
    "$root/src/sim/sim_projectile.c" \
    "$root/src/sim/sim_movement.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_rl.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/src/headless/main.c" \
    -o "$direct_dir/headless"

"$direct_dir/headless" --smoke >"$direct_dir/headless.txt"
grep -q '^headless-smoke=pass sim_abi=4 tick_hz=60$' \
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
        -DPF_BUILD_HEADLESS=ON \
        -DPF_SQLITE_SOURCE_DIR="$root/.toolchains/dependencies/sqlite-amalgamation-3530400"
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

    run_cmake_smoke()
    {
        executable=$1
        expected=$2
        output_file=$3

        "$cmake_dir/$executable" --smoke >"$cmake_dir/$output_file"
        grep -Fqx "$expected" "$cmake_dir/$output_file"
    }

    run_cmake_smoke \
        headless \
        "headless-smoke=pass sim_abi=4 tick_hz=60" \
        headless.txt
    run_cmake_smoke \
        pf_native_client \
        "native-client-smoke=pass sim_abi=4 tick_hz=60" \
        native_client.txt
    run_cmake_smoke \
        pf_web_client_host_smoke \
        "web-client-smoke=pass sim_abi=4 tick_hz=60" \
        web_client.txt
    run_cmake_smoke \
        pf_tools \
        "tools-smoke=pass sim_abi=4 tick_hz=60" \
        tools.txt
    run_cmake_smoke \
        pf_benchmarks \
        "benchmarks-smoke=pass sim_abi=4 tick_hz=60" \
        benchmarks.txt
    run_cmake_smoke \
        pf_verifier \
        "verifier-smoke=pass sim_abi=4 tick_hz=60" \
        verifier.txt

    if command -v nm >/dev/null 2>&1; then
        nm -u "$cmake_dir/libpf_sim.a" >"$cmake_dir/sim_undefined_symbols.txt"
        if grep -Eq \
            'CreateThread|SDL_CreateThread|_beginthread|emscripten_dispatch|pthread_create|thrd_create' \
            "$cmake_dir/sim_undefined_symbols.txt"; then
            echo "M1 foundation verification failed: sim references a thread-creation symbol" >&2
            exit 1
        fi
        echo "sim-thread-symbol-validation=pass"
    else
        echo "sim-thread-symbol-validation=skipped reason=nm-not-on-path"
    fi

    echo "cmake-validation=pass"
else
    echo "cmake-validation=skipped reason=cmake-not-on-path"
fi

echo "M1 foundation verification passed: $output_dir"

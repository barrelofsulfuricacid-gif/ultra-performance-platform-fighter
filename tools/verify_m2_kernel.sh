#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m2_kernel"}
compiler=${CC:-cc}

mkdir -p "$output_dir"

common_flags="
    -std=c17
    -O2
    -g
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wconversion
    -Wformat=2
    -Wmissing-prototypes
    -Wshadow
    -Wstrict-prototypes
    -Wundef
    -Wwrite-strings
"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    "$root/src/sim/sim.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/tests/sim/test_sim_world.c" \
    -o "$output_dir/sim_world_test"

"$output_dir/sim_world_test" >"$output_dir/sim_world.txt"
grep -Fqx \
    'sim-world=pass players=4 deterministic_ticks=180' \
    "$output_dir/sim_world.txt"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    "$root/src/sim/sim.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/tests/sim/test_sim_snapshot.c" \
    -o "$output_dir/sim_snapshot_test"

"$output_dir/sim_snapshot_test" >"$output_dir/sim_snapshot.txt"
grep -Fqx \
    'sim-snapshot=pass bytes=305 hash_algorithm=sha256' \
    "$output_dir/sim_snapshot.txt"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    "$root/src/sim/sim.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/tests/sim/test_replay_corpus.c" \
    -o "$output_dir/replay_corpus"

"$output_dir/replay_corpus" >"$output_dir/replay_corpus.txt"
grep -Fqx \
    'sim-replay=pass ticks=180 players=4 bytes=30997 corpus_sha256=a1008ac5f1d555ccd17a8f17fe48eab6ce08079fd635b26ee08155f0dea44dce final_sha256=335e31f2d830eea582f9e42fe7ee41469f81aa359ee14e661cf68002932d558a' \
    "$output_dir/replay_corpus.txt"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -c "$root/src/sim/sim.c" \
    -o "$output_dir/sim.o"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -c "$root/src/sim/sim_tick.c" \
    -o "$output_dir/sim_tick.o"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    -c "$root/src/sim/sim_replay.c" \
    -o "$output_dir/sim_replay.o"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -c "$root/src/sim/sim_sha256.c" \
    -o "$output_dir/sim_sha256.o"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -c "$root/src/sim/sim_snapshot.c" \
    -o "$output_dir/sim_snapshot.o"

if command -v nm >/dev/null 2>&1; then
    nm -u \
        "$output_dir/sim.o" \
        "$output_dir/sim_replay.o" \
        "$output_dir/sim_sha256.o" \
        "$output_dir/sim_snapshot.o" \
        "$output_dir/sim_tick.o" \
        >"$output_dir/undefined_symbols.txt"
    if grep -Eq \
        'calloc|clock_gettime|CreateThread|fopen|fprintf|free|malloc|mtx_|nanosleep|pthread_|printf|realloc|SDL_|Sleep|thrd_| time$' \
        "$output_dir/undefined_symbols.txt"; then
        echo "M2 kernel verification failed: forbidden tick dependency" >&2
        exit 1
    fi
    echo "m2-forbidden-symbol-validation=pass"
else
    echo "m2-forbidden-symbol-validation=skipped reason=nm-not-on-path"
fi

echo "m2-kernel-verification=pass deterministic_ticks=180 replay_ticks=180 players=4 abi=2"

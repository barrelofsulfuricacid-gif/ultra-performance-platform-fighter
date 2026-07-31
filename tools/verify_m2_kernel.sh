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
    "$root/src/sim/sim_combat.c" \
    "$root/src/sim/sim_content.c" \
    "$root/src/sim/sim_event.c" \
    "$root/src/sim/sim_movement.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_rl.c" \
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
    "$root/src/sim/sim_combat.c" \
    "$root/src/sim/sim_content.c" \
    "$root/src/sim/sim_event.c" \
    "$root/src/sim/sim_movement.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_rl.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/tests/sim/test_sim_snapshot.c" \
    -o "$output_dir/sim_snapshot_test"

"$output_dir/sim_snapshot_test" >"$output_dir/sim_snapshot.txt"
grep -Fqx \
    'sim-snapshot=pass bytes=635 hash_algorithm=sha256' \
    "$output_dir/sim_snapshot.txt"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    "$root/src/sim/sim.c" \
    "$root/src/sim/sim_combat.c" \
    "$root/src/sim/sim_content.c" \
    "$root/src/sim/sim_event.c" \
    "$root/src/sim/sim_movement.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_rl.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/tests/sim/test_rl_api.c" \
    -o "$output_dir/rl_api_test"

"$output_dir/rl_api_test" >"$output_dir/rl_api.txt"
grep -Fqx \
    'rl-api=pass compact_values=48 batch_environments=6 reward_q16=65536 engagement_limit_q16=16384 schema=4' \
    "$output_dir/rl_api.txt"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/checkpoint" \
    -I"$root/src/sim" \
    "$root/src/sim/sim.c" \
    "$root/src/sim/sim_combat.c" \
    "$root/src/sim/sim_content.c" \
    "$root/src/sim/sim_event.c" \
    "$root/src/sim/sim_movement.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_rl.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/src/checkpoint/m2_replay_fixture.c" \
    "$root/tests/sim/test_replay_corpus.c" \
    -o "$output_dir/replay_corpus"

"$output_dir/replay_corpus" >"$output_dir/replay_corpus.txt"
grep -Fqx \
    'sim-replay=pass ticks=180 players=4 bytes=31327 corpus_sha256=03243c053949a602fecbb56f83a63b6f71d6f7df2d552079b412989c8a8fc426 final_sha256=812770580726410ecf71653e93844eb25fdfeeabaa553a95d809588cf3afdd52 events_sha256=d2f5992ecc10cd4fb54a6c7bb5165e2983b019207b76c3792cc4bde4379be14f' \
    "$output_dir/replay_corpus.txt"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -c "$root/src/sim/sim.c" \
    -o "$output_dir/sim.o"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    -c "$root/src/sim/sim_combat.c" \
    -o "$output_dir/sim_combat.o"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    -c "$root/src/sim/sim_content.c" \
    -o "$output_dir/sim_content.o"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    -c "$root/src/sim/sim_event.c" \
    -o "$output_dir/sim_event.o"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    -c "$root/src/sim/sim_movement.c" \
    -o "$output_dir/sim_movement.o"

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
    -I"$root/src/sim" \
    -c "$root/src/sim/sim_rl.c" \
    -o "$output_dir/sim_rl.o"

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
        "$output_dir/sim_combat.o" \
        "$output_dir/sim_content.o" \
        "$output_dir/sim_event.o" \
        "$output_dir/sim_movement.o" \
        "$output_dir/sim_replay.o" \
        "$output_dir/sim_rl.o" \
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

echo "m2-kernel-verification=pass deterministic_ticks=180 replay_ticks=180 rl_batch=6 players=4 abi=4"

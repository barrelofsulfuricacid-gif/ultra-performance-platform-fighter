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
    "$root/src/sim/sim_item.c" \
    "$root/src/sim/sim_projectile.c" \
    "$root/src/sim/sim_reflector.c" \
    "$root/src/sim/sim_charge.c" \
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
    "$root/src/sim/sim_item.c" \
    "$root/src/sim/sim_projectile.c" \
    "$root/src/sim/sim_reflector.c" \
    "$root/src/sim/sim_charge.c" \
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
    'sim-snapshot=pass bytes=690 hash_algorithm=sha256' \
    "$output_dir/sim_snapshot.txt"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    "$root/src/sim/sim.c" \
    "$root/src/sim/sim_combat.c" \
    "$root/src/sim/sim_content.c" \
    "$root/src/sim/sim_event.c" \
    "$root/src/sim/sim_item.c" \
    "$root/src/sim/sim_projectile.c" \
    "$root/src/sim/sim_reflector.c" \
    "$root/src/sim/sim_charge.c" \
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
    'rl-api=pass compact_values=66 batch_environments=6 reward_q16=65536 engagement_limit_q16=16384 schema=7' \
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
    "$root/src/sim/sim_item.c" \
    "$root/src/sim/sim_projectile.c" \
    "$root/src/sim/sim_reflector.c" \
    "$root/src/sim/sim_charge.c" \
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
    'sim-replay=pass ticks=180 players=4 bytes=31382 corpus_sha256=bd6f3d511346bd7b5407c1cd99e7b06d8c4b12104088e334576ed5f10c114c54 final_sha256=b589652de041e5a6ae8baef49d539d971c5d465a17d117305473ee1cfbebfb42 events_sha256=32df182c93ce9143357b6472615d90c9cc01e622488400d4eec54d7c89cab35f' \
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
    -c "$root/src/sim/sim_item.c" \
    -o "$output_dir/sim_item.o"

"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    -c "$root/src/sim/sim_projectile.c" \
    -o "$output_dir/sim_projectile.o"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    -c "$root/src/sim/sim_reflector.c" \
    -o "$output_dir/sim_reflector.o"
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    -c "$root/src/sim/sim_charge.c" \
    -o "$output_dir/sim_charge.o"

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
        "$output_dir/sim_item.o" \
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

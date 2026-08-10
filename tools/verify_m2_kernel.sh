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

compile_sim_test()
{
    output=$1
    shift
    # shellcheck disable=SC2086
    "$compiler" $common_flags \
        -I"$root/include" \
        -I"$root/src/checkpoint" \
        -I"$root/src/sim" \
        "$root/src/sim/sim.c" \
        "$root/src/sim/sim_combat.c" \
        "$root/src/sim/sim_content.c" \
        "$root/src/sim/sim_falcon_frame_data.c" \
        "$root/src/sim/sim_ssbm_common_data.c" \
        "$root/src/sim/sim_ssbm_stage_data.c" \
        "$root/src/sim/sim_ssbm_damage.c" \
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
        "$@" \
        -o "$output"
}

compile_sim_test \
    "$output_dir/sim_world_test" \
    "$root/tests/sim/test_sim_world.c"

"$output_dir/sim_world_test" >"$output_dir/sim_world.txt"
grep -Fqx \
    'sim-world=pass players=4 deterministic_ticks=180' \
    "$output_dir/sim_world.txt"

compile_sim_test \
    "$output_dir/sim_snapshot_test" \
    "$root/tests/sim/test_sim_snapshot.c"

"$output_dir/sim_snapshot_test" >"$output_dir/sim_snapshot.txt"
grep -Fqx \
    'sim-snapshot=pass bytes=835 hash_algorithm=sha256' \
    "$output_dir/sim_snapshot.txt"

compile_sim_test \
    "$output_dir/rl_api_test" \
    "$root/tests/sim/test_rl_api.c"

"$output_dir/rl_api_test" >"$output_dir/rl_api.txt"
grep -Fqx \
    'rl-api=pass compact_values=102 batch_environments=6 reward_q16=65536 engagement_limit_q16=16384 schema=14' \
    "$output_dir/rl_api.txt"

compile_sim_test \
    "$output_dir/replay_corpus" \
    "$root/src/checkpoint/m2_replay_fixture.c" \
    "$root/tests/sim/test_replay_corpus.c"

"$output_dir/replay_corpus" >"$output_dir/replay_corpus.txt"
grep -Fqx \
    'sim-replay=pass ticks=240 players=4 bytes=41607 corpus_sha256=0d3ccb293d0735102c13d020d469f13b202eede2b54052881d0380efb765e172 final_sha256=3a9bb1e28fd635dcde8f1ec98d0705babd12ee64ee7e036e8f986c5a15a874d5 events_sha256=370975f72bbd6546f5253607ef62b811cb4f126889ad3c89bf4b2955703430cb' \
    "$output_dir/replay_corpus.txt"

compile_sim_object()
{
    # shellcheck disable=SC2086
    "$compiler" $common_flags \
        -I"$root/include" \
        -I"$root/src/sim" \
        -c "$1" \
        -o "$2"
}

compile_sim_object "$root/src/sim/sim.c" "$output_dir/sim.o"
compile_sim_object \
    "$root/src/sim/sim_combat.c" "$output_dir/sim_combat.o"
compile_sim_object \
    "$root/src/sim/sim_content.c" "$output_dir/sim_content.o"
compile_sim_object \
    "$root/src/sim/sim_falcon_frame_data.c" \
    "$output_dir/sim_falcon_frame_data.o"
compile_sim_object \
    "$root/src/sim/sim_ssbm_common_data.c" \
    "$output_dir/sim_ssbm_common_data.o"
compile_sim_object \
    "$root/src/sim/sim_ssbm_stage_data.c" \
    "$output_dir/sim_ssbm_stage_data.o"
compile_sim_object \
    "$root/src/sim/sim_ssbm_damage.c" "$output_dir/sim_ssbm_damage.o"
compile_sim_object \
    "$root/src/sim/sim_event.c" "$output_dir/sim_event.o"
compile_sim_object \
    "$root/src/sim/sim_item.c" "$output_dir/sim_item.o"
compile_sim_object \
    "$root/src/sim/sim_projectile.c" "$output_dir/sim_projectile.o"
compile_sim_object \
    "$root/src/sim/sim_reflector.c" "$output_dir/sim_reflector.o"
compile_sim_object \
    "$root/src/sim/sim_charge.c" "$output_dir/sim_charge.o"
compile_sim_object \
    "$root/src/sim/sim_movement.c" "$output_dir/sim_movement.o"
compile_sim_object \
    "$root/src/sim/sim_tick.c" "$output_dir/sim_tick.o"
compile_sim_object \
    "$root/src/sim/sim_replay.c" "$output_dir/sim_replay.o"
compile_sim_object \
    "$root/src/sim/sim_rl.c" "$output_dir/sim_rl.o"
compile_sim_object \
    "$root/src/sim/sim_sha256.c" "$output_dir/sim_sha256.o"
compile_sim_object \
    "$root/src/sim/sim_snapshot.c" "$output_dir/sim_snapshot.o"

if command -v nm >/dev/null 2>&1; then
    nm -u \
        "$output_dir/sim.o" \
        "$output_dir/sim_combat.o" \
        "$output_dir/sim_content.o" \
        "$output_dir/sim_falcon_frame_data.o" \
        "$output_dir/sim_ssbm_common_data.o" \
        "$output_dir/sim_ssbm_stage_data.o" \
        "$output_dir/sim_ssbm_damage.o" \
        "$output_dir/sim_event.o" \
        "$output_dir/sim_item.o" \
        "$output_dir/sim_projectile.o" \
        "$output_dir/sim_reflector.o" \
        "$output_dir/sim_charge.o" \
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

echo "m2-kernel-verification=pass deterministic_ticks=180 replay_ticks=240 rl_batch=6 players=4 abi=4"

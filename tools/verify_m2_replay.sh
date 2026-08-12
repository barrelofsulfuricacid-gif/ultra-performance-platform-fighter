#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m2_replay"}
compiler=${CC:-cc}
expected='sim-replay=pass ticks=240 players=4 bytes=42519 corpus_sha256=1b2d49314b692a03114396f7eb662b5b574a1a2e0b045b9fa0a366db12852301 final_sha256=d9552577f2a31dcbcf582045cfc5af4033c15b519d4d315271e79e74a177c2af events_sha256=7930e2a2d90ed4dd9f5234ba47f4d4fc11e2ce4fbc2cd22b9367473a71bb2451'

mkdir -p "$output_dir"

common_flags="
    -std=c17
    -O2
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
    -I"$root/src/checkpoint" \
    -I"$root/src/sim" \
    "$root/src/sim/sim.c" \
    "$root/src/sim/sim_combat.c" \
    "$root/src/sim/sim_content.c" \
    "$root/src/sim/sim_falcon_frame_data.c" \
    "$root/src/sim/sim_fixed_math.c" \
    "$root/src/sim/sim_hsd_pose.c" \
    "$root/src/sim/sim_ssbm_common_data.c" \
    "$root/src/sim/sim_ssbm_damage.c" \
    "$root/src/sim/sim_ssbm_stage_data.c" \
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
    -o "$output_dir/native_replay_corpus"

"$output_dir/native_replay_corpus" >"$output_dir/native.txt"
grep -Fqx "$expected" "$output_dir/native.txt"

web_corpus="$root/build/web/pf_replay_corpus.js"
if [ "${PF_REQUIRE_WEB_REPLAY:-0}" = "1" ]; then
    if [ ! -f "$web_corpus" ]; then
        echo "M2 replay verification failed: web corpus is missing" >&2
        exit 1
    fi
    if ! command -v node >/dev/null 2>&1; then
        echo "M2 replay verification failed: node is not on PATH" >&2
        exit 1
    fi
    node "$web_corpus" >"$output_dir/wasm.txt"
    grep -Fqx "$expected" "$output_dir/wasm.txt"
    cmp "$output_dir/native.txt" "$output_dir/wasm.txt"
    echo "m2-replay-cross-target=pass native=1 wasm=1 ticks=240"
else
    echo "m2-replay-cross-target=partial native=1 wasm=deferred ticks=240"
fi

echo "m2-replay-verification=pass bytes=42519 ticks=240 players=4"

#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m2_replay"}
compiler=${CC:-cc}
expected='sim-replay=pass ticks=240 players=4 bytes=41575 corpus_sha256=4f38617b574c30088ff374283918ddf5e89111d417d638bf164120908126caed final_sha256=34d2019a582d081cce10b8c7053909b8b5153d45cfe3d889f41bd7f135fc29ac events_sha256=6a58ca0e2ef8dd3e08308d8b8d3085c22c73530400286013f305f2343f38bf87'

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

echo "m2-replay-verification=pass bytes=41575 ticks=240 players=4"

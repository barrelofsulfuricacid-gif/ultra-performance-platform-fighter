#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m2_replay"}
compiler=${CC:-cc}
expected='sim-replay=pass ticks=180 players=4 bytes=31467 corpus_sha256=278844212e6d27dcbf2f859212289d589cb78785c77c865e927e2177e8486dd2 final_sha256=21092c36a062e6f6f13c3859b63b49fbcb1d747a6136548eb96868554cd26206 events_sha256=7dac547f463ec6995207dc41d8fab3449113b79cd6179d4037e821a8dc63b18f'

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
    echo "m2-replay-cross-target=pass native=1 wasm=1 ticks=180"
else
    echo "m2-replay-cross-target=partial native=1 wasm=deferred ticks=180"
fi

echo "m2-replay-verification=pass bytes=31467 ticks=180 players=4"

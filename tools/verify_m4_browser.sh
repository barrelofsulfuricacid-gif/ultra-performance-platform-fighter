#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_browser"}
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
    -I"$root/src/web_client" \
    "$root/src/sim/sim.c" \
    "$root/src/sim/sim_combat.c" \
    "$root/src/sim/sim_content.c" \
    "$root/src/sim/sim_movement.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_rl.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/src/web_client/m4_playtest.c" \
    "$root/tests/web/test_m4_playtest.c" \
    -o "$output_dir/m4_web_playtest_test"

"$output_dir/m4_web_playtest_test" >"$output_dir/m4_web_playtest.txt"
grep -Fq \
    'm4-browser-adapter=pass walk_axis=13500 dash_axis=32767 input_probe=1 combat_probe=1 reaction_probe=1' \
    "$output_dir/m4_web_playtest.txt"

command -v node >/dev/null 2>&1 ||
    {
        echo "M4 browser verification requires Node.js for JavaScript syntax checking" >&2
        exit 1
    }
node --check "$root/src/web_client/web_adapter.js"

grep -Fq \
    'held("ShiftLeft") || held("ShiftRight")' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'playtest=ready input_probe=' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'jumpQueued: [false, false]' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'attackQueued: [false, false]' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'shieldQueued: [false, false]' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'viewCount !== 64' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'controls=keyboard-two-player' \
    "$root/tools/verify_web_smoke.sh"

echo "m4-browser-verification=pass walk_axis=13500 dash_axis=32767 input_probe=1 combat_probe=1 reaction_probe=1"

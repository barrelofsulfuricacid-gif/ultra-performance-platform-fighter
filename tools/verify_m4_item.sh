#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_item"}
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
    "$root/tests/sim/test_m4_item.c" \
    -o "$output_dir/m4_item_test"

"$output_dir/m4_item_test" >"$output_dir/m4_item.txt"
grep -Fqx \
    'm4-item=pass content_schema=39 state_schema=38 save_bytes=694 item_invariants=44 bat_drop=1 glide_toss=1 jump_cancel_throw=1 directional_throws=4 replay=1 rl=1' \
    "$output_dir/m4_item.txt"

"$root/tools/verify_m4_technique_registry.sh"

echo "m4-item-verification=pass item_invariants=44 bat_drop=1 glide_toss=1 jump_cancel_throw=1 directional_throws=4 replay=1 rl=1"

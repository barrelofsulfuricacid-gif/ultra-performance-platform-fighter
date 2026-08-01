#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_combat"}
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
    "$root/src/sim/sim_movement.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_rl.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/tests/sim/test_m4_combat.c" \
    -o "$output_dir/m4_combat_test"

"$output_dir/m4_combat_test" >"$output_dir/m4_combat.txt"
grep -Fqx \
    'm4-combat=pass content_schema=30 deterministic_ticks=20000 combat_invariants=584 journal_invariants=50 double_jump_cancel_counter=1 approach=1 spacing=1 sharking=1 cross_up=1 mindgame=1 juggling=1 ladder=1 kill_confirm=1 zero_to_death=1 jab_reset=1 jab_cancel=1 boost_grab=1 jump_cancelled_grab=1 jump_cancel=1 directional_throws=1 chain_grab=1' \
    "$output_dir/m4_combat.txt"

"$root/tools/verify_m4_technique_registry.sh"

echo "m4-combat-verification=pass invariants=584 journal_invariants=50 deterministic_ticks=20000 approach=1 spacing=1 sharking=1 cross_up=1 mindgame=1 juggling=1 ladder=1 kill_confirm=1 zero_to_death=1 jab_reset=1 jab_cancel=1 boost_grab=1 jump_cancelled_grab=1 jump_cancel=1 directional_throws=1 chain_grab=1"

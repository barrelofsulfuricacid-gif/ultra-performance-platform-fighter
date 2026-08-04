#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_movement"}
compiler=${CC:-cc}

mkdir -p "$output_dir"

grep -Fq 'PF_M4_ACTION_LEDGE_ROLL = 85' "$root/include/pf/m4.h"
grep -Fq 'ledge_roll_distance_q16' "$root/include/pf/m4.h"
grep -Fq 'ledge_roll_movement_ticks' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_LEDGE_ROLL' "$root/src/sim/sim_movement.c"
grep -Fq 'ledge_roll=1' "$root/tests/sim/test_m4_movement.c"

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
    "$root/tests/sim/test_m4_movement.c" \
    -o "$output_dir/m4_movement_test"

"$output_dir/m4_movement_test" >"$output_dir/m4_movement.txt"
grep -Fqx \
    'm4-movement=pass content_schema=66 deterministic_ticks=20000 movement_core=pass tap_jump=1 jump_takeoff_momentum=1 player_push=1 teeter_cancel=1 taunt_cancel=1 double_jump_cancel=1 vector_ascent=1 ledge_roll=1 emergent_technique_tests=skipped' \
    "$output_dir/m4_movement.txt"

echo "m4-movement-verification=pass movement_core=pass teeter_cancel=1 taunt_cancel=1 deterministic_ticks=20000 double_jump_cancel=1 vector_ascent=1 ledge_roll=1 emergent_technique_tests=skipped"

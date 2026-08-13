#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/match"}
compiler=${CC:-cc}

mkdir -p "$output_dir"

grep -Fq 'PF_M4_ACTION_REVIVAL_PLATFORM = 94' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_SURFACE_REVIVAL_PLATFORM = 4' "$root/include/pf/m4.h"
grep -Fq 'PF_SIM_EVENT_REVIVAL_DROP = 23' "$root/include/pf/sim.h"
grep -Fq 'PF_SIM_EVENT_ACTION_TRANSITIONS = 24' "$root/include/pf/sim.h"

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
    "$root/src/sim/sim_falcon_frame_data.c" \
    "$root/src/sim/sim_ssbm_common_data.c" \
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
    "$root/tests/sim/test_match.c" \
    -o "$output_dir/match_test"

"$output_dir/match_test" >"$output_dir/match.txt"
grep -Fqx \
    'm4-match=pass stocks=4 respawn_delay=60 respawn_invulnerability=120 sudden_death=1 team_result=1 invariants=24 journal_invariants=62 revival_invariants=24' \
    "$output_dir/match.txt"

echo "m4-match-verification=pass invariants=24 journal_invariants=62 revival_invariants=24"

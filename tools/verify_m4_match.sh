#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_match"}
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
    "$root/src/sim/sim_movement.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_rl.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/tests/sim/test_m4_match.c" \
    -o "$output_dir/m4_match_test"

"$output_dir/m4_match_test" >"$output_dir/m4_match.txt"
grep -Fqx \
    'm4-match=pass stocks=4 respawn_delay=60 respawn_invulnerability=120 sudden_death=1 team_result=1 invariants=24 journal_invariants=44' \
    "$output_dir/m4_match.txt"

echo "m4-match-verification=pass invariants=24 journal_invariants=44"

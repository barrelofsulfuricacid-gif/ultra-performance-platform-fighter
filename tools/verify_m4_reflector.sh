#!/usr/bin/env sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir=${1:-/tmp/pf-m4-reflector}
compiler=${CC:-cc}
expected='m4-reflector=pass content_schema=35 state_schema=34 save_bytes=690 reflector_invariants=32 shine_spike=1 projectile_reflect=1 replay=1 rl=1'

mkdir -p "$output_dir"

"$compiler" \
    -std=c17 \
    -O2 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Werror \
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
    "$root/tests/sim/test_m4_reflector.c" \
    -o "$output_dir/m4_reflector_test"

actual=$($output_dir/m4_reflector_test)
printf '%s\n' "$actual"
printf '%s\n' "$actual" | grep -Fqx "$expected"

grep -Fq 'PF_M4_ACTION_REFLECTOR_GROUND = 66' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_REFLECTOR_AIR = 67' "$root/include/pf/m4.h"
grep -Fq 'PF_SIM_EVENT_PROJECTILE_REFLECT = 21' "$root/include/pf/sim.h"
grep -Fq 'pf_m4_prepare_reflector_input' "$root/src/sim/sim_tick.c"
grep -Fq 'const int reflector_active =' "$root/src/sim/sim_combat.c"
grep -Fq 'pf_web_m4_run_shine_spike_probe' "$root/src/web_client/m4_playtest.c"
"$root/tools/verify_m4_technique_registry.sh"

printf '%s\n' 'm4-reflector-verification=pass checks=25'

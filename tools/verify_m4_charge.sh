#!/usr/bin/env sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir=${1:-/tmp/pf-m4-charge}
compiler=${CC:-cc}
expected='m4-charge=pass content_schema=32 state_schema=31 save_bytes=690 charge_invariants=28 charge_storage_cancel=1 resumed_release=1 replay=1 rl=1'

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
    "$root/tests/sim/test_m4_charge.c" \
    -o "$output_dir/m4_charge_test"

actual=$($output_dir/m4_charge_test)
printf '%s\n' "$actual"
printf '%s\n' "$actual" | grep -Fqx "$expected"

grep -Fq 'PF_M4_ACTION_CHARGE_GROUND = 68' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_CHARGE_STORE_GROUND = 69' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_CHARGE_RELEASE_GROUND = 70' "$root/include/pf/m4.h"
grep -Fq 'pf_m4_prepare_charge_input' "$root/src/sim/sim_tick.c"
grep -Fq 'scratch->charge_ticks[target_index] = UINT16_C(0);' \
    "$root/src/sim/sim_combat.c"
grep -Fq 'pf_web_m4_run_charge_storage_probe' \
    "$root/src/web_client/m4_playtest.c"
"$root/tools/verify_m4_technique_registry.sh"

printf '%s\n' 'm4-charge-verification=pass checks=28'

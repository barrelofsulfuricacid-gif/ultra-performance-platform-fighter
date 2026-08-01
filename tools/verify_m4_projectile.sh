#!/usr/bin/env sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir=${1:-/tmp/pf-m4-projectile}
compiler=${CC:-cc}
expected='m4-projectile=pass content_schema=32 state_schema=31 save_bytes=690 projectile_invariants=38 short_hop_laser=1 powershield_reflect=1 replay=1 rl=1'

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
    "$root/tests/sim/test_m4_projectile.c" \
    -o "$output_dir/m4_projectile_test"

actual=$($output_dir/m4_projectile_test)
printf '%s\n' "$actual"
printf '%s\n' "$actual" | grep -Fqx "$expected"
printf '%s\n' 'm4-projectile-verification=pass checks=18'

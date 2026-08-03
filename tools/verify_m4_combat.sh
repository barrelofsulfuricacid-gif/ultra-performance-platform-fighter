#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_combat"}
compiler=${CC:-cc}

mkdir -p "$output_dir"

grep -Fq 'PF_M4_ACTION_PUMMEL = 78' "$root/include/pf/m4.h"
grep -Fq 'PF_SIM_EVENT_PUMMEL = 22' "$root/include/pf/sim.h"
grep -Fq 'pummel_damage_q16' "$root/include/pf/m4.h"
grep -Fq 'PF_SIM_EVENT_FLAG_CROUCH_CANCEL = 1 << 4' "$root/include/pf/sim.h"
grep -Fq 'crouch_cancel_max_damage_q16' "$root/include/pf/m4.h"
grep -Fq 'crouch_cancel_hitstun_scale_q16' "$root/include/pf/m4.h"
grep -Fq 'weight_q16' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_FORWARD_AERIAL = 81' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_BACK_AERIAL = 82' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_UP_AERIAL = 83' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_DOWN_AERIAL = 84' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_LEDGE_ROLL = 85' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_LEDGE_ATTACK = 86' "$root/include/pf/m4.h"
grep -Fq 'forward_aerial' "$root/include/pf/m4.h"
grep -Fq 'ledge_attack' "$root/include/pf/m4.h"
grep -Fq 'ledge_attack_invulnerability_ticks' "$root/include/pf/m4.h"
grep -Fq 'pf_m4_select_light_aerial_action' "$root/src/sim/sim_movement.c"
grep -Fq 'directional_aerials=1' "$root/tests/sim/test_m4_combat.c"
grep -Fq 'ledge_attack=1' "$root/tests/sim/test_m4_combat.c"

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
    "$root/tests/sim/test_m4_combat.c" \
    -o "$output_dir/m4_combat_test"

"$output_dir/m4_combat_test" >"$output_dir/m4_combat.txt"
grep -Fqx \
    'm4-combat=pass content_schema=54 deterministic_ticks=20000 combat_invariants=982 journal_invariants=74 weight=1 stale_move=1 prone_getup_roll=4 directional_ground_attacks=1 smash_charge=1 light_shield=1 shield_geometry=1 shield_sdi=1 directional_aerials=1 ledge_attack=1 crouch_cancel=1 double_jump_cancel_counter=1 approach=1 spacing=1 sharking=1 cross_up=1 mindgame=1 juggling=1 ladder=1 kill_confirm=1 zero_to_death=1 jab_reset=1 jab_cancel=1 boost_grab=1 jump_cancelled_grab=1 jump_cancel=1 pummel=1 directional_throws=1 chain_grab=1 team_wobble=1' \
    "$output_dir/m4_combat.txt"

"$root/tools/verify_m4_technique_registry.sh"

echo "m4-combat-verification=pass invariants=982 journal_invariants=74 deterministic_ticks=20000 weight=1 stale_move=1 prone_getup_roll=4 directional_ground_attacks=1 directional_aerials=1 ledge_attack=1 crouch_cancel=1 shield_sdi=1 approach=1 spacing=1 sharking=1 cross_up=1 mindgame=1 juggling=1 ladder=1 kill_confirm=1 zero_to_death=1 jab_reset=1 jab_cancel=1 boost_grab=1 jump_cancelled_grab=1 jump_cancel=1 pummel=1 directional_throws=1 chain_grab=1 team_wobble=1"

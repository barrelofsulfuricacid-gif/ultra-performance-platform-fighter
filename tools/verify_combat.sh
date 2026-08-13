#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/combat"}
compiler=${CC:-cc}

mkdir -p "$output_dir"

grep -Fq 'PF_M4_ACTION_PUMMEL = 78' "$root/include/pf/m4.h"
grep -Fq 'PF_SIM_EVENT_PUMMEL = 22' "$root/include/pf/sim.h"
grep -Fq 'pummel_damage_f32' "$root/include/pf/m4.h"
grep -Fq 'PF_SIM_EVENT_FLAG_CROUCH_CANCEL = 1 << 4' "$root/include/pf/sim.h"
grep -Fq 'crouch_cancel_max_damage_f32' "$root/include/pf/m4.h"
grep -Fq 'crouch_cancel_hitstun_scale_f32' "$root/include/pf/m4.h"
grep -Fq 'weight_f32' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_FORWARD_AERIAL = 81' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_BACK_AERIAL = 82' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_UP_AERIAL = 83' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_DOWN_AERIAL = 84' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_LEDGE_ROLL = 85' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_LEDGE_ATTACK = 86' "$root/include/pf/m4.h"
grep -Fq 'forward_aerial' "$root/include/pf/m4.h"
grep -Fq 'ledge_attack' "$root/include/pf/m4.h"
grep -Fq 'ledge_attack_invulnerability_ticks' "$root/include/pf/m4.h"
grep -Fq 'select_light_aerial_action' "$root/src/sim/sim_movement.c"
grep -Fq 'directional_aerials=1' "$root/tests/sim/test_combat.c"
grep -Fq 'ledge_attack=1' "$root/tests/sim/test_combat.c"

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
    "$root/tests/sim/ssbm_stored_oracle.c" \
    "$root/tests/sim/test_combat.c" \
    -o "$output_dir/combat_test"

"$output_dir/combat_test" >"$output_dir/combat.txt"
grep -Fqx \
    'm4-combat=pass content_schema=74 deterministic_ticks=20000 combat_core=pass journal_invariants=74 weight=1 stale_move=1 prone_getup_roll=4 directional_ground_attacks=1 smash_charge=1 light_shield=1 shield_geometry=1 shield_sdi=1 ssbm_damage=1 directional_aerials=1 ledge_attack=1 crouch_cancel=1 double_jump_cancel_counter=1 approach=1 spacing=1 mindgame=1 jab_cancel=1 pummel=1 directional_throws=1 chain_grab=1 team_resolution=1 team_wobble=skipped emergent_technique_tests=skipped' \
    "$output_dir/combat.txt"

"$root/tools/verify_technique_registry.sh"

echo "m4-combat-verification=pass combat_core=pass journal_invariants=74 deterministic_ticks=20000 weight=1 stale_move=1 prone_getup_roll=4 directional_ground_attacks=1 directional_aerials=1 ledge_attack=1 crouch_cancel=1 shield_sdi=1 approach=1 spacing=1 mindgame=1 jab_cancel=1 pummel=1 directional_throws=1 chain_grab=1 team_resolution=1 team_wobble=skipped emergent_technique_tests=skipped"

#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_browser"}
compiler=${CC:-cc}

mkdir -p "$output_dir"

grep -Fq 'PF_M4_ACTION_LEDGE_ROLL = 85' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_LEDGE_ATTACK = 86' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_FORWARD_ATTACK = 87' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_FORWARD_STRONG_ATTACK = 88' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_UP_STRONG_ATTACK = 89' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_DOWN_STRONG_ATTACK = 90' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_FORWARD_STRONG_CHARGE = 91' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_UP_STRONG_CHARGE = 92' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_ACTION_DOWN_STRONG_CHARGE = 93' "$root/include/pf/m4.h"
grep -Fq 'pf_web_m4_run_ledge_option_probe' \
    "$root/src/web_client/m4_playtest.c"

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
    -I"$root/src/web_client" \
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
    "$root/src/web_client/m4_playtest.c" \
    "$root/tests/web/test_m4_playtest.c" \
    -o "$output_dir/m4_web_playtest_test"

"$output_dir/m4_web_playtest_test" >"$output_dir/m4_web_playtest.txt"
grep -Fq \
    'm4-browser-adapter=pass walk_axis=13500 dash_axis=32767 input_probe=1 air_facing_probe=1 instant_double_jump_probe=1 double_jump_cancel_probe=1 double_jump_cancel_counter_probe=1 bat_drop_probe=1 glide_toss_probe=1 jump_cancel_throw_probe=1 jump_cancel_probe=1 edge_hop_probe=1 edge_dash_probe=1 fox_trot_probe=1 moonwalk_probe=1 teeter_cancel_probe=1 stage_humping_probe=1 taunt_cancel_probe=1 scar_jump_probe=1 team_wobble_probe=1 pivot_probe=1 dash_cancel_probe=1 dashing_shield_probe=1 shield_platform_drop_probe=1 small_step_forward_smash_probe=1 drop_cancel_probe=1 v_cancel_probe=1 approach_probe=1 spacing_probe=1 sharking_probe=1 cross_up_probe=1 mindgame_probe=1 juggling_probe=1 ladder_probe=1 kill_confirm_probe=1 zero_to_death_probe=1 ledge_cancel_probe=1 planking_probe=1 jump_cancelled_grab_probe=1 boost_grab_probe=1 jab_cancel_probe=1 jab_reset_probe=1 chain_grab_probe=1 combat_probe=1 reaction_probe=1 shield_probe=1 shield_break_probe=1 powershield_cancel_probe=1 tumble_probe=1 floor_recovery_probe=1 tech_chase_probe=1 surface_tech_probe=1 air_dodge_probe=1 ground_dodge_probe=1 aerial_l_cancel_probe=1 match_probe=1 short_hop_laser_probe=1 camping_probe=1 shine_spike_probe=1 charge_storage_probe=1 vector_ascent_probe=1 event_journal_probe=1' \
    "$output_dir/m4_web_playtest.txt"

command -v node >/dev/null 2>&1 ||
    {
        echo "M4 browser verification requires Node.js for JavaScript syntax checking" >&2
        exit 1
    }
node --check "$root/src/web_client/web_adapter.js"
node --check "$root/src/web_client/m4_owner_evidence.js"

grep -Fq \
    'PFInstallM4OwnerEvidence' \
    "$root/src/web_client/m4_owner_evidence.js"
grep -Fq \
    'pf-m4-owner-evidence' \
    "$root/src/web_client/m4_owner_evidence.js"
grep -Fq \
    'pf-m4-owner-techniques' \
    "$root/src/web_client/m4_owner_evidence.js"
grep -Fq \
    'technique-only simulation behavior or harnesses' \
    "$root/src/web_client/m4_owner_evidence.js"
grep -Fq \
    'root.localStorage.setItem(storageKey, JSON.stringify(evidence))' \
    "$root/src/web_client/m4_owner_evidence.js"
grep -Fq \
    'm4-owner-playtest-' \
    "$root/src/web_client/m4_owner_evidence.js"
grep -Fq \
    'ownerChecklist.techniques.length === 61' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'owner_checklist=' \
    "$root/src/web_client/web_adapter.js"

grep -Fq \
    'held("ShiftLeft") || held("ShiftRight")' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" playtest="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" input_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" air_facing_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" instant_double_jump_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" double_jump_cancel_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" bat_drop_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" glide_toss_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" jump_cancel_throw_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" jump_cancel_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" double_jump_cancel_counter_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" edge_hop_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" edge_dash_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" fox_trot_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" moonwalk_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" teeter_cancel_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" stage_humping_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" taunt_cancel_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" scar_jump_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" team_wobble_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" pivot_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" dash_cancel_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" dashing_shield_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" shield_platform_drop_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" small_step_forward_smash_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" drop_cancel_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" v_cancel_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" approach_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" spacing_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" sharking_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" cross_up_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" mindgame_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" planking_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" jump_cancelled_grab_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" boost_grab_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" jab_cancel_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" jab_reset_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" chain_grab_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'view[0] !== 42' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'pf_web_m4_run_directional_aerial_probe' \
    "$root/src/web_client/m4_playtest.c"
grep -Fq \
    '"DASH ATTACK"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"JAB FINAL"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"RESET BOUND"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"FORCED GETUP"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"DELAYED AIR JUMP"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"ITEM THROW"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"PULSE BOLT AIR"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'case 18:' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'case 21:' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'case 22:' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"DOWN THROW"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'case 13:' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'navigator.getGamepads()' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'gamepad.mapping !== "standard"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'Math.abs(value) < 0.2' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'jumpQueued: [false, false]' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'attackQueued: [false, false]' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'strongAttackQueued: [false, false]' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'shieldQueued: [false, false]' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'specialQueued: [false, false]' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'tauntQueued: [false, false]' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '_pf_web_m4_playtest_step_special' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '_pf_web_m4_playtest_refresh' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '_pf_web_m4_playtest_configure_duel' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '_pf_web_m4_playtest_set_team_lab' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"Team Wobble Lab"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"Local 1v1 match setup"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'viewCount !== 431' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"MASH OUT · "' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" event_journal_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"Deterministic combat event feed"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" shield_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" shield_break_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" powershield_cancel_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" tumble_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" floor_recovery_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" tech_chase_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" surface_tech_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" air_dodge_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" ground_dodge_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" aerial_l_cancel_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" match_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" short_hop_laser_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" camping_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" shine_spike_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" charge_storage_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" vector_ascent_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"PRISM BURST GROUND"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"PRISM BURST AIR"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"ARC RESERVOIR CHARGE"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"ARC RESERVOIR STORE"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"ARC RESERVOIR RELEASE"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"MOONWALK SETUP"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"MOONWALK"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"CROUCH STEP"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"TAUNT"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"WALL JUMP"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"VECTOR ASCENT"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"PUMMEL"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"UP ATTACK"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"DOWN ATTACK"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"FORWARD AERIAL"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"BACK AERIAL"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"UP AERIAL"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"DOWN AERIAL"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"LEDGE ROLL"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"LEDGE ATTACK"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"FORWARD ATTACK"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"FORWARD STRONG"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"UP STRONG"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"DOWN STRONG"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"FORWARD STRONG CHARGE"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"UP STRONG CHARGE"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"DOWN STRONG CHARGE"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'pummel for 3%' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'CROUCH CANCEL' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'to crouch cancel' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'Vector Ascent recovery in the air' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"MISSED STRONG L-CANCEL"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"STRONG L-CANCEL!"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'strongAerialLandingLagTicks: strongAerialLandingLagTicks' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" gamepad_probe="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '" gamepad_api="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'controls=keyboard-gamepad-two-controller-duel-team-lab' \
    "$root/tools/verify_web_smoke.sh"

echo "m4-browser-verification=pass walk_axis=13500 dash_axis=32767 input_probe=1 air_facing_probe=1 instant_double_jump_probe=1 double_jump_cancel_probe=1 double_jump_cancel_counter_probe=1 bat_drop_probe=1 glide_toss_probe=1 jump_cancel_throw_probe=1 jump_cancel_probe=1 edge_hop_probe=1 edge_dash_probe=1 fox_trot_probe=1 moonwalk_probe=1 teeter_cancel_probe=1 stage_humping_probe=1 taunt_cancel_probe=1 scar_jump_probe=1 team_wobble_probe=1 pivot_probe=1 dash_cancel_probe=1 dashing_shield_probe=1 shield_platform_drop_probe=1 small_step_forward_smash_probe=1 drop_cancel_probe=1 v_cancel_probe=1 approach_probe=1 spacing_probe=1 sharking_probe=1 cross_up_probe=1 mindgame_probe=1 juggling_probe=1 ladder_probe=1 kill_confirm_probe=1 zero_to_death_probe=1 ledge_cancel_probe=1 planking_probe=1 jump_cancelled_grab_probe=1 boost_grab_probe=1 jab_cancel_probe=1 jab_reset_probe=1 chain_grab_probe=1 combat_probe=1 event_journal_probe=1 reaction_probe=1 shield_probe=1 shield_break_probe=1 powershield_cancel_probe=1 tumble_probe=1 floor_recovery_probe=1 tech_chase_probe=1 surface_tech_probe=1 air_dodge_probe=1 ground_dodge_probe=1 aerial_l_cancel_probe=1 match_probe=1 short_hop_laser_probe=1 camping_probe=1 shine_spike_probe=1 charge_storage_probe=1 vector_ascent_probe=1 gamepad_polling=1 standard_mapping=1 team_lab=1 owner_checklist=61"

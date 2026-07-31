#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_browser"}
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
    -I"$root/src/web_client" \
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
    "$root/src/web_client/m4_playtest.c" \
    "$root/tests/web/test_m4_playtest.c" \
    -o "$output_dir/m4_web_playtest_test"

"$output_dir/m4_web_playtest_test" >"$output_dir/m4_web_playtest.txt"
grep -Fq \
    'm4-browser-adapter=pass walk_axis=13500 dash_axis=32767 input_probe=1 air_facing_probe=1 instant_double_jump_probe=1 double_jump_cancel_probe=1 double_jump_cancel_counter_probe=1 bat_drop_probe=1 glide_toss_probe=1 jump_cancel_throw_probe=1 edge_hop_probe=1 edge_dash_probe=1 fox_trot_probe=1 pivot_probe=1 dash_cancel_probe=1 dashing_shield_probe=1 shield_platform_drop_probe=1 small_step_forward_smash_probe=1 drop_cancel_probe=1 v_cancel_probe=1 approach_probe=1 spacing_probe=1 sharking_probe=1 cross_up_probe=1 mindgame_probe=1 juggling_probe=1 ladder_probe=1 kill_confirm_probe=1 zero_to_death_probe=1 ledge_cancel_probe=1 planking_probe=1 jump_cancelled_grab_probe=1 boost_grab_probe=1 jab_cancel_probe=1 jab_reset_probe=1 chain_grab_probe=1 combat_probe=1 reaction_probe=1 shield_probe=1 shield_break_probe=1 powershield_cancel_probe=1 tumble_probe=1 floor_recovery_probe=1 tech_chase_probe=1 surface_tech_probe=1 air_dodge_probe=1 ground_dodge_probe=1 aerial_l_cancel_probe=1 match_probe=1 event_journal_probe=1' \
    "$output_dir/m4_web_playtest.txt"

command -v node >/dev/null 2>&1 ||
    {
        echo "M4 browser verification requires Node.js for JavaScript syntax checking" >&2
        exit 1
    }
node --check "$root/src/web_client/web_adapter.js"

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
    'view[0] !== 22' \
    "$root/src/web_client/web_adapter.js"
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
    'case 18:' \
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
    'viewCount !== 290' \
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
    'controls=keyboard-gamepad-two-player' \
    "$root/tools/verify_web_smoke.sh"

echo "m4-browser-verification=pass walk_axis=13500 dash_axis=32767 input_probe=1 air_facing_probe=1 instant_double_jump_probe=1 double_jump_cancel_probe=1 double_jump_cancel_counter_probe=1 edge_hop_probe=1 edge_dash_probe=1 fox_trot_probe=1 pivot_probe=1 dash_cancel_probe=1 dashing_shield_probe=1 shield_platform_drop_probe=1 small_step_forward_smash_probe=1 drop_cancel_probe=1 v_cancel_probe=1 approach_probe=1 spacing_probe=1 sharking_probe=1 cross_up_probe=1 mindgame_probe=1 juggling_probe=1 ladder_probe=1 kill_confirm_probe=1 zero_to_death_probe=1 ledge_cancel_probe=1 planking_probe=1 jump_cancelled_grab_probe=1 boost_grab_probe=1 jab_cancel_probe=1 jab_reset_probe=1 chain_grab_probe=1 combat_probe=1 event_journal_probe=1 reaction_probe=1 shield_probe=1 shield_break_probe=1 powershield_cancel_probe=1 tumble_probe=1 floor_recovery_probe=1 tech_chase_probe=1 surface_tech_probe=1 air_dodge_probe=1 ground_dodge_probe=1 aerial_l_cancel_probe=1 match_probe=1 gamepad_polling=1 standard_mapping=1"

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
grep -Fq 'PF_M4_ACTION_REVIVAL_PLATFORM = 94' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_SURFACE_REVIVAL_PLATFORM = 4' "$root/include/pf/m4.h"
grep -Fq 'PF_M4_SURFACE_UPPER_PLATFORM = 5' "$root/include/pf/m4.h"
grep -Fq 'PF_SIM_EVENT_REVIVAL_DROP = 23' "$root/include/pf/sim.h"
grep -Fq 'PF_SIM_EVENT_ACTION_TRANSITIONS = 24' "$root/include/pf/sim.h"
grep -Fq 'pf_web_m4_view[PF_WEB_M4_VIEW_SCHEMA] = INT32_C(47);' \
    "$root/src/web_client/m4_playtest.c"
grep -Fq '#define PF_WEB_M4_VIEW_REVIVAL0 431' \
    "$root/src/web_client/m4_playtest.c"
grep -Fq '#define PF_WEB_M4_VIEW_STALE_MOVE0 447' \
    "$root/src/web_client/m4_playtest.c"
grep -Fq '#define PF_WEB_M4_VIEW_UPPER_PLATFORM0 496' \
    "$root/src/web_client/m4_playtest.c"
grep -Fq '#define PF_WEB_M4_VIEW_PRONE_ORIENTATION0 499' \
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
    "$root/src/sim/sim_falcon_frame_data.c" \
    "$root/src/sim/sim_ssbm_common_data.c" \
    "$root/src/sim/sim_ssbm_damage.c" \
    "$root/src/sim/sim_ssbm_stage_data.c" \
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
    'm4-browser-adapter=pass walk_axis=13500 dash_axis=32767 renders=' \
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
    'fighter->jump_horizontal_momentum_multiplier_q16' \
    "$root/src/sim/sim_movement.c"
grep -Fq \
    'immediately hold backward through jump squat' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'view[0] !== 47' \
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
    'case 23:' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'case 24:' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'eventMaskPlayers(event.detail) + " forfeited"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"REVIVAL PLATFORM"' \
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
    "$root/CMakeLists.txt"
grep -Fq \
    '_pf_web_m4_playtest_step_dual_trigger_special' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'secondaryHorizontal: 0' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'player0Gamepad.secondaryHorizontal' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'input.horizontal = mayflashStickAxis(gamepad, 0)' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'var cStickX = mayflashStickAxis(gamepad, 5)' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'var cStickX = gamepadAxis(gamepad, 2)' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'var cStickY = gamepadAxis(gamepad, 3)' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'no fresh C-stick edge is required' \
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
    'viewCount !== 603' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'stale queue newest first' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"MOVE / BUTTON TO DROP"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"MASH OUT · "' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"Deterministic combat event feed"' \
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
    'Falcon Dive recovery from the ground or air' \
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
    '" gamepad_api="' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'function mapMayflashGameCubeAdapter(gamepad)' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"standard-mayflash-0079-1843-webusb-057e-0337"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'function parseWiiUAdapterReport(report)' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'navigator.usb.requestDevice' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'new Uint8Array([0x13])' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'section.dataset.crouchCue = "squat-chevron-label"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'section.dataset.lightShieldCue = "expanded-translucent-percent-label"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'section.dataset.shieldCue = "readable-margin-strength-label"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'section.dataset.shieldHealthCue = "melee-health-density-scale"' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'context.fillText("CROUCH", x, crouchCueY)' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'var shieldHealthPresentationRatio =' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'var shieldPresentationPadding = lightShielding ? 22 : 14' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'leftShieldStrength: 0' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'rightShieldStrength: 0' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"LIGHT SHIELD "' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    '"FULL SHIELD "' \
    "$root/src/web_client/web_adapter.js"
grep -Fq \
    'controls=keyboard-gamepad-webusb-two-controller-duel-team-lab' \
    "$root/tools/verify_web_smoke.sh"

echo "m4-browser-verification=pass walk_axis=13500 dash_axis=32767 adapter_core=pass gamepad_polling=1 standard_mapping=1 wii_u_webusb=1 team_lab=1 owner_checklist=61"

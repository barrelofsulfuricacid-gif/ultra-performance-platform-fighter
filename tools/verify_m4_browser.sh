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

pf_require_source()
{
    file=$1
    label=$2
    expected=$3
    if ! grep -Fq "$expected" "$file"; then
        echo "M4 browser verification is missing $label in $file" >&2
        exit 1
    fi
}

adapter="$root/src/web_client/web_adapter.js"
owner_evidence="$root/src/web_client/m4_owner_evidence.js"

pf_require_source \
    "$owner_evidence" \
    "owner-evidence installer" \
    'PFInstallM4OwnerEvidence'
pf_require_source \
    "$adapter" \
    "view schema guard" \
    'viewCount !== 603'
pf_require_source \
    "$adapter" \
    "standard Gamepad polling" \
    'navigator.getGamepads()'
pf_require_source \
    "$adapter" \
    "Mayflash adapter mapping" \
    'function mapMayflashGameCubeAdapter(gamepad)'
pf_require_source \
    "$adapter" \
    "Mayflash C-stick mapping" \
    'var cStickX = mayflashStickAxis(gamepad, 5)'
pf_require_source \
    "$adapter" \
    "standard C-stick X mapping" \
    'var cStickX = gamepadAxis(gamepad, 2)'
pf_require_source \
    "$adapter" \
    "standard C-stick Y mapping" \
    'var cStickY = gamepadAxis(gamepad, 3)'
pf_require_source \
    "$adapter" \
    "independent left-trigger state" \
    'leftShieldStrength: 0'
pf_require_source \
    "$adapter" \
    "independent right-trigger state" \
    'rightShieldStrength: 0'
pf_require_source \
    "$adapter" \
    "Wii U adapter report parser" \
    'function parseWiiUAdapterReport(report)'
pf_require_source \
    "$adapter" \
    "WebUSB device request" \
    'navigator.usb.requestDevice'
pf_require_source \
    "$adapter" \
    "dual-trigger bridge endpoint" \
    '_pf_web_m4_playtest_step_dual_trigger_special'
pf_require_source \
    "$adapter" \
    "duel configuration endpoint" \
    '_pf_web_m4_playtest_configure_duel'
pf_require_source \
    "$adapter" \
    "team-lab endpoint" \
    '_pf_web_m4_playtest_set_team_lab'
pf_require_source \
    "$adapter" \
    "crouch visual cue" \
    'section.dataset.crouchCue = "squat-chevron-label"'
pf_require_source \
    "$adapter" \
    "light-shield visual cue" \
    'section.dataset.lightShieldCue = "expanded-translucent-percent-label"'
pf_require_source \
    "$adapter" \
    "shield-health visual scale" \
    'section.dataset.shieldHealthCue = "melee-health-density-scale"'

echo "m4-browser-verification=pass walk_axis=13500 dash_axis=32767 adapter_core=pass gamepad_polling=1 standard_mapping=1 wii_u_webusb=1 team_lab=1 owner_checklist=61"

#!/bin/sh
set -eu

repository_root=$(git rev-parse --show-toplevel)
web_root="$repository_root/build/web"
port=${PF_WEB_SMOKE_PORT:-8123}
url="http://127.0.0.1:$port/web_client.html"
server_log="$web_root/web_smoke_server.log"
dom_output="$web_root/web_smoke_dom.html"
browser_log="$web_root/web_smoke_browser.log"

[ -f "$web_root/web_client.html" ] ||
    {
        echo "web smoke is missing; run ./tools/workflow.sh web first" >&2
        exit 1
    }

browser=${PF_BROWSER:-}
if [ -z "$browser" ]; then
    for candidate in \
        google-chrome-stable \
        google-chrome \
        chromium \
        chromium-browser \
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
    do
        if command -v "$candidate" >/dev/null 2>&1; then
            browser=$(command -v "$candidate")
            break
        fi
        if [ -x "$candidate" ]; then
            browser=$candidate
            break
        fi
    done
fi

[ -n "$browser" ] && [ -x "$browser" ] ||
    {
        echo "Chrome/Chromium was not found; set PF_BROWSER to its executable" >&2
        exit 1
    }

python3 -m http.server \
    "$port" \
    --bind 127.0.0.1 \
    --directory "$web_root" \
    >"$server_log" 2>&1 &
server_pid=$!
trap 'kill "$server_pid" 2>/dev/null || true' EXIT HUP INT TERM

attempt=0
while ! curl -fsS "$url" >/dev/null 2>&1; do
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 20 ]; then
        echo "browser smoke server did not become ready: $server_log" >&2
        exit 1
    fi
    sleep 1
done

if ! "$browser" \
    --headless \
    --no-sandbox \
    --use-gl=angle \
    --use-angle=swiftshader \
    --enable-unsafe-swiftshader \
    --virtual-time-budget=10000 \
    --dump-dom \
    "$url" \
    >"$dom_output" \
    2>"$browser_log"
then
    echo "web browser smoke failed: Chrome exited unsuccessfully" >&2
    tail -n 80 "$browser_log" >&2
    exit 1
fi

pf_dump_browser_diagnostics()
{
    echo "web browser smoke captured status:" >&2
    sed -n \
        '/id="pf-status"/p;/id="output"/p;/id="pf-m4-playtest"/p' \
        "$dom_output" >&2
    echo "web browser smoke browser log tail:" >&2
    tail -n 40 "$browser_log" >&2
}

pf_require_dom()
{
    pf_label=$1
    pf_expected=$2
    if ! grep -Fq "$pf_expected" "$dom_output"; then
        echo "web browser smoke failed: missing $pf_label" >&2
        pf_dump_browser_diagnostics
        exit 1
    fi
}

pf_require_dom \
    "simulation ABI status" \
    'web-client-smoke=pass sim_abi=4 tick_hz=60'
pf_require_dom \
    "WebGL2 status" \
    'webgl2=pass batch_draws=1'
pf_require_dom \
    "deterministic replay status" \
    'replay=pass ticks=180 winner_mask=5 final_sha256=e4e13bede51559acd3dabed736b1cd961a0f97294ab772309fe7f0d1e0c0e535'
pf_require_dom \
    "replay inspector" \
    'id="pf-replay-inspector"'
pf_require_dom \
    "verified replay-file event visualization" \
    'data-replay-event-visualization="verified-per-tick-events"'
pf_require_dom \
    "replay file input" \
    'id="pf-replay-file"'
pf_require_dom \
    "replay event timeline" \
    'id="pf-replay-events"'
pf_require_dom \
    "re-simulated canonical events" \
    '3 typed events'
pf_require_dom \
    "M4 input/IDJ/ground-dodge/air-facing/air-dodge/combat/event-journal/reaction/shield/tumble/floor/tech-chase/surface-tech/charge probe status" \
    'playtest=ready input_probe=pass air_facing_probe=pass instant_double_jump_probe=pass double_jump_cancel_probe=pass double_jump_cancel_counter_probe=pass bat_drop_probe=pass glide_toss_probe=pass jump_cancel_throw_probe=pass jump_cancel_probe=pass edge_hop_probe=pass edge_dash_probe=pass fox_trot_probe=pass moonwalk_probe=pass teeter_cancel_probe=pass stage_humping_probe=pass taunt_cancel_probe=pass scar_jump_probe=pass team_wobble_probe=pass pivot_probe=pass dash_cancel_probe=pass dashing_shield_probe=pass shield_platform_drop_probe=pass small_step_forward_smash_probe=pass drop_cancel_probe=pass v_cancel_probe=pass approach_probe=pass spacing_probe=pass sharking_probe=pass cross_up_probe=pass mindgame_probe=pass juggling_probe=pass ladder_probe=pass kill_confirm_probe=pass zero_to_death_probe=pass ledge_cancel_probe=pass planking_probe=pass jump_cancelled_grab_probe=pass boost_grab_probe=pass jab_cancel_probe=pass jab_reset_probe=pass chain_grab_probe=pass combat_probe=pass event_journal_probe=pass reaction_probe=pass shield_probe=pass shield_break_probe=pass powershield_cancel_probe=pass tumble_probe=pass floor_recovery_probe=pass tech_chase_probe=pass surface_tech_probe=pass air_dodge_probe=pass ground_dodge_probe=pass aerial_l_cancel_probe=pass match_probe=pass short_hop_laser_probe=pass camping_probe=pass shine_spike_probe=pass charge_storage_probe=pass vector_ascent_probe=pass gamepad_probe=pass gamepad_api=available controls=keyboard-gamepad-two-controller-duel-team-lab'
pf_require_dom \
    "M4 playtest surface" \
    'id="pf-m4-playtest"'
pf_require_dom \
    "M4 owner checklist status" \
    'owner_checklist=ready-61'
pf_require_dom \
    "M4 owner checklist source" \
    'data-owner-checklist="ready"'
pf_require_dom \
    "M4 owner checklist schema" \
    'data-owner-checklist-schema="1"'
pf_require_dom \
    "M4 owner checklist revision" \
    'data-owner-checklist-revision="2048934"'
pf_require_dom \
    "M4 owner evidence panel" \
    'id="pf-m4-owner-evidence"'
pf_require_dom \
    "M4 owner evidence exports" \
    'id="pf-m4-owner-export-markdown"'
pf_require_dom \
    "M4 owner match gate" \
    'id="pf-m4-owner-complete-match"'
owner_recipe_count=$(grep -Fo 'class="pf-m4-owner-technique"' "$dom_output" | wc -l)
if [ "$owner_recipe_count" -ne 61 ]; then
    echo "web browser smoke failed: expected 61 owner recipes, got $owner_recipe_count" >&2
    pf_dump_browser_diagnostics
    exit 1
fi
pf_require_dom \
    "M4 collision inspector semantics" \
    'data-collision-overlay-semantics="stage-hurtbox-attack-grab-item-projectile-blast"'
pf_require_dom \
    "M4 collision inspector toggle" \
    'id="pf-m4-collision-overlay"'
pf_require_dom \
    "M4 collision inspector legend" \
    'id="pf-m4-collision-legend"'
pf_require_dom \
    "M4 local match setup state" \
    'data-match-flow="setup"'
pf_require_dom \
    "M4 local match setup panel" \
    'id="pf-m4-match-setup"'
pf_require_dom \
    "M4 stock selector" \
    'id="pf-m4-stock-count"'
pf_require_dom \
    "M4 explicit match start" \
    'id="pf-m4-start-match"'
pf_require_dom \
    "M4 browser title" \
    'Platform Fighter M4 Browser Playtest'

echo "web-browser-smoke=pass browser=$browser url=$url"

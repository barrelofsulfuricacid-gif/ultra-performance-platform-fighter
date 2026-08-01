#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_match_flow"}
adapter="$root/src/web_client/web_adapter.js"
bridge="$root/src/web_client/m4_playtest.c"

mkdir -p "$output_dir"

command -v node >/dev/null 2>&1 ||
    {
        echo "M4 match-flow verification requires Node.js" >&2
        exit 1
    }
node --check "$adapter"

pf_require_source()
{
    pf_label=$1
    pf_expected=$2
    pf_path=$3
    if ! grep -Fq "$pf_expected" "$pf_path"; then
        echo "M4 match-flow verification failed: missing $pf_label" >&2
        exit 1
    fi
}

pf_require_source \
    "setup state" \
    'section.dataset.matchFlow = "setup";' \
    "$adapter"
pf_require_source \
    "accessible setup panel" \
    'setupPanel.id = "pf-m4-match-setup";' \
    "$adapter"
pf_require_source \
    "bounded stock selector" \
    '[1, 2, 3, 4].forEach(function (stockCount)' \
    "$adapter"
pf_require_source \
    "explicit local start" \
    'startMatchButton.id = "pf-m4-start-match";' \
    "$adapter"
pf_require_source \
    "production duel configuration" \
    'Module._pf_web_m4_playtest_configure_duel(stockCount)' \
    "$adapter"
pf_require_source \
    "results state" \
    'state.setMatchFlow("results");' \
    "$adapter"
pf_require_source \
    "rematch label" \
    '? "Rematch" : "Reset";' \
    "$adapter"
pf_require_source \
    "return to setup" \
    'setupButton.addEventListener("click", openSetup);' \
    "$adapter"
pf_require_source \
    "step control state ownership" \
    'stepButton: stepButton,' \
    "$adapter"
pf_require_source \
    "setup control gating" \
    'state.stepButton.disabled = !playing;' \
    "$adapter"
pf_require_source \
    "bounded native stock validation" \
    'stock_count > (int)PF_WEB_M4_MAX_SETUP_STOCKS' \
    "$bridge"
pf_require_source \
    "native duel reinitialization" \
    'int pf_web_m4_playtest_configure_duel(int stock_count)' \
    "$bridge"

cat >"$output_dir/flow.txt" <<'EOF'
setup=local-1v1,fixed-fighter,fixed-stage,stocks-1-through-4
playing=pause,step,reset,team-lab,collision-inspector
results=winner-or-time-limit,rematch,change-setup
EOF

echo "m4-local-match-flow=pass setup=1 stock_choices=4 results=1 rematch=1 return_setup=1"

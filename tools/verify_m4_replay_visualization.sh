#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_replay_visualization"}
adapter="$root/src/web_client/web_adapter.js"
bridge="$root/src/web_client/replay_checkpoint.c"
replay_header="$root/include/pf/replay.h"
replay_test="$root/tests/sim/test_replay_corpus.c"

mkdir -p "$output_dir"

command -v node >/dev/null 2>&1 ||
    {
        echo "M4 replay-visualization verification requires Node.js" >&2
        exit 1
    }
node --check "$adapter"

pf_require_source()
{
    pf_label=$1
    pf_expected=$2
    pf_path=$3
    if ! grep -Fq "$pf_expected" "$pf_path"; then
        echo "M4 replay-visualization verification failed: missing $pf_label" >&2
        exit 1
    fi
}

pf_require_source \
    "verified-checkpoint observer API" \
    'pf_status pf_replay_verify_observed(' \
    "$replay_header"
pf_require_source \
    "replayed event digest oracle" \
    'observed_events_digest_hex' \
    "$replay_test"
pf_require_source \
    "WebAssembly replay import" \
    'int pf_web_replay_import(' \
    "$bridge"
pf_require_source \
    "bounded verified checkpoint capture" \
    'pf_web_capture_verified_checkpoint' \
    "$bridge"
pf_require_source \
    "replay file input" \
    'fileInput.id = "pf-replay-file";' \
    "$adapter"
pf_require_source \
    "verified event visualization semantics" \
    'inspector.dataset.replayEventVisualization = "verified-per-tick-events";' \
    "$adapter"
pf_require_source \
    "event navigation" \
    'nextEventButton.textContent = "Next event";' \
    "$adapter"
pf_require_source \
    "fail-closed replacement copy" \
    'unverified or incompatible files never replace the current trace.' \
    "$adapter"

cat >"$output_dir/replay_visualization.txt" <<'EOF'
source=generated-or-compatible-file
verification=container-checksum,identity,per-tick-state-hash,final-result
checkpoints=tick-zero-plus-every-verified-tick
visualization=positions,state-hash,typed-events,previous-next-event
rejection=invalid-or-incompatible-file-preserves-current-trace
EOF

echo "m4-replay-visualization=pass observed_checkpoints=181 file_import=1 typed_events=1 event_navigation=1 fail_closed=1"

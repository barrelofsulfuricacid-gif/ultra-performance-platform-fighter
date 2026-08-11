#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 /path/to/GALE01.iso [output.json]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iso="$1"
output="${2:-${repo_root}/build/oracle/falcon-ground-loop-ecb.json}"
manifest="${repo_root}/tools/ssbm_falcon_grounded_loop_hurt_coverage.json"
profile="${repo_root}/tools/data/ssbm_falcon_ground_loop_ecb.json"
toolchain_root="${repo_root}/build/oracle-toolchain"
python="${toolchain_root}/exiai-python/bin/python"
dolphin="${toolchain_root}/exiai-checkpoint/Binaries/dolphin-emu"
release_artifact="${toolchain_root}/exiai-0.2.0/Slippi_Online-x86_64-ExiAI.AppImage"
started_ns="$(date +%s%N)"

"${python}" "${repo_root}/tools/capture_ssbm_checkpoint_shards.py" \
    --dolphin "${dolphin}" \
    --oracle-release-artifact "${release_artifact}" \
    --iso "${iso}" \
    --manifest "${manifest}" \
    --output "${output}" \
    --base-port 51441 \
    --started-ns "${started_ns}" \
    --case crouch-wait-poses \
    --shard-count 1 \
    --memory-probe surface \
    --checkpoint-no-batch-inputs \
    --projection-warm-budget-seconds 10 \
    --projection-cold-budget-seconds 14

"${python}" "${repo_root}/tools/verify_ssbm_falcon_ground_loop_ecb.py" \
    "${profile}" --capture "${output}"

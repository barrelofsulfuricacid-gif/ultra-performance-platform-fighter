#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 /path/to/GALE01.iso [output.json]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iso="$1"
output="${2:-${repo_root}/build/oracle/falcon-getup-ecb.json}"
manifest="${repo_root}/tools/ssbm_falcon_prone_response_coverage.json"
profile="${repo_root}/tools/data/ssbm_falcon_getup_ecb.json"
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
    --case timeout \
    --case buffered_a_getup_attack \
    --case c_roll_forward \
    --case main_roll_backward \
    --case up_timeout \
    --case up_buffered_a_getup_attack \
    --case up_wait_c_roll_forward \
    --case up_wait_main_roll_backward \
    --shard-count 4 \
    --memory-probe surface \
    --checkpoint-no-batch-inputs \
    --projection-warm-budget-seconds 15 \
    --projection-cold-budget-seconds 18

"${python}" "${repo_root}/tools/verify_ssbm_falcon_getup_ecb.py" \
    "${profile}" --capture "${output}"

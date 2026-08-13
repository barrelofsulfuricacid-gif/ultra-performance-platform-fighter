#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 /path/to/GALE01.iso [output.json]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iso="$1"
output="${2:-${repo_root}/build/oracle/falcon-down-bound-ecb.json}"
toolchain_root="${repo_root}/build/oracle-toolchain"
python="${toolchain_root}/exiai-python/bin/python"
dolphin="${toolchain_root}/exiai-checkpoint/Binaries/dolphin-emu"
release_artifact="${toolchain_root}/exiai-0.2.0/Slippi_Online-x86_64-ExiAI.AppImage"
coverage="${repo_root}/tools/ssbm_falcon_prone_response_coverage.json"
profile="${repo_root}/tools/data/ssbm_falcon_down_bound_ecb.json"

"${python}" "${repo_root}/tools/capture_ssbm_movement.py" \
    --dolphin "${dolphin}" \
    --oracle-release-artifact "${release_artifact}" \
    --iso "${iso}" \
    --output "${output}" \
    --slippi-port 51445 \
    --damage-hit-only \
    --memory-probe-surface \
    --oracle-exiai \
    --oracle-checkpoint-pack \
    --oracle-checkpoint-no-batch-inputs \
    --oracle-coverage-manifest "${coverage}" \
    --oracle-case timeout \
    --oracle-case up_timeout

"${python}" "${repo_root}/tools/verify_ssbm_falcon_down_bound_ecb.py" \
    "${profile}" --capture "${output}"

#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 /path/to/GALE01.iso [output.json]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iso="$1"
output="${2:-${repo_root}/build/oracle/falcon-ledge-collision.json}"
coverage_manifest="${repo_root}/tools/ssbm_falcon_ledge_options_coverage.json"
profile="${repo_root}/tools/data/ssbm_falcon_ledge_hurt.json"
import_manifest="${repo_root}/tools/ssbm_falcon_ledge_hurt_import.json"
toolchain_root="${repo_root}/build/oracle-toolchain"
user_home="$(getent passwd "$(id -u)" | cut -d: -f6)"
python="${user_home}/.cache/pf-ssbm-exiai-python-0.47.2/bin/python"
if [[ ! -x "${python}" ]]; then
    python="${toolchain_root}/exiai-python/bin/python"
fi
dolphin="${toolchain_root}/exiai-checkpoint/Binaries/dolphin-emu"
release_artifact="${toolchain_root}/exiai-0.2.0/Slippi_Online-x86_64-ExiAI.AppImage"
started_ns="$(date +%s%N)"

"${python}" "${repo_root}/tools/capture_ssbm_checkpoint_shards.py" \
    --dolphin "${dolphin}" \
    --oracle-release-artifact "${release_artifact}" \
    --iso "${iso}" \
    --manifest "${coverage_manifest}" \
    --output "${output}" \
    --memory-probe hitbox \
    --disable-fast-forward \
    --case quick_climb_collision_hit \
    --case quick_climb_collision_miss \
    --shard-count 2 \
    --projection-warm-budget-seconds 8 \
    --projection-cold-budget-seconds 9 \
    --base-port 51541 \
    --started-ns "${started_ns}"

"${python}" "${repo_root}/tools/verify_ssbm_falcon_ledge_collision.py" \
    "${output}" \
    "${coverage_manifest}" \
    "${profile}" \
    "${import_manifest}"

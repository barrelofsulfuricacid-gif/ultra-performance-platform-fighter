#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 /path/to/GALE01.iso [output.json]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iso="$1"
output="${2:-${repo_root}/build/oracle/falcon-ledge-hurt-poses.json}"
manifest="${repo_root}/tools/ssbm_falcon_ledge_options_coverage.json"
toolchain_root="${repo_root}/build/oracle-toolchain"
user_home="$(getent passwd "$(id -u)" | cut -d: -f6)"
native_python="${user_home}/.cache/pf-ssbm-exiai-python-0.47.2/bin/python"
python="${toolchain_root}/exiai-python/bin/python"
if [[ -x "${native_python}" ]]; then
    python="${native_python}"
fi
dolphin="${toolchain_root}/exiai-checkpoint/Binaries/dolphin-emu"
release_artifact="${toolchain_root}/exiai-0.2.0/Slippi_Online-x86_64-ExiAI.AppImage"
started_ns="$(date +%s%N)"

"${python}" "${repo_root}/tools/capture_ssbm_checkpoint_shards.py" \
    --dolphin "${dolphin}" \
    --oracle-release-artifact "${release_artifact}" \
    --iso "${iso}" \
    --manifest "${manifest}" \
    --output "${output}" \
    --memory-probe hurtbox \
    --disable-fast-forward \
    --case quick_climb \
    --case slow_climb \
    --case quick_roll \
    --case slow_roll \
    --case quick_attack \
    --case slow_attack \
    --case quick_jump \
    --case slow_jump \
    --shard-count 4 \
    --projection-warm-budget-seconds 15 \
    --projection-cold-budget-seconds 16 \
    --base-port 51441 \
    --started-ns "${started_ns}"

printf 'ssbm-falcon-ledge-hurt-pose-capture=pass output=%s\n' "${output}"

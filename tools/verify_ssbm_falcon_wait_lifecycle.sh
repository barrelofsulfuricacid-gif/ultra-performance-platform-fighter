#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 || $# -gt 4 ]]; then
    echo "usage: $0 /path/to/GALE01.iso /path/to/melee-decomp /path/to/extracted-dat [output-directory]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iso="$1"
decomp_root="$2"
extract_root="$3"
output_root="${4:-${repo_root}/build/oracle}"
manifest="${repo_root}/tools/ssbm_falcon_wait_lifecycle_coverage.json"
pose_manifest="${repo_root}/tools/ssbm_falcon_ground_loop_hurt_import.json"
toolchain_root="${repo_root}/build/oracle-toolchain"
python="${toolchain_root}/exiai-python/bin/python"
dolphin="${toolchain_root}/exiai-checkpoint/Binaries/dolphin-emu"
release_artifact="${toolchain_root}/exiai-0.2.0/Slippi_Online-x86_64-ExiAI.AppImage"
control="${output_root}/falcon-wait-lifecycle-control.json"
repeat="${output_root}/falcon-wait-lifecycle-repeat.json"

for required in \
    "${iso}" \
    "${python}" \
    "${dolphin}" \
    "${release_artifact}" \
    "${extract_root}/PlCa.dat" \
    "${extract_root}/PlCaAJ.dat" \
    "${extract_root}/PlCo.dat" \
    "${extract_root}/PlCaGy.dat" \
    "${decomp_root}/src/melee/ft/ftwaitanim.c" \
    "${decomp_root}/src/sysdolphin/baselib/random.c"; do
    if [[ ! -f "${required}" ]]; then
        echo "missing required wait-lifecycle input: ${required}" >&2
        exit 1
    fi
done
mkdir -p -- "${output_root}"

capture() {
    local output="$1"
    "${python}" "${repo_root}/tools/capture_ssbm_movement.py" \
        --dolphin "${dolphin}" \
        --oracle-release-artifact "${release_artifact}" \
        --iso "${iso}" \
        --output "${output}" \
        --common-hurt-geometry-only \
        --memory-probe-hitbox \
        --oracle-exiai \
        --oracle-checkpoint-pack \
        --oracle-checkpoint-no-batch-inputs \
        --oracle-coverage-manifest "${manifest}"
}

capture "${control}"
capture "${repeat}"

"${python}" "${repo_root}/tools/verify_ssbm_wait_lifecycle_source.py" \
    --fresh-captures \
    "${manifest}" \
    "${pose_manifest}" \
    "${extract_root}/PlCa.dat" \
    "${extract_root}/PlCaAJ.dat" \
    "${decomp_root}/src/melee/ft/ftwaitanim.c" \
    "${decomp_root}/src/sysdolphin/baselib/random.c" \
    "${control}" \
    "${repeat}"

"${python}" "${repo_root}/tools/verify_ssbm_hsd_action_hurt_source.py" \
    --fresh-captures \
    --qualification-key wait_lifecycle_qualification \
    "${pose_manifest}" \
    "${extract_root}/PlCa.dat" \
    "${extract_root}/PlCaAJ.dat" \
    "${extract_root}/PlCo.dat" \
    "${extract_root}/PlCaGy.dat" \
    "${control}"

"${python}" "${repo_root}/tools/generate_ssbm_dynamic_hurt_pose_include.py" \
    --check \
    "${pose_manifest}" \
    "${extract_root}/PlCa.dat" \
    "${extract_root}/PlCaAJ.dat" \
    "${extract_root}/PlCo.dat" \
    "${extract_root}/PlCaGy.dat" \
    "${repo_root}/generated/data/m4_ssbm_falcon_ground_loop_hsd.inc"

printf 'ssbm-falcon-wait-lifecycle=pass captures=2 rows=880 output=%s\n' \
    "${output_root}"

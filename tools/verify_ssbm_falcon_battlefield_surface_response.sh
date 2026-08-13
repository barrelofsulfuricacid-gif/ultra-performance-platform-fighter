#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 4 ]]; then
    echo "usage: $0 /path/to/GALE01.iso /path/to/melee-decomp [output.json] [m4_combat_test]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iso="$1"
decomp_root="$2"
output="${3:-${repo_root}/build/oracle/falcon-battlefield-surface-response-qualified.json}"
sim_executable="${4:-}"
coverage_manifest="${repo_root}/tools/ssbm_falcon_battlefield_surface_response_coverage.json"
stage_source="${repo_root}/tools/data/ssbm_ntsc102_battlefield_collision.json"
toolchain_root="${repo_root}/build/oracle-toolchain"
python="${toolchain_root}/exiai-python/bin/python"
dolphin="${toolchain_root}/exiai-checkpoint/Binaries/dolphin-emu"
release_artifact="${toolchain_root}/exiai-0.2.0/Slippi_Online-x86_64-ExiAI.AppImage"
timing_log="$(mktemp)"
sim_output="$(mktemp)"
cleanup() {
    rm -f -- "${timing_log}" "${sim_output}"
}
trap cleanup EXIT

"${python}" "${repo_root}/tools/capture_ssbm_movement.py" \
    --dolphin "${dolphin}" \
    --oracle-release-artifact "${release_artifact}" \
    --iso "${iso}" \
    --output "${output}" \
    --damage-hit-only \
    --memory-probe-surface \
    --oracle-exiai \
    --oracle-checkpoint-pack \
    --oracle-coverage-manifest "${coverage_manifest}" \
    2> >(tee "${timing_log}" >&2)

warm_seconds="$(sed -n 's/.*warm_seconds=\([0-9.]*\).*/\1/p' "${timing_log}" | tail -n 1)"
warm_budget_seconds="$(
    "${python}" -c \
        'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["checkpoint_pack"]["warm_budget_seconds"])' \
        "${coverage_manifest}"
)"
if [[ -z "${warm_seconds}" ]]; then
    echo "Battlefield surface-response pack did not report a warm duration" >&2
    exit 1
fi
awk -v seconds="${warm_seconds}" -v budget="${warm_budget_seconds}" \
    'BEGIN { exit !(seconds <= budget) }' || {
    echo "Battlefield surface-response pack exceeded ${warm_budget_seconds}-second warm budget: ${warm_seconds}" >&2
    exit 1
}

sim_arguments=()
if [[ -n "${sim_executable}" ]]; then
    if [[ ! -f "${sim_executable}" ]]; then
        echo "missing simulation oracle executable: ${sim_executable}" >&2
        exit 1
    fi
    "${sim_executable}" \
        --ssbm-oracle falcon-common-battlefield-surface-response > "${sim_output}"
    sim_arguments=(--sim-output "${sim_output}")
fi

common_root="${decomp_root}/src/melee/ft/chara/ftCommon"
"${python}" "${repo_root}/tools/verify_ssbm_falcon_battlefield_surface_response.py" \
    "${output}" \
    "${coverage_manifest}" \
    "${stage_source}" \
    "${common_root}/ftCo_PassiveWall.c" \
    "${common_root}/ftCo_PassiveCeil.c" \
    "${common_root}/ftCo_FlyReflect.c" \
    "${decomp_root}/src/melee/ft/fighter.c" \
    "${decomp_root}/src/melee/ft/ftcommon.c" \
    "${decomp_root}/src/melee/mp/mpcoll.c" \
    "${decomp_root}/src/melee/mp/mplib.c" \
    "${sim_arguments[@]}"

printf 'ssbm-falcon-battlefield-surface-response-pack=pass warm_seconds=%s output=%s\n' \
    "${warm_seconds}" "${output}"

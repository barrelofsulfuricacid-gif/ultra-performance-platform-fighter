#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 4 ]]; then
    echo "usage: $0 /path/to/GALE01.iso /path/to/melee-decomp [output.json] [combat_test]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iso="$1"
decomp_root="$2"
output="${3:-${repo_root}/build/oracle/falcon-damage-response-checkpoint.json}"
sim_executable="${4:-}"
coverage_manifest="${repo_root}/tools/ssbm_falcon_damage_response_coverage.json"
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

for required in "${iso}" "${python}" "${dolphin}" "${release_artifact}"; do
    if [[ ! -f "${required}" ]]; then
        echo "missing required oracle input: ${required}" >&2
        exit 1
    fi
done

"${python}" "${repo_root}/tools/capture_ssbm_movement.py" \
    --dolphin "${dolphin}" \
    --oracle-release-artifact "${release_artifact}" \
    --iso "${iso}" \
    --output "${output}" \
    --damage-hit-only \
    --memory-probe-damage \
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
    echo "damage-response checkpoint pack did not report a warm duration" >&2
    exit 1
fi
awk -v seconds="${warm_seconds}" -v budget="${warm_budget_seconds}" \
    'BEGIN { exit !(seconds <= budget) }' || {
    echo "damage-response pack exceeded ${warm_budget_seconds}-second warm budget: ${warm_seconds}" >&2
    exit 1
}

sim_arguments=()
if [[ -n "${sim_executable}" ]]; then
    if [[ ! -f "${sim_executable}" ]]; then
        echo "missing simulation oracle executable: ${sim_executable}" >&2
        exit 1
    fi
    "${sim_executable}" \
        --ssbm-oracle falcon-common-damage-response > "${sim_output}"
    sim_arguments=(--sim-output "${sim_output}")
fi

"${python}" "${repo_root}/tools/verify_ssbm_falcon_damage_response.py" \
    "${output}" \
    "${coverage_manifest}" \
    "${decomp_root}/src/melee/ft/chara/ftCommon/ftCo_Damage.c" \
    "${decomp_root}/src/melee/ft/fighter.c" \
    "${sim_arguments[@]}"

printf 'ssbm-falcon-damage-response-pack=pass warm_seconds=%s output=%s\n' \
    "${warm_seconds}" "${output}"

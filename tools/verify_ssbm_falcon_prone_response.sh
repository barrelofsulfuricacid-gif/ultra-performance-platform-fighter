#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 4 ]]; then
    echo "usage: $0 /path/to/GALE01.iso /path/to/melee-decomp [output.json] [m4_combat_test]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iso="$1"
decomp_root="$2"
output="${3:-${repo_root}/build/oracle/falcon-prone-response-checkpoint.json}"
sim_executable="${4:-}"
coverage_manifest="${repo_root}/tools/ssbm_falcon_prone_response_coverage.json"
toolchain_root="${repo_root}/build/oracle-toolchain"
python="${toolchain_root}/exiai-python/bin/python"
dolphin="${toolchain_root}/exiai-checkpoint/Binaries/dolphin-emu"
release_artifact="${toolchain_root}/exiai-0.2.0/Slippi_Online-x86_64-ExiAI.AppImage"
sim_output="$(mktemp)"
shard_outputs=()
shard_dolphins=()
cleanup() {
    rm -f -- "${sim_output}"
    for path in "${shard_outputs[@]}"; do
        [[ -z "${path}" ]] || rm -f -- "${path}"
    done
    for path in "${shard_dolphins[@]}"; do
        [[ -z "${path}" ]] || rm -f -- "${path}"
    done
}
trap cleanup EXIT

run_shard() {
    local port="$1"
    local shard_output="$2"
    local shard_dolphin="$3"
    shift 3
    local case_arguments=()
    local case_id
    for case_id in "$@"; do
        case_arguments+=(--oracle-case "${case_id}")
    done
    "${python}" "${repo_root}/tools/capture_ssbm_movement.py" \
        --dolphin "${shard_dolphin}" \
        --oracle-release-artifact "${release_artifact}" \
        --iso "${iso}" \
        --output "${shard_output}" \
        --slippi-port "${port}" \
        --damage-hit-only \
        --memory-probe-surface \
        --oracle-exiai \
        --oracle-checkpoint-pack \
        --oracle-coverage-manifest "${coverage_manifest}" \
        "${case_arguments[@]}"
}

for _ in 0 1 2 3; do
    shard_outputs+=("$(mktemp --suffix=.json)")
done
for index in 0 1 2 3; do
    shard_dolphin="${toolchain_root}/exiai-checkpoint/Binaries/dolphin-o${index}"
    ln -- "${dolphin}" "${shard_dolphin}"
    shard_dolphins+=("${shard_dolphin}")
done

started_ns="$(date +%s%N)"
pids=()
run_shard 51441 "${shard_outputs[0]}" "${shard_dolphins[0]}" \
    timeout up_buffered_a_getup_attack c_roll_below_threshold &
pids+=("$!")
run_shard 51442 "${shard_outputs[1]}" "${shard_dolphins[1]}" \
    up_timeout buffered_a_getup_attack buffered_b_getup_attack &
pids+=("$!")
run_shard 51443 "${shard_outputs[2]}" "${shard_dolphins[2]}" \
    up_c_roll_forward up_main_roll_backward c_roll_forward c_up_getup_attack &
pids+=("$!")
run_shard 51444 "${shard_outputs[3]}" "${shard_dolphins[3]}" \
    main_roll_backward up_neutral_getup shield_neutral_getup \
    attack_over_roll_priority &
pids+=("$!")

capture_status=0
for pid in "${pids[@]}"; do
    if ! wait "${pid}"; then
        capture_status=1
    fi
done
if [[ "${capture_status}" -ne 0 ]]; then
    echo "parallel prone-response capture failed" >&2
    exit 1
fi
finished_ns="$(date +%s%N)"
warm_seconds="$(
    awk -v start="${started_ns}" -v finish="${finished_ns}" \
        'BEGIN { printf "%.6f", (finish - start) / 1000000000.0 }'
)"
"${python}" "${repo_root}/tools/merge_ssbm_checkpoint_captures.py" \
    --manifest "${coverage_manifest}" \
    --output "${output}" \
    "${shard_outputs[@]}"

warm_budget_seconds="$(
    "${python}" -c \
        'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["checkpoint_pack"]["warm_budget_seconds"])' \
        "${coverage_manifest}"
)"
awk -v seconds="${warm_seconds}" -v budget="${warm_budget_seconds}" \
    'BEGIN { exit !(seconds <= budget) }' || {
    echo "prone-response pack exceeded ${warm_budget_seconds}-second warm budget: ${warm_seconds}" >&2
    exit 1
}

sim_arguments=()
if [[ -n "${sim_executable}" ]]; then
    if [[ ! -f "${sim_executable}" ]]; then
        echo "missing simulation oracle executable: ${sim_executable}" >&2
        exit 1
    fi
    "${sim_executable}" --ssbm-oracle falcon-common-prone-response > "${sim_output}"
    sim_arguments=(--sim-output "${sim_output}")
fi

common_root="${decomp_root}/src/melee/ft/chara/ftCommon"
"${python}" "${repo_root}/tools/verify_ssbm_falcon_prone_response.py" \
    "${output}" \
    "${coverage_manifest}" \
    "${common_root}/ftCo_DownBound.c" \
    "${common_root}/ftCo_Down.c" \
    "${common_root}/ftCo_DownAttack.c" \
    "${common_root}/ftCo_DownStand.c" \
    "${decomp_root}/src/melee/ft/ft_0DF1.c" \
    "${decomp_root}/src/melee/ft/fighter.c" \
    "${decomp_root}/src/melee/ft/ftcommon.c" \
    "${sim_arguments[@]}"

printf 'ssbm-falcon-prone-response-pack=pass warm_seconds=%s output=%s\n' \
    "${warm_seconds}" "${output}"

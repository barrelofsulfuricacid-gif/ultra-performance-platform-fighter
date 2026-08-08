#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "usage: $0 /path/to/GALE01.iso /path/to/melee-decomp [output.json]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iso="$1"
decomp_root="$2"
output="${3:-${repo_root}/build/oracle/falcon-common-hurt-checkpoint-pack.json}"
toolchain_root="${repo_root}/build/oracle-toolchain"
python="${toolchain_root}/exiai-python/bin/python"
dolphin="${toolchain_root}/exiai-checkpoint/Binaries/dolphin-emu"
release_artifact="${toolchain_root}/exiai-0.2.0/Slippi_Online-x86_64-ExiAI.AppImage"
timing_log="$(mktemp)"
cleanup() {
    rm -f -- "${timing_log}"
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
    --common-hurt-geometry-only \
    --memory-probe-hitbox \
    --oracle-exiai \
    --oracle-checkpoint-pack \
    2> >(tee "${timing_log}" >&2)

warm_seconds="$(sed -n 's/.*warm_seconds=\([0-9.]*\).*/\1/p' "${timing_log}" | tail -n 1)"
if [[ -z "${warm_seconds}" ]]; then
    echo "checkpoint pack did not report a warm duration" >&2
    exit 1
fi
awk -v seconds="${warm_seconds}" 'BEGIN { exit !(seconds <= 10.0) }' || {
    echo "checkpoint pack exceeded 10-second warm budget: ${warm_seconds}" >&2
    exit 1
}

"${python}" "${repo_root}/tools/verify_ssbm_falcon_common_hurt.py" \
    "${output}" \
    "${decomp_root}/src/melee/lb/lbcollision.c" \
    "${decomp_root}/src/melee/ft/chara/ftCommon/ftCo_Dash.c" \
    "${decomp_root}/src/melee/ft/chara/ftCommon/ftCo_Squat.c" \
    "${decomp_root}/src/melee/ft/chara/ftCommon/ftCo_KneeBend.c" \
    "${decomp_root}/src/melee/ft/chara/ftCommon/ftCo_Escape.c" \
    "${decomp_root}/src/melee/ft/chara/ftCommon/ftCo_EscapeAir.c" \
    "${decomp_root}/src/melee/ft/chara/ftCommon/ftCo_FallSpecial.c" \
    "${decomp_root}/src/melee/ft/chara/ftCommon/ftCo_Landing.c" \
    --checkpoint-pack

printf 'ssbm-falcon-checkpoint-pack=pass warm_seconds=%s output=%s\n' \
    "${warm_seconds}" "${output}"

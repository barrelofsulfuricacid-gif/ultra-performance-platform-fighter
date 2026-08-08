#!/usr/bin/env bash
set -euo pipefail

readonly EXIAI_REPOSITORY="https://github.com/vladfi1/slippi-Ishiiruka.git"
readonly EXIAI_REVISION="bf1aec4de4856eab412996137287f447daa8ae17"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
toolchain_root="${1:-${repo_root}/build/oracle-toolchain}"
source_root="${toolchain_root}/slippi-exiai-${EXIAI_REVISION}"
build_root="${toolchain_root}/exiai-checkpoint-build"
install_root="${toolchain_root}/exiai-checkpoint/Binaries"
patch_file="${repo_root}/tools/ssbm_exiai_checkpoint.patch"

"${repo_root}/tools/bootstrap_ssbm_exiai_oracle.sh" \
    "${toolchain_root}/exiai-0.2.0" \
    "${toolchain_root}/exiai-python"

if [[ ! -d "${source_root}/.git" ]]; then
    git clone --filter=blob:none --no-checkout \
        "${EXIAI_REPOSITORY}" "${source_root}"
    git -C "${source_root}" checkout --detach "${EXIAI_REVISION}"
fi

if [[ "$(git -C "${source_root}" rev-parse HEAD)" != "${EXIAI_REVISION}" ]]; then
    echo "unexpected ExiAI source revision" >&2
    exit 1
fi

if git -C "${source_root}" apply --check "${patch_file}"; then
    git -C "${source_root}" apply "${patch_file}"
elif ! git -C "${source_root}" apply --check --reverse "${patch_file}"; then
    echo "ExiAI checkpoint source has unrelated modifications" >&2
    exit 1
fi

cmake -S "${source_root}" -B "${build_root}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_HEADLESS=true \
    -DENABLE_ALSA=false \
    -DENABLE_PULSEAUDIO=false \
    -DENABLE_EVDEV=false
cmake --build "${build_root}" --parallel --target dolphin-nogui

mkdir -p "${install_root}"
cp "${build_root}/Binaries/dolphin-emu-nogui" \
    "${install_root}/dolphin-emu"
mkdir -p "${install_root}/Sys"
cp -a "${build_root}/Binaries/Sys/." "${install_root}/Sys/"
touch "${install_root}/portable.txt"

version="$({ "${install_root}/dolphin-emu" --version 2>&1 || true; } | head -n 1)"
if [[ "${version}" != "Faster Melee - Slippi (3.5.1) - ExiAI" ]]; then
    echo "unexpected checkpoint Dolphin version: ${version}" >&2
    exit 1
fi

printf 'ssbm-checkpoint-oracle-bootstrap=pass\nlauncher=%s\npython=%s\n' \
    "${install_root}/dolphin-emu" \
    "${toolchain_root}/exiai-python/bin/python"

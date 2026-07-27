#!/bin/sh
set -eu

PF_REPOSITORY_ROOT=$(git rev-parse --show-toplevel)
export PF_REPOSITORY_ROOT

. "$PF_REPOSITORY_ROOT/tools/toolchain_common.sh"

pf_platform_key
pf_find_host_tools

preset=${1:-debug}

case "$preset" in
    debug|sanitizer|release|profile|benchmark|headless)
        pf_validate_compiler
        ;;
    web)
        pf_emsdk_root="$PF_TOOLCHAINS_DIR/web/emsdk-db04e88298d9916fc51fcd3743045ca3eb695127"
        [ -f "$pf_emsdk_root/emsdk_env.sh" ] ||
            pf_fail "pinned Emscripten SDK is missing; run ./tools/bootstrap.sh --web"
        # The pinned environment script exports EMSDK, EM_CONFIG, Node, and
        # Emscripten's Clang/Binaryen paths for this process only.
        pf_original_directory=$PWD
        cd "$pf_emsdk_root"
        . ./emsdk_env.sh >/dev/null
        cd "$pf_original_directory"
        EMSDK="$pf_emsdk_root"
        export EMSDK
        ;;
    *)
        pf_fail "unknown workflow '$preset'; expected debug, sanitizer, release, profile, benchmark, headless, or web"
        ;;
esac

exec "$PF_CMAKE" --workflow --preset "$preset"

#!/bin/sh
set -eu

PF_REPOSITORY_ROOT=$(git rev-parse --show-toplevel)
export PF_REPOSITORY_ROOT

. "$PF_REPOSITORY_ROOT/tools/toolchain_common.sh"

pf_platform_key
pf_find_host_tools

preset=${1:-debug}
pf_sdl_root="$PF_TOOLCHAINS_DIR/dependencies/SDL3-3.4.12"
pf_sqlite_root="$PF_TOOLCHAINS_DIR/dependencies/sqlite-amalgamation-3530400"
pf_tracy_root="$PF_TOOLCHAINS_DIR/dependencies/tracy-0.13.1"

case "$preset" in
    debug|sanitizer|release|profile|benchmark)
        [ -f "$pf_sqlite_root/sqlite3.c" ] &&
            [ -f "$pf_sqlite_root/sqlite3.h" ] ||
            pf_fail "pinned SQLite source is missing; run ./tools/bootstrap.sh"
        PF_SQLITE_SOURCE_DIR=$pf_sqlite_root
        export PF_SQLITE_SOURCE_DIR
        ;;
esac

if [ "$preset" = profile ]; then
    [ -f "$pf_tracy_root/public/tracy/TracyC.h" ] &&
        [ -f "$pf_tracy_root/public/TracyClient.cpp" ] ||
        pf_fail "pinned Tracy source is missing; run ./tools/bootstrap.sh"
    PF_TRACY_SOURCE_DIR=$pf_tracy_root
    export PF_TRACY_SOURCE_DIR
fi

case "$preset" in
    debug|sanitizer|release|profile)
        pf_validate_compiler
        [ -f "$pf_sdl_root/include/SDL3/SDL_version.h" ] ||
            pf_fail "pinned SDL3 source is missing; run ./tools/bootstrap.sh"
        PF_SDL_SOURCE_DIR=$pf_sdl_root
        export PF_SDL_SOURCE_DIR
        ;;
    benchmark|headless|verifier)
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
        pf_fail "unknown workflow '$preset'; expected debug, sanitizer, release, profile, benchmark, headless, verifier, or web"
        ;;
esac

exec "$PF_CMAKE" --workflow --preset "$preset"

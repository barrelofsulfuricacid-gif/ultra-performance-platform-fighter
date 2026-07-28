#!/bin/sh
set -eu

PF_REPOSITORY_ROOT=$(git rev-parse --show-toplevel)
export PF_REPOSITORY_ROOT

. "$PF_REPOSITORY_ROOT/tools/toolchain_common.sh"

pf_install_web=0
pf_run_smoke=1
pf_verify_only=0
PF_TOOLCHAINS_DIR=${PF_TOOLCHAINS_DIR:-"$PF_REPOSITORY_ROOT/.toolchains"}

usage()
{
    echo "usage: ./tools/bootstrap.sh [--web] [--verify-only] [--no-smoke] [--prefix PATH]"
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --web)
            pf_install_web=1
            ;;
        --verify-only)
            pf_verify_only=1
            ;;
        --no-smoke)
            pf_run_smoke=0
            ;;
        --prefix)
            shift
            [ "$#" -gt 0 ] || pf_fail "--prefix requires a path"
            PF_TOOLCHAINS_DIR=$1
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            usage >&2
            pf_fail "unknown bootstrap option: $1"
            ;;
    esac
    shift
done

export PF_TOOLCHAINS_DIR
pf_lock="$PF_REPOSITORY_ROOT/dependencies/toolchains.lock.tsv"
[ -f "$pf_lock" ] || pf_fail "missing archive lock: $pf_lock"

pf_platform_key

pf_record()
{
    awk -F '	' -v component="$1" -v platform="$2" '
        NR > 1 && $1 == component && $3 == platform {
            print
            found = 1
            exit
        }
        END {
            if (!found) {
                exit 1
            }
        }
    ' "$pf_lock"
}

pf_sha256()
{
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{ print $1 }'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{ print $1 }'
    else
        pf_fail "sha256sum or shasum is required"
    fi
}

pf_download_record()
{
    pf_download_component=$1
    pf_download_platform=$2
    pf_download_directory=$3
    pf_download_override=${4:-}
    pf_download_record=$(pf_record \
        "$pf_download_component" "$pf_download_platform") ||
        pf_fail "no lock record for $pf_download_component/$pf_download_platform"

    pf_old_ifs=$IFS
    IFS='	'
    set -- $pf_download_record
    IFS=$pf_old_ifs

    pf_archive=$4
    pf_expected_sha=$5
    pf_expected_size=$6
    pf_url=$7
    if [ -n "$pf_download_override" ]; then
        pf_destination="$pf_download_directory/$pf_download_override"
    else
        pf_destination="$pf_download_directory/$pf_archive"
    fi

    mkdir -p "$pf_download_directory"
    if [ -f "$pf_destination" ]; then
        pf_actual_sha=$(pf_sha256 "$pf_destination")
        pf_actual_size=$(wc -c <"$pf_destination" | tr -d ' ')
        if [ "$pf_actual_sha" = "$pf_expected_sha" ] &&
           [ "$pf_actual_size" = "$pf_expected_size" ]; then
            echo "archive=verified path=$pf_destination"
            PF_DOWNLOADED_ARCHIVE=$pf_destination
            export PF_DOWNLOADED_ARCHIVE
            return
        fi
    fi

    [ "$pf_verify_only" -eq 0 ] ||
        pf_fail "locked archive is missing or invalid: $pf_destination"

    pf_partial="$pf_destination.partial.$$"
    if command -v curl >/dev/null 2>&1; then
        curl -fL --retry 3 --connect-timeout 30 \
            -o "$pf_partial" "$pf_url"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$pf_partial" "$pf_url"
    else
        pf_fail "curl or wget is required to download locked tools"
    fi

    pf_actual_sha=$(pf_sha256 "$pf_partial")
    pf_actual_size=$(wc -c <"$pf_partial" | tr -d ' ')
    [ "$pf_actual_sha" = "$pf_expected_sha" ] ||
        pf_fail "SHA-256 mismatch for $pf_url"
    [ "$pf_actual_size" = "$pf_expected_size" ] ||
        pf_fail "byte-length mismatch for $pf_url"

    mv "$pf_partial" "$pf_destination"
    echo "archive=downloaded-and-verified path=$pf_destination"
    PF_DOWNLOADED_ARCHIVE=$pf_destination
    export PF_DOWNLOADED_ARCHIVE
}

pf_host_root="$PF_TOOLCHAINS_DIR/host/$PF_PLATFORM_KEY"
pf_download_root="$PF_TOOLCHAINS_DIR/downloads"
pf_temp_root="$PF_TOOLCHAINS_DIR/tmp"
mkdir -p "$pf_host_root" "$pf_download_root" "$pf_temp_root"

pf_cmake_root="$pf_host_root/cmake-4.4.0"
if [ ! -d "$pf_cmake_root" ]; then
    pf_download_record cmake "$PF_PLATFORM_KEY" "$pf_download_root"
    pf_cmake_temp=$(mktemp -d "$pf_temp_root/cmake.XXXXXX")
    tar -xzf "$PF_DOWNLOADED_ARCHIVE" -C "$pf_cmake_temp"
    mv "$pf_cmake_temp" "$pf_cmake_root"
fi

pf_ninja_root="$pf_host_root/ninja-1.13.2"
if [ ! -d "$pf_ninja_root" ]; then
    command -v unzip >/dev/null 2>&1 ||
        pf_fail "unzip is required to install pinned Ninja"
    pf_download_record ninja "$PF_PLATFORM_KEY" "$pf_download_root"
    pf_ninja_temp=$(mktemp -d "$pf_temp_root/ninja.XXXXXX")
    unzip -q "$PF_DOWNLOADED_ARCHIVE" -d "$pf_ninja_temp"
    chmod +x "$pf_ninja_temp/ninja"
    mv "$pf_ninja_temp" "$pf_ninja_root"
fi

pf_find_host_tools
pf_validate_compiler

pf_sdl_root="$PF_TOOLCHAINS_DIR/dependencies/SDL3-3.4.12"
if [ ! -d "$pf_sdl_root" ]; then
    pf_download_record sdl-source all "$pf_download_root"
    pf_sdl_temp=$(mktemp -d "$pf_temp_root/sdl.XXXXXX")
    tar -xzf "$PF_DOWNLOADED_ARCHIVE" \
        --strip-components=1 \
        -C "$pf_sdl_temp"
    mkdir -p "$(dirname "$pf_sdl_root")"
    mv "$pf_sdl_temp" "$pf_sdl_root"
fi

pf_sdl_version_header="$pf_sdl_root/include/SDL3/SDL_version.h"
[ -f "$pf_sdl_version_header" ] ||
    pf_fail "locked SDL3 source is incomplete: $pf_sdl_root"
grep -Eq '^#define SDL_MAJOR_VERSION[[:space:]]+3$' \
    "$pf_sdl_version_header" &&
    grep -Eq '^#define SDL_MINOR_VERSION[[:space:]]+4$' \
        "$pf_sdl_version_header" &&
    grep -Eq '^#define SDL_MICRO_VERSION[[:space:]]+12$' \
        "$pf_sdl_version_header" ||
    pf_fail "locked SDL3 source is not version 3.4.12"
PF_SDL_SOURCE_DIR=$pf_sdl_root
export PF_SDL_SOURCE_DIR
echo "sdl-source=3.4.12 path=$PF_SDL_SOURCE_DIR"

pf_sqlite_root="$PF_TOOLCHAINS_DIR/dependencies/sqlite-amalgamation-3530400"
if [ ! -d "$pf_sqlite_root" ]; then
    command -v unzip >/dev/null 2>&1 ||
        pf_fail "unzip is required to install pinned SQLite"
    pf_download_record sqlite-source all "$pf_download_root"
    pf_sqlite_temp=$(mktemp -d "$pf_temp_root/sqlite.XXXXXX")
    unzip -q "$PF_DOWNLOADED_ARCHIVE" -d "$pf_sqlite_temp"
    mkdir -p "$(dirname "$pf_sqlite_root")"
    mv \
        "$pf_sqlite_temp/sqlite-amalgamation-3530400" \
        "$pf_sqlite_root"
fi

pf_sqlite_header="$pf_sqlite_root/sqlite3.h"
pf_sqlite_source="$pf_sqlite_root/sqlite3.c"
[ -f "$pf_sqlite_header" ] && [ -f "$pf_sqlite_source" ] ||
    pf_fail "locked SQLite source is incomplete: $pf_sqlite_root"
grep -Fq '#define SQLITE_VERSION        "3.53.4"' \
    "$pf_sqlite_header" ||
    pf_fail "locked SQLite source is not version 3.53.4"
grep -Fq \
    'bf7c7f30031888f4e796e429ab3978879485813aaca6f641c7b33e4e09459bcc' \
    "$pf_sqlite_source" ||
    pf_fail "locked SQLite source ID is incorrect"
PF_SQLITE_SOURCE_DIR=$pf_sqlite_root
export PF_SQLITE_SOURCE_DIR
echo "sqlite-source=3.53.4 path=$PF_SQLITE_SOURCE_DIR"

pf_tracy_root="$PF_TOOLCHAINS_DIR/dependencies/tracy-0.13.1"
if [ ! -d "$pf_tracy_root" ]; then
    pf_download_record tracy-source all "$pf_download_root"
    pf_tracy_temp=$(mktemp -d "$pf_temp_root/tracy.XXXXXX")
    tar -xzf "$PF_DOWNLOADED_ARCHIVE" \
        --strip-components=1 \
        -C "$pf_tracy_temp"
    mkdir -p "$(dirname "$pf_tracy_root")"
    mv "$pf_tracy_temp" "$pf_tracy_root"
fi

pf_tracy_header="$pf_tracy_root/public/tracy/TracyC.h"
pf_tracy_client="$pf_tracy_root/public/TracyClient.cpp"
[ -f "$pf_tracy_header" ] && [ -f "$pf_tracy_client" ] ||
    pf_fail "locked Tracy source is incomplete: $pf_tracy_root"
grep -Fq 'enum { Major = 0 };' \
    "$pf_tracy_root/public/common/TracyVersion.hpp" &&
    grep -Fq 'enum { Minor = 13 };' \
        "$pf_tracy_root/public/common/TracyVersion.hpp" &&
    grep -Fq 'enum { Patch = 1 };' \
        "$pf_tracy_root/public/common/TracyVersion.hpp" ||
    pf_fail "locked Tracy source is not version 0.13.1"
PF_TRACY_SOURCE_DIR=$pf_tracy_root
export PF_TRACY_SOURCE_DIR
echo "tracy-source=0.13.1 path=$PF_TRACY_SOURCE_DIR"

pf_install_tar_dependency()
{
    pf_dependency_component=$1
    pf_dependency_directory=$2
    if [ ! -d "$pf_dependency_directory" ]; then
        pf_download_record \
            "$pf_dependency_component" \
            all \
            "$pf_download_root"
        pf_dependency_temp=$(
            mktemp -d "$pf_temp_root/$pf_dependency_component.XXXXXX"
        )
        tar -xzf "$PF_DOWNLOADED_ARCHIVE" \
            --strip-components=1 \
            -C "$pf_dependency_temp"
        mkdir -p "$(dirname "$pf_dependency_directory")"
        mv "$pf_dependency_temp" "$pf_dependency_directory"
    fi
}

pf_tracy_capstone_root=\
"$PF_TOOLCHAINS_DIR/dependencies/capstone-6.0.0-Alpha5"
pf_tracy_ppqsort_root=\
"$PF_TOOLCHAINS_DIR/dependencies/ppqsort-1.0.6"
pf_tracy_zstd_root="$PF_TOOLCHAINS_DIR/dependencies/zstd-1.5.7"
pf_install_tar_dependency \
    tracy-capstone-source \
    "$pf_tracy_capstone_root"
pf_install_tar_dependency \
    tracy-ppqsort-source \
    "$pf_tracy_ppqsort_root"
pf_install_tar_dependency \
    tracy-zstd-source \
    "$pf_tracy_zstd_root"
[ -f "$pf_tracy_capstone_root/include/capstone/capstone.h" ] ||
    pf_fail "locked Tracy Capstone source is incomplete"
[ -f "$pf_tracy_ppqsort_root/include/ppqsort.h" ] ||
    pf_fail "locked Tracy PPQSort source is incomplete"
[ -f "$pf_tracy_zstd_root/lib/zstd.h" ] ||
    pf_fail "locked Tracy Zstd source is incomplete"
PF_TRACY_CAPSTONE_SOURCE_DIR=$pf_tracy_capstone_root
PF_TRACY_PPQSORT_SOURCE_DIR=$pf_tracy_ppqsort_root
PF_TRACY_ZSTD_SOURCE_DIR=$pf_tracy_zstd_root
export \
    PF_TRACY_CAPSTONE_SOURCE_DIR \
    PF_TRACY_PPQSORT_SOURCE_DIR \
    PF_TRACY_ZSTD_SOURCE_DIR
echo "tracy-capture-dependencies=locked"

if [ "$pf_install_web" -eq 1 ]; then
    command -v python3 >/dev/null 2>&1 ||
        pf_fail "Python 3 is required for the pinned Emscripten SDK"

    case "$PF_PLATFORM_KEY" in
        windows-*)
            pf_fail "use tools/bootstrap.ps1 for Windows"
            ;;
    esac

    pf_emsdk_root="$PF_TOOLCHAINS_DIR/web/emsdk-db04e88298d9916fc51fcd3743045ca3eb695127"
    if [ ! -d "$pf_emsdk_root" ]; then
        pf_download_record emsdk all "$pf_download_root"
        pf_emsdk_temp=$(mktemp -d "$pf_temp_root/emsdk.XXXXXX")
        tar -xzf "$PF_DOWNLOADED_ARCHIVE" \
            --strip-components=1 \
            -C "$pf_emsdk_temp"
        mkdir -p "$(dirname "$pf_emsdk_root")"
        mv "$pf_emsdk_temp" "$pf_emsdk_root"
    fi

    pf_emsdk_downloads="$pf_emsdk_root/downloads"
    pf_release_revision=9074aa513b501925adb1361e208932ad32a29a5f

    pf_sdk_record=$(pf_record emscripten-sdk "$PF_PLATFORM_KEY") ||
        pf_fail "Emscripten 6.0.3 is not locked for $PF_PLATFORM_KEY"
    pf_old_ifs=$IFS
    IFS='	'
    set -- $pf_sdk_record
    IFS=$pf_old_ifs
    pf_sdk_archive=$4
    pf_download_record \
        emscripten-sdk \
        "$PF_PLATFORM_KEY" \
        "$pf_emsdk_downloads" \
        "$pf_release_revision-$pf_sdk_archive"

    pf_node_record=$(pf_record node "$PF_PLATFORM_KEY") ||
        pf_fail "Node 22.16.0 is not locked for $PF_PLATFORM_KEY"
    pf_old_ifs=$IFS
    IFS='	'
    set -- $pf_node_record
    IFS=$pf_old_ifs
    pf_node_archive=$4
    pf_download_record \
        node \
        "$PF_PLATFORM_KEY" \
        "$pf_emsdk_downloads" \
        "$pf_node_archive"

    (
        cd "$pf_emsdk_root"
        # Archive owner IDs are irrelevant to a repository-local toolchain and
        # can fail under containers/user namespaces. GNU and BSD tar both
        # honor TAR_OPTIONS for emsdk's child extraction process.
        TAR_OPTIONS=--no-same-owner \
            EMSDK_KEEP_DOWNLOADS=1 \
            ./emsdk install 6.0.3
        ./emsdk activate 6.0.3
    )

    pf_emcc="$pf_emsdk_root/upstream/emscripten/emcc"
    [ -x "$pf_emcc" ] || pf_fail "emcc was not installed"
    "$pf_emcc" --version | grep -Fq '6.0.3' ||
        pf_fail "installed emcc is not Emscripten 6.0.3"
    echo "emscripten=6.0.3 revision=$pf_release_revision"
fi

git -C "$PF_REPOSITORY_ROOT" config core.hooksPath .githooks

if [ "$pf_run_smoke" -eq 1 ]; then
    if [ "$pf_install_web" -eq 1 ]; then
        "$PF_REPOSITORY_ROOT/tools/workflow.sh" web
    else
        "$PF_REPOSITORY_ROOT/tools/workflow.sh" headless
    fi
fi

echo "bootstrap=pass platform=$PF_PLATFORM_KEY toolchains=$PF_TOOLCHAINS_DIR"

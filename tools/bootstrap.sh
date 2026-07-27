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

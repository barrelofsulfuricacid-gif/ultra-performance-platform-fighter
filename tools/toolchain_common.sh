#!/bin/sh

pf_fail()
{
    echo "toolchain error: $*" >&2
    exit 1
}

pf_platform_key()
{
    pf_os=$(uname -s)
    pf_arch=$(uname -m)

    case "$pf_os" in
        Linux)
            pf_platform_os=linux
            ;;
        Darwin)
            pf_platform_os=macos
            ;;
        *)
            pf_fail "unsupported POSIX host operating system: $pf_os"
            ;;
    esac

    case "$pf_arch" in
        x86_64|amd64)
            pf_platform_arch=x86_64
            ;;
        aarch64|arm64)
            pf_platform_arch=arm64
            ;;
        *)
            pf_fail "unsupported host architecture: $pf_arch"
            ;;
    esac

    PF_PLATFORM_KEY="$pf_platform_os-$pf_platform_arch"
    export PF_PLATFORM_KEY
}

pf_find_host_tools()
{
    : "${PF_REPOSITORY_ROOT:?PF_REPOSITORY_ROOT must be set}"
    PF_TOOLCHAINS_DIR=${PF_TOOLCHAINS_DIR:-"$PF_REPOSITORY_ROOT/.toolchains"}
    pf_host_root="$PF_TOOLCHAINS_DIR/host/$PF_PLATFORM_KEY"

    PF_CMAKE=$(find "$pf_host_root/cmake-4.4.0" \
        -type f -path '*/bin/cmake' 2>/dev/null | head -n 1)
    PF_NINJA=$(find "$pf_host_root/ninja-1.13.2" \
        -type f -name ninja 2>/dev/null | head -n 1)

    [ -n "$PF_CMAKE" ] && [ -x "$PF_CMAKE" ] ||
        pf_fail "pinned CMake is missing; run ./tools/bootstrap.sh"
    [ -n "$PF_NINJA" ] && [ -x "$PF_NINJA" ] ||
        pf_fail "pinned Ninja is missing; run ./tools/bootstrap.sh"

    pf_cmake_version=$("$PF_CMAKE" --version | sed -n '1s/^cmake version //p')
    [ "$pf_cmake_version" = "4.4.0" ] ||
        pf_fail "expected CMake 4.4.0, got $pf_cmake_version"

    pf_ninja_version=$("$PF_NINJA" --version)
    [ "$pf_ninja_version" = "1.13.2" ] ||
        pf_fail "expected Ninja 1.13.2, got $pf_ninja_version"

    PATH="$(dirname "$PF_CMAKE"):$(dirname "$PF_NINJA"):$PATH"
    export PATH PF_CMAKE PF_NINJA PF_TOOLCHAINS_DIR
}

pf_validate_compiler()
{
    case "$PF_PLATFORM_KEY" in
        linux-*)
            if command -v gcc-13 >/dev/null 2>&1; then
                CC=$(command -v gcc-13)
            elif command -v cc >/dev/null 2>&1; then
                CC=$(command -v cc)
            else
                pf_fail "GNU C 13.3.x is required; install gcc-13"
            fi
            pf_compiler_version=$("$CC" --version | sed -n '1p')
            pf_expected_pattern='13\.3\.'
            pf_expected_description='GNU C 13.3.x'
            ;;
        macos-*)
            command -v clang >/dev/null 2>&1 ||
                pf_fail "Clang 17.0.x is required; install Xcode command-line tools"
            CC=$(command -v clang)
            pf_compiler_version=$("$CC" --version | sed -n '1p')
            pf_expected_pattern='clang version 17\.0\.'
            pf_expected_description='Clang 17.0.x'
            ;;
        *)
            pf_fail "no compiler lane is defined for $PF_PLATFORM_KEY"
            ;;
    esac

    if ! printf '%s\n' "$pf_compiler_version" |
        grep -Eq "$pf_expected_pattern"; then
        if [ "${PF_ALLOW_UNPINNED_COMPILER:-0}" = "1" ]; then
            echo "warning: expected $pf_expected_description; using $pf_compiler_version" >&2
        else
            pf_fail "expected $pf_expected_description; got $pf_compiler_version"
        fi
    fi

    export CC
    echo "compiler=$pf_compiler_version"
}

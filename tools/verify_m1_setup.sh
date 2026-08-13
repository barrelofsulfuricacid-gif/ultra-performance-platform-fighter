#!/bin/sh
set -eu

repository_root=$(git rev-parse --show-toplevel)
lock_file="$repository_root/dependencies/toolchains.lock.tsv"
presets_file="$repository_root/CMakePresets.json"
ci_file="$repository_root/.github/workflows/ci.yml"

fail()
{
    echo "M1 setup verification failed: $*" >&2
    exit 1
}

[ -f "$lock_file" ] || fail "missing toolchain lock"
[ -f "$presets_file" ] || fail "missing CMakePresets.json"
[ -f "$ci_file" ] || fail "missing clean-machine CI workflow"

awk -F '	' '
    NR == 1 {
        if ($0 != "component\tversion\tplatform\tarchive\tsha256\tsize\turl") {
            print "invalid lock header" >"/dev/stderr"
            exit 1
        }
        next
    }
    {
        if (NF != 7) {
            print "lock row does not have seven fields at line " NR >"/dev/stderr"
            exit 1
        }
        key = $1 SUBSEP $3
        if (seen[key]++) {
            print "duplicate component/platform at line " NR >"/dev/stderr"
            exit 1
        }
        if (length($5) != 64 || $5 ~ /[^0-9a-f]/) {
            print "invalid SHA-256 at line " NR >"/dev/stderr"
            exit 1
        }
        if ($6 !~ /^[0-9]+$/ || $6 == 0) {
            print "invalid byte length at line " NR >"/dev/stderr"
            exit 1
        }
        if ($7 !~ /^https:\/\//) {
            print "non-HTTPS lock URL at line " NR >"/dev/stderr"
            exit 1
        }
        records++
    }
    END {
        if (records != 29) {
            print "expected 29 lock records, got " records >"/dev/stderr"
            exit 1
        }
    }
' "$lock_file" || fail "archive lock structure is invalid"

require_lock_record()
{
    component=$1
    version=$2
    platform=$3
    awk -F '	' \
        -v component="$component" \
        -v version="$version" \
        -v platform="$platform" '
            NR > 1 &&
            $1 == component &&
            $2 == version &&
            $3 == platform {
                found = 1
            }
            END {
                exit !found
            }
        ' "$lock_file" ||
        fail "missing exact lock $component $version $platform"
}

for platform in \
    linux-x86_64 \
    linux-arm64 \
    macos-x86_64 \
    macos-arm64 \
    windows-x86_64 \
    windows-arm64
do
    require_lock_record cmake 4.4.0 "$platform"
    require_lock_record ninja 1.13.2 "$platform"
done

require_lock_record emsdk 6.0.3 all
require_lock_record sdl-source 3.4.12 all
require_lock_record sqlite-source 3.53.4 all
require_lock_record tracy-source 0.13.1 all
require_lock_record tracy-capstone-source 6.0.0-Alpha5 all
require_lock_record tracy-ppqsort-source 1.0.6 all
require_lock_record tracy-zstd-source 1.5.7 all
for platform in \
    linux-x86_64 \
    linux-arm64 \
    macos-x86_64 \
    macos-arm64 \
    windows-x86_64
do
    require_lock_record emscripten-sdk 6.0.3 "$platform"
    require_lock_record node 22.16.0 "$platform"
done

for script in \
    tools/bootstrap.sh \
    tools/capture_profile.sh \
    tools/run_verifier.sh \
    tools/serve_web.sh \
    tools/run_performance.sh \
    tools/toolchain_common.sh \
    tools/verify_m3_performance.sh \
    tools/verify_m3_verifier.sh \
    tools/verify_web_smoke.sh \
    tools/workflow.sh
do
    [ -x "$repository_root/$script" ] ||
        fail "$script is not executable"
    sh -n "$repository_root/$script" ||
        fail "$script has invalid POSIX shell syntax"
done

for script in \
    tools/bootstrap.ps1 \
    tools/run_performance.ps1 \
    tools/run_verifier.ps1 \
    tools/serve_web.ps1 \
    tools/toolchain_common.ps1 \
    tools/workflow.ps1
do
    [ -f "$repository_root/$script" ] ||
        fail "missing $script"
done

if command -v pwsh >/dev/null 2>&1; then
    for script in \
        tools/bootstrap.ps1 \
        tools/run_performance.ps1 \
        tools/run_verifier.ps1 \
        tools/serve_web.ps1 \
        tools/toolchain_common.ps1 \
        tools/workflow.ps1
    do
        PF_POWERSHELL_FILE="$repository_root/$script" \
            pwsh -NoLogo -NoProfile -NonInteractive -Command '
            $tokens = $null
            $errors = $null
            [System.Management.Automation.Language.Parser]::ParseFile(
                $env:PF_POWERSHELL_FILE,
                [ref]$tokens,
                [ref]$errors) | Out-Null
            if ($errors.Count -ne 0) {
                $errors | ForEach-Object { [Console]::Error.WriteLine($_) }
                exit 1
            }
        ' ||
            fail "$script has invalid PowerShell syntax"
    done
else
    echo "powershell-syntax=deferred-to-windows-ci"
fi

. "$repository_root/tools/toolchain_common.sh"
PF_REPOSITORY_ROOT=$repository_root
export PF_REPOSITORY_ROOT
pf_platform_key
pf_find_host_tools

"$PF_CMAKE" --list-presets=all >/dev/null ||
    fail "CMake rejected CMakePresets.json"

actual_workflows=$(
    "$PF_CMAKE" --list-presets=workflow |
        sed -n 's/^  "\([^"]*\)".*/\1/p' |
        sort
)
expected_workflows=$(
    printf '%s\n' \
        benchmark \
        debug \
        headless \
        profile \
        release \
        sanitizer \
        verifier \
        web |
        sort
)
[ "$actual_workflows" = "$expected_workflows" ] ||
    fail "workflow preset names differ from the required eight"

"$repository_root/tools/bootstrap.sh" --verify-only --no-smoke >/dev/null ||
    fail "idempotent bootstrap verification failed"

git -C "$repository_root" check-ignore -q .toolchains/probe ||
    fail ".toolchains is not ignored"
git -C "$repository_root" check-ignore -q performance/local/profile-probe ||
    fail "local profile output is not ignored"

grep -Fq -- "--js-library=" "$repository_root/CMakeLists.txt" ||
    fail "web JavaScript adapter is not linked"
grep -Fq -- "--pre-js=" "$repository_root/CMakeLists.txt" ||
    fail "web owner-evidence data is not linked"
grep -Fq "pf_web_set_status" \
    "$repository_root/src/web_client/web_adapter.js" ||
    fail "web JavaScript adapter is missing its status boundary"
grep -Fq "pf_web_render_probe" \
    "$repository_root/src/web_client/web_adapter.js" ||
    fail "web JavaScript adapter is missing its WebGL2 boundary"
[ -f "$repository_root/include/pf/render_packet.h" ] ||
    fail "shared render packet contract is missing"
[ -f "$repository_root/src/native_client/main.c" ] ||
    fail "SDL3 native-client adapter is missing"
for native_api in \
    SDL_CreateGPURenderer \
    SDL_RenderGeometryRaw \
    SDL_AddGamepadMapping \
    SDL_PushEvent
do
    grep -R -Fq "$native_api" "$repository_root/src/native_client" ||
        fail "native SDL adapter is missing $native_api"
done
for web_api in \
    'getContext("webgl2"' \
    'gl.drawArrays' \
    'gl.readPixels'
do
    grep -Fq "$web_api" "$repository_root/src/web_client/web_adapter.js" ||
        fail "browser renderer adapter is missing $web_api"
done
if grep -R -E -n 'SDL3/|SDL_[A-Za-z0-9_]+' \
    "$repository_root/src/sim" \
    "$repository_root/src/headless" \
    "$repository_root/include/pf/sim.h"
then
    fail "SDL leaked into sim or headless source"
fi
grep -Fq '"PF_ENABLE_SDL_CLIENT": false' "$presets_file" ||
    fail "isolated presets do not explicitly disable SDL"
if grep -Fq "EM_ASM" "$repository_root/src/product/product_main.c"; then
    fail "strict authored C contains inline Emscripten JavaScript"
fi

if command -v node >/dev/null 2>&1; then
    node --check "$repository_root/src/web_client/web_adapter.js" ||
        fail "browser JavaScript adapter has invalid syntax"
    node --check "$repository_root/src/web_client/owner_evidence.js" ||
        fail "browser owner-evidence adapter has invalid syntax"
else
    echo "javascript-syntax=deferred-to-web-ci"
fi

action_uses=$(
    sed -n 's/^[[:space:]]*uses:[[:space:]]*//p' "$ci_file" |
        sort -u
)
expected_action_uses=$(
    printf '%s\n' \
        "actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1" \
        "actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02" |
        sort
)
[ "$action_uses" = "$expected_action_uses" ] ||
    fail "CI actions are not restricted to the reviewed commits"

for runner in \
    ubuntu-24.04 \
    ubuntu-24.04-arm \
    macos-15-intel \
    macos-15 \
    windows-2025
do
    grep -Fq -- "- $runner" "$ci_file" ||
        fail "CI is missing runner $runner"
done

grep -Fq '.\tools\bootstrap.ps1' "$ci_file" ||
    fail "CI does not exercise the PowerShell bootstrap"
grep -Fq './tools/verify_web_smoke.sh' "$ci_file" ||
    fail "CI does not run the browser DOM smoke"
for package in \
    libx11-dev \
    libxcursor-dev \
    libxext-dev \
    libxfixes-dev \
    libxi-dev \
    libxrandr-dev \
    libxss-dev \
    libxtst-dev
do
    grep -Fq "$package" "$ci_file" ||
        fail "CI does not install Linux platform SDK package $package"
done

echo "m1-setup-verification=pass records=29 workflows=8"

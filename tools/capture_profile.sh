#!/bin/sh
set -eu

PF_REPOSITORY_ROOT=$(git rev-parse --show-toplevel)
export PF_REPOSITORY_ROOT

. "$PF_REPOSITORY_ROOT/tools/toolchain_common.sh"

pf_label=${1:-M3}
pf_output_dir=${2:-"$PF_REPOSITORY_ROOT/performance/local/profiles/$pf_label"}
pf_capture_seconds=${PF_PROFILE_CAPTURE_SECONDS:-5}
pf_capture_build="$PF_REPOSITORY_ROOT/performance/local/pf-tracy-capture-build"
pf_trace="$pf_output_dir/$pf_label.tracy"
pf_workload_log="$pf_output_dir/workload.log"
pf_capture_log="$pf_output_dir/tracy_capture.log"
pf_os_log="$pf_output_dir/os_profiler.log"
pf_manifest="$pf_output_dir/profile_manifest.tsv"
pf_analysis="$pf_output_dir/analysis.md"

printf '%s\n' "$pf_label" | grep -Eq '^[A-Za-z0-9._-]+$' ||
    pf_fail "profile label may contain only letters, digits, dot, underscore, or dash"
printf '%s\n' "$pf_capture_seconds" | grep -Eq '^[1-9][0-9]*$' ||
    pf_fail "PF_PROFILE_CAPTURE_SECONDS must be a positive integer"

pf_platform_key
pf_find_host_tools
pf_validate_compiler

pf_tracy="$PF_TOOLCHAINS_DIR/dependencies/tracy-0.13.1"
pf_capstone="$PF_TOOLCHAINS_DIR/dependencies/capstone-6.0.0-Alpha5"
pf_ppqsort="$PF_TOOLCHAINS_DIR/dependencies/ppqsort-1.0.6"
pf_zstd="$PF_TOOLCHAINS_DIR/dependencies/zstd-1.5.7"
for pf_required in \
    "$pf_tracy/public/TracyClient.cpp" \
    "$pf_capstone/include/capstone/capstone.h" \
    "$pf_ppqsort/include/ppqsort.h" \
    "$pf_zstd/lib/zstd.h"
do
    [ -f "$pf_required" ] ||
        pf_fail "locked profile dependency is missing; run ./tools/bootstrap.sh"
done

mkdir -p "$pf_output_dir" "$pf_capture_build"

"$PF_REPOSITORY_ROOT/tools/workflow.sh" profile

pf_profile_cache="$PF_REPOSITORY_ROOT/build/profile/CMakeCache.txt"
pf_timer_fallback=$(
    sed -n 's/^PF_TRACY_TIMER_FALLBACK:BOOL=//p' "$pf_profile_cache"
)
case "$pf_timer_fallback" in
    ON|OFF)
        ;;
    *)
        pf_fail "profile build did not record its Tracy timer policy"
        ;;
esac

"$PF_CMAKE" \
    -S "$PF_REPOSITORY_ROOT/tools/tracy_capture" \
    -B "$pf_capture_build" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MAKE_PROGRAM="$PF_NINJA" \
    -DPF_TRACY_SOURCE_DIR="$pf_tracy" \
    -DPF_CAPSTONE_SOURCE_DIR="$pf_capstone" \
    -DPF_PPQSORT_SOURCE_DIR="$pf_ppqsort" \
    -DPF_ZSTD_SOURCE_DIR="$pf_zstd"
"$PF_CMAKE" --build "$pf_capture_build" --parallel

pf_benchmark="$PF_REPOSITORY_ROOT/build/profile/pf_benchmarks"
pf_capture="$pf_capture_build/pf_tracy_capture"
[ -x "$pf_benchmark" ] || pf_fail "profile benchmark executable is missing"
[ -x "$pf_capture" ] || pf_fail "Tracy capture executable is missing"

"$pf_benchmark" --profile-workload >"$pf_workload_log" 2>&1 &
pf_workload_pid=$!
sleep 1
if "$pf_capture" \
    -o "$pf_trace" \
    -f \
    -s "$pf_capture_seconds" >"$pf_capture_log" 2>&1
then
    pf_tracy_status=pass
else
    pf_tracy_status=fail
fi
if wait "$pf_workload_pid"; then
    pf_workload_status=pass
else
    pf_workload_status=fail
fi

[ "$pf_tracy_status" = pass ] ||
    pf_fail "Tracy capture failed; inspect $pf_capture_log"
[ "$pf_workload_status" = pass ] ||
    pf_fail "profile workload failed; inspect $pf_workload_log"
[ -s "$pf_trace" ] || pf_fail "Tracy capture is empty"
grep -Fq 'profile-workload=pass' "$pf_workload_log" ||
    pf_fail "profile workload did not emit its pass contract"

pf_os_profiler=unavailable
pf_os_reason=tool-not-installed
case "$PF_PLATFORM_KEY" in
    linux-*)
        if command -v perf >/dev/null 2>&1; then
            if perf stat \
                -o "$pf_os_log" \
                "$pf_benchmark" \
                --profile-workload >/dev/null 2>&1
            then
                pf_os_profiler=pass
                pf_os_reason=perf-stat
            else
                pf_os_reason=perf-permission-or-kernel-policy
            fi
        fi
        ;;
    macos-*)
        if command -v xctrace >/dev/null 2>&1; then
            if xctrace record \
                --template 'Time Profiler' \
                --output "$pf_output_dir/$pf_label.trace" \
                --launch -- \
                "$pf_benchmark" \
                --profile-workload >"$pf_os_log" 2>&1
            then
                pf_os_profiler=pass
                pf_os_reason=xctrace-time-profiler
            else
                pf_os_reason=xctrace-unavailable-or-denied
            fi
        fi
        ;;
esac

pf_sha256()
{
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{ print $1 }'
    else
        shasum -a 256 "$1" | awk '{ print $1 }'
    fi
}

pf_commit=$(git -C "$PF_REPOSITORY_ROOT" rev-parse HEAD)
if [ -n "$(git -C "$PF_REPOSITORY_ROOT" status --porcelain)" ]; then
    pf_dirty=true
else
    pf_dirty=false
fi
pf_trace_sha=$(pf_sha256 "$pf_trace")
pf_trace_size=$(wc -c <"$pf_trace" | tr -d ' ')
pf_binary_sha=$(pf_sha256 "$pf_benchmark")

{
    printf 'field\tvalue\n'
    printf 'label\t%s\n' "$pf_label"
    printf 'commit\t%s\n' "$pf_commit"
    printf 'dirty\t%s\n' "$pf_dirty"
    printf 'tracy_version\t0.13.1\n'
    printf 'tracy_capture\tpass\n'
    printf 'tracy_timer_fallback\t%s\n' "$pf_timer_fallback"
    printf 'tracy_trace\t%s\n' "${pf_trace#"$PF_REPOSITORY_ROOT"/}"
    printf 'tracy_trace_sha256\t%s\n' "$pf_trace_sha"
    printf 'tracy_trace_bytes\t%s\n' "$pf_trace_size"
    printf 'profile_binary_sha256\t%s\n' "$pf_binary_sha"
    printf 'os_profiler\t%s\n' "$pf_os_profiler"
    printf 'os_profiler_reason\t%s\n' "$pf_os_reason"
    printf 'platform\t%s\n' "$PF_PLATFORM_KEY"
} >"$pf_manifest"

{
    printf '# %s profile capture\n\n' "$pf_label"
    printf 'Tracy 0.13.1 capture: **pass**. '
    printf 'The trace contains canonical benchmark scenario zones and frame marks.\n\n'
    printf -- '- Commit: `%s`\n' "$pf_commit"
    printf -- '- Dirty tree: `%s`\n' "$pf_dirty"
    printf -- '- Trace: `%s`\n' "${pf_trace#"$PF_REPOSITORY_ROOT"/}"
    printf -- '- Trace SHA-256: `%s`\n' "$pf_trace_sha"
    printf -- '- Trace bytes: `%s`\n' "$pf_trace_size"
    printf -- '- Tracy timer fallback: `%s`\n' "$pf_timer_fallback"
    printf -- '- OS profiler: `%s` (`%s`)\n' \
        "$pf_os_profiler" \
        "$pf_os_reason"
    printf '\nThe raw trace and tool build remain local by default. '
    printf 'A milestone report may commit this manifest and analysis without '
    printf 'creating a recursive per-commit benchmark loop.\n'
} >"$pf_analysis"

printf 'profile-capture=pass tracy=0.13.1 timer_fallback=%s os_profiler=%s manifest=%s\n' \
    "$pf_timer_fallback" \
    "$pf_os_profiler" \
    "$pf_manifest"

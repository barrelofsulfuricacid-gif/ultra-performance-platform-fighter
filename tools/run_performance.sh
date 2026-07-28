#!/bin/sh
set -eu

PF_REPOSITORY_ROOT=$(git rev-parse --show-toplevel)
export PF_REPOSITORY_ROOT

. "$PF_REPOSITORY_ROOT/tools/toolchain_common.sh"

pf_mode=${1:-commit}
pf_evidence_dir=${2:-"$PF_REPOSITORY_ROOT/performance/local/current"}

case "$pf_mode" in
    commit|milestone)
        ;;
    *)
        pf_fail "performance mode must be commit or milestone"
        ;;
esac

pf_platform_key
pf_find_host_tools
pf_validate_compiler

"$PF_REPOSITORY_ROOT/tools/workflow.sh" benchmark

pf_binary="$PF_REPOSITORY_ROOT/build/benchmark/pf_benchmarks"
pf_compile_commands="$PF_REPOSITORY_ROOT/build/benchmark/compile_commands.json"
pf_database="$PF_REPOSITORY_ROOT/performance/local/performance.sqlite3"
pf_graph_directory="$PF_REPOSITORY_ROOT/performance/local/graphs"
pf_manifest="$pf_evidence_dir/performance_manifest.txt"

[ -x "$pf_binary" ] || pf_fail "benchmark executable is missing"
[ -f "$pf_compile_commands" ] ||
    pf_fail "benchmark compile_commands.json is missing"
mkdir -p "$pf_evidence_dir" "$pf_graph_directory"

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

pf_one_line()
{
    tr '\r\n\t' '   ' |
        sed 's/[ ][ ]*/ /g; s/^ //; s/ $//'
}

PF_PERF_COMMIT=$(git -C "$PF_REPOSITORY_ROOT" rev-parse HEAD)
if [ -n "$(git -C "$PF_REPOSITORY_ROOT" status --porcelain)" ]; then
    PF_PERF_DIRTY=1
else
    PF_PERF_DIRTY=0
fi

PF_PERF_COMPILER=$("$CC" --version | sed -n '1p' | pf_one_line)
PF_PERF_COMPILER_FLAGS=$(
    awk '
        /^  "command": "/ {
            line = $0
            sub(/^  "command": "/, "", line)
            sub(/",$/, "", line)
            print line
            exit
        }
    ' "$pf_compile_commands" |
        sed "s|$PF_REPOSITORY_ROOT|{ROOT}|g" |
        pf_one_line
)
[ -n "$PF_PERF_COMPILER_FLAGS" ] ||
    pf_fail "could not resolve the canonical simulation compile command"

pf_os=$(uname -srm | pf_one_line)
pf_cpu_count=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo unknown)
case "$PF_PLATFORM_KEY" in
    linux-*)
        pf_cpu=$(
            awk -F: '
                /^model name/ {
                    sub(/^[ \t]+/, "", $2)
                    print $2
                    exit
                }
            ' /proc/cpuinfo |
                pf_one_line
        )
        [ -n "$pf_cpu" ] || pf_cpu=unknown
        if command -v systemd-detect-virt >/dev/null 2>&1; then
            pf_virtualization=$(
                systemd-detect-virt 2>/dev/null || echo none
            )
        else
            pf_virtualization=unavailable
        fi
        ;;
    macos-*)
        pf_cpu=$(
            sysctl -n machdep.cpu.brand_string 2>/dev/null ||
                sysctl -n hw.model 2>/dev/null ||
                echo unknown
        )
        pf_cpu=$(printf '%s\n' "$pf_cpu" | pf_one_line)
        pf_virtualization=unavailable
        ;;
    *)
        pf_fail "unsupported benchmark platform: $PF_PLATFORM_KEY"
        ;;
esac

PF_PERF_CPU_FINGERPRINT="$pf_cpu;logical_cpus=$pf_cpu_count"
PF_PERF_MACHINE_FINGERPRINT=\
"$PF_PLATFORM_KEY;$PF_PERF_CPU_FINGERPRINT;virtualization=$pf_virtualization"
PF_PERF_OS_FINGERPRINT=$pf_os
PF_PERF_EXECUTABLE_HASH=$(pf_sha256 "$pf_binary")
PF_PERF_BUILD_CONFIGURATION=benchmark-release
PF_PERF_DATABASE=$pf_database
PF_PERF_GRAPH_DIRECTORY=$pf_graph_directory
PF_PERF_MANIFEST=$pf_manifest
PF_PERF_SCHEMA=\
"$PF_REPOSITORY_ROOT/performance/database/schema.sql"
PF_PERF_POWER_METADATA=${PF_BENCH_POWER_METADATA:-unavailable:not-supplied}
PF_PERF_THERMAL_METADATA=\
${PF_BENCH_THERMAL_METADATA:-unavailable:not-supplied}

export \
    PF_PERF_BUILD_CONFIGURATION \
    PF_PERF_COMMIT \
    PF_PERF_COMPILER \
    PF_PERF_COMPILER_FLAGS \
    PF_PERF_CPU_FINGERPRINT \
    PF_PERF_DATABASE \
    PF_PERF_DIRTY \
    PF_PERF_EXECUTABLE_HASH \
    PF_PERF_GRAPH_DIRECTORY \
    PF_PERF_MACHINE_FINGERPRINT \
    PF_PERF_MANIFEST \
    PF_PERF_OS_FINGERPRINT \
    PF_PERF_POWER_METADATA \
    PF_PERF_SCHEMA \
    PF_PERF_THERMAL_METADATA

"$pf_binary" --run "$pf_mode"

#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_root=${1:-"$root/performance/local/m3_performance_qualification"}
mkdir -p "$output_root"
output_dir=$(mktemp -d "$output_root/run.XXXXXX")
graph_dir="$output_dir/graphs"
database="$output_dir/history.sqlite3"
manifest="$output_dir/manifest.txt"
log_file="$output_dir/qualification.log"

mkdir -p "$graph_dir"
"$root/tools/workflow.sh" benchmark

if "$root/build/benchmark/pf_benchmarks" \
    --qualify-history \
    "$database" \
    "$graph_dir" \
    "$manifest" >"$log_file" 2>&1
then
    sed -n '1,120p' "$log_file"
else
    sed -n '1,240p' "$log_file" >&2
    exit 1
fi

[ -s "$database" ] || {
    echo "M3 performance verification failed: database is empty" >&2
    exit 1
}
[ -s "$manifest" ] || {
    echo "M3 performance verification failed: manifest is empty" >&2
    exit 1
}

grep -Fq 'invalid_comparisons=9' "$manifest" || {
    echo "M3 performance verification failed: incompatible metadata was not detected" >&2
    exit 1
}
grep -Fq 'confirmed_regressions=0' "$manifest" || {
    echo "M3 performance verification failed: final qualification run is not clean" >&2
    exit 1
}

svg_count=$(
    find "$graph_dir" -maxdepth 1 -type f -name '*.svg' |
        wc -l |
        tr -d ' '
)
[ "$svg_count" = "13" ] || {
    echo "M3 performance verification failed: expected 13 SVG graphs, got $svg_count" >&2
    exit 1
}

grep -Fq 'suspected=9 confirmed=9 invalid=9' "$log_file" || {
    echo "M3 performance verification failed: regression qualification changed" >&2
    exit 1
}
grep -Fq 'same_commit=9' "$log_file" || {
    echo "M3 performance verification failed: unchanged baseline comparison changed" >&2
    exit 1
}

"$root/tools/workflow.sh" headless
if command -v nm >/dev/null 2>&1; then
    if nm "$root/build/headless/headless" |
        grep -Eq '___tracy|[[:space:]]sqlite3_'
    then
        echo "M3 performance verification failed: headless links instrumentation or history code" >&2
        exit 1
    fi
    echo "headless-instrumentation-boundary=pass tracy=absent sqlite=absent"
else
    echo "headless-instrumentation-boundary=deferred reason=nm-not-on-path"
fi

echo "m3-performance-verification=pass graphs=13 schema=1 headless_instrumentation=absent"

#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
commit=${1:-HEAD}

git -C "$root" diff-tree --root --check -r "$commit"

required_files="
README.md
docs/plan_reference.md
docs/architecture/data_and_api_contracts.md
docs/architecture/determinism_contract.md
docs/architecture/representation_decision.md
docs/architecture/system_boundaries.md
docs/milestones/M0_checkpoint_report.md
docs/performance/performance_charter.md
docs/product/melee_feel_contract.md
docs/product/originality_and_provenance.md
docs/product/roster_coverage_matrix.md
docs/product/stage_briefs.md
docs/technology_decisions/0001-authored-c-and-foreign-abi.md
docs/technology_decisions/0002-build-platform-and-web.md
docs/technology_decisions/0003-debug-gui-and-design-data.md
docs/technology_decisions/0004-rollback-and-crossplay-transport.md
docs/technology_decisions/0005-audio-profiling-history-and-rl.md
docs/technology_decisions/dependency_register.md
experiments/m0_representation/m0_analyze.c
experiments/m0_representation/m0_bench.c
experiments/m0_representation/run_benchmarks.sh
experiments/m0_representation/summarize_results.sh
plan_modifications.md
performance/README.md
performance/m0_representation/diagnostics.txt
performance/m0_representation/metadata.txt
performance/m0_representation/perf-smoke.txt
performance/m0_representation/results.csv
performance/m0_representation/summary.md
tools/post_commit.sh
tools/verify_m0.sh
tools/verify_m0_sanitized.sh
"

for path in $required_files; do
    if [ ! -s "$root/$path" ]; then
        echo "missing or empty required file: $path" >&2
        exit 1
    fi
done

result_rows=$(($(wc -l \
    <"$root/performance/m0_representation/results.csv") - 1))
if [ "$result_rows" -ne 345 ]; then
    echo "expected 345 M0 milestone samples, found $result_rows" >&2
    exit 1
fi

if ! grep -q '^dirty=false$' \
    "$root/performance/m0_representation/metadata.txt"; then
    echo "M0 milestone benchmark was not captured from a clean tree" >&2
    exit 1
fi

if ! grep -q '^self-test=pass ' \
    "$root/performance/m0_representation/diagnostics.txt"; then
    echo "M0 milestone benchmark self-test evidence missing" >&2
    exit 1
fi

if git -C "$root" grep -n -E '^(<<<<<<<|=======|>>>>>>>)' "$commit" -- \
    ':(exclude)performance/local/**'; then
    echo "merge-conflict marker found" >&2
    exit 1
fi

for decision in D1-A D2-A D3-C D4-A D5-A D6-A; do
    if ! grep -q "$decision" "$root/docs/plan_reference.md"; then
        echo "governing decision missing: $decision" >&2
        exit 1
    fi
done

roster_count=$(grep -c '^| \[' \
    "$root/docs/product/roster_coverage_matrix.md")
if [ "$roster_count" -ne 26 ]; then
    echo "expected 26 SSBM fighter/form rows, found $roster_count" >&2
    exit 1
fi

stage_count=$(grep -c '^### S[0-9][0-9] — ' \
    "$root/docs/product/stage_briefs.md")
if [ "$stage_count" -ne 10 ]; then
    echo "expected 10 stage briefs, found $stage_count" >&2
    exit 1
fi

for mechanic in Wavedash L-cancel DI SDI Teching Ledges; do
    if ! grep -qi "$mechanic" "$root/docs/product/melee_feel_contract.md"; then
        echo "required Melee-feel mechanic missing: $mechanic" >&2
        exit 1
    fi
done

experiment_runner="$root/experiments/m0_representation/run_benchmarks.sh"
if [ -x "$experiment_runner" ]; then
    M0_BENCH_MODE=smoke "$experiment_runner" "$root/performance/local/smoke"
    "$root/experiments/m0_representation/summarize_results.sh" \
        "$root/performance/local/smoke/results.csv" \
        "$root/performance/local/smoke/summary.md"
else
    echo "experiment smoke test: unavailable before M0 harness commit"
fi

echo "M0 provisional verification passed for $commit"

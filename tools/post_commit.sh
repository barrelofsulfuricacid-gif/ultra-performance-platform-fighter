#!/bin/sh
set -u

root=$(git rev-parse --show-toplevel)
commit=$(git rev-parse HEAD)
evidence_dir="$root/performance/local/commits/$commit"
log_file="$evidence_dir/post_commit.log"
manifest_file="$evidence_dir/manifest.txt"

mkdir -p "$evidence_dir"

status=pass
{
    echo "commit=$commit"
    echo "started_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"

    if "$root/tools/verify_m0.sh" "$commit"; then
        echo "verification=pass"
    else
        echo "verification=fail"
        status=fail
    fi

    if "$root/tools/verify_m1_foundation.sh" \
        "$evidence_dir/m1_foundation"; then
        echo "m1_foundation_verification=pass"
    else
        echo "m1_foundation_verification=fail"
        status=fail
    fi

    if "$root/tools/verify_m1_workflow.sh"; then
        echo "m1_workflow_verification=pass"
    else
        echo "m1_workflow_verification=fail"
        status=fail
    fi

    if "$root/tools/verify_m1_setup.sh"; then
        echo "m1_setup_verification=pass"
    else
        echo "m1_setup_verification=fail"
        status=fail
    fi

    benchmark_runner="$root/experiments/m0_representation/run_benchmarks.sh"
    if [ -x "$benchmark_runner" ]; then
        if M0_BENCH_MODE=commit "$benchmark_runner" "$evidence_dir"; then
            echo "benchmark=pass"
        else
            echo "benchmark=fail"
            status=fail
        fi
    else
        echo "benchmark=unavailable"
        echo "benchmark_reason=m0_harness_not_implemented"
    fi

    echo "finished_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "status=$status"
} >"$log_file" 2>&1

{
    echo "commit=$commit"
    echo "status=$status"
    echo "evidence=$log_file"
} >"$manifest_file"

echo "post-commit $status: $manifest_file"
[ "$status" = pass ]

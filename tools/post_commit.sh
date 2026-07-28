#!/bin/sh
set -u

root=$(git rev-parse --show-toplevel)
commit=$(git rev-parse HEAD)
evidence_dir="$root/performance/local/commits/$commit"
verifier_dir="$evidence_dir/verifier"
log_file="$evidence_dir/post_commit.log"
manifest_file="$evidence_dir/manifest.txt"

mkdir -p "$evidence_dir"

if "$root/tools/run_verifier.sh" \
    "$commit" \
    "$verifier_dir" >"$log_file" 2>&1
then
    status=pass
else
    status=fail
fi

{
    echo "commit=$commit"
    echo "status=$status"
    echo "evidence=$log_file"
    echo "verifier_manifest=$verifier_dir/pass_manifest.md"
    echo "benchmark_database=$root/performance/local/performance.sqlite3"
    echo "benchmark_graphs=$root/performance/local/graphs"
} >"$manifest_file"

echo "post-commit $status: $manifest_file"
[ "$status" = pass ]

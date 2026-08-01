#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m3_verifier_qualification"}
qualification_dir="$output_dir/qualification"
pass_dir="$output_dir/pass"
fail_dir="$output_dir/fail"
coverage_dir="$output_dir/coverage"
pass_issues="$output_dir/pass_issues"
fail_issues="$output_dir/fail_issues"
coverage_issues="$output_dir/coverage_issues"
diff_file="$output_dir/commit_files.txt"
pass_checks="$output_dir/pass_checks.tsv"
fail_checks="$output_dir/fail_checks.tsv"
coverage_checks="$output_dir/coverage_checks.tsv"
commit=$(git -C "$root" rev-parse HEAD)
content_hash=1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526

mkdir -p \
    "$qualification_dir" \
    "$pass_dir" \
    "$fail_dir" \
    "$coverage_dir" \
    "$pass_issues" \
    "$fail_issues" \
    "$coverage_issues"

"$root/tools/workflow.sh" verifier
verifier="$root/build/verifier/pf_verifier"

if command -v sha256sum >/dev/null 2>&1; then
    build_hash=$(sha256sum "$verifier" | awk '{ print $1 }')
else
    build_hash=$(shasum -a 256 "$verifier" | awk '{ print $1 }')
fi

"$verifier" --qualify "$qualification_dir"
grep -Fq 'Status: pass' \
    "$qualification_dir/qualification_manifest.md"
for detector in \
    'Mechanical oracle mismatch | yes' \
    'Visual tolerance mismatch | yes' \
    'Unreachable menu state | yes' \
    'Deterministic hash mismatch | yes'
do
    grep -Fq "$detector" \
        "$qualification_dir/qualification_manifest.md"
done

git -C "$root" \
    diff-tree --root --no-commit-id --name-only -r "$commit" \
    >"$diff_file"

{
    printf 'check\tstatus\tevidence\n'
    printf 'm0-contract\tpass\tqualification fixture\n'
    printf 'm1-foundation\tpass\tqualification fixture\n'
    printf 'm1-workflow\tpass\tqualification fixture\n'
    printf 'm1-setup\tpass\tqualification fixture\n'
    printf 'm2-replay\tpass\tqualification fixture\n'
    printf 'm3-performance\tpass\tqualification fixture\n'
    printf 'm3-regression-qualification\tpass\tqualification fixture\n'
    printf 'mechanical-oracle\tpass\tqualification fixture\n'
    printf 'm4-combat\tpass\tqualification fixture\n'
    printf 'm4-browser-adapter\tpass\tqualification fixture\n'
    printf 'collision-hitbox-overlay\tpass\tqualification fixture\n'
    printf 'm4-local-match-flow\tpass\tqualification fixture\n'
} >"$pass_checks"

"$verifier" \
    --verify \
    "$root/verifier/acceptance_manifest.tsv" \
    "$diff_file" \
    "$pass_checks" \
    "$pass_dir" \
    "$pass_issues" \
    "$commit" \
    "$build_hash" \
    "$content_hash"

grep -Fq 'Status: pass' "$pass_dir/pass_manifest.md"
grep -Fq 'Failures: 0' "$pass_dir/pass_manifest.md"
[ -z "$(find "$pass_issues" -type f -name 'VRF-*.md' -print)" ]

cp "$pass_checks" "$fail_checks"
printf 'sanitizer\tfail\tseeded sanitizer failure\n' \
    >>"$fail_checks"
if "$verifier" \
    --verify \
    "$root/verifier/acceptance_manifest.tsv" \
    "$diff_file" \
    "$fail_checks" \
    "$fail_dir" \
    "$fail_issues" \
    "$commit" \
    "$build_hash" \
    "$content_hash"
then
    echo "M3 verifier qualification failed: seeded failure passed" >&2
    exit 1
fi

issue_count=$(
    find "$fail_issues" -type f -name 'VRF-*.md' |
        wc -l |
        tr -d ' '
)
[ "$issue_count" = "1" ] || {
    echo "M3 verifier qualification failed: expected one issue, got $issue_count" >&2
    exit 1
}
issue_file=$(find "$fail_issues" -type f -name 'VRF-*.md' | head -n 1)
grep -Fq 'Status: unfixed' "$issue_file"
grep -Fq 'Severity: critical' "$issue_file"
grep -Fq "Detected commit: $commit" "$issue_file"
grep -Fq 'seeded sanitizer failure' "$issue_file"
grep -Fq 'Status: fail' "$fail_dir/pass_manifest.md"

grep -Fv 'm3-performance' "$pass_checks" >"$coverage_checks"
if "$verifier" \
    --verify \
    "$root/verifier/acceptance_manifest.tsv" \
    "$diff_file" \
    "$coverage_checks" \
    "$coverage_dir" \
    "$coverage_issues" \
    "$commit" \
    "$build_hash" \
    "$content_hash"
then
    echo "M3 verifier qualification failed: missing acceptance coverage passed" >&2
    exit 1
fi
coverage_issue_count=$(
    find "$coverage_issues" -type f -name 'VRF-*.md' |
        wc -l |
        tr -d ' '
)
[ "$coverage_issue_count" = "1" ] || {
    echo "M3 verifier qualification failed: expected one coverage issue, got $coverage_issue_count" >&2
    exit 1
}
coverage_issue=$(
    find "$coverage_issues" -type f -name 'VRF-*.md' |
        head -n 1
)
grep -Fq 'acceptance:M3-PERFORMANCE' "$coverage_issue"

echo "m3-verifier-qualification=pass internal=3 seeded_defects=4 issue_lifecycle=1 acceptance_coverage=1"

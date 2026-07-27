#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
workflow_doc="$root/docs/workflow_scaffolding.md"
fixtures="$root/tests/fixtures/workflow"

fail()
{
    echo "M1 workflow verification failed: $*" >&2
    exit 1
}

require_directory()
{
    [ -d "$root/$1" ] || fail "missing directory: $1"
    grep -Fq "$1" "$workflow_doc" ||
        fail "directory is not documented: $1"
}

require_file()
{
    [ -f "$root/$1" ] || fail "missing file: $1"
}

require_heading()
{
    grep -Fqx "$2" "$1" ||
        fail "missing heading '$2' in ${1#"$root/"}"
}

field_value()
{
    sed -n "s/^$2: //p" "$1" | head -n 1
}

require_field()
{
    value=$(field_value "$1" "$2")
    [ -n "$value" ] ||
        fail "missing field '$2' in ${1#"$root/"}"
}

require_field_value()
{
    actual=$(field_value "$1" "$2")
    [ "$actual" = "$3" ] ||
        fail "expected '$2: $3' in ${1#"$root/"}, got '$actual'"
}

require_commit_hash()
{
    value=$(field_value "$1" "$2")
    printf '%s\n' "$value" | grep -Eq '^[0-9a-f]{40}$' ||
        fail "field '$2' is not a 40-character lowercase commit hash in ${1#"$root/"}"
}

section_value()
{
    awk -v heading="$2" '
        $0 == heading {
            found = 1
            next
        }
        found && /^## / {
            exit
        }
        found {
            print
        }
    ' "$1"
}

require_file "docs/workflow_scaffolding.md"
require_file "plan_modifications.md"
require_file ".githooks/post-commit"
require_file "tools/post_commit.sh"

for path in \
    src \
    tests \
    design/workbooks \
    generated/data \
    assets/original \
    performance/database \
    performance/graphs \
    performance/reports \
    verifier/issues/unfixed \
    verifier/issues/fixed \
    human_feedback/unfixed \
    human_feedback/fixed \
    optimizations/pending \
    optimizations/merged \
    optimizations/discarded \
    docs \
    release/output
do
    require_directory "$path"
done

for path in \
    design/workbooks/README.md \
    generated/data/README.md \
    assets/original/README.md \
    performance/README.md \
    performance/database/README.md \
    performance/graphs/README.md \
    performance/reports/README.md \
    verifier/README.md \
    verifier/issues/unfixed/README.md \
    verifier/issues/fixed/README.md \
    human_feedback/README.md \
    human_feedback/unfixed/README.md \
    human_feedback/fixed/README.md \
    optimizations/README.md \
    optimizations/pending/README.md \
    optimizations/merged/README.md \
    optimizations/discarded/README.md \
    release/README.md \
    release/output/README.md
do
    require_file "$path"
done

for template in \
    issue.md \
    human_feedback.md \
    optimization_analysis.md \
    decision_record.md \
    milestone_report.md \
    plan_modification.md
do
    require_file "docs/templates/$template"
    grep -Fq "docs/templates/$template" "$workflow_doc" ||
        fail "template is not documented: docs/templates/$template"
done

issue_template="$root/docs/templates/issue.md"
for heading in \
    "## Reproduction" \
    "## Expected behavior" \
    "## Observed behavior" \
    "## Evidence" \
    "## Resolution" \
    "## Fix verification"
do
    require_heading "$issue_template" "$heading"
done

feedback_template="$root/docs/templates/human_feedback.md"
for heading in \
    "## Feedback" \
    "## Expected experience" \
    "## Observed experience" \
    "## Evidence" \
    "## Resolution" \
    "## Verification"
do
    require_heading "$feedback_template" "$heading"
done

optimization_template="$root/docs/templates/optimization_analysis.md"
for heading in \
    "## Hypothesis" \
    "## Invariant contract" \
    "## Baseline" \
    "## Candidate change" \
    "## Benchmark protocol" \
    "## Results" \
    "## Correctness evidence" \
    "## Decision rationale"
do
    require_heading "$optimization_template" "$heading"
done

decision_template="$root/docs/templates/decision_record.md"
for heading in \
    "## Context" \
    "## Options considered" \
    "## Decision" \
    "## Consequences" \
    "## Evidence" \
    "## Replacement seam"
do
    require_heading "$decision_template" "$heading"
done

milestone_template="$root/docs/templates/milestone_report.md"
for heading in \
    "## Scope delivered" \
    "## Acceptance criteria" \
    "## Verification" \
    "## Performance evidence" \
    "## Unresolved issues" \
    "## Owner checkpoint" \
    "## Follow-up"
do
    require_heading "$milestone_template" "$heading"
done

plan_template="$root/docs/templates/plan_modification.md"
for heading in \
    "## Before" \
    "## After" \
    "## Reason" \
    "## Acceptance impact" \
    "## Evidence"
do
    require_heading "$plan_template" "$heading"
done

for sample in \
    plan_modification.md \
    verifier_issue_unfixed.md \
    verifier_issue_fixed.md \
    human_feedback_unfixed.md \
    human_feedback_fixed.md \
    optimization_analysis.md
do
    require_file "tests/fixtures/workflow/$sample"
    if grep -Eq '<[^>]+>' "$fixtures/$sample"; then
        fail "fixture contains an unresolved placeholder: tests/fixtures/workflow/$sample"
    fi
done

plan_sample="$fixtures/plan_modification.md"
require_field_value "$plan_sample" "Status" "accepted"
for field in "ID" "Date" "Governing plan section" "Owner approval"
do
    require_field "$plan_sample" "$field"
done
for heading in \
    "## Before" \
    "## After" \
    "## Reason" \
    "## Acceptance impact" \
    "## Evidence"
do
    require_heading "$plan_sample" "$heading"
done

issue_unfixed="$fixtures/verifier_issue_unfixed.md"
issue_fixed="$fixtures/verifier_issue_fixed.md"
for sample in "$issue_unfixed" "$issue_fixed"
do
    for field in \
        "ID" \
        "Status" \
        "Severity" \
        "Detected commit" \
        "Build hash" \
        "Content hash" \
        "Fixed commit"
    do
        require_field "$sample" "$field"
    done
    require_commit_hash "$sample" "Detected commit"
    for heading in \
        "## Reproduction" \
        "## Expected behavior" \
        "## Observed behavior" \
        "## Evidence" \
        "## Resolution" \
        "## Fix verification"
    do
        require_heading "$sample" "$heading"
    done
done
require_field_value "$issue_unfixed" "Status" "unfixed"
require_field_value "$issue_unfixed" "Fixed commit" "not-fixed"
require_field_value "$issue_fixed" "Status" "fixed"
require_commit_hash "$issue_fixed" "Fixed commit"
for field in "ID" "Severity" "Detected commit" "Build hash" "Content hash"
do
    before=$(field_value "$issue_unfixed" "$field")
    after=$(field_value "$issue_fixed" "$field")
    [ "$before" = "$after" ] ||
        fail "verifier lifecycle changed preserved field '$field'"
done
for heading in \
    "## Reproduction" \
    "## Expected behavior" \
    "## Observed behavior" \
    "## Evidence"
do
    before=$(section_value "$issue_unfixed" "$heading")
    after=$(section_value "$issue_fixed" "$heading")
    [ "$before" = "$after" ] ||
        fail "verifier lifecycle changed preserved section '$heading'"
done

feedback_unfixed="$fixtures/human_feedback_unfixed.md"
feedback_fixed="$fixtures/human_feedback_fixed.md"
for sample in "$feedback_unfixed" "$feedback_fixed"
do
    for field in "ID" "Status" "Reported" "Affected commit" "Fixed commit"
    do
        require_field "$sample" "$field"
    done
    require_commit_hash "$sample" "Affected commit"
    for heading in \
        "## Feedback" \
        "## Expected experience" \
        "## Observed experience" \
        "## Evidence" \
        "## Resolution" \
        "## Verification"
    do
        require_heading "$sample" "$heading"
    done
done
require_field_value "$feedback_unfixed" "Status" "unfixed"
require_field_value "$feedback_unfixed" "Fixed commit" "not-fixed"
require_field_value "$feedback_fixed" "Status" "fixed"
require_commit_hash "$feedback_fixed" "Fixed commit"
for field in "ID" "Reported" "Affected commit"
do
    before=$(field_value "$feedback_unfixed" "$field")
    after=$(field_value "$feedback_fixed" "$field")
    [ "$before" = "$after" ] ||
        fail "human-feedback lifecycle changed preserved field '$field'"
done
for heading in \
    "## Feedback" \
    "## Expected experience" \
    "## Observed experience" \
    "## Evidence"
do
    before=$(section_value "$feedback_unfixed" "$heading")
    after=$(section_value "$feedback_fixed" "$heading")
    [ "$before" = "$after" ] ||
        fail "human-feedback lifecycle changed preserved section '$heading'"
done

optimization_sample="$fixtures/optimization_analysis.md"
require_field_value "$optimization_sample" "Status" "pending"
require_field_value "$optimization_sample" "Decision" "undecided"
require_field "$optimization_sample" "ID"
require_commit_hash "$optimization_sample" "Base commit"
for heading in \
    "## Hypothesis" \
    "## Invariant contract" \
    "## Baseline" \
    "## Candidate change" \
    "## Benchmark protocol" \
    "## Results" \
    "## Correctness evidence" \
    "## Decision rationale"
do
    require_heading "$optimization_sample" "$heading"
done

git -C "$root" check-ignore -q \
    performance/local/commits/sample/manifest.txt ||
    fail "performance/local is not ignored"

tracked_local=$(git -C "$root" ls-files -- performance/local)
[ -z "$tracked_local" ] ||
    fail "generated performance/local evidence is tracked"

echo "M1 workflow verification passed"

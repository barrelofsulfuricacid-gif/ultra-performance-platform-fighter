# Repository and evidence workflow

This document defines the M1 execution scaffold. The canonical automated check
is `tools/verify_m1_workflow.sh`.

## Required locations

| Location | Purpose |
|---|---|
| `src/` | Authored runtime and tool source |
| `tests/` | Contract, conformance, regression, and lifecycle tests |
| `design/workbooks/` | Authoritative design workbooks introduced in M5 |
| `generated/data/` | Validated packed data derived from workbooks |
| `assets/original/` | Original assets with provenance records |
| `performance/database/` | Performance schemas, migrations, and approved snapshots |
| `performance/graphs/` | Reproducible milestone graph exports |
| `performance/profiles/` | Profile capture instructions and approved analyses |
| `performance/reports/` | Milestone and optimization performance reports |
| `verifier/issues/unfixed/` | Open verifier findings |
| `verifier/issues/fixed/` | Preserved resolved verifier findings |
| `human_feedback/unfixed/` | Open owner or playtester feedback |
| `human_feedback/fixed/` | Preserved resolved human feedback |
| `optimizations/pending/` | Proposed or active measured optimizations |
| `optimizations/merged/` | Accepted optimizations with evidence |
| `optimizations/discarded/` | Rejected experiments with retained evidence |
| `docs/` | Product, architecture, decision, milestone, and workflow records |
| `release/output/` | Local packaged release output; artifacts are ignored |

## Templates

- `docs/templates/issue.md`
- `docs/templates/human_feedback.md`
- `docs/templates/optimization_analysis.md`
- `docs/templates/decision_record.md`
- `docs/templates/milestone_report.md`
- `docs/templates/plan_modification.md`

Copy a template; do not edit the template in place. Replace every angle-bracket
placeholder. Stable metadata fields stay at the top so validation and later
report tooling do not need to infer state from prose.

## Lifecycles

A verifier issue or human-feedback record begins in its `unfixed/` directory.
The corrective code lands first. A following bookkeeping commit moves the
record to `fixed/`, preserves the original report and evidence, and records the
actual corrective commit hash.

An optimization begins in `pending/`. It moves to `merged/` only after its
correctness and comparable performance evidence pass. Failed, neutral, or
superseded experiments move to `discarded/`; their evidence is retained to
avoid repeating work.

Material execution-time changes to the governing plan are appended to
`plan_modifications.md` using the plan-modification template. Owner decisions
already resolved by the governing plan are not duplicated there.

## Generated evidence and recursion guard

The versioned post-commit command is `tools/post_commit.sh`, invoked by
`.githooks/post-commit`. It delegates to `tools/run_verifier.sh`, which selects
checks from the commit diff, runs the required global contracts and performance
jobs, and writes a pass manifest or one durable issue per failure. Per-commit
logs, raw measurements, temporary databases, graphs, captures, and build
products go under `performance/local/`, which is ignored by Git. Tracked
performance artifacts are explicit milestone exports, never automatic
post-commit output. This separation prevents generated benchmark data from
causing a new commit and recursively launching the workflow.

Valid sample records live under `tests/fixtures/workflow/`. Run:

```sh
./tools/verify_m1_workflow.sh
```

The command checks all required paths and templates, validates the sample
lifecycles, and proves the per-commit evidence path is ignored and untracked.

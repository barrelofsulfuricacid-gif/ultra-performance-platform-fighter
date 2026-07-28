# [VRF-2026-7001] Milestone same-commit baseline is excluded

ID: VRF-2026-7001
Status: fixed
Severity: high
Detected commit: ecc671fc5a77c7fff62d4ea6d01be66afb72bd85
Build hash: f0f8f22a7e9f892c90885eb0818106e1e92c092969cf88cdcd5b501dbd8b3589
Content hash: 1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526
Fixed commit: b285e0fe55e24e05d5be3aef3b6c96c706ccff9d

## Reproduction

On a clean checkout of the detected commit, run
`tools/run_performance.sh milestone performance/local/m3_milestone` twice.

## Expected behavior

The first 15-sample milestone measurement establishes the unchanged baseline.
The second otherwise compatible measurement compares against it so stability
and the environmental noise floor can be reviewed.

## Observed behavior

Both runs recorded nine `invalid` comparisons with reason
`prior-measurements-have-incompatible-metadata`. The run metadata differed
only in run ID and timestamps. The compatible-baseline query explicitly
excluded rows with the current commit hash, so a repeated unchanged baseline
could never be selected.

## Evidence

- `performance/local/performance.sqlite3`, runs 10 and 11
- `performance/local/m3_milestone/performance_manifest.txt`

## Resolution

Corrective commit `b285e0fe55e24e05d5be3aef3b6c96c706ccff9d`
allows any earlier compatible run to serve as the baseline, including a
repeated measurement of the same unchanged commit. The history qualification
now requires all nine measured scenarios to compare successfully in that
case.

## Fix verification

- The isolated history qualification passed with `same_commit=9`.
- The full 13-check verifier qualification passed with zero failures.
- Follow-up corrective commit
  `229f6f633b12740f9bbdbf966ccfd00e938d90d4` made that qualification
  repeatable against an already populated artifact root, and its post-commit
  gate passed.
- This bookkeeping commit preserves the original evidence after both
  corrective behaviors were verified.

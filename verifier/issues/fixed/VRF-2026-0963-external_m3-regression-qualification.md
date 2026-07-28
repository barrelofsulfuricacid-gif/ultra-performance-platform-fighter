# [VRF-2026-0963] external:m3-regression-qualification

ID: VRF-2026-0963
Status: fixed
Severity: high
Detected commit: b285e0fe55e24e05d5be3aef3b6c96c706ccff9d
Build hash: aa4a178d7cfaac82c9923167b59cd80e2125f09fe324b3502788c7bd32b48ff4
Content hash: 1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526
Fixed commit: 229f6f633b12740f9bbdbf966ccfd00e938d90d4

## Reproduction

Run `tools/run_verifier.sh b285e0fe55e24e05d5be3aef3b6c96c706ccff9d`
after a previous verifier invocation has populated the default
`build/verifier-artifacts/m3_performance_qualification` directory.

## Expected behavior

The synthetic history qualification is isolated and repeatable. Existing
generated artifacts from an earlier invocation cannot change its expected run
IDs or comparison counts.

## Observed behavior

The qualification reused its existing SQLite database. Its synthetic
baseline, same-commit, suspected, confirmed, and incompatible runs were
appended to prior fixtures, causing the expected comparison outcomes to change
and `m3-regression-qualification` to fail.

## Evidence

`performance/local/commits/b285e0fe55e24e05d5be3aef3b6c96c706ccff9d/verifier/checks/m3-regression-qualification.log`

## Resolution

Corrective commit `229f6f633b12740f9bbdbf966ccfd00e938d90d4`
creates a unique `run.XXXXXX` subdirectory for each synthetic qualification.
The qualification database, graphs, manifest, and log therefore always begin
from an empty isolated fixture while the durable verifier check log keeps its
stable per-commit path.

## Fix verification

- Two consecutive qualification invocations against the same output root both
  passed with `baseline=1`, `same_commit=9`, `suspected=9`, `confirmed=9`, and
  `invalid=9`.
- The full 13-check verifier passed using the previously populated default
  artifact root.
- The corrective commit's post-commit gate passed.
- This following bookkeeping commit moves the report only after the corrective
  commit passed, preserving the required two-commit issue lifecycle.

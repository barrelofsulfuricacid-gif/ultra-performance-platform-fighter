# [VRF-2026-0963] external:m3-regression-qualification

ID: VRF-2026-0963
Status: unfixed
Severity: high
Detected commit: b285e0fe55e24e05d5be3aef3b6c96c706ccff9d
Build hash: aa4a178d7cfaac82c9923167b59cd80e2125f09fe324b3502788c7bd32b48ff4
Content hash: 1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526
Fixed commit: not-fixed

## Reproduction

Run `tools/run_verifier.sh b285e0fe55e24e05d5be3aef3b6c96c706ccff9d` from the recorded commit.

## Expected behavior

The selected external check passes or is explicitly deferred by capability.

## Observed behavior

fail

## Evidence

performance/local/commits/b285e0fe55e24e05d5be3aef3b6c96c706ccff9d/verifier/checks/m3-regression-qualification.log

## Resolution

Not fixed.

## Fix verification

Pending a corrective commit and following bookkeeping commit.

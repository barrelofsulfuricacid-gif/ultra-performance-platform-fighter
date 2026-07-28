# [VRF-2026-6761] external:m1-foundation

ID: VRF-2026-6761
Status: fixed
Severity: high
Detected commit: 0646aac7cccf08577b7720b6e45b1b9dd551b74c
Build hash: aa4a178d7cfaac82c9923167b59cd80e2125f09fe324b3502788c7bd32b48ff4
Content hash: 1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526
Fixed commit: 7035e6a905824f2e99618682f0eb8a3c4ef1de61

## Reproduction

Run `tools/run_verifier.sh 0646aac7cccf08577b7720b6e45b1b9dd551b74c`
from the recorded commit.

## Expected behavior

The selected external check passes or is explicitly deferred by capability.

## Observed behavior

The M1 foundation check failed because its generated `pf_verifier` binary was
created without execute permission inside the nested verifier report tree.
CTest reported `Permission denied`; the M1 source and tests themselves were
unchanged.

## Evidence

`performance/local/commits/0646aac7cccf08577b7720b6e45b1b9dd551b74c/verifier/checks/m1-foundation.log`

## Resolution

Corrective commit `7035e6a905824f2e99618682f0eb8a3c4ef1de61` keeps durable
verifier reports under `verifier/checks` and moves generated build and test
artifacts into the sibling `artifacts` directory. This avoids the
workspace-specific executable-mode loss while preserving the report layout.

## Fix verification

- The corrective commit's post-commit gate passed all 13 selected checks with
  zero failures.
- The integrated verifier qualification passed its internal checks, all four
  seeded-defect detectors, the single-issue lifecycle test, and the missing
  acceptance-coverage test.
- This following bookkeeping commit moves the report only after the corrective
  commit passed, preserving the required two-commit issue lifecycle.

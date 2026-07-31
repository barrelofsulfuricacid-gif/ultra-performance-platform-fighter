# [VRF-2026-6813] external:m4-combat

ID: VRF-2026-6813
Status: unfixed
Severity: high
Detected commit: 54ced17d3a38547a5b53dda0cf0afbfac8a6e891
Build hash: 52bbf3010ff58ebfe56be899d203061162ceca97b77de11ab4790a7dd8aeb620
Content hash: 1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526
Fixed commit: not-fixed

## Reproduction

Run `tools/run_verifier.sh 54ced17d3a38547a5b53dda0cf0afbfac8a6e891` from the recorded commit.

## Expected behavior

The selected external check passes or is explicitly deferred by capability.

## Observed behavior

fail

## Evidence

performance/local/commits/54ced17d3a38547a5b53dda0cf0afbfac8a6e891/verifier/checks/m4-combat.log

## Resolution

Not fixed.

## Fix verification

Pending a corrective commit and following bookkeeping commit.

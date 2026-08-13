# [VRF-2026-6813] external:m4-combat

ID: VRF-2026-6813
Status: fixed
Severity: high
Detected commit: 54ced17d3a38547a5b53dda0cf0afbfac8a6e891
Build hash: 52bbf3010ff58ebfe56be899d203061162ceca97b77de11ab4790a7dd8aeb620
Content hash: 1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526
Fixed commit: dcfdafacb42824d2a733bbb5aab6570163fcc962

## Reproduction

Run `tools/run_verifier.sh 54ced17d3a38547a5b53dda0cf0afbfac8a6e891` from the recorded commit.

## Expected behavior

The selected external check passes or is explicitly deferred by capability.

## Observed behavior

The `m4-combat` external check ran the technique-registry verifier after the
dashing-shield row changed from `planned` to `playable`. Its AWK assertion
still expected 36 planned and 17 playable rows, so it rejected the correct new
counts of 35 planned and 18 playable even though the native combat and browser
tests passed.

## Evidence

`performance/local/commits/54ced17d3a38547a5b53dda0cf0afbfac8a6e891/verifier/checks/m4-combat.log`

## Resolution

Corrective commit `dcfdafacb42824d2a733bbb5aab6570163fcc962` updates the
shell verifier's internal expected counts to the same 35 planned and 18
playable contract already enforced by CMake and printed by the registry
summary.

## Fix verification

- The corrective commit's WSL post-commit gate passed all 16 selected checks
  with zero failures, including `external:m4-combat` and the 61-row registry.
- The affected feature commit separately passed the 18-test sanitizer suite,
  the browser adapter with `dashing_shield_probe=1`, and benchmark run 6 with
  nine available scenarios, four capability-unavailable scenarios, and zero
  suspected or confirmed regressions.
- This following bookkeeping commit moves the report only after the corrective
  commit passed, preserving the required two-commit issue lifecycle.

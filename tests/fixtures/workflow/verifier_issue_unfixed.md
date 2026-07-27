# [VRF-SAMPLE-0001] Synthetic state-hash mismatch

ID: VRF-SAMPLE-0001
Status: unfixed
Severity: high
Detected commit: 1111111111111111111111111111111111111111
Build hash: sample-build-001
Content hash: sample-content-001
Fixed commit: not-fixed

## Reproduction

Run the synthetic replay with seed 7 and compare the state hash at tick 60.

## Expected behavior

The replay and direct simulation hashes match.

## Observed behavior

The synthetic hashes differ at tick 60.

## Evidence

Fixture replay `sample-replay-001` and state diff `sample-diff-001`.

## Resolution

Not fixed.

## Fix verification

Not fixed.

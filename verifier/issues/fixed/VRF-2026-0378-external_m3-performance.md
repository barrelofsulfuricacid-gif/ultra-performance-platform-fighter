# [VRF-2026-0378] external:m3-performance

ID: VRF-2026-0378
Status: fixed
Severity: high
Detected commit: 8eef6f1b46393c5e9c9828e32b53819e8aeae156
Build hash: aa4a178d7cfaac82c9923167b59cd80e2125f09fe324b3502788c7bd32b48ff4
Content hash: 1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526
Fixed commit: ca7d7f17128b4918dc4012879b91db8b248f64a6

## Reproduction

Run `tools/run_verifier.sh 8eef6f1b46393c5e9c9828e32b53819e8aeae156`
from the recorded commit.

## Expected behavior

The selected external check passes or is explicitly deferred by capability.
A five-sample commit-mode suspicion remains visible but only a fifteen-sample
milestone run can confirm a regression and block the milestone.

## Observed behavior

The commit benchmark detected an `empty_tick` median decrease of 1.861% against
a 1.827% meaningful-change threshold. It recorded one suspected regression
and zero confirmed regressions, but returned a hard-failure status, causing the
verifier to report `external:m3-performance` as failed.

## Evidence

`performance/local/commits/8eef6f1b46393c5e9c9828e32b53819e8aeae156/verifier/checks/m3-performance.log`

## Resolution

Corrective commit `ca7d7f17128b4918dc4012879b91db8b248f64a6`
separates the benchmark process status into `pass`, `suspected`, and
`regression`. Suspected commit-mode results stay in SQLite, the manifest,
graphs, and verifier log but return success. Only a milestone-confirmed
regression returns the blocking exit code. The benchmark self-test now covers
all three process-status outcomes.

## Fix verification

- The corrective commit's post-commit gate passed all 13 selected checks with
  zero failures.
- Its real commit benchmark reported `benchmarks=suspected`,
  `suspected_regressions=1`, and `confirmed_regressions=0`; the verifier
  correctly recorded `m3-performance` as passed without erasing the signal.
- A clean fifteen-sample milestone measurement remains required for the M3
  owner checkpoint.
- This following bookkeeping commit moves the report only after the corrective
  commit passed, preserving the required two-commit issue lifecycle.

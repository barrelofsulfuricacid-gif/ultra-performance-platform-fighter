# M4 combat profile capture

Tracy 0.13.1 capture: **pass**. The canonical profile workload entered its
instrumented scenario zones and emitted frame marks while exercising all 13
scenario slots after the complete action-transition journal and its
scratch-mask hot-path optimization.

- Commit: `58d9e5487c062242add32a3330d85f5b540d3e9d`
- Dirty tree: `false`
- Profile workload: `pass`
- Measured scenarios: 10
- Explicitly unavailable scenarios: 3
- Target per sample: 100 ms
- Repetitions: 15
- Trace SHA-256:
  `a5a8879cc6d299eb16dd99dfa2a4b178250ab053290f5145cfd8a3e801c772d3`
- Trace size: 11,808 bytes
- Profile binary SHA-256:
  `d9c01d5eea9c1e6e2d3fd9bf6bbba555b0613205ce181c7f14360766b03a43c0`
- Tracy timer fallback: `ON`
- Platform-profiler claim: none (`perf` was not installed in the WSL image)

WSL did not expose the timer capability used by Tracy's default configuration.
The profile-only build therefore used Tracy's supported timer fallback;
release, benchmark, and maximum-throughput headless products remain
uninstrumented and do not inherit this policy.

The trace qualifies the production M4 benchmark workload and capture path. It
does not establish a machine-independent throughput claim. The scratch-mask
optimization is accepted by the two unsampled Windows milestone distributions,
not by instrumented timing. The raw trace, capture log, workload log, manifest,
and capture tool remain local to the clean measuring worktree.

See the [M4 combat performance checkpoint](../../reports/2026-08-01_m4_combat.md)
for the repeated unsampled benchmark distributions. Those measurements, not
the instrumented trace, are the authoritative throughput evidence.

# M4 combat profile capture

Tracy 0.13.1 capture: **pass**. The canonical profile workload entered its
instrumented scenario zones and emitted frame marks while exercising all 13
scenario slots after the stationary upper-platform slice.

- Commit: `39bb0c4590b8d000f73b69996ecbde14f2b62892`
- Dirty tree: `false`
- Profile workload: `pass`
- Measured scenarios: 10
- Explicitly unavailable scenarios: 3
- Target per sample: 100 ms
- Repetitions: 15
- Captured span: 6.1 s
- Captured frames: 3
- Captured zones: 2
- Trace SHA-256:
  `e7d561777aa10bce2d84c4288f293bb2367bb0828b9d9a669851818dce9d6be8`
- Trace size: 11,776 bytes
- Profile binary SHA-256:
  `6bc710ee3d417a586937af25666349ea5c2084eb5e38476cc2e6a0268b330ba2`
- Tracy timer fallback: `ON`
- Platform-profiler claim: none (`perf` was not installed in the WSL image)

WSL did not expose the timer capability used by Tracy's default configuration.
The profile-only build therefore used Tracy's supported timer fallback;
release, benchmark, and maximum-throughput headless products remain
uninstrumented and do not inherit this policy.

The trace qualifies the production M4 benchmark workload and capture path. It
does not establish a machine-independent throughput claim or attribute a new
optimization. The raw trace, capture log, workload log, manifest, and capture
tool remain local to the clean measuring worktree.

See the [M4 combat performance checkpoint](../../reports/2026-08-01_m4_combat.md)
for the repeated unsampled benchmark distributions. Those measurements, not
the instrumented trace, are the authoritative throughput evidence.

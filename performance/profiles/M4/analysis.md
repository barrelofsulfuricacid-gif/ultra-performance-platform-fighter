# M4 combat profile capture

Tracy 0.13.1 capture: **pass**. The canonical profile workload entered its
instrumented scenario zones and emitted frame marks while exercising all 13
scenario slots after the prone-orientation and packed-state slice.

- Commit: `91d69f5f03a3a6205011af61f99c3d23c6d88f6d`
- Dirty tree: `false`
- Profile workload: `pass`
- Measured scenarios: 10
- Explicitly unavailable scenarios: 3
- Target per sample: 100 ms
- Repetitions: 15
- Trace SHA-256:
  `5a720f86d7ece6c609d71a7864322a418456c60c544f5af2dc289cbcc6cebe4f`
- Trace size: 11,814 bytes
- Profile binary SHA-256:
  `df35269387c0c62e8505c8cf7fb6a14033126fe9c4dc2dfec482abbb69ac5c0b`
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

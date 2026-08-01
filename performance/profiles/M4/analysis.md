# M4 combat profile capture

Tracy 0.13.1 capture: **pass**. The canonical profile workload entered its
instrumented scenario zones and emitted frame marks while exercising all 13
scenario slots.

- Commit: `1df69239f54f45be4d64270fa2eacb1332d803b3`
- Dirty tree: `false`
- Profile workload: `pass`
- Measured scenarios: 10
- Explicitly unavailable scenarios: 3
- Target per sample: 100 ms
- Repetitions: 15
- Captured span: 6.1 s
- Captured frames: 4
- Captured zones: 3
- Trace SHA-256:
  `59b474890f34a94470e847c02d96e160667507b254b70be4503e26ac0f6dd3d4`
- Trace size: 11,819 bytes
- Tracy timer fallback: `ON`
- Platform-profiler claim: none (`perf` was not installed in the WSL image)

WSL did not expose the invariant-TSC capability required by Tracy's default
timer. The profile-only build therefore used Tracy's timer fallback; release,
benchmark, and maximum-throughput headless products remain uninstrumented and
do not inherit this policy.

The trace qualifies the production M4 benchmark workload and capture path. It
does not establish a machine-independent throughput claim or attribute a new
optimization. The raw trace, capture log, workload log, manifest, exact machine
fingerprint, and capture-tool build remain under
`performance/local/profiles/M4-combat-1df6923/` in the measuring worktree.

See the [M4 combat performance checkpoint](../../reports/2026-08-01_m4_combat.md)
for the repeated unsampled benchmark distributions. Those measurements, not
the instrumented trace, are the authoritative throughput evidence.

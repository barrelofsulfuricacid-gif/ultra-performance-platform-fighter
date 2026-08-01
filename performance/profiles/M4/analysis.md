# M4 combat profile capture

Tracy 0.13.1 capture: **pass**. The canonical profile workload entered its
instrumented scenario zones and emitted frame marks while exercising all 13
scenario slots.

- Commit: `08c927a618d42e6f355383937aca026a1c8e566c`
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
  `82374094bab9f4c79695b6d628601f10862d8318693ff261f84c86f24dd88967`
- Trace size: 11,777 bytes
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
`performance/local/profiles/M4-shield-geometry-08c927a/` in the measuring
worktree.

See the [M4 combat performance checkpoint](../../reports/2026-08-01_m4_combat.md)
for the repeated unsampled benchmark distributions. Those measurements, not
the instrumented trace, are the authoritative throughput evidence.

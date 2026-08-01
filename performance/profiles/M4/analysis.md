# M4 combat profile capture

Tracy 0.13.1 capture: **pass**. The canonical profile workload entered its
instrumented scenario zones and emitted frame marks while exercising all 13
scenario slots.

- Commit: `fecd6ac9c03f05145fc00dd2ed873dad602c3d60`
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
  `79c40233d75676750bba213579d6cb679dbb347c414b7ace8805af3565d4aa74`
- Trace size: 11,791 bytes
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
`performance/local/profiles/M4-revival-fecd6ac/` in the measuring
worktree.

See the [M4 combat performance checkpoint](../../reports/2026-08-01_m4_combat.md)
for the repeated unsampled benchmark distributions. Those measurements, not
the instrumented trace, are the authoritative throughput evidence.

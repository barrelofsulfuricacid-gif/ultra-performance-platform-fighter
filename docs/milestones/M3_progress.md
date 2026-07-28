# M3 performance and verifier progress

**Status:** Implementation qualification passed; clean-commit evidence and
owner checkpoint pending

## Performance history

M3 replaces the provisional benchmark smoke product with an authored-C runner
and a versioned SQLite history schema. The runner calibrates each available
scenario, performs a warmup, stores every raw repetition, calculates median,
MAD, p50, p95, and p99, and regenerates one SVG evolution graph per canonical
scenario.

| Scenario | M3 availability | Unit or reason |
|---|---|---|
| Empty tick | Measured | Logical ticks/s |
| Representative 1v1 | Measured | Logical ticks/s |
| Representative 2v2 | Measured | Logical ticks/s |
| Maximum combat entities | Deferred to M4 | Combat entities do not exist yet |
| Hazard-heavy four-player | Deferred to M6 | Hazard framework does not exist yet |
| Snapshot save | Measured | Operations/s |
| Snapshot restore | Measured | Operations/s |
| Rollback re-simulation depth 8 | Measured | Logical ticks/s |
| Replay verification | Measured | Logical ticks/s |
| Single-environment RL calls | Measured | Environment ticks/s |
| Batched RL step | Measured | Environment ticks/s |
| Design-data import | Deferred to M5 | Workbook/pack import does not exist yet |
| Client frame | Deferred to M7 | Representative client rendering does not exist yet |

Commit runs use five calibrated repetitions targeting 20 ms per sample.
Milestone runs use fifteen repetitions targeting 100 ms. A commit regression
is suspected only beyond the larger of 1% or three measured MAD ratios. A
milestone regression additionally requires a deterministic 2,000-round
bootstrap 95% interval below zero.

Comparison keys include dirty state, run mode, build configuration, full
compiler command, dependency and content hashes, executable hash, machine,
OS, CPU, power/thermal metadata, benchmark schema, and scenario identity.
Dirty measurements and metadata mismatches are stored but explicitly marked
invalid. The qualification corpus proves baseline, compatible, suspected,
confirmed, and invalid paths; it deliberately detects nine regressions and
nine incompatible comparisons before ending with a clean fixture.

SQLite 3.53.4 is checksum-locked and linked only to `pf_benchmarks`.
Per-commit databases and graphs remain under ignored `performance/local/`.
The maximum-throughput headless target is checked for the absence of SQLite
and Tracy symbols.

## Profile boundary

The `profile` configuration enables the pinned Tracy 0.13.1 C++ client only
for the benchmark executable. C-authored benchmark scenario zones and frame
marks compile out when `PF_TRACY_ENABLED=0`. Tracy's command-line capture
utility is built locally from checksum-locked Tracy, Capstone, PPQSort, and
Zstd sources; no generic build-time dependency fetch is used.

The M3 qualification capture produced a valid trace with scenario zones.
Linux `perf` is not installed in the current container, so the capture
manifest records `os_profiler=unavailable` and
`os_profiler_reason=tool-not-installed`; it makes no hardware-counter claim.
A final clean-commit milestone capture remains part of checkpoint packaging.

## Verifier agent

The authored-C verifier reads `verifier/acceptance_manifest.tsv`, the exact
commit file list, and an external check manifest. It drives two-player
exploratory traces and four-player snapshot continuation through
`pf_rl_reset`/`pf_rl_step`, compares exact transitions and hashes, confirms
that policy observations redact the seed while diagnostic observation retains
it, and performs a tolerant semantic render-packet comparison.

The post-commit workflow selects sanitizer and browser checks from the diff
while always running the product contract, foundation/workflow/setup checks,
kernel and replay verification, performance-history qualification, and a real
commit benchmark. Every active acceptance row must have an internal or
external result. Browser execution may be explicitly deferred to its clean
Chrome CI lane; a deferred result is preserved as `deferred`, not rewritten as
a pass.

Every run writes `pass_manifest.md`. Each failed check creates one stable
Markdown issue under `verifier/issues/unfixed/` with severity, detecting
commit, build/content hashes, reproduction, expected/observed behavior, and
evidence. Existing issue evidence is never overwritten.

Qualification proves:

- deterministic mechanical-oracle mismatch detection;
- tolerant visual mismatch detection;
- unreachable menu-state detection;
- state-hash determinism mismatch detection;
- failure when an active acceptance entry lacks a result; and
- one issue file for one seeded external failure.

Menu navigation, controller prompts, collision/hitbox overlays, and screenshot
references are explicit planned acceptance capabilities for M4/M7 rather than
false M3 passes.

## Local verification

- Benchmark workflow: 7/7 tests.
- Verifier workflow: 7/7 tests.
- Release/profile/sanitizer workflows: 13/13 tests.
- Headless workflow: 8/8 tests.
- Performance qualification: 13 graphs, schema 1, regression and
  incompatibility fixtures passed.
- Verifier qualification: three live internal invariants, four seeded defect
  classes, issue lifecycle, and acceptance coverage passed.
- Full combined verifier workflow: 13 checks, zero failures.
- Tracy qualification capture: valid trace; OS profiler explicitly
  unavailable.

The Work Mode container requires LeakSanitizer discovery to be disabled
because traced `/proc` task reads are denied. AddressSanitizer and
UndefinedBehaviorSanitizer still run all 13 tests. The GitHub sanitizer lane
does not set that override and retains leak discovery.

## Remaining checkpoint work

- Commit the M3 implementation and let the post-commit hook measure the clean
  commit.
- Capture and summarize the clean M3 milestone performance/profile evidence.
- Pass the clean-machine GitHub matrix.
- Present benchmark stability, graphs, profile status, and verifier
  qualification for the mandatory owner checkpoint.

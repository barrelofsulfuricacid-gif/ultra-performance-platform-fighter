# M3 checkpoint report

**Status:** Qualified; owner approval required before M4

**Checkpoint build:** `55619230599dddff2833bfc6e90e1bf6172e166c`

## Scope delivered

M3 establishes the persistent performance and automated verifier foundation:

- An authored-C benchmark runner with calibrated commit and milestone modes,
  raw samples, median/MAD, p50/p95/p99, and 13 canonical scenarios.
- A versioned SQLite history with compatibility metadata, invalid-comparison
  detection, commit suspicions, milestone-confirmed regressions, and one SVG
  evolution graph per canonical scenario.
- Nine currently measurable scenarios and four explicit machine-readable
  capability deferrals.
- Checksum-locked SQLite 3.53.4 and Tracy 0.13.1 tooling, with Tracy isolated
  to profile builds and SQLite isolated to the benchmark target.
- A maximum-throughput headless target verified to contain neither Tracy nor
  SQLite instrumentation.
- An authored-C verifier that reads the acceptance manifest and exact commit
  diff, drives public player/RL action APIs, checks deterministic state and
  snapshot continuation, performs tolerant render semantics, selects external
  checks, and preserves one durable report per discovered issue.
- Qualification fixtures that deliberately detect mechanical, visual, menu,
  determinism, acceptance-coverage, regression, and metadata-compatibility
  defects.

## Acceptance criteria

| Criterion | Result | Evidence |
|---|---|---|
| Every commit is measured or records an explicit reason | Pass | Post-commit workflow and SQLite history |
| Raw samples and complete compatibility metadata are stored | Pass | Versioned schema 1 |
| Every canonical scenario has an evolution graph | Pass | 13-graph milestone snapshot |
| Changed hardware/content/compiler/scenario comparisons are invalidated | Pass | Nine seeded incompatible comparisons |
| Commit suspicion and milestone confirmation are distinct | Pass | Process-policy self-test and regression qualification |
| Repeated unchanged-commit baselines compare successfully | Pass | `same_commit=9` qualification |
| Final headless build excludes history/profiling code | Pass | Symbol-boundary verification |
| Verifier always writes a pass/fail manifest | Pass | 13-check post-commit manifests |
| Seeded mechanical, visual, menu, and determinism defects are detected | Pass | Verifier qualification |
| Unfixed critical issues block completion | Pass | Issue lifecycle and post-commit gate |

## Performance checkpoint

Two clean 15-sample milestone runs on the final code produced nine compatible
comparisons, zero invalid comparisons, and zero confirmed regressions. The
second run compared directly with the first unchanged-commit run. Full
medians, thresholds, confidence intervals, unavailable-reason records, and
methodology are in
[the M3 performance report](../../performance/reports/2026-07-27_m3_checkpoint.md).

The committed [graph snapshot](../../performance/reports/2026-07-27_m3_graphs/index.md)
shows the compatible per-commit evolution series. It is review evidence, not
an absolute TPS release claim.

## Verifier checkpoint

The final post-commit manifest reports:

- status `pass`;
- 13 checks and zero failures;
- 10 active acceptance entries and 5 planned entries;
- exact RL-action determinism, four-player snapshot continuation, and tolerant
  render-packet semantics;
- M0, M1, M2, performance-history, and real benchmark external checks passed;
  and
- no unfixed issue.

The qualification separately proves all four seeded defect classes, one
issue per failed check, missing-active-acceptance failure, and non-overwriting
issue evidence. All verifier defects found while preparing this checkpoint
have completed the required corrective-commit plus bookkeeping-commit
lifecycle and remain under `verifier/issues/fixed/`.

## Profile checkpoint

The final clean Tracy 0.13.1 capture passed with canonical scenario zones and
frame marks. Exact trace, environment, and platform-profiler metadata remain
in the local evidence store; no hardware-counter claim is made. See
[the profile analysis](../../performance/profiles/M3/analysis.md).

## Verification

- Release and profile workflows: 13/13 tests.
- Sanitizer workflow: 13/13 tests; the normal clean-machine CI lane retains
  LeakSanitizer while this restricted workspace disables leak discovery.
- Benchmark and verifier workflows: 7/7 tests each.
- Headless workflow: 8/8 tests.
- Native-to-WebAssembly replay verification: exact 180-tick match.
- Performance qualification: 13 graphs; baseline, same-commit, suspected,
  confirmed, and invalid paths passed.
- Verifier qualification: 3 live invariants, 4 seeded defect classes, issue
  lifecycle, and acceptance coverage passed.
- Final integrated post-commit verifier: 13 checks, zero failures.

M3 does not change browser presentation code, so the diff-selected browser
check is explicitly delegated to the repository's clean Chrome CI lane.

## Unresolved issues

No unfixed verifier issue blocks M3. Four benchmark scenarios and five
verifier capabilities are explicitly planned for the milestone in which their
underlying game/client systems first exist; none is represented as a false
pass.

## Owner checkpoint

The governing plan requires owner review of benchmark stability, graphs, and
verifier qualification before M4 begins.

- **A — Approve M3 and proceed to M4.**
- **B — Request changes before M4.**

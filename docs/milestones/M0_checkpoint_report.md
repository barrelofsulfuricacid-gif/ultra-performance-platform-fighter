# M0 checkpoint report

**Status:** Engineering work complete; human representation checkpoint open.

**Date:** 2026-07-27

**Benchmark commit:** `9e757df3d4c222ee55d100bd2db7572b94c748fb`

## Outcome

M0 now contains:

- A Melee-feel system contract and human playtest rubric.
- One original mechanical coverage row for all 26 SSBM fighter/forms.
- Ten original stage briefs covering every required theme.
- Originality, clean-reference, and asset provenance rules.
- A relative-only performance charter implementing D3-C.
- Reproducible C microkernels for 23 representation candidates.
- Raw clean-tree milestone samples, environment metadata, correctness
  diagnostics, and generated bootstrap analysis.
- Deterministic system, state, event, replay, content-pack, and API boundaries.
- Authored-C/foreign-C++ ABI rules implementing D2-A.
- Current first-spike dependency pins, licenses, target roles, replacement
  seams, and adoption gates.

No external game assets or third-party game implementation data are present.

## Verification

| Check | Result | Evidence |
|---|---|---|
| Required product matrices | Pass | `tools/verify_m0.sh` |
| 26 fighter/form coverage rows | Pass | `docs/product/roster_coverage_matrix.md` |
| 10 stage briefs | Pass | `docs/product/stage_briefs.md` |
| C17 warnings as errors | Pass | GCC 13.3.0 |
| Benchmark self-tests | Pass | `performance/m0_representation/diagnostics.txt` |
| Milestone samples | Pass | 23 cases × 15 rounds = 345 rows |
| Clean measured tree | Pass | `dirty=false` in benchmark metadata |
| CPU affinity | Pass | Pinned to CPU 0 |
| ASan/UBSan smoke | Pass | `tools/verify_m0_sanitized.sh` |
| LeakSanitizer | Environment exception | Work Mode container cannot enumerate ptraced threads; M1 CI restores it |
| Hardware counters | Unavailable | `perf=unavailable` recorded |

## Representation result

The measured candidate is:

- Q16.16 deterministic motion/geometry.
- High-resolution authored world coordinates rather than a universal 256-cell
  world.
- Structure-of-arrays hot pools with separate cold state.
- Uniform-grid broadphase plus exact narrow phase.
- Data tables for common move math with explicit C transitions.
- Mutation-tracked snapshot chunks checked against a full-copy oracle.

Key evidence:

- Q16.16 motion: 1.410× float32, 95% CI [1.389, 1.440], equal tested motion
  state bytes.
- SoA update: 7.076× AoS-with-cold, 95% CI [6.966, 7.193].
- Dense uniform grid: 2.678× naive, 95% CI [2.556, 2.764].
- Data-table dispatch: 2.893× switch, 95% CI [2.752, 2.937].
- Tracked sparse dirty snapshot: 83.812× full 64 KiB copy, 95% CI
  [83.178, 84.259].

These are isolated microkernel ratios on one virtualized compatibility key, not
final-engine or machine-independent claims. The complete Pareto reasoning and
reconsideration triggers are in
`docs/architecture/representation_decision.md`.

## Open human checkpoint

M0 requires a human comparison of leading quantized and higher-precision
movement prototypes. The current experiments are headless kernels and cannot
satisfy that subjective test.

The owner must choose one path:

1. Keep M0 open and build a disposable side-by-side SDL3 movement playtest.
2. Explicitly defer the playtest to the first playable vertical slice, approve
   Q16.16 provisionally, and record the exception in `plan_modifications.md`.
3. Select float32 provisionally, accepting its lower measured throughput and
   higher cross-target determinism burden.

No M1 implementation begins until one path is selected.

## Source research

Technical dependency evidence uses primary project documentation linked from
`docs/technology_decisions/`. Originality guidance is linked from
`docs/product/originality_and_provenance.md`. This report is engineering
documentation and not legal advice; D1-A still requires formal IP/originality
review before public release.

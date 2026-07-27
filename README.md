# Ultra-Performance Platform Fighter

An original deterministic 2D platform fighter, authored in C and designed for
native, web, rollback, headless, and reinforcement-learning targets.

The project is currently at **M0: product contract and measured architecture
decisions**. M0 contains disposable C experiments used to choose
representations before permanent engine implementation begins. The measured
candidate and its remaining human checkpoint are summarized in
[`docs/milestones/M0_checkpoint_report.md`](docs/milestones/M0_checkpoint_report.md).

## Verify the checkpoint

Requirements: a POSIX shell, Git, GCC with C17 support, and standard Unix
utilities.

```sh
tools/verify_m0.sh
tools/verify_m0_sanitized.sh
```

The milestone benchmark evidence is preserved in
[`performance/m0_representation/`](performance/m0_representation/). Results
are relative measurements from one virtualized compatibility key, not
machine-independent performance claims.

## Repository guide

- [`docs/product/`](docs/product/) — gameplay feel, roster coverage, stage,
  and originality contracts.
- [`docs/architecture/`](docs/architecture/) — deterministic boundaries,
  data/API contracts, and the proposed representation decision.
- [`docs/technology_decisions/`](docs/technology_decisions/) — authored-C and
  dependency decisions with current pins and licenses.
- [`experiments/m0_representation/`](experiments/m0_representation/) —
  reproducible C microbenchmarks and analysis.
- [`docs/plan_reference.md`](docs/plan_reference.md) — governing plan identity
  and owner choices D1-A through D6-A.

## Commit workflow

Every commit invokes `.githooks/post-commit`, which:

1. Runs the provisional M0 verifier.
2. Runs the M0 performance harness when it exists.
3. Stores local evidence under `performance/local/commits/<commit>/`.

The M3 verifier will replace this provisional gate with full mechanical,
visual, replay, and performance verification.

## Originality and licensing

No external game assets or third-party game implementation data are included.
Comparative references identify gameplay coverage only; the planned
characters, stages, art, audio, names, and presentation are original.

No project license has been granted yet. Public visibility permits observation
of this repository but does not grant reuse rights. Referenced third-party
projects retain their own licenses and are not vendored here.

# Ultra-Performance Platform Fighter

An original deterministic 2D platform fighter, authored in C and designed for
native, web, rollback, headless, and reinforcement-learning targets.

The project has completed **M0: product contract and measured architecture
decisions** and is entering **M1: reproducible foundation**. M0 selected
Q16.16 deterministic motion and geometry after benchmark, verifier, and blind
human-playtest evidence. The accepted decision is summarized in
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

## Build the current M1 foundation

The permanent authored-C `sim` library and renderer-free `headless` smoke
product require the pinned CMake 4.4 toolchain:

```sh
cmake -S . -B build/m1 -DCMAKE_BUILD_TYPE=Release
cmake --build build/m1 --parallel
ctest --test-dir build/m1 --output-on-failure
build/m1/headless --smoke
```

The bootstrap and preset layer is still active M1 work. Current progress and
remaining acceptance items are tracked in
[`docs/milestones/M1_progress.md`](docs/milestones/M1_progress.md).

## Reproduce the M0 movement playtest

The completed M0 gate used a blind human comparison of float32 and Q16.16
movement. The archived pure-C models, SDL3 client, build instructions,
controls, and protocol remain in
[`experiments/m0_playtest/`](experiments/m0_playtest/).

```sh
cmake -S experiments/m0_playtest -B build/m0_playtest
cmake --build build/m0_playtest --config Release
ctest --test-dir build/m0_playtest --output-on-failure
```

Use
[`docs/milestones/M0_playtest_worksheet.md`](docs/milestones/M0_playtest_worksheet.md)
to reproduce the comparison. The owner reported no perceptible difference and
approved Q16.16 on 2026-07-27; M1 is unblocked.

## Repository guide

- [`docs/product/`](docs/product/) — gameplay feel, roster coverage, stage,
  and originality contracts.
- [`docs/architecture/`](docs/architecture/) — deterministic boundaries,
  data/API contracts, and the accepted representation decision.
- [`docs/technology_decisions/`](docs/technology_decisions/) — authored-C and
  dependency decisions with current pins and licenses.
- [`experiments/m0_representation/`](experiments/m0_representation/) —
  reproducible C microbenchmarks and analysis.
- [`docs/plan_reference.md`](docs/plan_reference.md) — governing plan identity
  and owner choices D1-A through D6-A.

## Commit workflow

Every commit invokes `.githooks/post-commit`, which:

1. Runs the M0 evidence verifier and the current M1 foundation checks.
2. Runs the M0 performance harness until M3 replaces it.
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

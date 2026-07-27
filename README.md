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

The repository bootstrap installs and verifies the locked CMake 4.4.0 and
Ninja 1.13.2 archives inside `.toolchains`, validates the pinned compiler
compatibility lane, configures the Git hook, and runs a headless smoke build.
It is safe to rerun.

On Linux or macOS:

```sh
./tools/bootstrap.sh
./tools/workflow.sh release
```

On Windows PowerShell:

```powershell
.\tools\bootstrap.ps1
.\tools\workflow.ps1 release
```

The release workflow configures, builds, and tests the native products. The
same commands accept `debug`, `sanitizer`, `release`, `profile`, `benchmark`,
and `headless`; `web` uses the browser toolchain described below.

The default native build also creates `native_client`, a host-compiled
`web_client` source smoke, `tools`, `benchmarks`, and `verifier`. These are
clean product boundaries; SDL3 and Emscripten adoption remain active M1 spikes.

Current progress and remaining M1 adoption/checkpoint items are tracked in
[`docs/milestones/M1_progress.md`](docs/milestones/M1_progress.md).

## Build and serve the browser smoke

The web bootstrap additionally installs the checksum-verified Emscripten 6.0.3
SDK and its pinned Node.js runtime. Browser-specific JavaScript remains in
`src/web_client/web_adapter.js`; the product and simulation sources remain
strict C17.

On Linux or macOS:

```sh
./tools/bootstrap.sh --web
./tools/workflow.sh web
./tools/serve_web.sh
```

On Windows PowerShell:

```powershell
.\tools\bootstrap.ps1 -Web
.\tools\workflow.ps1 web
.\tools\serve_web.ps1
```

Then open
[`http://127.0.0.1:8000/web_client.html`](http://127.0.0.1:8000/web_client.html).
The page must contain
`web-client-smoke=pass sim_abi=1 tick_hz=60`. Clean-machine CI runs this
generated HTML and Wasm in headless Chrome rather than checking files alone.

Validate the complete lock, bootstrap, preset, and CI contract with:

```sh
./tools/verify_m1_setup.sh
```

The repository/evidence workflow is documented in
[`docs/workflow_scaffolding.md`](docs/workflow_scaffolding.md). Validate its
directories, templates, lifecycle samples, and recursion guard with:

```sh
./tools/verify_m1_workflow.sh
```

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

1. Runs the M0 evidence verifier and the current M1 foundation/workflow checks.
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

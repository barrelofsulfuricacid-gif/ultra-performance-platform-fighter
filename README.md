# Ultra-Performance Platform Fighter

An original deterministic 2D platform fighter, authored in C and designed for
native, web, rollback, headless, and reinforcement-learning targets.

The project has completed **M0: product contract and measured architecture
decisions** and **M1: reproducible foundation**. The M2 deterministic
simulation and reinforcement-learning candidate is complete and awaiting its
owner checkpoint. M0 selected Q16.16 deterministic motion and geometry after
benchmark, verifier, and blind human-playtest evidence. The accepted
milestones are summarized in
[`docs/milestones/M0_checkpoint_report.md`](docs/milestones/M0_checkpoint_report.md)
and
[`docs/milestones/M1_checkpoint_report.md`](docs/milestones/M1_checkpoint_report.md).
The M2 candidate evidence and required owner choices are in
[`docs/milestones/M2_checkpoint_report.md`](docs/milestones/M2_checkpoint_report.md).

## Verify the checkpoint

Requirements: a POSIX shell, Git, GCC with C17 support, and standard Unix
utilities. Native Linux client builds also require the standard X11
development headers: `libx11-dev`, `libxcursor-dev`, `libxext-dev`,
`libxfixes-dev`, `libxi-dev`, `libxrandr-dev`, `libxss-dev`, and
`libxtst-dev` on Ubuntu. Headless and web-only workflows do not.

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

The default native build also creates an SDL3 `native_client`, a host-compiled
`web_client` source smoke, `tools`, `benchmarks`, and `verifier`. The native
client consumes a platform-neutral render packet through SDL3; the browser
client consumes the same packet through WebGL 2. Neither dependency crosses
into `sim` or the `headless` link graph.

Current progress and remaining M1 adoption/checkpoint items are tracked in
[`docs/milestones/M1_progress.md`](docs/milestones/M1_progress.md).

## Build the current M2 deterministic kernel

M2 now provides a caller-owned strict-C17 simulation ABI with seeded reset,
normalized two/four-player inputs, fixed Q16.16 ticks, deterministic episode
completion, and a structured observation candidate. It is a kernel
conformance slice, not playable combat.

Run its focused allocation/platform-boundary and scripted determinism checks:

```sh
./tools/verify_m2_kernel.sh
```

Current scope, verification evidence, and checkpoint work are tracked in
[`docs/milestones/M2_progress.md`](docs/milestones/M2_progress.md).

The optional Gymnasium 1.3 vector adapter is under
[`bindings/python/`](bindings/python/). After the headless workflow builds
`pf_sim_rl`, run its deterministic API and Python-to-C batch-overhead checks:

```sh
./tools/verify_m2_python.sh
```

## Inspect the M2 replay in a browser

Open the owner-only browser checkpoint in a current desktop browser:

[`https://platform-fighter-m1.lol1234.chatgpt.site`](https://platform-fighter-m1.lol1234.chatgpt.site)

The live page loads the repository's generated JavaScript and Wasm, verifies
the authored-C 180-tick four-player replay, and presents a draggable timeline
with positions and a SHA-256 state hash at every tick. It must report
`web-client-smoke=pass sim_abi=2 tick_hz=60`, `webgl2=pass batch_draws=1`,
and `replay=pass ticks=180 winner_mask=5`.

This is an M2 deterministic-kernel review, not the M4 gameplay playtest.
Keyboard movement, dash dancing, short/full-hop mechanics, combat, and the
first complete stage remain later milestone work.

To reproduce the same check locally, the web bootstrap additionally installs
the checksum-verified Emscripten 6.0.3 SDK and its pinned Node.js runtime.
Browser-specific JavaScript remains in `src/web_client/web_adapter.js`; the
product and simulation sources remain strict C17.

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
The current source build must contain the same smoke and replay result as the
hosted checkpoint. Clean-machine CI runs this generated HTML and Wasm in
headless Chrome, compiles and links shaders, submits the shared
textured/blended batch, verifies a rendered pixel, and checks the replay
inspector rather than checking files alone.

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

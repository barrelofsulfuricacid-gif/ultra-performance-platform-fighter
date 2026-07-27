# M1 checkpoint report

**Status:** Accepted by the owner on 2026-07-27.

**Build commit:** `05362824d2fa84f46c7c663050f937b56d22403a`

**Content hash:** Not applicable; authoritative gameplay data begins in M5.

## Scope delivered

M1 established the reproducible authored-C17 project foundation:

- Separate deterministic simulation, headless, native, web, tools, benchmark,
  and verifier products.
- Strict warning and deterministic-boundary checks.
- Pinned, checksum-verified CMake, Ninja, Emscripten, Node.js, and SDL3 setup
  for POSIX and PowerShell workflows.
- Clean-machine CI across Linux x64/Arm64, macOS Intel/Arm64, Windows x64,
  sanitizers, and a real Chrome/WebGL 2 lane.
- SDL3 native input/rendering probes and an Emscripten/WebGL 2 browser probe
  consuming the same platform-neutral render packet.
- The required evidence, feedback, optimization, generated-data, release, and
  verifier directory lifecycle.

Seeded gameplay state, deterministic ticks, snapshots, replay, and the RL API
remain M2 scope.

## Acceptance criteria

| Criterion | Result | Evidence |
|---|---|---|
| Native, headless, and browser products build from pinned setup | Pass | `tools/verify_m1_setup.sh` and CI run `30300523749` |
| Headless links no client systems | Pass | CMake link guard and boundary verification |
| Simulation remains strict C17 and platform-free | Pass | `sim.contract` and deterministic-target checks |
| Native SDL3 platform probe | Pass | SDL 3.4.12 event, gamepad, geometry, draw, and pixel smoke |
| Browser Wasm/WebGL 2 probe | Pass | Chrome shader, draw, and pixel-readback job |
| Workflow scaffold and lifecycle fixtures | Pass | `tools/verify_m1_workflow.sh` |
| Owner browser/setup checkpoint | Pass | Explicit owner choice A on 2026-07-27 |

## Verification

GitHub Actions run
[`30300523749`](https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30300523749)
passed all eight clean-machine jobs. Local setup and workflow verifiers passed,
and the post-commit hook recorded a passing manifest.

The published browser artifact is
[`platform-fighter-m1.lol1234.chatgpt.site`](https://platform-fighter-m1.lol1234.chatgpt.site).

## Performance evidence

M1 validates platform boundaries rather than representative gameplay frame
time. The headless product remains a small renderer-free executable, and the
native and browser clients submit the shared 12-vertex packet in one draw.
Representative simulation baselines begin with M2/M3 scenarios.

## Unresolved issues

None blocking M1 acceptance.

## Owner checkpoint

The owner selected option A on 2026-07-27, approving the M1 browser/setup
checkpoint and authorizing work to begin on M2.

## Follow-up

M2 implements the preallocated deterministic world, snapshots, replay,
cross-target hashing, and the versioned C reinforcement-learning surface.

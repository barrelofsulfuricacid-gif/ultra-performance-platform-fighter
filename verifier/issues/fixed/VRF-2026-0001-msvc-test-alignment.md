# [VRF-2026-0001] MSVC test-storage alignment type is unavailable

ID: VRF-2026-0001
Status: fixed
Severity: high
Detected commit: 05fcdd16ee62276a8970f91310072f2f35fa64ae
Build hash: 05fcdd16ee62276a8970f91310072f2f35fa64ae
Content hash: not-applicable
Fixed commit: 6052af8a480fbc06db291d1f184e6431c9ecdd72

## Reproduction

Run the `M2 clean-machine CI` native release job on the `windows-2025` GitHub
runner for detected commit `05fcdd16ee62276a8970f91310072f2f35fa64ae`.
The pinned bootstrap invokes the headless CMake workflow with MSVC
19.44.35228 in C17 mode.

## Expected behavior

The simulation conformance tests declare caller-owned state and scratch
buffers with an alignment at least as large as the values returned by
`pf_sim_query_memory`, and compile on every supported compiler.

## Observed behavior

MSVC's C17 standard-library surface does not declare `max_align_t`.
`test_sim_world.c` and `test_sim_snapshot.c` therefore fail to compile at
their `alignas(max_align_t)` storage declarations. The simulation sources
compile; the failure is isolated to test storage.

## Evidence

- Workflow run:
  <https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30304091824>
- Failed job: `Native windows-2025`, job ID `90103941228`
- Diagnostics: MSVC `C2065` (`max_align_t`: undeclared identifier) and
  cascading parse errors.
- The same revision passes Ubuntu x86-64, Ubuntu arm64, macOS arm64, macOS
  x86-64, sanitizer, setup-contract, and Emscripten/Chrome jobs.

## Resolution

Corrective commit `6052af8a480fbc06db291d1f184e6431c9ecdd72`
replaces the test-only `max_align_t` dependency with an explicit 64-byte
buffer alignment and retains the runtime assertion that queried state and
scratch requirements fit that alignment. The same declaration is used by the
world, snapshot, and replay-corpus tests.

## Fix verification

- The following bookkeeping commit moves this report after the corrective
  commit, preserving the required two-commit relationship.
- `M2 clean-machine CI` run
  <https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30305313145>
  completed the `Native windows-2025` release workflow successfully with MSVC
  19.44.35228.
- Local GCC debug, release, sanitizer, strict-C17 kernel, and replay tests pass.
- The native and Emscripten replay corpus outputs byte-match for all 180 ticks.

# M1 reproducible foundation progress

**Status:** In progress

## M1.1 repository and build products

The first permanent foundation slice establishes:

- `sim`, an authored-C17 static library with a public `pf_` ABI.
- `headless`, a separate renderer/audio/input-free executable that explicitly
  links only `sim`.
- An SDL3 `native_client`, an Emscripten/WebGL 2 `web_client`, and clean
  `tools`, `benchmarks`, and `verifier` products.
- A platform-neutral presentation packet shared by the native and browser
  adapters without entering deterministic state.
- A fixed 60 Hz simulation contract and versioned ABI smoke surface.
- Strict warnings-as-errors for GCC, Clang, and MSVC project code.
- Native direct-compiler and CMake/CTest verification.

This is intentionally not the M2 simulation kernel. Seeded world state, input
frames, ticks, observations, save/load, and hashing remain M2 work.
SDL3 and Emscripten are now adopted for the M1 platform baseline. Final
renderer performance details remain an M7 evidence decision.

## M1.3 workflow scaffold

The tracked workflow now includes all required design, generated-data,
original-asset, performance, verifier-issue, human-feedback, optimization, and
release locations. Versioned templates and valid lifecycle fixtures are checked
by `tools/verify_m1_workflow.sh`. The validator also proves that per-commit
evidence remains ignored, preventing the post-commit workflow from recursively
creating commits.

## M1.2 reproducible setup

The setup slice now provides:

- Conceptually matched POSIX shell and Windows PowerShell bootstrap/workflow
  commands.
- Repository-local, checksum-verified CMake 4.4.0 and Ninja 1.13.2 installs.
- Exact Emsdk, Emscripten SDK, and Node.js web payload locks, including byte
  lengths and SHA-256 digests.
- Strict compiler lanes for GNU C 13.3.x, Clang 17.0.x, and MSVC 19.44.x.
- Debug, sanitizer, release, profile, benchmark, headless, and web CMake
  workflow presets.
- A strict-C17 web product with its browser-only JavaScript isolated in one
  adapter.
- Documented one-command web serving and an automated Chrome DOM/Wasm smoke.
- Clean-machine CI definitions for Linux x64/Arm64, macOS Intel/Arm64,
  Windows x64, sanitizers, and Emscripten/Chrome.
- `tools/verify_m1_setup.sh`, which validates the lock, scripts, presets,
  boundary, and pinned CI actions.

Local Linux validation has passed for every native preset and the Emscripten
build. LeakSanitizer is left enabled in the preset and CI; only this restricted
Work Mode container requires `ASAN_OPTIONS=detect_leaks=0` because it cannot
use the process-inspection facilities LeakSanitizer needs.

The initial clean-machine matrix passed on Linux x64/Arm64, macOS Intel/Arm64,
Windows x64, the sanitizer lane, and real Chrome/Wasm. It also verified the
PowerShell setup contract and the pinned MSVC 19.44 compatibility toolset.

## M1.4 native/web platform adoption

The current adoption slice adds:

- Checksum-verified SDL 3.4.12 bootstrap and static native builds.
- SDL event and gamepad mapping probes.
- A shared 12-vertex textured/blended render packet.
- A native GPU-renderer-first path with an ordinary SDL renderer fallback.
- A WebGL 2 adapter with runtime shader compile/link, one batched draw, and
  pixel readback.
- Explicit guards keeping SDL out of `sim` and `headless`.

Detailed local evidence and deferred M7 renderer questions are recorded in
[`M1_platform_spike.md`](M1_platform_spike.md).

## Remaining M1 work

- Confirm the platform-adoption change on the full clean-machine CI matrix,
  including the WebGL 2 Chrome draw and Windows/macOS SDL builds.
- Stop for the mandatory owner setup checkpoint.

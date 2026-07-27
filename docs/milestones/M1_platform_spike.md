# M1 native/web platform adoption evidence

**Status:** Accepted
**Date:** 2026-07-27

## Outcome

SDL 3.4.12 is the native M1 platform baseline and Emscripten 6.0.3 plus
WebGL 2 is the browser baseline. Both clients consume one strict-C17,
platform-neutral render packet. SDL, browser APIs, backend objects, and
rendering floats remain outside `sim` and the `headless` link graph.

The native interactive adapter requests SDL's GPU-backed renderer first and
falls back to the ordinary SDL renderer. The browser adapter uses WebGL 2 and
fails with a visible reason when WebGL 2 is unavailable. WebGPU is detected
only for evidence; it is not a hidden alternate path.

## Local evidence

| Check | Result |
|---|---|
| Strict native workflows | Debug, sanitizer, release, and profile: 8/8 tests each |
| Isolated workflows | Benchmark: 3/3; headless: 3/3 |
| Native platform smoke | SDL 3.4.12, event queue, gamepad mapping, geometry, one batch, pixel `87,139,188,255` |
| Shared packet test | 12 vertices and a 2×2 RGBA texture |
| Web build | 19,605-byte HTML, 49,516-byte JavaScript, 15,408-byte Wasm |
| Wasm structure | Binaryen parsed and rewrote the module with all used Wasm features enabled |
| Headless isolation | 16,224 bytes; no SDL symbols; dynamic dependencies are libc and the loader |
| Native static link | 3,692,712 bytes; no dynamic SDL dependency |
| Prior native boundary smoke | 16,320 bytes |

The local container has neither X11 nor Wayland development headers. Its
native build therefore used the explicit `PF_SDL_UNIX_CONSOLE_BUILD=ON`
diagnostic switch and validated SDL's software/offscreen render path. Normal
bootstrap and CI leave that switch off. Linux CI installs the standard X11
platform SDK set first, and a missing desktop backend fails configuration
instead of silently producing a non-interactive client.

No Chrome executable is installed in this local environment. The generated
Wasm and JavaScript passed compilation, JavaScript syntax checking, and
Binaryen parsing locally; the clean-machine web job is responsible for the
real DOM, shader, draw, and pixel test. That job opts into Chromium's
ANGLE/SwiftShader path so the rendering assertion is reproducible on a
GPU-less hosted runner.

## Backend evaluation

The shared packet proves the semantics needed by the first renderer seam:
clear color, positions, texture coordinates, vertex tint/alpha, texture data,
triangle order, and a single batched submission.

For native M1, `SDL_CreateGPURenderer` provides a narrow 2D renderer backed by
SDL's GPU API while keeping backend-specific Vulkan, D3D12, Metal, and shader
objects out of authored client code. `SDL_CreateRenderer` is the compatibility
fallback. Direct SDL GPU pipelines remain possible behind the same packet
boundary if an M7 workload proves the abstraction insufficient.

For browser M1, WebGL 2 requires no extra source dependency and directly
expresses the probe. Emscripten's documented WebGPU route uses the external
Emdawnwebgpu port, while direct browser WebGPU would add a second asynchronous
pipeline and WGSL workflow. Without a representative M7 workload, that
complexity has no measured win. WebGPU remains an optional candidate to
benchmark later.

## Deferred evidence

This probe is not a renderer benchmark. M7 must supply a representative scene
with atlas pressure, animation, particles, camera, HUD, and effects before
comparing:

- frame-time distributions and CPU submission cost;
- direct SDL GPU versus GPU-backed `SDL_Renderer`;
- WebGL 2 versus WebGPU on the approved browser/device matrix;
- project shader compilation, caching, and precompilation;
- fallback quality and device-loss recovery.

No claim about playable frame time or final renderer performance is made at
M1.

## Clean-machine result

GitHub Actions run
[`30299564266`](https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30299564266)
passed all eight jobs:

- Native SDL3 builds and smokes on Ubuntu x64, Ubuntu Arm64, macOS Intel,
  macOS Arm64, and Windows x64.
- Linux address and undefined-behavior sanitizers.
- Lock, preset, PowerShell, boundary, and CI setup-contract verification.
- Emscripten compilation followed by a real Chrome/WebGL 2 shader, batch, and
  pixel-readback smoke under ANGLE/SwiftShader.

The platform adoption slice is complete. M1 now waits only at its mandatory
owner setup checkpoint. Its public browser artifact is
[`platform-fighter-m1.lol1234.chatgpt.site`](https://platform-fighter-m1.lol1234.chatgpt.site);
a passing owner run begins with
`web-client-smoke=pass sim_abi=1 tick_hz=60 webgl2=pass batch_draws=1`.

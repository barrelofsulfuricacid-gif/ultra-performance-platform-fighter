# TDR-0002: Build, native platform, and web baseline

- **Status:** Accepted for M1 spikes; renderer backend remains provisional
- **Date:** 2026-07-27

## Decision

- Use CMake workflow presets with Ninja for the cross-platform build.
- Use SDL3 as the native window, event, gamepad, and platform layer.
- Use Emscripten to compile the same C simulation sources to WebAssembly.
- Spike SDL's GPU API for the native renderer.
- Establish WebGL 2 as the browser compatibility baseline and compare WebGPU
  before locking the rendering command backend.

Rendering is not deterministic and does not affect cross-play compatibility.
Native and web clients consume the same logical render/event outputs even when
their GPU backends differ.

## Locked setup pins

- CMake 4.4.0 (`44125a9`), with every supported host archive locked by full
  SHA-256 and byte length.
- Ninja 1.13.2 (`3441b63`), with every supported host archive locked by full
  SHA-256 and byte length.
- SDL 3.4.12 (`f87239e`); official source tarball SHA-256
  `f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7`.
- Emsdk commit `db04e88298d9916fc51fcd3743045ca3eb695127` and
  Emscripten 6.0.3 release revision
  `9074aa513b501925adb1361e208932ad32a29a5f`, including locked SDK and
  Node.js payloads for every supported web host.

The exact archive URLs, sizes, and digests are versioned in
`dependencies/toolchains.lock.tsv`. SDL remains locked but unfetched until the
platform-adoption spike passes.

## Evidence

- [CMake 4.4 release list and documentation](https://cmake.org/cmake/help/latest/release/index.html)
- [Ninja 1.13.2 release](https://github.com/ninja-build/ninja/releases/tag/v1.13.2)
- [SDL 3.4.12 release and checksums](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.12)
- [SDL GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU)
- [Emscripten 6.0.3 release](https://github.com/emscripten-core/emscripten/releases/tag/6.0.3)
- [Emscripten WebGL/OpenGL guidance](https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html)

Licenses are BSD-3-Clause (CMake), Apache-2.0 (Ninja), zlib (SDL), and
MIT plus University of Illinois/NCSA (Emscripten).

## Spike acceptance

- One preset configures and builds on each supported OS and Emscripten.
- SDL event/gamepad behavior passes a thin adapter test.
- A textured, blended, batched 2D scene works on native and current major
  browsers.
- Render command semantics match even if native and web backends differ.
- No SDL header or symbol appears in `sim` or `headless`.
- Binary size, frame time, shader workflow, fallback behavior, and debugging
  evidence are recorded before choosing SDL GPU/WebGL/WebGPU details.

## Replacement plan

CMake can emit another backend if Ninja becomes unsuitable. SDL is isolated in
the platform adapter. Renderer implementations consume backend-neutral render
commands. Emscripten-specific JavaScript stays in one web adapter and can be
replaced without changing simulation.

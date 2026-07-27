# TDR-0002: Build, native platform, and web baseline

- **Status:** Accepted M1 platform baseline; final renderer performance lock
  remains provisional
- **Date:** 2026-07-27

## Decision

- Use CMake workflow presets with Ninja for the cross-platform build.
- Use SDL3 3.4.12 as the native window, event, gamepad, and platform layer.
- Use Emscripten to compile the same C simulation sources to WebAssembly.
- Feed both clients one authored-C, platform-neutral batched render packet.
- Use SDL 3.4's GPU-backed `SDL_Renderer` path for native 2D presentation:
  request `SDL_CreateGPURenderer` first and fall back to
  `SDL_CreateRenderer` when the GPU renderer cannot initialize.
- Use WebGL 2 as the browser compatibility baseline. Keep WebGPU as a future
  optional backend to measure against a representative M7 workload rather
  than adding its external Emdawnwebgpu port and WGSL pipeline before the
  renderer has that workload.

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
`dependencies/toolchains.lock.tsv`. Both bootstrap implementations now fetch,
verify, and extract the SDL archive into the ignored repository-local
toolchain directory.

## Evidence

- [CMake 4.4 release list and documentation](https://cmake.org/cmake/help/latest/release/index.html)
- [Ninja 1.13.2 release](https://github.com/ninja-build/ninja/releases/tag/v1.13.2)
- [SDL 3.4.12 release and checksums](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.12)
- [SDL GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU)
- [SDL_CreateGPURenderer](https://wiki.libsdl.org/SDL3/SDL_CreateGPURenderer)
- [SDL_RenderGeometryRaw](https://wiki.libsdl.org/SDL3/SDL_RenderGeometryRaw)
- [Emscripten 6.0.3 release](https://github.com/emscripten-core/emscripten/releases/tag/6.0.3)
- [Emscripten WebGL/OpenGL guidance](https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html)
- [Emscripten WebGPU guidance](https://emscripten.org/docs/porting/multimedia_and_graphics/WebGPU-support.html)
- [Chromium SwiftShader testing guidance](https://chromium.googlesource.com/chromium/src/+/main/docs/gpu/swiftshader.md)

Licenses are BSD-3-Clause (CMake), Apache-2.0 (Ninja), zlib (SDL), and
MIT plus University of Illinois/NCSA (Emscripten).

## Spike result

- Native smoke tests exercise the SDL event queue, gamepad mapping database,
  exact runtime/header version agreement, textured/blended geometry submission,
  and pixel readback.
- The Emscripten target compiles and links WebGL 2 shaders, submits the same
  12-vertex packet in one draw, and verifies the center pixel in real Chrome.
- The render packet owns backend-neutral positions, UVs, colors, clear color,
  and texture bytes. Backend handles and platform headers remain private.
- `sim` and `headless` contain no SDL reference, and CMake rejects an expanded
  explicit headless link graph.
- The Linux x64 release smoke measured 3,692,712 bytes for the statically linked
  SDL client versus 16,320 bytes for the prior boundary-only client. The
  headless executable remained 16,224 bytes and dynamically linked only libc.
- Native shader selection is internal to SDL's GPU renderer for this M1 2D
  batch. WebGL 2 shaders are isolated in `src/web_client/web_adapter.js`.
- Native fallback is explicit; WebGL 2 absence is an explicit unsupported
  browser failure with a DOM diagnostic. WebGPU availability is reported but
  does not alter behavior.

The tiny probe is deliberately not a frame-time benchmark. It does not provide
a representative scene, batching pressure, atlas workload, or effects load.
Direct SDL GPU pipelines, WebGPU, precompiled project shaders, and renderer
frame-time selection therefore remain an M7 evidence decision rather than an
unmeasured M1 claim.

## Replacement plan

CMake can emit another backend if Ninja becomes unsuitable. SDL is isolated in
the platform adapter. Renderer implementations consume backend-neutral render
commands. Emscripten-specific JavaScript stays in one web adapter and can be
replaced without changing simulation.

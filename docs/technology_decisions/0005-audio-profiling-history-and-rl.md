# TDR-0005: Audio, profiling, performance history, and RL wrapper

- **Status:** Tool pins accepted; audio backend deferred to M7 measurement
- **Date:** 2026-07-27

## Audio candidates

Benchmark:

- miniaudio 0.11.25 (`9634bed`), written in C with a public-domain or MIT-0
  choice and an Emscripten/Web Audio backend.
- SDL_mixer 3.2.4 (`72a8186`), zlib licensed and designed for SDL3.

The test must cover output/callback latency, simultaneous voice mixing, stream
decode, resampling, music transitions, device loss, web behavior, binary size,
dependencies, and integration cost. Audio consumes reconciled logical events
and never affects deterministic state.

Evidence:

- [miniaudio 0.11.25 release](https://github.com/mackron/miniaudio/releases/tag/0.11.25)
- [miniaudio platforms and license](https://github.com/mackron/miniaudio)
- [SDL_mixer 3.2.4 release](https://github.com/libsdl-org/SDL_mixer/releases/tag/release-3.2.4)
- [SDL3_mixer overview and license](https://wiki.libsdl.org/SDL3_mixer)

## Profiling

Pin Tracy 0.13.1 (`05cceee`, BSD-3-Clause) for profile builds. Instrumentation
is wrapped in project macros and compiles to nothing in maximum-throughput
headless builds. Use OS-native profilers in addition when available.

Evidence:

- [Tracy 0.13.1 release](https://github.com/wolfpld/tracy/releases/tag/v0.13.1)
- [Tracy license](https://github.com/wolfpld/tracy/blob/master/LICENSE)

Linux `perf` was unavailable in the current M0 container, so no hardware
counter claim is included in the M0 result.

## Performance history

Pin SQLite 3.53.4 using source ID
`bf7c7f30031888f4e796e429ab3978879485813aaca6f641c7b33e4e09459bcc`
and amalgamation SHA3-256
`67f423e9ebbbdc473cbc4772c872ee6b89f31fde4ed0279a5c25d5f65c043a16`.
SQLite is public-domain software.

SQLite enters the benchmark/reporting tool only. Raw per-commit measurements
remain local; explicit milestone exports are committed.

Evidence:

- [SQLite release history and hashes](https://www.sqlite.org/changes.html)
- [SQLite copyright/public-domain statement](https://www.sqlite.org/copyright.html)

## RL interoperability

The stable batched C ABI is authoritative. Pin Gymnasium 1.3.0 (`53bf3e9`,
MIT) only for the thin Python compatibility/test package.

Evidence:

- [Gymnasium 1.3.0 release](https://github.com/Farama-Foundation/Gymnasium/releases/tag/v1.3.0)
- [Gymnasium license](https://github.com/Farama-Foundation/Gymnasium/blob/main/LICENSE)

Python ownership, NumPy conversion, and Gym wrappers remain outside simulation
benchmarks. Shared memory or zero-copy extensions require measured benefit and
cannot change C observation/action semantics.

## Replacement plan

Audio, profiler, database, and Python layers each sit behind separate adapters.
Replacing any one leaves deterministic state, simulation APIs, replay bytes,
and content packs unchanged.

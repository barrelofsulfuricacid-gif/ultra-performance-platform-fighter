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

Gymnasium's `VectorEnv` contract was selected over a project-specific Python
protocol because it standardizes batched spaces, reset/step returns, info
arrays, and autoreset metadata without entering simulation. PettingZoo remains
a possible later multi-agent presentation adapter; it is not needed to expose
the current match-as-environment C contract.

The implemented adapter:

- Loads the separately built `pf_sim_rl` shared library through `ctypes`.
- Owns aligned state and scratch buffers outside the engine.
- Maps exact integer actions and 36-word seed-redacted observations without
  defining game logic in Python.
- Uses Gymnasium 1.3 next-step autoreset.
- Selects one configurable player for Gymnasium's scalar shaped/outcome reward
  while retaining exact four-player Q16.16 rewards in `info`.
- Crosses Python-to-C once per active batch rather than once per environment.

Repeated M2 Linux qualification runs with 64 duel environments measured an
18.2x–21.6x boundary speedup from one `ctypes` call per environment to one
batch call. The latest sample measured 317,231 versus 6,866,198
environment-ticks/s (21.6441x). The native C benchmark remains authoritative;
its latest sample measured 15,974,047 single-step versus 15,246,088 batched
environment-ticks/s (0.9544x), with exact state-hash equality. This shows that
the measured batch benefit is specifically at the foreign-function boundary,
not a claim that the small C loop itself is faster. The shared adapter remains
separate from the static headless library and does not change the headless
link graph. These are local comparative measurements, not cross-machine
performance claims.

The immutable source archive is locked in `dependencies/python.lock.tsv`:
commit `53bf3e9a884783eb72ad3fc8b15780914c97c3e1`, 119,161,195 bytes, SHA-256
`1fed3f2b523aad174cab6bed6357f65c897c79afce674578ac2b7e32fe524bc2`.
Clean-machine CI installs exactly Gymnasium 1.3.0, asserts the runtime version,
builds the wrapper package without dependency substitution, and runs API,
determinism, autoreset, duel/team reward, masked-reset, and boundary-overhead
checks. Linux executes the Python tests; Linux, macOS, and Windows all compile
the shared C ABI, while WebAssembly continues to compile the same static
simulation sources.

Evidence:

- [Gymnasium 1.3.0 release](https://github.com/Farama-Foundation/Gymnasium/releases/tag/v1.3.0)
- [Gymnasium license](https://github.com/Farama-Foundation/Gymnasium/blob/main/LICENSE)
- [Gymnasium vector API](https://gymnasium.farama.org/api/vector/)

Python ownership, NumPy conversion, and Gym wrappers remain outside simulation
benchmarks. Shared memory or zero-copy extensions require measured benefit and
cannot change C observation/action semantics.

The engine maintainer owns version/security review. Replacement removes the
optional Python package and shared-library build target; the C ABI, replay
format, deterministic state, and all native products remain unchanged.

## Replacement plan

Audio, profiler, database, and Python layers each sit behind separate adapters.
Replacing any one leaves deterministic state, simulation APIs, replay bytes,
and content packs unchanged.

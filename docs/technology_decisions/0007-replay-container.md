# TDR-0007: Replay container and verification

- **Status:** Accepted for replay format 1
- **Date:** 2026-07-27

## Decision

Replay format 1 is a little-endian, length-delimited container with a 40-byte
file header and 48-byte chunk headers. Every chunk payload carries a SHA-256
checksum. A chunk declares whether it is required:

- Unknown optional chunks are checksum-validated and skipped.
- Unknown required chunks fail closed as an unsupported version.
- Missing, duplicate, corrupt, or trailing chunks fail before simulation
  mutation.

The format-1 required chunks are:

| Type | Contents |
|---|---|
| Match | Simulation/state/input/arithmetic/RNG versions, content and configuration hashes, seed, tick count, tick rate, players, and mode |
| Initial state | Canonical save-format-1 checkpoint at tick zero |
| Inputs | Count followed by tick-major, slot-major normalized input frames |
| State hashes | Initial hash plus one canonical state hash after every tick |
| Result | Final tick, fault flags, termination, truncation, and winner mask |

Input chunk version 1 is deliberately uncompressed. It provides the simple
canonical oracle against which a later delta-compressed chunk can be tested.
Compression can be introduced as a new chunk version without changing
simulation input semantics.

## API and ownership

`pf_replay_query_size` and `pf_replay_encode` consume a `pf_replay_source`
containing a retained tick-zero state, the normalized input stream, per-tick
hashes, and final result. The caller owns all source and destination memory.

`pf_replay_verify`:

1. Checks the whole container, every chunk checksum, schema, count, input, hash,
   result, content hash, and configuration hash without mutating simulation.
2. Atomically loads the initial checkpoint.
3. Calls the same `pf_sim_tick` used by local and RL execution.
4. Compares every resulting canonical hash and reports the first mismatching
   tick plus expected/actual hashes.

All three functions perform no allocation or I/O.

## Corpus

`tests/sim/test_replay_corpus.c` defines the format-1 golden corpus:

- Four-player team configuration.
- Seed `0x0123456789abcdef`.
- 180 normalized input ticks and 181 state hashes.
- 30,997 replay bytes.
- Replay SHA-256
  `a1008ac5f1d555ccd17a8f17fe48eab6ce08079fd635b26ee08155f0dea44dce`.
- Final state SHA-256
  `335e31f2d830eea582f9e42fe7ee41469f81aa359ee14e661cf68002932d558a`.

The test also proves checksum rejection without state mutation, exact
localization of a deliberately wrong tick-51 hash, content incompatibility,
unknown-required rejection, and unknown-optional skipping.

`tools/verify_m2_replay.sh` compiles and runs the corpus natively, runs the
Emscripten build through Node when present, and requires byte-identical output.
The Web CI job makes the WebAssembly comparison mandatory.

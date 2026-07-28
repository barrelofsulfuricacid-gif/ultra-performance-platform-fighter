# TDR-0007: Replay container and verification

- **Status:** Accepted for replay format 1
- **Date:** 2026-07-28

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
| Initial state | Canonical checkpoint for the match's declared state/save schema at tick zero |
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

`src/checkpoint/m2_replay_fixture.c` defines the shared format-1 golden trace
used by the native/WebAssembly corpus test and browser inspector:

- Four-player team configuration.
- Seed `0x0123456789abcdef`.
- 180 normalized input ticks and 181 state hashes.
- 31,261 replay bytes with M4 state schema 6 / save format 5.
- Ground-attack, vertical-stick, and trigger inputs plus confirmed damage/hit,
  SDI, tech-window, and shield state in the production combat path.
- Replay SHA-256
  `628685db3a1ce96383608dc48f356346f8a0ddfc785a8fe0a00bb21c3977e3b3`.
- Final state SHA-256
  `d0d2eab988ab7c8597829297601d533c69259ee9f0f8203cd6c385d2ed20db17`.

The test also proves checksum rejection without state mutation, exact
localization of a deliberately wrong tick-51 hash, content incompatibility,
unknown-required rejection, and unknown-optional skipping.

`tools/verify_m2_replay.sh` compiles and runs the corpus natively, runs the
Emscripten build through Node when present, and requires byte-identical output.
The Web CI job makes the WebAssembly comparison mandatory. The browser build
also generates, encodes, verifies, and displays the same trace inside
WebAssembly, with a draggable tick timeline and the canonical state hash at
every checkpoint.

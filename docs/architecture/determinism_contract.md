# Determinism contract

## Authority

For a matching simulation ABI, content hash, match configuration, seed, and
ordered input stream, every supported native and WebAssembly build must produce
the same canonical state hash and deterministic event journal after every
logical tick.

The fixed logical rate is 60 Hz. Wall-clock time controls presentation and
input collection only; it is never read by a tick.

## Canonical inputs

The client normalizes hardware before simulation:

- Buttons become a versioned bitset.
- Main and secondary stick axes become signed 16-bit integers with an explicit
  range, dead-zone, snapback, and calibration version.
- Analog triggers become unsigned 16-bit integers.
- One frame contains every player's input for exactly one numbered tick.
- Missing remote input is a netcode prediction concern. The simulation only
  receives the concrete predicted or confirmed frame selected by its caller.

Controller names, floating-point SDL axis values, timestamps, and device IDs do
not enter deterministic state.

## Arithmetic rules

The approved numeric representation uses IEEE-754 binary32 values for motion,
geometry, damage, shield state, animation clocks, and other formerly
quantized simulation channels. This became binding with the project-wide
float32 migration on 2026-08-13.

The following arithmetic rules are binding:

- Supported targets must satisfy the public compile-time binary32 assertions:
  8-bit bytes, 4-byte `float`, radix 2, 24-bit significand, and exponent range
  128.
- Every deterministic target uses `-fno-fast-math` and disables floating-point
  contraction, or the compiler-specific equivalent. Reassociation and implicit
  fused operations are not allowed.
- Source operation order is part of the contract. Shared inline helpers own
  conversions and nonlinear operations so native and Wasm do not grow separate
  formulas.
- Canonical state accepts finite values only. Negative zero is normalized at
  state boundaries where sign is not semantic; NaN and infinity are rejected.
- Serialization writes each binary32 value as its exact IEEE bit pattern in
  little-endian order. Hashing consumes those canonical bytes rather than host
  structure memory.
- Authored decimal and big-endian source floats are rounded once to binary32 by
  validated importers. Runtime code does not route them through a legacy
  quantization layer.
- Small source-oracle tolerances must be finite, field-specific, and
  manifest-owned. They never relax action, callback, clock, support, or
  collision-result equality.
- Compiler flags that relax these semantics are excluded unless a versioned
  replacement contract and the full native/Wasm replay gate prove it.

## State rules

- State contains canonical binary32 values, fixed-width integers, stable
  indices, bounded arrays, and versioned IDs. It contains no owning pointers,
  native handles, locks, atomics, function pointers, wall-clock values, file
  paths, or presentation objects.
- Capacities are compile-time or content-pack constants validated before match
  start. Exhaustion returns a deterministic fault instead of allocating.
- Iteration order is explicit. Removal uses a documented stable or
  swap-with-last rule whose result is included in tests.
- Randomness uses one named, versioned algorithm with explicit state and stream
  ownership. Consuming random values from rendering, audio, logging, or thread
  timing is impossible.
- Cold diagnostics are not part of canonical state unless they affect future
  simulation.
- Padding bytes are never hashed or serialized.

## Canonical serialization and hashing

- Wire integers and binary32 bit patterns are little-endian and emitted field
  by field.
- A save state begins with format version, simulation ABI, content hash, match
  configuration hash, tick, payload length, and payload checksum.
- Serialization order is schema order, never memory-address order.
- Hashing uses the canonical serialized byte stream. The hash algorithm and
  version are stored beside the digest.
- Load validates every length, count, enum, index, numeric range, and hash
  before replacing a live state.
- Failed loads leave the destination unchanged.

## Side effects and rollback

Simulation emits a bounded journal of logical events. Each event has:

- Processed input tick and a match-monotonic sequence.
- Versioned event kind.
- Stable subject/object indices where applicable.
- Fixed-size deterministic payload.
- Event ID formed by match identity plus the sequence; the tick remains
  explicit diagnostic context.

Clients may preview speculative events according to per-kind policy, but must
reconcile by event ID after rollback. The journal array is per-tick output;
only its sequence authority is canonical state. Restoring a checkpoint and
re-simulating inputs must reproduce the exact event bytes.

The native/WebAssembly corpus hashes the complete event stream under the
`PFEVT001` domain in addition to canonical state hashes. Replay format 1
re-simulates events but does not yet carry an expected per-replay journal
digest, so ranked event-digest verification remains a future replay-chunk
version. Rendered particles, UI, and mixed audio are never authoritative.

## Verification matrix

The following comparisons are required before M2 acceptance:

| Dimension | Required variants |
|---|---|
| Native OS | Windows, macOS, Linux |
| Architecture | Every shipped architecture, including x86-64 and arm64 where shipped |
| Compiler | Pinned supported compilers and optimization levels |
| Web | Pinned Emscripten release in current Chromium, Firefox, and Safari |
| Runtime mode | Local, replay, rollback re-simulation, verifier, single RL, batched RL |
| Save/load | Fresh run, periodic checkpoint restore, rollback ring restore |

Each canonical replay stores per-tick hashes. The first divergence reports the
tick, first differing schema field, build fingerprint, content hash, and input
frame.

## Change control

Any change to arithmetic, state order, RNG, input normalization, content
packing, replay encoding, or hashing increments an appropriate schema or
compatibility version. Compatible peers reject mismatched versions before a
match. Ranked verification uses the exact accepted build/content pair rather
than attempting best-effort conversion.

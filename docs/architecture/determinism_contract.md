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

The approved numeric representation uses signed Q16.16 values for deterministic
motion and geometry, with signed 64-bit intermediates. This became binding when
the owner accepted the M0 decision on 2026-07-27.

The following arithmetic rules are binding:

- Signed overflow is forbidden.
- Negative signed shifts and implementation-defined right shifts are not used
  to define rounding.
- Every multiply/divide states its widening, rounding, saturation, and
  divide-by-zero behavior.
- Conversion from authored decimal values happens in the validated packer, not
  during a tick.
- Trigonometric and other nonlinear values are generated as versioned tables or
  implemented by a tested integer algorithm.
- Negligible values have an explicit canonical zero rule; denormals cannot
  silently change performance or results.
- Compiler flags that relax arithmetic semantics are excluded from
  deterministic targets unless cross-target hash tests prove an explicitly
  versioned replacement contract.

## State rules

- State contains fixed-width integers, stable indices, bounded arrays, and
  versioned IDs. It contains no owning pointers, native handles, locks, atomics,
  function pointers, wall-clock values, file paths, or presentation objects.
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

- Wire integers are little-endian and emitted field by field.
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

- Tick and monotonically increasing sequence within that tick.
- Versioned event kind.
- Stable subject/object indices where applicable.
- Fixed-size deterministic payload.
- Event ID derived from match identity, tick, and sequence.

Clients may preview speculative events according to per-kind policy, but must
reconcile by event ID after rollback. Server verification hashes the
deterministic journal; it does not verify rendered particles or mixed audio.

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

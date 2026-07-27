# M2 deterministic simulation and RL progress

**Status:** M2.1 deterministic-world implementation in progress

## Current deterministic kernel

The first M2 slice replaces the M1 ABI placeholder with simulation ABI 2 and:

- A caller-owned `pf_sim` lifecycle with separately queried, aligned state and
  scratch memory.
- No allocation, I/O, logging, lock, wall-clock, SDL, or thread dependency in
  deterministic sources.
- Versioned content identity, configuration, input, state, arithmetic, RNG,
  and observation schemas.
- Deterministic SplitMix64 seeded reset with capacity for four fighter slots.
- One authored-C tick path supporting two-player duel and four-player team
  configurations.
- Normalized per-player input frames with exact tick and stable-slot checks.
- Provisional Q16.16 arena movement, jump-edge handling, fixed 60 Hz stepping,
  forfeit termination, and time-limit truncation.
- Atomic rejection of invalid input: a failed tick leaves state unchanged.
- A structured observation view suitable for kernel conformance tests.
- A fixed 305-byte canonical little-endian save format with content and
  configuration compatibility identity.
- Allocation-free save, atomic validated load, compatible-state clone, and
  versioned SHA-256 state hashing.

The provisional movement constants exercise deterministic arithmetic and state
transitions only. They are not M4 gameplay tuning and make no claim about
final movement feel.

## Verification

`tools/verify_m2_kernel.sh` builds the kernel directly as strict C17 and checks:

- Two independent seeded simulations remain byte-identical through a
  180-tick scripted trace.
- A mismatched input tick is rejected without state mutation.
- Reset, movement, jump edges, termination, truncation, and four-player
  capacity behave as specified.
- The SHA-256 standard vector, exact save-stream hash, save/load/clone future
  equality, malformed-input rejection, and failed-load atomicity pass.
- Deterministic object files reference no allocation, platform, I/O,
  wall-clock, or synchronization symbols.

The same tests are also CTest targets in debug, release, sanitizer, and
headless workflows. Emscripten compiles the same simulation sources.

## Remaining M2.1 work

- Replay container, input playback, and per-tick hash corpus.
- Cross-compiler, operating-system, optimization-level, and WebAssembly hash
  comparison.

## Remaining M2.2 work

- Final candidate structured/compact observations, legal masks, rewards, and
  diagnostics.
- Caller-owned single and batched RL stepping.
- Uncapped headless throughput runner and boundary-overhead comparison.
- Thin Gymnasium-compatible vector wrapper.
- Mandatory owner review of deterministic replays and the observation/action
  contract.

# Numeric representation decision

**Current status:** IEEE-754 binary32 accepted for production on 2026-08-13.

**Historical M0 benchmark commit:**
`9e757df3d4c222ee55d100bd2db7572b94c748fb`

**Raw historical evidence:** `performance/m0_representation/`

## Decision history

M0 originally selected a signed 32-bit integer representation with sixteen
fractional bits. On 2026-08-13 the owner explicitly superseded that decision
and required every simulation value that used the quantized representation to
use IEEE-754 binary32 instead. The retired implementation, helpers, field
names, and generated integer geometry have been removed; the historical
benchmark ratios below remain evidence about the old choice, not the current
architecture.

## Historical measurement validity

- 23 candidates, 15 interleaved samples each, 345 samples total.
- Optimized C17 build using GCC 13.3.0 and `-O3 -march=native`.
- One pinned CPU in a KVM virtualized environment.
- Every calibration and sample started from the identical seeded state.
- Candidate self-tests covered bounds, exact broadphase outcomes, logical
  layout equality, dispatch equality, and snapshot restore checksums.
- AddressSanitizer and UndefinedBehaviorSanitizer smoke execution passed.

The results establish relative direction on that compatibility key. They do
not predict final-engine throughput or compare different machines.

## Historical Pareto results

Ratios are paired medians against each family baseline with deterministic
20,000-resample 95% bootstrap intervals.

| Concern | Leading result | Baseline | Relative result | Historical decision |
|---|---|---|---:|---|
| Motion arithmetic | retired 16-fractional-bit integer | float32 | 1.410x [1.389, 1.440] | Superseded by float32 |
| Compact cell motion | 256-cell int8 | float32 | 0.636x [0.626, 0.640] | Rejected as universal motion |
| Hybrid motion | integer position/float velocity | float32 | 0.141x [0.138, 0.147] | Rejected tested form |
| World range | 4096-cell u16 | 256-cell u8 | 1.033x [1.017, 1.055] | Prefer range and precision |
| Sparse broadphase | rebuilt sweep | naive | 1.625x [1.591, 1.655] | Keep as sparse challenger |
| Dense broadphase | 16x16 grid | naive | 2.678x [2.556, 2.764] | Select uniform-grid default |
| Layout | structure of arrays | AoS with cold data | 7.076x [6.966, 7.193] | Select SoA hot pools |
| Dispatch | data table | switch | 2.893x [2.752, 2.937] | Select tables for shared math |
| Snapshot | tracked 8x64-byte dirty chunks | full 64 KiB copy | 83.812x [83.178, 84.259] | Select tracked-dirty prototype |

The higher-resolution world result's 3.3% median advantage was below three
times the baseline MAD, so it was preferred for range and precision rather
than claimed as a meaningful speed win.

## Current accepted architecture

1. **IEEE-754 binary32 motion and geometry.** Public, canonical, generated,
   snapshot, replay, observation, item, projectile, and collision values use
   C `float`. Supported targets must provide 32-bit radix-2 floats with a
   24-bit significand and exponent range matching binary32.
2. **Controlled floating-point evaluation.** Deterministic targets forbid
   fast-math and contraction, retain source operation order, canonicalize
   serialized float bits, reject non-finite state, and verify native/Wasm
   replay state and event hashes.
3. **SoA hot deterministic pools.** Fighters and homogeneous object pools keep
   frequently updated fields in separate arrays; diagnostics and presentation
   remain outside canonical hot state.
4. **Uniform-grid broadphase plus exact narrow phase.** The 16x16 grid remains
   the default, with a sweep challenger for genuinely sparse static sets.
5. **Data tables for shared move math; explicit C transitions for behavior.**
6. **Mutation-tracked rollback chunks with a full-copy oracle.**

## Float32 portability contract

The migration accepts small explicitly declared source-oracle differences
caused by binary32 evaluation. It does not permit different actions, callback
order, clocks, supports, collision outcomes, or unbounded drift. Native and
WebAssembly run the same C source with stable operation order and must match
the canonical replay corpus exactly unless a future, reviewed cross-target
case documents a bounded binary32 difference.

### Closure evidence

The 2026-08-14 completion gate found no Q16, 16.16, or fixed-point identifiers
in active headers, source, tests, tools, generated data, or build rules.
Windows/MSVC Release passes 47/47 tests, WSL/GCC Release passes 41/41, and the
pinned Emscripten 6.0.3 build passes with implicit conversions treated as
errors. The full 35-domain / 238-case stored registry plus replay passes under
manifest SHA-256
`5099bcdc63d0160c09bcda4248536a02c515892a466342ee9faa139f49542b5c`.
Native and WebAssembly exactly agree on the canonical 240-input replay's
state and event hashes.

## Reconsideration triggers

- Reconsider numeric representation only through a new owner decision backed
  by representative native/Wasm behavior, replay, rollback, and performance
  evidence.
- Reconsider broadphase, layout, dispatch, or snapshot choices independently;
  none requires changing the binary32 simulation contract.

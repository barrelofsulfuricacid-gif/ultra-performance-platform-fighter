# M0 representation Pareto decision

**Status:** Proposed; blocked on the mandatory human playtest/owner checkpoint.

**Measured commit:** `9e757df3d4c222ee55d100bd2db7572b94c748fb`

**Raw evidence:** `performance/m0_representation/`

**Interactive comparison prototype:**
`6f6ebd792a57d8387b9eb5a8ea51bf807e5540cf`

## Measurement validity

- 23 candidates, 15 interleaved samples each, 345 samples total.
- Optimized C17 build using GCC 13.3.0 and `-O3 -march=native`.
- One pinned CPU in a KVM virtualized environment.
- Every calibration and sample starts from the identical seeded state.
- Candidate self-tests cover bounds, exact broadphase hit outcomes, logical
  layout equality, bit-exact dispatch equality, and identical snapshot
  mutation/restore checksums.
- AddressSanitizer and UndefinedBehaviorSanitizer smoke execution passed.
- Linux `perf` counters were unavailable in this container.

The results establish relative direction on this compatibility key. They do not
predict final-engine throughput or compare different machines.

## Measured Pareto table

Ratios are paired medians against the family baseline. Confidence intervals are
deterministic 20,000-resample 95% bootstrap intervals. State bytes are the
experiment's working arrays, not a final engine budget.

| Concern | Leading result | Baseline | Relative result | Decision |
|---|---|---|---:|---|
| Motion arithmetic | Q16.16 fixed | float32 | 1.410× [1.389, 1.440] | Propose Q16.16 |
| Compact cell motion | 256-cell int8 | float32 | 0.636× [0.626, 0.640] | Reject as universal motion |
| Hybrid motion | integer position/float velocity | float32 | 0.141× [0.138, 0.147] | Reject tested form |
| World range | 4096-cell u16 | 256-cell u8 | 1.033× [1.017, 1.055] | Prefer range/precision; speed gain is below noise rule |
| Sparse broadphase | rebuilt sweep | naive | 1.625× [1.591, 1.655] | Keep as sparse challenger |
| Sparse broadphase | 16×16 grid | naive | 1.579× [1.564, 1.603] | Near sparse leader |
| Dense broadphase | 16×16 grid | naive | 2.678× [2.556, 2.764] | Select uniform grid default |
| Dense broadphase | rebuilt sweep | naive | 0.777× [0.762, 0.799] | Reject as universal default |
| Layout | structure of arrays | AoS with cold data | 7.076× [6.966, 7.193] | Select SoA hot pools |
| Layout | hot/cold structs | AoS with cold data | 1.377× [1.331, 1.391] | Use for non-vector pools |
| Dispatch | data table | switch | 2.893× [2.752, 2.937] | Select tables for common math |
| Dispatch | function pointer table | switch | 0.076× [0.074, 0.077] | Reject in hot entity loop |
| Snapshot | tracked 8×64-byte dirty chunks | full 64 KiB copy | 83.812× [83.178, 84.259] | Select tracked dirty prototype |
| Snapshot | scan 64-byte chunks | full copy | 1.866× [1.827, 1.905] | Keep as validation/fallback candidate |

The higher-resolution world result's 3.3% median advantage is below three
times the 1.63% baseline MAD, so it is not claimed as a meaningful speed win.
It is preferred because it avoids the severe precision/range compromise
without demonstrating a material slowdown.

## Proposed architecture candidate A

1. **Q16.16 deterministic motion and geometry.** Use signed 32-bit stored
   values, signed 64-bit intermediates, explicit rounding, and checked range.
   A nominal 4096-unit authored arena envelope leaves ample offstage/blast-zone
   room while retaining subpixel precision.
2. **SoA hot deterministic pools.** Fighters and homogeneous object pools keep
   frequently updated fields in separate arrays. Large diagnostics,
   presentation state, names, and other cold data remain separate. This is not
   a general-purpose ECS.
3. **Uniform-grid broadphase plus exact narrow phase.** The tested 16×16 grid
   is the initial design, with cell size retuned on realistic M4 traces.
   A sweep challenger remains for genuinely sparse static sets. Occupancy
   bitboards are limited to boolean occupancy queries they can represent
   exactly.
4. **Data tables for shared move math; explicit transitions for behavior.**
   Common coefficients and operations use compact tables. Distinct state
   transitions use readable authored C switches. Per-entity function-pointer
   dispatch is prohibited unless a later representative benchmark overturns
   this result.
5. **Mutation-tracked rollback chunks with a full-copy oracle.** Mutation APIs
   mark deterministic chunks. Tests compare every restore against a canonical
   full-copy implementation. If realistic mutation density removes the
   advantage, full copy or scan-delta can replace it behind the snapshot
   interface.

## Gameplay and determinism tradeoff

| Candidate | Precision/range | Cross-target determinism risk | State | Complexity | Expected feel risk |
|---|---|---|---:|---|---|
| float32 | High | Highest; compiler/FMA/denormal policy must be controlled | 16 bytes/actor | Low | Lowest before tuning |
| Q16.16 | High for this game | Low when overflow/rounding is explicit | 16 bytes/actor | Medium | Low, but must be played |
| 256-cell int8 | Very low | Low | 4 bytes/actor | Medium | High: coarse movement and collision |
| tested hybrid | High | Medium; mixed-domain rounding | 24 bytes/actor | High | Medium |

Q16.16 is not selected merely because it is deterministic. In the measured
kernel it was meaningfully faster than float32 with equal stored motion bytes,
while retaining much finer precision than the quantized candidate.

## Rejected candidates and reconsideration triggers

- **Universal 256×256 cell simulation:** reconsider only for a separate coarse
  occupancy/AI representation or if a blind playtest finds no loss and a
  realistic full-engine benchmark wins.
- **Tested hybrid arithmetic:** reconsider only with a materially different
  conversion strategy that removes per-tick mixed-domain rounding and wins
  both native and Wasm tests.
- **Universal occupancy bitboards:** reconsider for stage masks or boolean
  queries; the tested version lost to the grid and cannot provide hit
  attribution by itself.
- **Universal rebuilt sweep:** reconsider if realistic traces are consistently
  sparse enough to beat the grid without dense-case regression.
- **AoS hot loops:** retain only for tiny or behaviorally heterogeneous pools.
- **Function-pointer state dispatch:** reconsider only if real move logic is
  large enough that code locality reverses the microkernel result.
- **Blind delta scan:** retain as a correctness diagnostic or when mutation
  sites cannot mark dirty ranges cheaply.

## Unclosed acceptance item

The blind SDL3 comparison prototype is ready under
`experiments/m0_playtest/`. The plan still requires the owner to play both
hidden candidates and approve a representation. Candidate A therefore remains
proposed, not accepted, until the completed worksheet records that decision.

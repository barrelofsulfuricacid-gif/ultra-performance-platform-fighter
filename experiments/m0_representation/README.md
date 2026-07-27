# M0 representation experiments

These disposable C microkernels compare representation and data-flow choices
before permanent engine code exists. They are not a game prototype and cannot
establish final-engine performance.

## Candidate families

- Motion arithmetic: float32, Q16.16 fixed point, 256-cell integers, and
  integer-position/float-velocity hybrid.
- World coordinate range: 256-cell `uint8_t` and 4096-cell `uint16_t`.
- Sparse and dense broadphase: naive pairs, rebuilt sweep, 16×16 grid, and a
  256×256 occupancy bitboard.
- Entity update layout: array of structures with cold data, structure of
  arrays, and hot/cold split.
- Action dispatch: switch, data table, and function table.
- 64 KiB rollback state: full copy/restore, tracked dirty chunks, and scanned
  changed chunks.

The executable first checks bounds, exact broadphase outcomes, logical layout
equivalence, bit-exact dispatch equivalence, and identical snapshot mutation
and restore behavior. A timing run is invalid if any check fails.

## Run

```sh
experiments/m0_representation/run_benchmarks.sh smoke
experiments/m0_representation/run_benchmarks.sh commit
experiments/m0_representation/run_benchmarks.sh milestone \
  performance/m0_representation
experiments/m0_representation/summarize_results.sh
```

`smoke`, `commit`, and `milestone` collect one, five, and fifteen interleaved
samples per case respectively. Milestone samples target at least 100 ms each.
Every calibration and measured sample starts from the identical seeded state.
Float motion snaps negligible velocity to zero so denormal arithmetic cannot
distort long-duration results.
The summarizer reports median throughput, median absolute deviation, and a
paired bootstrap confidence interval against the defined family baseline.

## Interpretation limits

- These kernels isolate a single concern and omit the interactions of a real
  deterministic simulation.
- This environment is virtualized and is useful only for compatible relative
  comparisons.
- Integer motion candidates have different precision and feel properties; raw
  throughput cannot select one without a human-controlled gameplay prototype.
- Occupancy bitboards answer “any overlap,” not hit attribution. A final
  collision system needs IDs, narrow-phase resolution, and gameplay ordering.
- Tracked dirty snapshots assume mutation sites can cheaply mark bounded
  chunks. Their advantage must be re-measured against realistic engine traces.

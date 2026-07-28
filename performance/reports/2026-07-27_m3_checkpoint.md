# M3 performance checkpoint

**Status:** Qualified; owner review pending

**Measured commit:** `55619230599dddff2833bfc6e90e1bf6172e166c`

## Method

Two clean milestone runs measured each available scenario with 15 calibrated
samples targeting at least 100 ms of work per sample. The first was compatible
with the prior milestone series; the second used the first as its
unchanged-commit baseline. Both runs recorded nine compatible comparisons,
zero invalid comparisons, and zero confirmed regressions.

The meaningful-change threshold is the larger of 1% or three times the
relative median absolute deviation. Milestone decisions also require a
deterministic 2,000-round bootstrap 95% confidence interval to exclude zero.
Per decision D3-C, these are same-host relative measurements, not an absolute
or machine-independent speed claim.

## Public evidence boundary

This public checkpoint intentionally omits the exact host, operating system,
compiler identity, power/thermal metadata, absolute throughput, and local
artifact hashes. Those fields remain in the ignored canonical SQLite history.
The public evidence preserves relative outcomes, statistical envelopes,
scenario availability, and verification status.

## Stability comparison

| Scenario | Relative change | Threshold | Bootstrap 95% CI | Result |
|---|---:|---:|---:|---|
| Empty tick | -0.41% | 3.15% | [-1.07%, +1.98%] | Compatible |
| Representative 1v1 | +5.39% | 7.50% | [+1.72%, +7.58%] | Compatible |
| Representative 2v2 | +3.01% | 4.76% | [+0.70%, +5.52%] | Compatible |
| Snapshot save | +2.59% | 4.23% | [+0.37%, +4.50%] | Compatible |
| Snapshot restore | +2.84% | 7.10% | [+0.73%, +5.55%] | Compatible |
| Rollback re-simulation depth 8 | +10.35% | 31.02% | [+1.63%, +42.89%] | Compatible |
| Replay verification | +0.52% | 4.71% | [-0.63%, +4.98%] | Compatible |
| RL single-environment calls | -1.93% | 3.15% | [-3.10%, -0.09%] | Compatible |
| RL batched step | +0.29% | 1.48% | [-0.36%, +1.03%] | Compatible |

The RL single-environment interval is below zero, but its 1.93% change does
not cross the measured 3.15% meaningful-change threshold; both statistical
conditions are required. Rollback has the widest envelope because run 17 was
noisy, so it is not presented as a meaningful improvement.

## Explicitly unavailable scenarios

| Scenario | Reason |
|---|---|
| Maximum combat entities | Hitbox, hurtbox, projectile, and effect entities enter in M4 |
| Hazard-heavy four-player | Deterministic stage hazards enter in M6 |
| Design-data import | Workbook import and packed design data enter in M5 |
| Client frame | Representative client rendering and frame timing enter in M7 |

Each unavailable result is stored with a machine-readable reason. The graph
snapshot contains an explanatory panel for each unavailable scenario.

## Evidence

- [M3 evolution graphs](2026-07-27_m3_graphs/index.md)
- [M3 profile analysis](../profiles/M3/analysis.md)

The canonical database and generated per-commit data remain local to avoid
commit/benchmark recursion. This report and graph snapshot are the explicit
privacy-safe M3 milestone artifacts.

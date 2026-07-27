# M0 relative-performance charter

## Binding decision

D3-C is in force:

- There is no absolute ticks-per-second release target.
- “Ultra-performant” is a continuing optimization direction, not a
  machine-independent product claim.
- Acceptance depends on reproducible relative comparisons, non-regression, and
  evidence that selected representations beat credible alternatives without
  unacceptable gameplay cost.

## Primary metric

The primary metric is single-thread deterministic headless simulation
throughput:

`logical ticks completed / measured wall-clock second`

The benchmark performs no sleeping, rendering, audio, GUI, networking, logging,
or per-tick allocation.

Secondary metrics:

- Nanoseconds per logical tick.
- p50, p95, and p99 sample time.
- Instructions, cycles, branches, branch misses, cache references, and cache
  misses where platform tools expose them reliably.
- Deterministic state size.
- Snapshot and restore nanoseconds.
- Bytes copied or encoded per snapshot.
- Rollback re-simulation ticks/second at configured depths.
- Batch RL boundary overhead and ticks/second.
- Native binary and WebAssembly size.

## Compatibility key

Two results are comparable only when all of these match:

- Scenario version and seed corpus.
- Experiment or engine ABI/version.
- Commit and dirty-tree state.
- Compiler identity/version and complete flags.
- Linked dependency/content/data hashes.
- CPU architecture and feature set.
- Operating system/kernel and execution environment.
- Benchmark mode, warm-up, sample duration, repetition count, and affinity.

Machine fingerprints are recorded, not normalized away. Results from different
machines may be displayed together but cannot establish a regression or
improvement.

## Current exploratory machine

The initial M0 experiments run in a virtualized development container:

- CPU reported as Intel Xeon Platinum 8573C, x86-64.
- KVM hypervisor.
- Nine visible logical CPUs with one thread per visible core.
- AVX2 and AVX-512 available.
- GCC 13.3.0.
- Linux 6.12.13.

This is not the owner’s reference hardware and cannot support absolute claims.
It is sufficient for same-session and same-environment representation
comparisons. Milestone results must disclose the virtualized environment.

## Canonical scenario families

### M0 representation microkernels

- **numeric_motion:** Four fighters across many independent environments,
  applying acceleration, traction, gravity, fast fall, bounds, and simple
  platform contacts.
- **world_resolution:** Equivalent motion/contact workload at 256×256 and
  higher-resolution logical coordinates.
- **broadphase_sparse:** Four fighters, move hitboxes, and a small stage hazard
  set.
- **broadphase_dense:** Artificial maximum legal hitbox/projectile/hazard load.
- **layout_update:** Identical fighter-state update using AoS, SoA, and hot/cold
  separation.
- **state_dispatch:** Branch/switch versus table/function-data dispatch over the
  same action trace.
- **snapshot_full:** Full deterministic-state copy into a rollback ring.
- **snapshot_delta:** Bounded changed-region/delta encoding and restore.

M0 experiments are directional. They cannot prove final-engine performance
until realistic M4 traces exist.

### Later engine scenarios

- Empty tick overhead.
- Representative 1v1.
- Representative 2v2.
- Maximum simultaneous combat entities.
- Hazard-heavy four-fighter stage.
- Snapshot, restore, and rollback at multiple depths.
- Replay/server verification.
- Single and batched RL stepping.

## Measurement protocol

### Commit mode

- Compile an optimized benchmark with the pinned flags.
- Run a short smoke/correctness pass.
- Warm each case before measurement.
- Collect at least five independent samples per canonical case.
- Target at least 20 ms measured work per sample by adaptive iteration count.
- Record every sample, not just the mean.

### Milestone mode

- Use a clean tree and release benchmark configuration.
- Pin to one eligible CPU when the environment permits it.
- Record governor/power/thermal/virtualization limitations.
- Warm every case through adaptive calibration.
- Collect at least fifteen independent samples per case.
- Randomize candidate order to reduce drift bias.
- Repeat the complete candidate set in interleaved rounds.
- Target at least 100 ms measured work per sample.
- Preserve raw CSV plus a Markdown analysis.

### Correctness before timing

A result is invalid unless:

- Candidate outputs match the scenario’s canonical checksum or tolerance rule.
- No undefined-behavior or sanitizer defect is known in the measured kernel.
- The compiler cannot remove the intended work.
- Candidate semantics are comparable.
- State save/restore candidates reconstruct the same canonical state.

## Statistical rule

For each compatible candidate/baseline pair:

1. Report median throughput and median absolute deviation.
2. Report the relative median change.
3. Bootstrap a 95% confidence interval for the relative median change during
   milestone analysis.
4. Define the environment noise floor from repeated unchanged-baseline runs.

An improvement or regression is meaningful only when:

- Its absolute relative change is at least the greater of 1% or three times the
  measured relative noise floor; and
- The milestone 95% confidence interval excludes zero; and
- Correctness, deterministic state, memory, and other canonical scenarios do
  not hide a material regression.

Commit-mode results may flag a suspected regression but milestone mode decides
architectural selection and M12 merge eligibility.

## Baseline policy

- The first valid run of a scenario/compiler/machine key becomes its baseline.
- A deliberate scenario or compiler change starts a new series; it does not
  rewrite history.
- Every selected optimization records both its immediate parent and the last
  accepted milestone baseline.
- Rebaselining requires a tracked report explaining why comparison continuity
  ended.

## M0 selection rule

Representation selection uses a Pareto table containing:

- Median relative throughput.
- State and snapshot size.
- Snapshot/restore throughput.
- Cross-platform determinism risk.
- Precision/range.
- Implementation complexity.
- Ability to express the approved Melee-feel contract.
- Human playtest scores.

The fastest candidate does not automatically win. It must be meaningfully
faster than credible alternatives or offer another decisive systems advantage,
and it must pass the human feel thresholds.

## Per-commit evidence

The versioned post-commit hook stores:

- Commit hash.
- Verification status.
- Benchmark availability/status.
- Raw output and metadata.
- Failure reason when measurement is unavailable.

Raw per-commit files remain under `performance/local/` and are not committed,
preventing recursive benchmark commits. Milestone definitions, reports,
profiles, and selected summarized results are tracked under `performance/`.

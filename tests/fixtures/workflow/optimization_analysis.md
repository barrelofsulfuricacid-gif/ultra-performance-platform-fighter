# [OPT-SAMPLE-0001] Synthetic lookup compaction

ID: OPT-SAMPLE-0001
Status: pending
Base commit: 1111111111111111111111111111111111111111
Decision: undecided

## Hypothesis

Compacting the synthetic lookup table reduces median lookup time by at least
five percent under the compatible fixture benchmark.

## Invariant contract

All lookup outputs, iteration order, state hashes, and memory bounds remain
identical.

## Baseline

Fixture scenario `lookup-001`, seed 7, compiler `sample-cc`, 30 measured runs.

## Candidate change

Store the synthetic keys in one compact immutable array.

## Benchmark protocol

Five warm-ups followed by 30 paired runs on one fixture machine key.

## Results

Pending; raw results will live under the ignored local evidence directory.

## Correctness evidence

Pending contract and sanitizer checks.

## Decision rationale

Undecided until comparable performance and correctness evidence exist.
